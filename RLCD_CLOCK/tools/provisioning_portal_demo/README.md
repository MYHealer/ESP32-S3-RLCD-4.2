# 配网页电脑调试 Demo

这个 Demo 用于在电脑浏览器里调试设备配网页的布局和交互，不需要连接 ESP32。

Demo 直接读取固件中的 `wifi_portal_ui_assets.h`，因此 CSS、基础交互脚本和表单字段与设备端共用同一份资源。页面底部的状态工具栏只在 Demo 中出现，不会进入固件。

## 启动

在项目根目录执行：

```bash
scripts/preview_provisioning_portal.sh
```

浏览器访问：

```text
http://127.0.0.1:8096/
```

可以预览表单、验证中、成功、Wi-Fi 失败、API 失败、城市无效和离线结果。提交一份带 Wi-Fi 名称的表单后，Demo 会先显示验证中，并在约 2.4 秒后模拟验证成功。

如需使用其它端口：

```bash
scripts/preview_provisioning_portal.sh 8100
```

## 维护规则

- 调整正式配网页的 CSS、基础脚本或表单结构时，修改 `RLCD_CLOCK/main/network/wifi_portal_ui_assets.h`。
- Demo 专属状态工具栏和模拟数据只修改本目录的 `server.py`。
- 不在 Demo 中复制另一套正式 CSS 或设备业务校验逻辑。
- 修改后运行 `scripts/tests/host/network/test_setup_portal_demo_host.sh`，并检查手机和桌面宽度截图。
