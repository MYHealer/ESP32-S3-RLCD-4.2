# WeatherClock 固件独立工程 / Standalone Firmware

本目录可单独下载并编译，无需 `host_web/`、仓库根目录脚本或其他示例工程。
固件源码、板级组件、中文字体、内置图片/音频、分区表及依赖锁均保存在本目录。

安装并激活 ESP-IDF **v5.5.3**，进入本目录执行：

```sh
idf.py build
```

首次构建需联网，由 ESP-IDF 组件管理器按 `dependencies.lock` 下载 LVGL、ESP-SR 等依赖到 `managed_components/`；语音模型由构建过程生成。构建产物在 `build/`，不需要从其他项目目录复制文件。

完整串口烧录并查看日志：

```sh
idf.py -p YOUR_SERIAL_PORT flash monitor
```

`flash` 会写入分区表、应用、OTA 初始化数据和语音模型；需要保留设备原数据时先核对现有分区布局，不执行 `erase-flash`。

公开源码默认使用占位 OTA 地址。自建固件需要在 `main/core/ota_endpoint_local.h` 配置自己的 `WEATHER_CLOCK_OTA_MANIFEST_URL` 和 `WEATHER_CLOCK_OTA_BACKUP_MANIFEST_URL`；该本地配置不能提交。缺少此文件不影响编译。正式发行固件的 OTA 地址由 GitHub Actions 注入。

## English

This directory is a standalone ESP-IDF **v5.5.3** project. Activate that SDK, enter this directory and run `idf.py build`. No sibling directory or repository-root script is required.

Firmware sources, board components, fonts, bundled media, partition configuration and the dependency lock are included here. The first build requires network access to download locked managed components; speech models are generated during the build. Output is written to `build/`.

Public source uses placeholder OTA endpoints unless a local `main/core/ota_endpoint_local.h` defines the two OTA URL macros. This optional configuration is not needed for compilation and must not be committed. Official Release builds inject their endpoints in GitHub Actions.
