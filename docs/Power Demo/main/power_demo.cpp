// 最低功耗单页面固件：每分钟唤醒一次，更新后立即进入深度睡眠。
#include "power_ui.h"

// 私密配置只放本机忽略文件；公开源码缺少该文件时使用脱敏模板，仍可独立编译。
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#else
#include "wifi_secrets.example.h"
#endif

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "display_bsp.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_attr.h"
#include "es8311_codec.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "power_codec.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sys/time.h>
#include <time.h>
#include <type_traits>

namespace {

constexpr char kTag[] = "power_demo";
constexpr char kTimeZone[] = "CST-8";
constexpr const char *kNtpServers[] = {
    "pool.ntp.org",
    "ntp.aliyun.com",
    "time.windows.com",
};
constexpr size_t kNtpServerCount =
    sizeof(kNtpServers) / sizeof(kNtpServers[0]);

constexpr gpio_num_t kDisplayMosi = GPIO_NUM_12;
constexpr gpio_num_t kDisplayClock = GPIO_NUM_11;
constexpr gpio_num_t kDisplayDc = GPIO_NUM_5;
constexpr gpio_num_t kDisplayCs = GPIO_NUM_40;
constexpr gpio_num_t kDisplayReset = GPIO_NUM_41;
constexpr gpio_num_t kAmplifierEnable = GPIO_NUM_46;
constexpr gpio_num_t kKey = GPIO_NUM_18;
constexpr gpio_num_t kI2cSda = GPIO_NUM_13;
constexpr gpio_num_t kI2cScl = GPIO_NUM_14;
constexpr gpio_num_t kAudioMclk = GPIO_NUM_16;
constexpr gpio_num_t kAudioBclk = GPIO_NUM_9;
constexpr gpio_num_t kAudioWordSelect = GPIO_NUM_45;
constexpr gpio_num_t kAudioDataOut = GPIO_NUM_8;

constexpr i2c_port_num_t kI2cPort = I2C_NUM_0;
constexpr uint8_t kRtcAddress = 0x51;
constexpr uint8_t kShtc3Address = 0x70;
constexpr uint32_t kI2cSpeedHz = 100000;
constexpr int kI2cTimeoutMs = 100;
constexpr int kShtc3WakeDelayMs = 50;
constexpr int kShtc3MeasureDelayMs = 20;
constexpr int kShtc3SleepRetryDelayMs = 10;
constexpr int kShtc3SleepAttempts = 2;

constexpr adc_unit_t kBatteryUnit = ADC_UNIT_1;
constexpr adc_channel_t kBatteryChannel = ADC_CHANNEL_3;
constexpr adc_bitwidth_t kBatteryWidth = ADC_BITWIDTH_12;
constexpr adc_atten_t kBatteryAttenuation = ADC_ATTEN_DB_12;
constexpr float kBatteryDivider = 3.0f;
constexpr float kBatteryEmptyVolts = 3.00f;
constexpr float kBatteryFullVolts = 4.12f;

constexpr uint32_t kRetainedMagic = 0x50445732U;
constexpr int kLongPressMs = 1200;
constexpr int kKeyPollMs = 20;
constexpr int kNtpRetryMinutes = 5;
constexpr int kWifiConnectTimeoutMs = 20000;
constexpr int kNtpSyncTimeoutMs = 20000;
constexpr uint64_t kMicrosecondsPerSecond = 1000000ULL;

constexpr EventBits_t kWifiConnectedBit = BIT0;
constexpr EventBits_t kWifiFailedBit = BIT1;
constexpr int kWifiMaxRetries = 8;

struct RetainedState {
    uint32_t magic;
    PowerUiSnapshot ui;
    int last_chime_hour_key;
    int last_ntp_day;
    bool sound_enabled;
    bool key_latched;
    bool ntp_pending;
    uint8_t ntp_retry_wakes;
    PowerSensorReading last_sensor;
};

static_assert(std::is_trivial_v<RetainedState>,
              "RTC retained state must not run constructors after wakeup");

RTC_NOINIT_ATTR RetainedState s_retained;

EventGroupHandle_t s_wifi_events = nullptr;
int s_wifi_retries = 0;
int s_wifi_last_disconnect_reason = 0;

extern const uint8_t hourly_chime_pcm_start[]
    asm("_binary_hourly_chime_pcm_start");
extern const uint8_t hourly_chime_pcm_end[]
    asm("_binary_hourly_chime_pcm_end");

class I2cDevices {
public:
    bool begin()
    {
        i2c_master_bus_config_t bus_config = {};
        bus_config.i2c_port = kI2cPort;
        bus_config.sda_io_num = kI2cSda;
        bus_config.scl_io_num = kI2cScl;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = true;
        esp_err_t error = i2c_new_master_bus(&bus_config, &bus_);
        if (error != ESP_OK) {
            ESP_LOGW(kTag, "I2C 总线初始化失败: %s", esp_err_to_name(error));
            return false;
        }

        i2c_device_config_t rtc_config = {};
        rtc_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        rtc_config.device_address = kRtcAddress;
        rtc_config.scl_speed_hz = kI2cSpeedHz;
        error = i2c_master_bus_add_device(bus_, &rtc_config, &rtc_);
        if (error != ESP_OK) {
            ESP_LOGW(kTag, "RTC 初始化失败: %s", esp_err_to_name(error));
        } else {
            prepare_rtc();
        }

        i2c_device_config_t sensor_config = {};
        sensor_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        sensor_config.device_address = kShtc3Address;
        sensor_config.scl_speed_hz = kI2cSpeedHz;
        error = i2c_master_bus_add_device(bus_, &sensor_config, &sensor_);
        if (error != ESP_OK) {
            ESP_LOGW(kTag, "SHTC3 初始化失败: %s", esp_err_to_name(error));
        }
        return rtc_ || sensor_;
    }

    bool read_rtc(struct tm *local) const
    {
        if (!rtc_ || !local) {
            return false;
        }
        const uint8_t first_register = 0x04;
        uint8_t value[7] = {};
        const esp_err_t error = i2c_master_transmit_receive(
            rtc_, &first_register, 1, value, sizeof(value), kI2cTimeoutMs);
        if (error != ESP_OK) {
            if (!rtc_failure_logged_) {
                ESP_LOGW(kTag, "RTC 读取失败: %s", esp_err_to_name(error));
                rtc_failure_logged_ = true;
            }
            return false;
        }
        if ((value[0] & 0x80U) != 0) {
            if (!rtc_failure_logged_) {
                ESP_LOGW(kTag, "RTC 时间无效: 电压丢失标志已置位");
                rtc_failure_logged_ = true;
            }
            return false;
        }

        struct tm decoded = {};
        decoded.tm_sec = bcd(value[0] & 0x7FU);
        decoded.tm_min = bcd(value[1] & 0x7FU);
        decoded.tm_hour = bcd(value[2] & 0x3FU);
        decoded.tm_mday = bcd(value[3] & 0x3FU);
        decoded.tm_mon = bcd(value[5] & 0x1FU) - 1;
        decoded.tm_year = 100 + bcd(value[6]);
        decoded.tm_isdst = -1;
        if (decoded.tm_year < 124 || decoded.tm_mon < 0 || decoded.tm_mon > 11 ||
            decoded.tm_mday < 1 || decoded.tm_mday > 31 ||
            decoded.tm_hour > 23 || decoded.tm_min > 59 || decoded.tm_sec > 59) {
            return false;
        }
        struct tm normalized = decoded;
        const time_t timestamp = mktime(&normalized);
        if (timestamp < 0 || normalized.tm_year != decoded.tm_year ||
            normalized.tm_mon != decoded.tm_mon ||
            normalized.tm_mday != decoded.tm_mday ||
            normalized.tm_hour != decoded.tm_hour ||
            normalized.tm_min != decoded.tm_min) {
            return false;
        }
        *local = normalized;
        rtc_failure_logged_ = false;
        return true;
    }

    bool write_rtc(const struct tm &local) const
    {
        if (!rtc_) {
            return false;
        }
        const int year = local.tm_year + 1900;
        if (year < 2000 || year > 2099) {
            return false;
        }
        const uint8_t value[8] = {
            0x04,
            to_bcd(local.tm_sec),
            to_bcd(local.tm_min),
            to_bcd(local.tm_hour),
            to_bcd(local.tm_mday),
            static_cast<uint8_t>(local.tm_wday),
            to_bcd(local.tm_mon + 1),
            to_bcd(year % 100),
        };
        return i2c_master_transmit(rtc_, value, sizeof(value), kI2cTimeoutMs) ==
               ESP_OK;
    }

    PowerSensorReading read_sensor() const
    {
        PowerSensorReading reading = {};
        if (!sensor_) {
            return reading;
        }
        const uint8_t wake[] = {0x35, 0x17};
        const uint8_t measure[] = {0x78, 0x66};
        const uint8_t sleep[] = {0xB0, 0x98};
        uint8_t value[6] = {};
        const esp_err_t wake_error =
            i2c_master_transmit(sensor_, wake, sizeof(wake), kI2cTimeoutMs);
        if (wake_error != ESP_OK) {
            ESP_LOGW(kTag, "SHTC3 唤醒失败: %s", esp_err_to_name(wake_error));
            return reading;
        }
        vTaskDelay(pdMS_TO_TICKS(kShtc3WakeDelayMs));
        const esp_err_t measure_error = i2c_master_transmit(
            sensor_, measure, sizeof(measure), kI2cTimeoutMs);
        if (measure_error == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(kShtc3MeasureDelayMs));
            const esp_err_t read_error = i2c_master_receive(
                sensor_, value, sizeof(value), kI2cTimeoutMs);
            if (read_error == ESP_OK && crc8(value, 2) == value[2] &&
                crc8(value + 3, 2) == value[5]) {
                const uint16_t raw_temperature =
                    static_cast<uint16_t>(value[0] << 8U) | value[1];
                const uint16_t raw_humidity =
                    static_cast<uint16_t>(value[3] << 8U) | value[4];
                reading.temperature = -45.0f +
                                      175.0f * raw_temperature / 65536.0f;
                reading.humidity = 100.0f * raw_humidity / 65536.0f;
                reading.available = true;
            } else if (read_error != ESP_OK) {
                ESP_LOGW(kTag, "SHTC3 读取失败: %s", esp_err_to_name(read_error));
            } else {
                ESP_LOGW(kTag, "SHTC3 读取失败: CRC 校验错误");
            }
        } else {
            ESP_LOGW(kTag, "SHTC3 测量启动失败: %s",
                     esp_err_to_name(measure_error));
        }
        esp_err_t sleep_error = ESP_FAIL;
        for (int attempt = 0; attempt < kShtc3SleepAttempts; ++attempt) {
            sleep_error = i2c_master_transmit(
                sensor_, sleep, sizeof(sleep), kI2cTimeoutMs);
            if (sleep_error == ESP_OK) {
                break;
            }
            if (attempt + 1 < kShtc3SleepAttempts) {
                vTaskDelay(pdMS_TO_TICKS(kShtc3SleepRetryDelayMs));
            }
        }
        if (sleep_error != ESP_OK) {
            ESP_LOGW(kTag, "SHTC3 休眠失败: %s", esp_err_to_name(sleep_error));
        }
        return reading;
    }

    i2c_master_bus_handle_t bus() const { return bus_; }

    void end()
    {
        if (sensor_) {
            (void)i2c_master_bus_rm_device(sensor_);
            sensor_ = nullptr;
        }
        if (rtc_) {
            (void)i2c_master_bus_rm_device(rtc_);
            rtc_ = nullptr;
        }
        if (bus_) {
            (void)i2c_del_master_bus(bus_);
            bus_ = nullptr;
        }
    }

private:
    void prepare_rtc() const
    {
        const uint8_t control_register = 0x00;
        uint8_t control = 0;
        const esp_err_t read_error = i2c_master_transmit_receive(
            rtc_, &control_register, 1, &control, 1, kI2cTimeoutMs);
        if (read_error != ESP_OK) {
            ESP_LOGW(kTag, "RTC 控制寄存器读取失败: %s",
                     esp_err_to_name(read_error));
            return;
        }
        // 中文说明：清除 STOP 和 12 小时模式位，让 RTC 持续以 24 小时制运行。
        const uint8_t normalized =
            static_cast<uint8_t>(control & ~(uint8_t{1} << 5U) &
                                 ~(uint8_t{1} << 1U));
        if (normalized == control) {
            return;
        }
        const uint8_t frame[] = {control_register, normalized};
        const esp_err_t write_error =
            i2c_master_transmit(rtc_, frame, sizeof(frame), kI2cTimeoutMs);
        if (write_error != ESP_OK) {
            ESP_LOGW(kTag, "RTC 启动失败: %s", esp_err_to_name(write_error));
        }
    }

    static int bcd(uint8_t value)
    {
        return ((value >> 4U) & 0x0FU) * 10 + (value & 0x0FU);
    }

    static uint8_t to_bcd(int value)
    {
        return static_cast<uint8_t>(((value / 10) << 4U) | (value % 10));
    }

    static uint8_t crc8(const uint8_t *data, size_t length)
    {
        uint8_t crc = 0xFF;
        for (size_t i = 0; i < length; ++i) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc & 0x80U) != 0
                          ? static_cast<uint8_t>((crc << 1U) ^ 0x31U)
                          : static_cast<uint8_t>(crc << 1U);
            }
        }
        return crc;
    }

    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t rtc_ = nullptr;
    i2c_master_dev_handle_t sensor_ = nullptr;
    mutable bool rtc_failure_logged_ = false;
};

void release_deep_sleep_holds()
{
    gpio_deep_sleep_hold_dis();
    constexpr gpio_num_t pins[] = {
        kDisplayMosi,
        kDisplayClock,
        kDisplayDc,
        kDisplayCs,
        kDisplayReset,
        kAmplifierEnable,
    };
    for (const gpio_num_t pin : pins) {
        (void)gpio_hold_dis(pin);
    }
    // 解除保持后立刻恢复空闲电平，避免每日 NTP 等待期间显示控制器复位。
    constexpr struct {
        gpio_num_t pin;
        int level;
    } stable[] = {
        {kDisplayMosi, 0},
        {kDisplayClock, 0},
        {kDisplayDc, 0},
        {kDisplayCs, 1},
        {kDisplayReset, 1},
        {kAmplifierEnable, 0},
    };
    for (const auto &state : stable) {
        (void)gpio_set_direction(state.pin, GPIO_MODE_OUTPUT);
        (void)gpio_set_level(state.pin, state.level);
    }
}

void configure_key()
{
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << kKey;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&config));
}

bool key_long_pressed()
{
    if (gpio_get_level(kKey) != 0) {
        return false;
    }
    for (int elapsed = 0; elapsed < kLongPressMs; elapsed += kKeyPollMs) {
        vTaskDelay(pdMS_TO_TICKS(kKeyPollMs));
        if (gpio_get_level(kKey) != 0) {
            return false;
        }
    }
    return true;
}

bool wait_for_key_release()
{
    for (int attempts = 0; attempts < 250 && gpio_get_level(kKey) == 0;
         ++attempts) {
        vTaskDelay(pdMS_TO_TICKS(kKeyPollMs));
    }
    return gpio_get_level(kKey) != 0;
}

bool init_nvs()
{
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES ||
        error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
        error = nvs_flash_init();
    }
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "NVS 初始化失败: %s", esp_err_to_name(error));
    }
    return error == ESP_OK;
}

bool load_sound_enabled()
{
    nvs_handle_t handle = 0;
    uint8_t enabled = 1;
    if (nvs_open("power_demo", NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_u8(handle, "sound", &enabled);
        nvs_close(handle);
    }
    return enabled != 0;
}

void save_sound_enabled(bool enabled)
{
    nvs_handle_t handle = 0;
    if (nvs_open("power_demo", NVS_READWRITE, &handle) == ESP_OK) {
        (void)nvs_set_u8(handle, "sound", enabled ? 1 : 0);
        (void)nvs_commit(handle);
        nvs_close(handle);
    }
}

int load_ntp_day()
{
    nvs_handle_t handle = 0;
    int32_t day = 0;
    if (nvs_open("power_demo", NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_i32(handle, "ntp_day", &day);
        nvs_close(handle);
    }
    return day;
}

void save_ntp_day(int day)
{
    nvs_handle_t handle = 0;
    if (nvs_open("power_demo", NVS_READWRITE, &handle) == ESP_OK) {
        (void)nvs_set_i32(handle, "ntp_day", day);
        (void)nvs_commit(handle);
        nvs_close(handle);
    }
}

int read_battery_percent()
{
    adc_oneshot_unit_handle_t adc = nullptr;
    adc_oneshot_unit_init_cfg_t unit_config = {};
    unit_config.unit_id = kBatteryUnit;
    if (adc_oneshot_new_unit(&unit_config, &adc) != ESP_OK) {
        return -1;
    }
    adc_oneshot_chan_cfg_t channel_config = {};
    channel_config.atten = kBatteryAttenuation;
    channel_config.bitwidth = kBatteryWidth;
    if (adc_oneshot_config_channel(adc, kBatteryChannel, &channel_config) !=
        ESP_OK) {
        (void)adc_oneshot_del_unit(adc);
        return -1;
    }

    int raw_sum = 0;
    int sample_count = 0;
    for (int i = 0; i < 4; ++i) {
        int raw = 0;
        if (adc_oneshot_read(adc, kBatteryChannel, &raw) == ESP_OK) {
            raw_sum += raw;
            ++sample_count;
        }
    }
    if (sample_count == 0) {
        (void)adc_oneshot_del_unit(adc);
        return -1;
    }
    const int raw = raw_sum / sample_count;
    int millivolts = raw * 3300 / 4095;

    adc_cali_handle_t calibration = nullptr;
    adc_cali_curve_fitting_config_t calibration_config = {};
    calibration_config.unit_id = kBatteryUnit;
    calibration_config.chan = kBatteryChannel;
    calibration_config.atten = kBatteryAttenuation;
    calibration_config.bitwidth = kBatteryWidth;
    if (adc_cali_create_scheme_curve_fitting(&calibration_config,
                                              &calibration) == ESP_OK) {
        (void)adc_cali_raw_to_voltage(calibration, raw, &millivolts);
        (void)adc_cali_delete_scheme_curve_fitting(calibration);
    }
    (void)adc_oneshot_del_unit(adc);

    const float volts = millivolts * 0.001f * kBatteryDivider;
    const float scaled = (volts - kBatteryEmptyVolts) * 100.0f /
                         (kBatteryFullVolts - kBatteryEmptyVolts);
    return std::clamp(static_cast<int>(std::lround(scaled)), 0, 100);
}

void wifi_event_handler(void *, esp_event_base_t base, int32_t event_id,
                        void *event_data)
{
    if (!s_wifi_events) {
        return;
    }
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        (void)esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const auto *event =
            static_cast<const wifi_event_sta_disconnected_t *>(event_data);
        s_wifi_last_disconnect_reason = event ? event->reason : 0;
        if (s_wifi_retries++ < kWifiMaxRetries) {
            (void)esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_events, kWifiFailedBit);
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_retries = 0;
        xEventGroupSetBits(s_wifi_events, kWifiConnectedBit);
    }
}

bool sync_ntp(I2cDevices &devices, struct tm *local)
{
    if (!local) {
        return false;
    }
    bool success = false;
    bool sntp_started = false;
    bool netif_initialized = false;
    bool event_loop_created = false;
    bool wifi_initialized = false;
    bool wifi_started = false;
    esp_netif_t *station = nullptr;
    esp_event_handler_instance_t wifi_handler = nullptr;
    esp_event_handler_instance_t ip_handler = nullptr;

    esp_err_t error = esp_netif_init();
    if (error == ESP_OK) {
        netif_initialized = true;
    } else {
        ESP_LOGW(kTag, "NTP: 网络栈初始化失败: %s", esp_err_to_name(error));
        goto cleanup;
    }
    error = esp_event_loop_create_default();
    if (error == ESP_OK) {
        event_loop_created = true;
    } else {
        ESP_LOGW(kTag, "NTP: 事件循环初始化失败: %s", esp_err_to_name(error));
        goto cleanup;
    }
    station = esp_netif_create_default_wifi_sta();
    if (!station) {
        ESP_LOGW(kTag, "NTP: Wi-Fi STA 网络接口创建失败");
        goto cleanup;
    }
    s_wifi_events = xEventGroupCreate();
    if (!s_wifi_events) {
        ESP_LOGW(kTag, "NTP: Wi-Fi 事件组创建失败");
        goto cleanup;
    }
    s_wifi_retries = 0;
    s_wifi_last_disconnect_reason = 0;

    {
        wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
        init_config.nvs_enable = 0;
        error = esp_wifi_init(&init_config);
        if (error != ESP_OK) {
            ESP_LOGW(kTag, "NTP: Wi-Fi 初始化失败: %s", esp_err_to_name(error));
            goto cleanup;
        }
        wifi_initialized = true;
    }
    error = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                &wifi_event_handler, nullptr,
                                                &wifi_handler);
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "NTP: Wi-Fi 事件注册失败: %s", esp_err_to_name(error));
        goto cleanup;
    }
    error = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                &wifi_event_handler, nullptr,
                                                &ip_handler);
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "NTP: IP 事件注册失败: %s", esp_err_to_name(error));
        goto cleanup;
    }

    {
        wifi_config_t wifi_config = {};
        std::strncpy(reinterpret_cast<char *>(wifi_config.sta.ssid),
                     POWER_DEMO_WIFI_SSID,
                     sizeof(wifi_config.sta.ssid) - 1);
        std::strncpy(reinterpret_cast<char *>(wifi_config.sta.password),
                     POWER_DEMO_WIFI_PASSWORD,
                     sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_config.sta.pmf_cfg.capable = true;
        wifi_config.sta.pmf_cfg.required = false;
        error = esp_wifi_set_mode(WIFI_MODE_STA);
        if (error == ESP_OK) {
            error = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        }
        if (error == ESP_OK) {
            error = esp_wifi_start();
        }
        if (error != ESP_OK) {
            ESP_LOGW(kTag, "NTP: Wi-Fi 启动失败: %s", esp_err_to_name(error));
            goto cleanup;
        }
        wifi_started = true;
        (void)esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    }

    {
        const EventBits_t bits = xEventGroupWaitBits(
            s_wifi_events, kWifiConnectedBit | kWifiFailedBit, pdFALSE, pdFALSE,
            pdMS_TO_TICKS(kWifiConnectTimeoutMs));
        if ((bits & kWifiConnectedBit) == 0) {
            ESP_LOGW(kTag,
                     "NTP: Wi-Fi 连接失败 retries=%d reason=%d",
                     s_wifi_retries,
                     s_wifi_last_disconnect_reason);
            goto cleanup;
        }
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    for (size_t i = 0; i < kNtpServerCount; ++i) {
        esp_sntp_setservername(i, const_cast<char *>(kNtpServers[i]));
    }
    esp_sntp_init();
    sntp_started = true;
    for (int elapsed = 0; elapsed < kNtpSyncTimeoutMs; elapsed += 250) {
        if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        time_t now = 0;
        struct tm ntp_local = {};
        time(&now);
        if (localtime_r(&now, &ntp_local) && ntp_local.tm_year >= 124) {
            *local = ntp_local;
            success = true;
            if (!devices.write_rtc(ntp_local)) {
                ESP_LOGW(kTag, "NTP 已成功，但写入 RTC 失败");
            }
        }
    } else {
        ESP_LOGW(kTag, "NTP: 服务器响应超时");
    }

cleanup:
    if (sntp_started) {
        esp_sntp_stop();
    }
    if (wifi_started) {
        (void)esp_wifi_stop();
    }
    if (wifi_handler) {
        (void)esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler);
    }
    if (ip_handler) {
        (void)esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, ip_handler);
    }
    if (wifi_initialized) {
        (void)esp_wifi_deinit();
    }
    if (s_wifi_events) {
        vEventGroupDelete(s_wifi_events);
        s_wifi_events = nullptr;
    }
    if (station) {
        esp_netif_destroy_default_wifi(station);
    }
    if (event_loop_created) {
        (void)esp_event_loop_delete_default();
    }
    if (netif_initialized) {
        (void)esp_netif_deinit();
    }
    return success;
}

bool play_first_chime(I2cDevices &devices)
{
    constexpr int kSampleRate = 24000;
    constexpr int kSourceChannels = 4;
    constexpr int kSourceSlot = 3;
    constexpr float kVolumeDb = -10.0f;
    constexpr size_t kBufferFrames = 1024;
    constexpr int kWarmupFrames = kSampleRate * 90 / 1000;
    constexpr int kTailFrames = kSampleRate * 40 / 1000;
    static int16_t stereo[kBufferFrames * 2] = {};

    i2s_chan_handle_t tx = nullptr;
    const audio_codec_ctrl_if_t *control = nullptr;
    const audio_codec_gpio_if_t *gpio = nullptr;
    const audio_codec_if_t *codec = nullptr;
    bool success = false;

    do {
        if (!devices.bus()) {
            break;
        }
        i2s_chan_config_t channel =
            I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
        channel.auto_clear = true;
        channel.dma_desc_num = 3;
        channel.dma_frame_num = 64;
        if (i2s_new_channel(&channel, &tx, nullptr) != ESP_OK) {
            break;
        }
        i2s_std_config_t standard = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRate),
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
                .mclk = kAudioMclk,
                .bclk = kAudioBclk,
                .ws = kAudioWordSelect,
                .dout = kAudioDataOut,
                .din = I2S_GPIO_UNUSED,
                .invert_flags = {},
            },
        };
        if (i2s_channel_init_std_mode(tx, &standard) != ESP_OK) {
            break;
        }

        audio_codec_i2c_cfg_t i2c = {
            .port = kI2cPort,
            .addr = ES8311_CODEC_DEFAULT_ADDR,
            .bus_handle = devices.bus(),
        };
        control = audio_codec_new_i2c_ctrl(&i2c);
        gpio = audio_codec_new_gpio();
        if (!control || !gpio) {
            break;
        }
        es8311_codec_cfg_t es8311 = {};
        es8311.ctrl_if = control;
        es8311.gpio_if = gpio;
        es8311.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
        es8311.pa_pin = kAmplifierEnable;
        es8311.use_mclk = true;
        es8311.hw_gain.pa_gain = 6.0f;
        codec = es8311_codec_new(&es8311);
        esp_codec_dev_sample_info_t sample = {};
        sample.bits_per_sample = 16;
        sample.channel = 2;
        sample.sample_rate = kSampleRate;
        if (!codec || codec->set_fs(codec, &sample) != ESP_CODEC_DEV_OK ||
            codec->set_vol(codec, kVolumeDb) != ESP_CODEC_DEV_OK ||
            codec->mute(codec, true) != ESP_CODEC_DEV_OK ||
            codec->enable(codec, true) != ESP_CODEC_DEV_OK ||
            i2s_channel_enable(tx) != ESP_OK) {
            break;
        }

        auto write_frames = [&](size_t frames) {
            size_t written = 0;
            const size_t bytes = frames * 2 * sizeof(stereo[0]);
            return i2s_channel_write(tx, stereo, bytes, &written,
                                     portMAX_DELAY) == ESP_OK &&
                   written == bytes;
        };
        std::memset(stereo, 0, sizeof(stereo));
        bool write_ok = true;
        for (int remaining = kWarmupFrames; remaining > 0;) {
            const size_t frames = std::min(
                static_cast<size_t>(remaining), kBufferFrames);
            write_ok = write_frames(frames);
            if (!write_ok) {
                break;
            }
            remaining -= frames;
        }
        if (!write_ok || codec->mute(codec, false) != ESP_CODEC_DEV_OK) {
            break;
        }

        const size_t source_bytes = hourly_chime_pcm_end - hourly_chime_pcm_start;
        const size_t total_frames =
            source_bytes / (kSourceChannels * sizeof(int16_t));
        const auto *source =
            reinterpret_cast<const int16_t *>(hourly_chime_pcm_start);
        for (size_t offset = 0; offset < total_frames;) {
            const size_t frames = std::min(kBufferFrames, total_frames - offset);
            for (size_t i = 0; i < frames; ++i) {
                const int16_t value =
                    source[(offset + i) * kSourceChannels + kSourceSlot];
                stereo[i * 2] = value;
                stereo[i * 2 + 1] = value;
            }
            if (!write_frames(frames)) {
                write_ok = false;
                break;
            }
            offset += frames;
        }
        std::memset(stereo, 0, sizeof(stereo));
        if (write_ok) {
            write_ok = write_frames(kTailFrames);
        }
        success = write_ok;
    } while (false);

    if (codec) {
        (void)codec->mute(codec, true);
        (void)audio_codec_delete_codec_if(codec);
    }
    if (control) {
        (void)audio_codec_delete_ctrl_if(control);
    }
    if (gpio) {
        (void)audio_codec_delete_gpio_if(gpio);
    }
    if (tx) {
        i2s_chan_info_t info = {};
        if (i2s_channel_get_info(tx, &info) == ESP_OK && info.is_enabled) {
            (void)i2s_channel_disable(tx);
        }
        (void)i2s_del_channel(tx);
    }
    (void)gpio_set_direction(kAmplifierEnable, GPIO_MODE_OUTPUT);
    (void)gpio_set_level(kAmplifierEnable, 0);
    return success;
}

int date_key(const struct tm &local)
{
    return (local.tm_year + 1900) * 10000 +
           (local.tm_mon + 1) * 100 + local.tm_mday;
}

int hour_key(const struct tm &local)
{
    return date_key(local) * 24 + local.tm_hour;
}

bool daily_ntp_due(bool time_valid, const struct tm &local, int last_ntp_day)
{
    return time_valid && local.tm_hour == 0 &&
           local.tm_min < kNtpRetryMinutes &&
           last_ntp_day != date_key(local);
}

bool local_time_plausible(const struct tm &local)
{
    return local.tm_year >= 124 && local.tm_year <= 199 &&
           local.tm_mon >= 0 && local.tm_mon <= 11 &&
           local.tm_mday >= 1 && local.tm_mday <= 31 &&
           local.tm_hour >= 0 && local.tm_hour <= 23 &&
           local.tm_min >= 0 && local.tm_min <= 59 &&
           local.tm_sec >= 0 && local.tm_sec <= 59;
}

bool read_system_local_time(struct tm *local)
{
    if (!local) {
        return false;
    }
    const time_t now = time(nullptr);
    struct tm candidate = {};
    if (now < 0 || !localtime_r(&now, &candidate) ||
        !local_time_plausible(candidate)) {
        return false;
    }
    *local = candidate;
    return true;
}

void seed_system_time_from_rtc(const struct tm &local)
{
    struct tm copy = local;
    const time_t timestamp = mktime(&copy);
    if (timestamp < 0) {
        return;
    }
    const timeval now = {.tv_sec = timestamp, .tv_usec = 0};
    (void)settimeofday(&now, nullptr);
}

void retain_display_pins()
{
    struct PinState {
        gpio_num_t pin;
        int level;
    };
    constexpr PinState states[] = {
        {kDisplayMosi, 0},
        {kDisplayClock, 0},
        {kDisplayDc, 0},
        {kDisplayCs, 1},
        {kDisplayReset, 1},
        {kAmplifierEnable, 0},
    };
    for (const PinState &state : states) {
        (void)gpio_set_direction(state.pin, GPIO_MODE_OUTPUT);
        (void)gpio_set_level(state.pin, state.level);
        (void)gpio_hold_en(state.pin);
    }
    gpio_deep_sleep_hold_en();
}

[[noreturn]] void enter_deep_sleep(int rtc_second)
{
    const bool key_released = wait_for_key_release();
    if (key_released) {
        s_retained.key_latched = false;
    }
    const int seconds = std::clamp(60 - rtc_second, 1, 60);
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        esp_sleep_enable_timer_wakeup(seconds * kMicrosecondsPerSecond));
    if (key_released) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_sleep_enable_ext1_wakeup_io(
            1ULL << kKey, ESP_EXT1_WAKEUP_ANY_LOW));
    }
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    retain_display_pins();
    ESP_LOGI(kTag, "%d 秒后唤醒", seconds);
    esp_deep_sleep_start();
    abort();
}

} // namespace

extern "C" void app_main(void)
{
    setenv("TZ", kTimeZone, 1);
    tzset();
    release_deep_sleep_holds();
    configure_key();

    const bool deep_wakeup = esp_reset_reason() == ESP_RST_DEEPSLEEP;
    bool nvs_ready = false;
    if (!deep_wakeup || s_retained.magic != kRetainedMagic) {
        std::memset(&s_retained, 0, sizeof(s_retained));
        s_retained.magic = kRetainedMagic;
        s_retained.last_chime_hour_key = -1;
        s_retained.ntp_pending = true;
        nvs_ready = init_nvs();
        s_retained.sound_enabled = nvs_ready ? load_sound_enabled() : true;
        s_retained.last_ntp_day = nvs_ready ? load_ntp_day() : 0;
    }

    bool sound_enabled = s_retained.sound_enabled;
    if (gpio_get_level(kKey) != 0) {
        s_retained.key_latched = false;
    }
    const bool sound_toggled = !s_retained.key_latched && key_long_pressed();
    if (sound_toggled) {
        s_retained.key_latched = true;
        sound_enabled = !sound_enabled;
        s_retained.sound_enabled = sound_enabled;
        nvs_ready = init_nvs();
        if (nvs_ready) {
            save_sound_enabled(sound_enabled);
        }
    }

    I2cDevices devices;
    (void)devices.begin();
    struct tm local = {};
    bool rtc_valid = devices.read_rtc(&local);
    bool time_valid = rtc_valid;
    if (rtc_valid) {
        seed_system_time_from_rtc(local);
    } else {
        time_valid = read_system_local_time(&local);
    }
    if (!s_retained.ntp_pending &&
        daily_ntp_due(time_valid, local, s_retained.last_ntp_day)) {
        s_retained.ntp_pending = true;
        s_retained.ntp_retry_wakes = 0;
    }
    if (s_retained.ntp_pending && s_retained.ntp_retry_wakes > 0) {
        --s_retained.ntp_retry_wakes;
    }
    if (s_retained.ntp_pending && s_retained.ntp_retry_wakes == 0) {
        esp_log_level_set(kTag, ESP_LOG_INFO);
        ESP_LOGI(kTag, "开始每日 NTP 校时");
        if (sync_ntp(devices, &local)) {
            time_valid = true;
            struct tm rtc_local = {};
            rtc_valid = devices.read_rtc(&rtc_local);
            if (rtc_valid) {
                local = rtc_local;
            }
            s_retained.ntp_pending = false;
            s_retained.last_ntp_day = date_key(local);
            if (!nvs_ready) {
                nvs_ready = init_nvs();
            }
            if (nvs_ready) {
                save_ntp_day(s_retained.last_ntp_day);
            }
            ESP_LOGI(kTag, "NTP 校时完成");
        } else {
            s_retained.ntp_retry_wakes = kNtpRetryMinutes;
            ESP_LOGW(kTag, "NTP 校时失败，%d 分钟后重试", kNtpRetryMinutes);
        }
        esp_log_level_set(kTag, ESP_LOG_WARN);
    }
    if (!time_valid) {
        // RTC 与网络都不可用时使用明确的占位日期，避免访问无效 tm。
        local.tm_year = 124;
        local.tm_mon = 0;
        local.tm_mday = 1;
        local.tm_isdst = -1;
        (void)mktime(&local);
    }

    bool played_confirmation = false;
    if (sound_toggled) {
        played_confirmation = play_first_chime(devices);
    }
    if (devices.read_rtc(&local)) {
        rtc_valid = true;
        time_valid = true;
    } else if (read_system_local_time(&local)) {
        time_valid = true;
    }
    const int current_hour_key = hour_key(local);
    if (time_valid && played_confirmation && local.tm_min == 0) {
        s_retained.last_chime_hour_key = current_hour_key;
    } else if (time_valid && sound_enabled && local.tm_min == 0 &&
               s_retained.last_chime_hour_key != current_hour_key) {
        if (play_first_chime(devices)) {
            s_retained.last_chime_hour_key = current_hour_key;
        }
        (void)devices.read_rtc(&local);
    }

    PowerSensorReading sensor = devices.read_sensor();
    if (sensor.available) {
        s_retained.last_sensor = sensor;
    } else if (s_retained.last_sensor.available) {
        sensor = s_retained.last_sensor;
    }
    const int battery_percent = read_battery_percent();
    auto *display = new DisplayPort(kDisplayMosi,
                                    kDisplayClock,
                                    kDisplayDc,
                                    kDisplayCs,
                                    kDisplayReset,
                                    400,
                                    300);
    if (display && display->IsReady()) {
        const bool panel_retained = deep_wakeup &&
                                    s_retained.ui.magic == kPowerUiSnapshotMagic;
        if (!panel_retained) {
            display->RLCD_Init();
        }
        power_ui_render(*display, local, sensor, battery_percent, sound_enabled);
        const PowerUiSnapshot current = power_ui_snapshot(
            local, sensor, battery_percent, sound_enabled);
        power_ui_refresh(*display, s_retained.ui, current, !panel_retained);
        s_retained.ui = current;
    } else {
        ESP_LOGW(kTag, "显示初始化失败");
    }

    // 在绘制完成后再读取秒数，保证下一次唤醒尽量对齐整分钟。
    struct tm sleep_time = local;
    bool sleep_time_valid = devices.read_rtc(&sleep_time);
    if (!sleep_time_valid) {
        sleep_time_valid = read_system_local_time(&sleep_time);
    }
    devices.end();
    if (nvs_ready) {
        (void)nvs_flash_deinit();
    }
    enter_deep_sleep(sleep_time_valid ? sleep_time.tm_sec : 0);
}
