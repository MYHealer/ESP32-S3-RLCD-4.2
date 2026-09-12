# Power Demo

这是面向 Waveshare ESP32-S3-RLCD-4.2 的独立最低功耗时钟 Demo。`Power Demo` 本身就是完整的 ESP-IDF 工程，可单独复制到其他位置编译；它不引用同级完整版源码目录。工程只有一个页面，不包含设置菜单、配网页面、闹钟、OTA、双应用分区或其他业务页面。

## 保留的功能

- 三个时间卡片依次显示 `AM/PM`、12 小时制小时、分钟，不显示秒。
- 保留电池、声音、顶部日期/星期、原版顶部黑色分割条、日进度条、温湿度、舒适度表情、日期和农历信息。
- 顶部 Wi-Fi 和闹钟图标已删除；Wi-Fi 只在校时期间短暂工作，界面不会显示它。
- 每次唤醒采样一次 SHTC3；定时唤醒对齐下一整分钟，其余时间进入深度睡眠。
- 冷启动或跨日时整屏刷新；普通分钟更新通过 `RLCD_DisplayXRange()` 只传输变化的纵向区域。
- 每次冷启动都会先发起 NTP 校时，避免 RTC 数值虽然合法但已经走偏；每天 `00:00` 再同步一次。失败后每 5 分钟重试，成功后立即关闭 Wi-Fi。
- `Keys`（GPIO 18）长按约 1.2 秒切换声音，并写入 NVS。开启时保留顶部声音图标和整点报时。
- 固件只嵌入原声音列表第一个 `hourly_chime.pcm`；开启、关闭确认和整点报时均使用这一份声音。

## 最小化范围

- UI 直接写 15 KB 的 1-bit 帧缓冲，不创建 LVGL 页面、任务或动画。温湿度、日期和农历均从完整版原字体裁出必要字形，并预合成原版的叠字加粗效果，运行时只绘制一遍。
- 声音只编译 ES8311 需要的控制、GPIO、音量和芯片驱动源文件，不带通用 `codec_board`、麦克风、SD 卡或 FATFS 路径。
- Wi-Fi 仅保留 STA + WPA2 校时能力；WPA3、SoftAP、企业认证和 Wi-Fi NVS 均关闭。
- 只有一个 2 MB factory 应用分区，没有 OTA 数据分区或第二应用分区。
- 显示驱动、必要数码和表情资源、农历算法、ES8311 最小驱动以及唯一 PCM 都放在本目录内。
- 不含私密 Wi-Fi 文件和本机 `sdkconfig` 的干净构建已通过，`power_demo.bin` 为 `0x1340a0` 字节；其中唯一 PCM 资源为 545,952 字节。

## 独立工程结构

```text
Power Demo/
├── CMakeLists.txt
├── components/
│   ├── power_display/      # 本 Demo 使用的最小 RLCD 驱动
│   └── power_codec/        # ES8311 播放所需的最小驱动
├── main/
│   ├── assets/             # 数码、表情和唯一 PCM
│   ├── fonts/              # 页面实际用到的字体子集
│   ├── calendar_lunar.*    # 本地农历计算
│   ├── power_demo.cpp
│   └── power_ui.cpp
├── partitions.csv
└── sdkconfig.defaults
```

## 私密 Wi-Fi 配置

真实凭据只允许放在本机文件 `main/wifi_secrets.h`。这个文件和整个 `build/` 已写入 `.gitignore`，GitHub 中只保留脱敏模板：

```sh
cd "Power Demo"
cp main/wifi_secrets.example.h main/wifi_secrets.h
```

然后只在本机编辑两个宏。不要把真实凭据写回模板。

注意：真实 Wi-Fi 密码也会存在于本机编译出的 `power_demo.bin` 中。因此不仅不能提交 `wifi_secrets.h`，也不能发布用真实密码构建的 BIN、ELF、MAP 或完整 `build/`。需要公开发布二进制时，必须先换成专用测试网络凭据并重新构建。

## 编译

本机当前 ESP-IDF 是 5.5.3。复制整个 `Power Demo` 目录后，进入该目录并加载 ESP-IDF 环境：

```sh
cd "Power Demo"
source /path/to/esp-idf/export.sh
idf.py build
```

如果 `command -v xtensa-esp32s3-elf-gcc` 没有输出，说明当前 ESP-IDF 环境没有把 ESP32-S3 Xtensa 工具链加入 `PATH`。请先按 ESP-IDF 5.5.3 安装流程补齐或激活对应工具链，再执行 `idf.py build`；编译器属于 ESP-IDF 开发环境，不需要复制进本工程。

## 烧录

插入开发板后先查询串口：

```sh
ls /dev/cu.* 2>/dev/null | rg 'usb(modem|serial)'
```

把下面的 `PORT` 换成查到的设备名：

```sh
idf.py -p PORT flash monitor
```

只烧录、不进入串口监视：

```sh
idf.py -p PORT flash
```

也可以直接使用构建输出给出的底层命令：

```sh
python -m esptool --chip esp32s3 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/power_demo.bin
```

## 功耗边界

代码已关闭稳态 Wi-Fi、让 SHTC3 回到 sleep、关闭功放，并在刷新后进入 ESP32 深度睡眠。实际整机功耗还取决于显示控制器、电源芯片和板级漏电，不能仅凭代码声称达到某个微安数值。首次烧录后应实测：

1. 深睡时显示画面是否保持；
2. 下一分钟是否只做局部刷新；
3. GPIO 18 是否可以从深睡唤醒；
4. USB 线、串口芯片和充电电路对静态电流的影响。

如果板上显示电源在深睡时被切断，显示控制器不会保留 GRAM；此时必须每分钟重新初始化并整屏刷新，无法同时保证真局刷。
