// 声明 RLCD 显示屏尺寸、颜色和刷新接口。
#pragma once

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>


enum ColorSelection {
    ColorBlack = 0,
    ColorWhite = 0xff
};

class DisplayPort {
  private:
    esp_lcd_panel_io_handle_t io_handle = NULL;
    int                 mosi_;
    int                 scl_;
    int                 dc_;
    int                 cs_;
    int                 rst_;
    int                 width_;
    int                 height_;
    spi_host_device_t   spihost_;
    uint8_t            *DispBuffer = NULL;
    int                 DisplayLen = 0;
    bool                spi_bus_initialized_ = false;
    bool                reset_gpio_configured_ = false;
    bool                ready_ = false;
    bool                initializing_ = false;
    void ReleaseResources();
    void Set_ResetIOLevel(uint8_t level);
    bool RLCD_SendParamChecked(int command, const void *data, size_t data_size, const char *kind, uint8_t value);
    bool RLCD_SendCommand(uint8_t Reg);
    bool RLCD_SendData(uint8_t Data);
    bool RLCD_WaitForPendingColorTransfers();
    bool RLCD_Sendbuffera(uint8_t *Data, int len);
    void RLCD_Reset(void);
    void KeepPinsActiveInLightSleep(void);

  public:
    DisplayPort(int mosi, int scl, int dc, int cs, int rst, int width, int height, spi_host_device_t spihost = SPI3_HOST);
    ~DisplayPort();
    DisplayPort(const DisplayPort &) = delete;
    DisplayPort &operator=(const DisplayPort &) = delete;
    bool IsReady() const;
    void RLCD_Init();
    void RLCD_ColorClear(uint8_t color);
    void RLCD_Display();
    void RLCD_DisplayXRange(uint16_t x1, uint16_t x2);
    void RLCD_SetPixel(uint16_t x, uint16_t y, uint8_t color);
};
