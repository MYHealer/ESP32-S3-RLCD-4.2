// 维护本地温湿度趋势和 24 小时历史样本的内存与 NVS 数据。
#include "sensor_services.h"
#include "sensor_services_internal.h"

#include "app_metadata.h"
#include "app_text_format.h"
#include "hourly_sensor_history_state_internal.h"
#include "i2c_bsp.h"
#include "local_sensor_state_internal.h"
#include "scoped_nvs_handle.h"
#include "sensor_history_format.h"
#include "sensor_time.h"
#include "sensor_trend.h"

#include "i2c_equipment.h"

#include <esp_attr.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs.h>

#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <new>
#include <type_traits>

#define HOURLY_SLOT_KEY_INDEX_INVALID_LOG_FORMAT "hourly sensor slot key index invalid: %d"
#define HOURLY_SLOT_KEY_TRUNCATED_LOG_FORMAT "hourly sensor slot key truncated index=%d"
#define SENSOR_HISTORY_NVS_OPEN_FAILED_LOG_FORMAT "open sensor history nvs failed: %s"
#define HOURLY_SLOT_READ_FAILED_LOG_FORMAT "read hourly sensor slot %s failed: %s"
#define HOURLY_SLOT_INVALID_LOG_FORMAT "hourly sensor slot %s blob invalid"
#define HOURLY_META_READ_FAILED_LOG_FORMAT "read hourly sensor meta failed: %s"
constexpr const char *kHourlyMetaInvalidLog = "hourly sensor meta blob invalid";
#define LEGACY_HOURLY_HISTORY_READ_FAILED_LOG_FORMAT "read legacy hourly sensor history failed: %s"
constexpr const char *kLegacyHourlyHistoryInvalidLog = "legacy hourly sensor history blob invalid";
#define HOURLY_SLOT_INDEX_INVALID_LOG_FORMAT "hourly sensor slot index invalid: %d"
#define SENSOR_NVS_OPEN_FAILED_LOG_FORMAT "open sensor nvs failed: %s"
#define HOURLY_SLOT_SAVE_FAILED_LOG_FORMAT "save hourly sensor slot failed: %s"
#define HOURLY_SNAPSHOT_INVALID_ARG_LOG "hourly sensor snapshot invalid arg"
constexpr const char *kHourlyStateResetFailedLog = "hourly sensor history state reset failed";
constexpr const char *kHourlyStatePublishFailedLog = "hourly sensor history state publish failed";
constexpr const char *kLocalSensorTrendPublishFailedLog = "local sensor trend publish failed";
constexpr const char *kLocalSensorSamplePublishFailedLog = "local sensor sample publish failed";
constexpr const char *kLocalSensorUnavailablePublishFailedLog =
    "local sensor unavailable state publish failed";

namespace {
using sensor_history_format::HourlySensorHistoryMeta;
using sensor_history_format::LegacyHourlySensorHistoryBlob;
using app_storage::ScopedNvsHandle;

constexpr const char *kSensorNvsNamespace = "sensor";
constexpr const char *kHourlyHistoryMetaKey = "hourmeta";
constexpr const char *kLegacyHourlyHistoryKey = "hourly24";
constexpr const char *kHourlySlotKeyFormat = "h%02d";
constexpr size_t kHourlySlotKeyBufferSize = 8;
constexpr int kMsPerSecond = 1000;
constexpr int kUsPerMs = 1000;
constexpr int kSecondsPerMinute = 60;
constexpr int kMinutesPerHour = 60;
constexpr int kSensorTrendWindowHours = 4;
constexpr int64_t kSensorTrendWindowMs = (int64_t)kSensorTrendWindowHours * kMinutesPerHour * kSecondsPerMinute * kMsPerSecond;
struct SensorHistoryLoadWorkspace {
    alignas(HourlySensorHistoryBlob)
        uint8_t current[sizeof(HourlySensorHistoryBlob)];
    alignas(LegacyHourlySensorHistoryBlob)
        uint8_t legacy[sizeof(LegacyHourlySensorHistoryBlob)];
};

alignas(Shtc3Port) unsigned char s_shtc3_storage[sizeof(Shtc3Port)] = {};
Shtc3Port *s_shtc3 = nullptr;
EXT_RAM_BSS_ATTR SensorSample s_sensor_trend_samples[kSensorHistoryMinutes];
// app_main is the sole caller of load_hourly_sensor_history(). Keep both wire
// formats off its stack without introducing startup heap-allocation failures.
EXT_RAM_BSS_ATTR SensorHistoryLoadWorkspace s_sensor_history_load_workspace;
int s_sensor_trend_next = 0;
int s_sensor_trend_count = 0;
bool s_sensor_average_valid = false;
float s_last_temperature_average = 0.0f;
float s_last_humidity_average = 0.0f;
static_assert(kHourlyHistoryCount > 0, "hourly history must keep at least one slot");
static_assert(kLegacyHourlyHistoryCount > 0, "legacy hourly history must keep at least one sample");
static_assert(kHourlyHistoryCount <= 99, "hourly slot key format h%02d supports two-digit indexes");
static_assert(kHourlyHistoryCount >= kLegacyHourlyHistoryCount,
              "new hourly history must cover legacy history samples");
static_assert(sensor_history_format::kHourlyHistoryMetaVersion >
                  sensor_history_format::kLegacyHourlyHistoryVersion,
              "hourly sensor history meta version must be newer than legacy blob version");
static_assert(kHourlySlotKeyBufferSize >= sizeof("h00"), "hourly slot key buffer must fit hNN plus terminator");
static_assert(kSensorSampleDayMinutes > 0, "day sensor sample interval must be positive");
static_assert(kSensorSampleNightMinutes >= kSensorSampleDayMinutes,
              "night sensor sample interval must not be faster than day interval");
static_assert(kSensorTrendWindowMs > 0, "sensor trend window in ms must be positive");
static_assert(kSensorHistoryMinutes >= (kSensorTrendWindowHours * kMinutesPerHour) / kSensorSampleDayMinutes,
              "sensor trend history must cover the full day-sampling trend window");
static_assert(std::is_trivially_destructible<HourlySensorHistoryBlob>::value,
              "hourly history staging storage reuses raw memory without a destructor call");
static_assert(std::is_trivially_destructible<LegacyHourlySensorHistoryBlob>::value,
              "legacy hourly history staging storage reuses raw memory without a destructor call");
static_assert(std::is_trivially_destructible<SensorHistoryLoadWorkspace>::value,
              "sensor history load workspace must not require static destruction");
static_assert(sizeof(s_sensor_history_load_workspace.current) ==
                  sizeof(HourlySensorHistoryBlob),
              "current history workspace must exactly fit its wire object");
static_assert(sizeof(s_sensor_history_load_workspace.legacy) ==
                  sizeof(LegacyHourlySensorHistoryBlob),
              "legacy history workspace must exactly fit its wire object");
bool should_log_nvs_read_error(esp_err_t err)
{
    return err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND;
}

void store_loaded_hourly_sample(HourlySensorHistoryBlob *history,
                                int index,
                                const HourlySensorSample &sample,
                                int64_t *newest_slot)
{
    if (!history || !sensor_history_format::hourly_index_valid(index)) {
        return;
    }
    history->samples[index] = sample;
    if (newest_slot && sample.valid && sample.timestamp > *newest_slot) {
        *newest_slot = sample.timestamp;
    }
}

void store_legacy_hourly_history_samples(const LegacyHourlySensorHistoryBlob &legacy,
                                         HourlySensorHistoryBlob *history,
                                         int64_t *newest_slot)
{
    for (int i = 0; i < kLegacyHourlyHistoryCount; ++i) {
        store_loaded_hourly_sample(history, i, legacy.samples[i], newest_slot);
    }
}

int64_t sensor_trend_now_ms()
{
    return esp_timer_get_time() / kUsPerMs;
}

void append_sensor_history_sample(float temp, float humi)
{
    s_sensor_trend_samples[s_sensor_trend_next].sampled_at_ms = sensor_trend_now_ms();
    s_sensor_trend_samples[s_sensor_trend_next].temperature = temp;
    s_sensor_trend_samples[s_sensor_trend_next].humidity = humi;
    s_sensor_trend_next = (s_sensor_trend_next + 1) % kSensorHistoryMinutes;
    if (s_sensor_trend_count < kSensorHistoryMinutes) {
        ++s_sensor_trend_count;
    }
}

SensorTrendAverage calculate_sensor_history_average()
{
    return calculate_sensor_trend_average(s_sensor_trend_samples,
                                          s_sensor_trend_count,
                                          kSensorHistoryMinutes,
                                          sensor_trend_now_ms(),
                                          kSensorTrendWindowMs);
}

void calculate_sensor_trend_from_average(const SensorTrendAverage &average,
                                          int *temperature_trend,
                                          int *humidity_trend)
{
    if (!temperature_trend || !humidity_trend) {
        return;
    }
    if (average.count <= 0) {
        *temperature_trend = 0;
        *humidity_trend = 0;
        s_sensor_average_valid = false;
        return;
    }
    calculate_sensor_trend_directions(average,
                                      s_sensor_average_valid,
                                      s_last_temperature_average,
                                      s_last_humidity_average,
                                      kTrendEpsilon,
                                      temperature_trend,
                                      humidity_trend);
    s_last_temperature_average = average.temperature;
    s_last_humidity_average = average.humidity;
    s_sensor_average_valid = true;
}
} // namespace

static bool hourly_slot_key(int index, char *out, size_t out_len)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return false;
    }
    if (!sensor_history_format::hourly_index_valid(index)) {
        out[0] = '\0';
        ESP_LOGW(TAG, HOURLY_SLOT_KEY_INDEX_INVALID_LOG_FORMAT, index);
        return false;
    }
    int written = snprintf(out, out_len, kHourlySlotKeyFormat, index);
    if (app_text::format_failed(written, out_len)) {
        out[0] = '\0';
        ESP_LOGW(TAG, HOURLY_SLOT_KEY_TRUNCATED_LOG_FORMAT, index);
        return false;
    }
    return true;
}

static bool load_hourly_sensor_slot(nvs_handle_t nvs,
                                    int index,
                                    HourlySensorHistoryBlob *history,
                                    int64_t *newest_slot)
{
    HourlySensorSample sample = {};
    size_t sample_len = sizeof(sample);
    char key[kHourlySlotKeyBufferSize] = {};
    if (!hourly_slot_key(index, key, sizeof(key))) {
        return false;
    }
    esp_err_t err = nvs_get_blob(nvs, key, &sample, &sample_len);
    if (err == ESP_OK && sample_len == sizeof(sample)) {
        store_loaded_hourly_sample(history, index, sample, newest_slot);
        return true;
    }
    if (err == ESP_OK) {
        ESP_LOGW(TAG, HOURLY_SLOT_INVALID_LOG_FORMAT, key);
        return false;
    }
    if (should_log_nvs_read_error(err)) {
        ESP_LOGW(TAG, HOURLY_SLOT_READ_FAILED_LOG_FORMAT, key, esp_err_to_name(err));
    }
    return false;
}

static inline bool load_current_hourly_sensor_slots(nvs_handle_t nvs,
                                                    const HourlySensorHistoryMeta &meta,
                                                    HourlySensorHistoryBlob *history,
                                                    int64_t *last_saved_at)
{
    if (!history || !last_saved_at) {
        return false;
    }
    int loaded = 0;
    int64_t newest_slot = 0;
    for (int i = 0; i < kHourlyHistoryCount; ++i) {
        if (load_hourly_sensor_slot(nvs, i, history, &newest_slot)) {
            ++loaded;
        }
    }
    if (loaded <= 0) {
        return false;
    }
    *last_saved_at = meta.last_saved_at > newest_slot
                         ? meta.last_saved_at
                         : newest_slot;
    return true;
}

static inline bool read_legacy_hourly_sensor_history(nvs_handle_t nvs,
                                                     LegacyHourlySensorHistoryBlob *legacy)
{
    if (!legacy) {
        return false;
    }
    size_t legacy_len = sizeof(*legacy);
    esp_err_t err = nvs_get_blob(nvs, kLegacyHourlyHistoryKey, legacy, &legacy_len);
    if (err != ESP_OK) {
        if (should_log_nvs_read_error(err)) {
            ESP_LOGW(TAG, LEGACY_HOURLY_HISTORY_READ_FAILED_LOG_FORMAT, esp_err_to_name(err));
        }
        return false;
    }
    if (!sensor_history_format::legacy_history_valid(*legacy, legacy_len)) {
        ESP_LOGW(TAG, "%s", kLegacyHourlyHistoryInvalidLog);
        return false;
    }
    return true;
}

static esp_err_t save_hourly_sensor_meta_and_slot(nvs_handle_t nvs,
                                                  int index,
                                                  int64_t last_saved_at,
                                                  const HourlySensorSample &sample)
{
    HourlySensorHistoryMeta meta = {};
    meta.last_saved_at = last_saved_at;
    esp_err_t err = nvs_set_blob(nvs, kHourlyHistoryMetaKey, &meta, sizeof(meta));
    if (err != ESP_OK) {
        return err;
    }

    char key[kHourlySlotKeyBufferSize] = {};
    if (!hourly_slot_key(index, key, sizeof(key))) {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_set_blob(nvs, key, &sample, sizeof(sample));
}

static void reset_hourly_sensor_history()
{
    if (!reset_hourly_sensor_history_state()) {
        ESP_LOGW(TAG, "%s", kHourlyStateResetFailedLog);
    }
}

void load_hourly_sensor_history()
{
    reset_hourly_sensor_history();
    ScopedNvsHandle nvs;
    esp_err_t err = nvs.open(kSensorNvsNamespace, NVS_READONLY);
    if (err != ESP_OK) {
        if (should_log_nvs_read_error(err)) {
            ESP_LOGW(TAG, SENSOR_HISTORY_NVS_OPEN_FAILED_LOG_FORMAT, esp_err_to_name(err));
        }
        return;
    }

    HourlySensorHistoryBlob *loaded_history =
        new (s_sensor_history_load_workspace.current) HourlySensorHistoryBlob;
    sensor_history_format::initialize_empty_hourly_history(loaded_history);
    int64_t loaded_last_saved_at = 0;
    HourlySensorHistoryMeta meta = {};
    size_t meta_len = sizeof(meta);
    err = nvs_get_blob(nvs.get(), kHourlyHistoryMetaKey, &meta, &meta_len);
    bool meta_valid = err == ESP_OK && sensor_history_format::hourly_meta_valid(meta, meta_len);
    if (meta_valid &&
        load_current_hourly_sensor_slots(nvs.get(),
                                         meta,
                                         loaded_history,
                                         &loaded_last_saved_at)) {
        nvs.close();
        if (!publish_loaded_hourly_sensor_history(*loaded_history,
                                                  loaded_last_saved_at)) {
            ESP_LOGW(TAG, "%s", kHourlyStatePublishFailedLog);
        }
        return;
    }
    if (err == ESP_OK && !meta_valid) {
        ESP_LOGW(TAG, "%s", kHourlyMetaInvalidLog);
    } else if (should_log_nvs_read_error(err)) {
        ESP_LOGW(TAG, HOURLY_META_READ_FAILED_LOG_FORMAT, esp_err_to_name(err));
    }

    LegacyHourlySensorHistoryBlob *legacy =
        new (s_sensor_history_load_workspace.legacy)
            LegacyHourlySensorHistoryBlob;
    bool legacy_loaded = read_legacy_hourly_sensor_history(nvs.get(), legacy);
    nvs.close();
    if (!legacy_loaded) {
        return;
    }
    sensor_history_format::initialize_empty_hourly_history(loaded_history);
    loaded_last_saved_at = 0;
    store_legacy_hourly_history_samples(*legacy,
                                        loaded_history,
                                        &loaded_last_saved_at);
    if (!publish_loaded_hourly_sensor_history(*loaded_history,
                                              loaded_last_saved_at)) {
        ESP_LOGW(TAG, "%s", kHourlyStatePublishFailedLog);
    }
}

static bool save_hourly_sensor_slot(int index,
                                    int64_t last_saved_at,
                                    const HourlySensorSample &sample)
{
    if (!sensor_history_format::hourly_index_valid(index)) {
        ESP_LOGW(TAG, HOURLY_SLOT_INDEX_INVALID_LOG_FORMAT, index);
        return false;
    }
    ScopedNvsHandle nvs;
    esp_err_t err = nvs.open(kSensorNvsNamespace, NVS_READWRITE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, SENSOR_NVS_OPEN_FAILED_LOG_FORMAT, esp_err_to_name(err));
        return false;
    }
    err = save_hourly_sensor_meta_and_slot(nvs.get(), index, last_saved_at, sample);
    if (err == ESP_OK) {
        err = nvs_commit(nvs.get());
    }
    nvs.close();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, HOURLY_SLOT_SAVE_FAILED_LOG_FORMAT, esp_err_to_name(err));
        return false;
    }
    return true;
}

static bool record_hourly_sensor_sample(float temp, float humi)
{
    struct tm local = {};
    if (!is_system_time_plausible(&local)) {
        return false;
    }
    time_t now = mktime(&local);
    time_t hour_start = hour_start_from_time(now);
    bool already_saved = hour_start == hourly_sensor_history_last_saved_at();
    if (hour_start <= 0 || already_saved) {
        return false;
    }
    int index = sensor_history_format::hourly_slot_index_for_time(hour_start);
    HourlySensorSample sample = {};
    sample.timestamp = hour_start;
    sample.temperature = temp;
    sample.humidity = humi;
    sample.valid = 1;
    if (!save_hourly_sensor_slot(index, hour_start, sample)) {
        return false;
    }
    if (!publish_hourly_sensor_sample(index, hour_start, sample)) {
        ESP_LOGW(TAG, "%s", kHourlyStatePublishFailedLog);
        return false;
    }
    return true;
}

bool get_hourly_sensor_history_snapshot(HourlySensorHistoryBlob *history, uint32_t *version)
{
    if (!history && !version) {
        ESP_LOGW(TAG, "%s", HOURLY_SNAPSHOT_INVALID_ARG_LOG);
        return false;
    }
    return hourly_sensor_history_snapshot(history, version);
}

uint32_t get_hourly_sensor_history_version()
{
    return hourly_sensor_history_version_load();
}

static void calculate_updated_sensor_trends(float temp,
                                            float humi,
                                            int *temperature_trend,
                                            int *humidity_trend)
{
    append_sensor_history_sample(temp, humi);
    calculate_sensor_trend_from_average(calculate_sensor_history_average(),
                                        temperature_trend,
                                        humidity_trend);
}

void init_shtc3_sensor(I2cMasterBus &i2c)
{
    if (!s_shtc3) {
        s_shtc3 = new (s_shtc3_storage) Shtc3Port(i2c);
    }
}

bool sample_sensor()
{
    static int consecutive_read_failures = 0;
    float temp = 0.0f;
    float humi = 0.0f;
    bool sensor_ok = s_shtc3 && s_shtc3->Shtc3_ReadTempHumi(&temp, &humi) == 0;
    bool ui_state_changed = false;
    if (sensor_ok) {
        consecutive_read_failures = 0;
        int temperature_trend = 0;
        int humidity_trend = 0;
        calculate_updated_sensor_trends(temp, humi, &temperature_trend, &humidity_trend);
        bool state_published =
            local_sensor_state_publish_sample(temp,
                                              humi,
                                              temperature_trend,
                                              humidity_trend,
                                              &ui_state_changed);
        if (!state_published) {
            ESP_LOGW(TAG, "%s", kLocalSensorSamplePublishFailedLog);
        }
        ui_state_changed |= record_hourly_sensor_sample(temp, humi);
    } else {
        if (consecutive_read_failures <= kLocalSensorReadFailureGraceSamples) {
            ++consecutive_read_failures;
        }
        if (!local_sensor_read_failure_within_grace(consecutive_read_failures)) {
            bool state_published =
                local_sensor_state_publish_unavailable(&ui_state_changed);
            if (!state_published) {
                ESP_LOGW(TAG, "%s", kLocalSensorUnavailablePublishFailedLog);
            }
        }
    }
    return ui_state_changed;
}
