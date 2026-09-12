// 封装 RLCD 显示屏初始化、像素写入和整屏/局部刷新接口。
#include <string.h>
#include <initializer_list>
#include <limits>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include "display_bsp.h"

#define RLCD_TX_FAILED_LOG_FORMAT "RLCD tx failed err=%s len=%d offset=%d dma_free=%u dma_largest=%u"
#define RLCD_PARAM_TX_FAILED_LOG_FORMAT "RLCD %s tx failed value=0x%02x err=%s dma_free=%u dma_largest=%u"
#define RLCD_INIT_INVALID_SIZE_LOG_FORMAT "RLCD invalid display size width=%d height=%d"
#define RLCD_INIT_UNSUPPORTED_GEOMETRY_LOG_FORMAT "RLCD unsupported display geometry width=%d height=%d pixels=%d bytes=%u"
#define RLCD_INIT_STAGE_FAILED_LOG_FORMAT "RLCD %s failed: %s"
#define RLCD_RELEASE_STAGE_FAILED_LOG_FORMAT "RLCD release %s failed: %s"

static constexpr int kRlcdSpiClockHz = 5 * 1000 * 1000;
static constexpr int kRlcdTxChunkBytes = 2048;
static constexpr int kRlcdTxRetryCount = 4;
static constexpr int kRlcdTxRetryBaseDelayMs = 2;
static constexpr int kRlcdTxRetryStepDelayMs = 2;
static constexpr int kRlcdLcdCommandBits = 8;
static constexpr int kRlcdLcdParamBits = 8;
static constexpr int kRlcdSpiMode = 0;
static constexpr int kRlcdSpiTransQueueDepth = 10;
static constexpr int kRlcdPixelsPerByte = 8;
static constexpr int kRlcdLandscapeBlockWidth = 2;
static constexpr int kRlcdLandscapeBlockHeight = 4;
static constexpr char kDisplayLogTag[] = "Display";
static constexpr const char *kRlcdKeepPinsActiveLog = "keep RLCD pins active in light sleep";
static constexpr uint32_t kRlcdSleepOutDelayMs = 200;
static constexpr uint32_t kRlcdResetHighDelayMs = 50;
static constexpr uint32_t kRlcdResetLowDelayMs = 20;
static constexpr TickType_t kRlcdSleepOutDelay = pdMS_TO_TICKS(kRlcdSleepOutDelayMs);
static constexpr TickType_t kRlcdResetHighDelay = pdMS_TO_TICKS(kRlcdResetHighDelayMs);
static constexpr TickType_t kRlcdResetLowDelay = pdMS_TO_TICKS(kRlcdResetLowDelayMs);
static_assert(kRlcdSleepOutDelayMs > 0, "RLCD sleep-out delay must be positive");
static_assert(kRlcdResetHighDelayMs > 0, "RLCD reset high delay must be positive");
static_assert(kRlcdResetLowDelayMs > 0, "RLCD reset low delay must be positive");
static_assert(kRlcdLcdCommandBits > 0, "RLCD command bit width must be positive");
static_assert(kRlcdLcdParamBits > 0, "RLCD parameter bit width must be positive");
static_assert(kRlcdTxChunkBytes > 0, "RLCD transfer chunk size must be positive");
static_assert(kRlcdTxRetryCount > 0, "RLCD retry count must be positive");
static_assert(kRlcdTxRetryBaseDelayMs > 0 && kRlcdTxRetryStepDelayMs >= 0,
              "RLCD normal retry delays must be valid");
static_assert(kRlcdSpiMode >= 0, "RLCD SPI mode must not be negative");
static_assert(kRlcdSpiTransQueueDepth > 0, "RLCD SPI transaction queue depth must be positive");
static_assert(kRlcdPixelsPerByte > 0, "RLCD packed pixel count must be positive");
static_assert(kRlcdLandscapeBlockWidth * kRlcdLandscapeBlockHeight == kRlcdPixelsPerByte,
              "RLCD landscape block must contain one packed byte");
static_assert(kRlcdKeepPinsActiveLog[0] != '\0', "RLCD light-sleep pin log must not be empty");
static_assert(kRlcdSleepOutDelay > 0, "RLCD sleep-out tick delay must be positive");
static_assert(kRlcdResetHighDelay > 0, "RLCD reset high tick delay must be positive");
static_assert(kRlcdResetLowDelay > 0, "RLCD reset low tick delay must be positive");
static int RlcdTxRetryDelayMs(int attempt)
{
    return kRlcdTxRetryBaseDelayMs + attempt * kRlcdTxRetryStepDelayMs;
}

static TickType_t RlcdTxRetryDelayTicks(int attempt)
{
    const TickType_t delay = pdMS_TO_TICKS(RlcdTxRetryDelayMs(attempt));
    return delay > 0 ? delay : 1;
}

static bool RlcdTxCanRetry(esp_err_t err)
{
    return err == ESP_ERR_NO_MEM || err == ESP_ERR_TIMEOUT;
}

template <typename Transmit>
static esp_err_t RlcdTransmitWithRetry(Transmit transmit)
{
    for (int attempt = 0; attempt < kRlcdTxRetryCount; ++attempt) {
        esp_err_t err = transmit();
        if (err == ESP_OK || !RlcdTxCanRetry(err) ||
            attempt + 1 >= kRlcdTxRetryCount) {
            return err;
        }
        vTaskDelay(RlcdTxRetryDelayTicks(attempt));
    }
    return ESP_FAIL;
}

static void LogDisplayAllocationFailure(const char *name, size_t bytes)
{
    ESP_LOGE(kDisplayLogTag,
             "%s allocation failed bytes=%u psram_free=%u psram_largest=%u internal_free=%u dma_largest=%u",
             name,
             (unsigned)bytes,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
}

static void LogDisplayStageFailure(const char *stage, esp_err_t err)
{
    ESP_LOGE(kDisplayLogTag, RLCD_INIT_STAGE_FAILED_LOG_FORMAT, stage, esp_err_to_name(err));
}

static void LogDisplayReleaseFailure(const char *stage, esp_err_t err)
{
    if (err != ESP_OK) {
        ESP_LOGW(kDisplayLogTag, RLCD_RELEASE_STAGE_FAILED_LOG_FORMAT, stage, esp_err_to_name(err));
    }
}

DisplayPort::DisplayPort(int mosi, int scl, int dc, int cs, int rst, int width, int height, spi_host_device_t spihost) :
mosi_(mosi),
scl_(scl),
dc_(dc),
cs_(cs),
rst_(rst),
width_(width),
height_(height),
spihost_(spihost)
{
    if (width_ <= 0 || height_ <= 0 ||
        width_ > std::numeric_limits<uint16_t>::max() ||
        height_ > std::numeric_limits<uint16_t>::max() ||
        width_ > std::numeric_limits<int>::max() / height_) {
        ESP_LOGE(kDisplayLogTag, RLCD_INIT_INVALID_SIZE_LOG_FORMAT, width_, height_);
        return;
    }

    esp_err_t        ret;
    spi_bus_config_t buscfg   = {};
    const int        pixel_count = width_ * height_;
    const size_t     display_len = static_cast<size_t>(pixel_count) /
                                   kRlcdPixelsPerByte;
    const bool       geometry_aligned =
        width_ % kRlcdLandscapeBlockWidth == 0 &&
        height_ % kRlcdLandscapeBlockHeight == 0;
    if (pixel_count % kRlcdPixelsPerByte != 0 ||
        !geometry_aligned) {
        ESP_LOGE(kDisplayLogTag,
                 RLCD_INIT_UNSUPPORTED_GEOMETRY_LOG_FORMAT,
                 width_,
                 height_,
                 pixel_count,
                 static_cast<unsigned>(display_len));
        return;
    }
    buscfg.miso_io_num                   = -1;
    buscfg.mosi_io_num                   = mosi;
    buscfg.sclk_io_num                   = scl;
    buscfg.quadwp_io_num                 = -1;
    buscfg.quadhd_io_num                 = -1;
    buscfg.max_transfer_sz               = kRlcdTxChunkBytes;
    ret                                  = spi_bus_initialize(spihost_, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        LogDisplayStageFailure("SPI bus initialization", ret);
        return;
    }
    spi_bus_initialized_ = true;

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.dc_gpio_num = dc_;
    io_config.cs_gpio_num = cs_;
    io_config.pclk_hz = kRlcdSpiClockHz;
    io_config.lcd_cmd_bits = kRlcdLcdCommandBits;
    io_config.lcd_param_bits = kRlcdLcdParamBits;
    io_config.spi_mode = kRlcdSpiMode;
    io_config.trans_queue_depth = kRlcdSpiTransQueueDepth;

    ret = esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)spihost_, &io_config, &io_handle);
    if (ret != ESP_OK) {
        LogDisplayStageFailure("panel IO creation", ret);
        ReleaseResources();
        return;
    }

    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_conf.mode          = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask  = (0x1ULL << rst_);
    gpio_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    ret = gpio_config(&gpio_conf);
    if (ret != ESP_OK) {
        LogDisplayStageFailure("reset GPIO configuration", ret);
        ReleaseResources();
        return;
    }
    reset_gpio_configured_ = true;

    Set_ResetIOLevel(1);

    DisplayLen = static_cast<int>(display_len);
    // 低功耗 Demo 使用移位寻址，15 KB 帧缓冲可放内部 RAM，无需启动 PSRAM。
    constexpr uint32_t kDisplayBufferCaps = MALLOC_CAP_INTERNAL;
    DispBuffer = (uint8_t *) heap_caps_malloc(DisplayLen, kDisplayBufferCaps);
    if (DispBuffer == NULL) {
        LogDisplayAllocationFailure("RLCD display buffer", DisplayLen);
        ReleaseResources();
        return;
    }

    ready_ = true;
}

DisplayPort::~DisplayPort() {
    ReleaseResources();
}

void DisplayPort::ReleaseResources() {
    ready_ = false;
    initializing_ = false;
    free(DispBuffer);
    DispBuffer = NULL;
    DisplayLen = 0;
    if (reset_gpio_configured_) {
        LogDisplayReleaseFailure("reset GPIO", gpio_reset_pin((gpio_num_t)rst_));
        reset_gpio_configured_ = false;
    }
    if (io_handle) {
        LogDisplayReleaseFailure("panel IO", esp_lcd_panel_io_del(io_handle));
        io_handle = NULL;
    }
    if (spi_bus_initialized_) {
        LogDisplayReleaseFailure("SPI bus", spi_bus_free(spihost_));
        spi_bus_initialized_ = false;
    }
}

bool DisplayPort::IsReady() const {
    return ready_;
}

void DisplayPort::RLCD_Init() {
    if (!ready_) {
        return;
    }
    RLCD_Reset();
    KeepPinsActiveInLightSleep();
    initializing_ = true;

    auto send_register = [this](uint8_t command,
                                std::initializer_list<uint8_t> values) {
        if (!RLCD_SendCommand(command)) {
            return false;
        }
        for (uint8_t value : values) {
            if (!RLCD_SendData(value)) {
                return false;
            }
        }
        return true;
    };

    if (!send_register(0xD6, {0x17, 0x02}) ||
        !send_register(0xD1, {0x01}) ||
        !send_register(0xC0, {0x11, 0x04}) ||
        !send_register(0xC1, {0x69, 0x69, 0x69, 0x69}) ||
        !send_register(0xC2, {0x19, 0x19, 0x19, 0x19}) ||
        !send_register(0xC4, {0x4B, 0x4B, 0x4B, 0x4B}) ||
        !send_register(0xC5, {0x19, 0x19, 0x19, 0x19}) ||
        !send_register(0xD8, {0x80, 0xE9}) ||
        !send_register(0xB2, {0x02}) ||
        !send_register(0xB3,
                       {0xE5, 0xF6, 0x05, 0x46, 0x77,
                        0x77, 0x77, 0x77, 0x76, 0x45}) ||
        !send_register(0xB4,
                       {0x05, 0x46, 0x77, 0x77,
                        0x77, 0x77, 0x76, 0x45}) ||
        !send_register(0x62, {0x32, 0x03, 0x1F}) ||
        !send_register(0xB7, {0x13}) ||
        !send_register(0xB0, {0x64}) ||
        !send_register(0x11, {})) {
        return;
    }
    vTaskDelay(kRlcdSleepOutDelay);
    if (!send_register(0xC9, {0x00}) ||
        !send_register(0x36, {0x48}) ||
        !send_register(0x3A, {0x11}) ||
        !send_register(0xB9, {0x20}) ||
        !send_register(0xB8, {0x29}) ||
        !send_register(0x21, {}) ||
        !send_register(0x2A, {0x12, 0x2A}) ||
        !send_register(0x2B, {0x00, 0xC7}) ||
        !send_register(0x35, {0x00}) ||
        !send_register(0xD0, {0xFF}) ||
        !send_register(0x38, {}) ||
        !send_register(0x29, {})) {
        return;
    }

    initializing_ = false;
    RLCD_ColorClear(ColorWhite);
}

void DisplayPort::KeepPinsActiveInLightSleep(void) {
    const gpio_num_t pins[] = {
        (gpio_num_t)mosi_,
        (gpio_num_t)scl_,
        (gpio_num_t)dc_,
        (gpio_num_t)cs_,
        (gpio_num_t)rst_,
    };

    for (gpio_num_t pin : pins) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_sleep_sel_dis(pin));
    }
    ESP_LOGI(kDisplayLogTag, "%s", kRlcdKeepPinsActiveLog);
}

void DisplayPort::RLCD_ColorClear(uint8_t color) {
    if (!ready_ || !DispBuffer || DisplayLen <= 0) {
        return;
    }
    memset(DispBuffer, color, DisplayLen);
}

void DisplayPort::RLCD_Display() {
    if (!ready_ || !DispBuffer) {
        return;
    }
    if (!RLCD_SendCommand(0x2A) ||     // Column Address Set
        !RLCD_SendData(0x12) ||
        !RLCD_SendData(0x2A) ||
        !RLCD_SendCommand(0x2B) ||     // Page Address Set
        !RLCD_SendData(0x00) ||
        !RLCD_SendData(0xC7) ||
        !RLCD_SendCommand(0x2c)) {     // Memory Write
        return;
    }

	if (!RLCD_Sendbuffera(DispBuffer,DisplayLen)) {
        return;
    }
}

void DisplayPort::RLCD_DisplayXRange(uint16_t x1, uint16_t x2) {
    if (!ready_ || !DispBuffer || x1 >= (uint16_t)width_ ||
        x2 >= (uint16_t)width_ || x1 > x2) {
        return;
    }
    uint16_t start_pair = x1 >> 1;
    uint16_t end_pair = x2 >> 1;
    uint16_t rows_per_pair = height_ >> 2;
    uint32_t offset = (uint32_t)start_pair * rows_per_pair;
    uint32_t len = (uint32_t)(end_pair - start_pair + 1) * rows_per_pair;

    if (!RLCD_SendCommand(0x2A) ||
        !RLCD_SendData(0x12) ||
        !RLCD_SendData(0x2A) ||
        !RLCD_SendCommand(0x2B) ||
        !RLCD_SendData(start_pair & 0xFF) ||
        !RLCD_SendData(end_pair & 0xFF) ||
        !RLCD_SendCommand(0x2c)) {
        return;
    }

	if (!RLCD_Sendbuffera(DispBuffer + offset, len)) {
        return;
    }
}

void DisplayPort::RLCD_Reset(void) {
    Set_ResetIOLevel(1);
    vTaskDelay(kRlcdResetHighDelay);
    Set_ResetIOLevel(0);
    vTaskDelay(kRlcdResetLowDelay);
    Set_ResetIOLevel(1);
    vTaskDelay(kRlcdResetHighDelay);
}

bool DisplayPort::RLCD_SendParamChecked(int command,
                                       const void *data,
                                       size_t data_size,
                                       const char *kind,
                                       uint8_t value) {
    if (!ready_ || !io_handle) {
        return false;
    }
    const esp_err_t err = RlcdTransmitWithRetry([&]() {
        return esp_lcd_panel_io_tx_param(io_handle,
                                         command,
                                         data,
                                         data_size);
    });
    if (err == ESP_OK) {
        return true;
    }

    ESP_LOGW(kDisplayLogTag,
             RLCD_PARAM_TX_FAILED_LOG_FORMAT,
             kind,
             value,
             esp_err_to_name(err),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    if (initializing_) {
        LogDisplayStageFailure("panel register initialization", err);
        ReleaseResources();
    }
    return false;
}

bool DisplayPort::RLCD_SendCommand(uint8_t Reg) {
    return RLCD_SendParamChecked(Reg, NULL, 0, "command", Reg);
}

bool DisplayPort::RLCD_SendData(uint8_t Data) {
    return RLCD_SendParamChecked(-1, &Data, 1, "data", Data);
}

bool DisplayPort::RLCD_WaitForPendingColorTransfers() {
    // tx_color only queues DMA work. A no-op parameter transaction is the
    // documented barrier that waits until the queued color transfers finish.
    return RLCD_SendParamChecked(-1, NULL, 0, "color sync", 0);
}

bool DisplayPort::RLCD_Sendbuffera(uint8_t *Data, int len) {
    if (!ready_ || !io_handle || !Data || len <= 0) {
        return false;
    }
    int offset = 0;
    bool all_chunks_queued = true;
    const int max_chunk = kRlcdTxChunkBytes;
    while (offset < len) {
        int chunk = len - offset;
        if (chunk > max_chunk) {
            chunk = max_chunk;
        }

        const esp_err_t err = RlcdTransmitWithRetry([&]() {
            return esp_lcd_panel_io_tx_color(io_handle,
                                             -1,
                                             Data + offset,
                                             chunk);
        });

        if (err != ESP_OK) {
            ESP_LOGW(kDisplayLogTag,
                     RLCD_TX_FAILED_LOG_FORMAT,
                     esp_err_to_name(err),
                     len,
                     offset,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
            all_chunks_queued = false;
            break;
        }
        offset += chunk;
    }
    bool queue_drained = RLCD_WaitForPendingColorTransfers();
    return all_chunks_queued && queue_drained;
}

void DisplayPort::Set_ResetIOLevel(uint8_t level) {
    gpio_set_level((gpio_num_t) rst_, level ? 1 : 0);
}
void DisplayPort::RLCD_SetPixel(uint16_t x, uint16_t y, uint8_t color) {
    if (!ready_ || !DispBuffer || x >= static_cast<uint16_t>(width_) ||
        y >= static_cast<uint16_t>(height_)) {
        return;
    }

    const uint16_t inv_y = height_ - 1 - y;
    const uint16_t block_y = inv_y >> 2;
    const uint32_t index = static_cast<uint32_t>(x >> 1) *
                               static_cast<uint32_t>(height_ >> 2) +
                           block_y;
    const uint8_t bit = 7 - (((inv_y & 0x03) << 1) | (x & 0x01));
    const uint8_t mask = static_cast<uint8_t>(1U << bit);
    if (color) {
        DispBuffer[index] |= mask;
    } else {
        DispBuffer[index] &= static_cast<uint8_t>(~mask);
    }
}
