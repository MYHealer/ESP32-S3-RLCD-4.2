# 安全政策 / Security Policy

## 中文

### 支持范围

安全修复优先面向 `main` 分支和最新正式版本。较早版本可能不会单独回补；如果问题只存在于旧版本，维护者可能建议先升级到最新版本再验证。

安全问题包括但不限于：

- 配网页、网络请求、OTA 清单或固件下载中的认证与完整性问题。
- Wi-Fi、天气服务、上位机或自定义服务配置的凭据泄露。
- NVS 中敏感配置被未授权读取、覆盖或清除。
- 可由远程输入触发的崩溃、内存破坏、任意代码执行或持久拒绝服务。
- 日志、Release、构建产物或公开源码意外包含私有地址、Token、API Key 或本机路径。

普通功能缺陷、兼容性问题和不涉及安全边界的崩溃请使用普通 Issue。

### 私密报告漏洞

请优先通过本仓库 **Security** 页面中的 **Report a vulnerability** 提交私密报告。在修复可用并完成协调披露之前，请不要创建公开 Issue、公开利用细节或传播可直接复现的攻击代码。

如果私密报告入口不可用，请通过仓库所有者 GitHub 个人资料中公开的联系渠道联系维护者，并明确标注“安全问题”。不要在公开评论中粘贴漏洞细节。

报告建议包含：

- 受影响版本、提交或硬件条件。
- 问题类型、影响和可能的攻击前提。
- 最小复现步骤或概念验证。
- 经过脱敏的日志、请求和响应。
- 你已经尝试过的缓解措施。

请勿在 Issue、Pull Request、评论、日志、截图或提交中公开 Wi-Fi 密码、QWeather API Key、API Host 私有配置、Token、NVS 镜像、设备私钥、本机绝对路径、私有服务地址或其它用户数据。需要说明字段结构时，请使用虚构值或删除敏感部分；截图和串口日志在上传前也必须逐项检查并脱敏。若已误提交，请立即撤销或轮换对应凭据，并通过私密漏洞报告告知维护者，因为删除当前文件不能清除 Git 历史中的泄露内容。

### 处理与披露

维护者会尽力确认报告、评估影响并协调修复时间。响应速度取决于问题复杂度和硬件复现条件，因此不承诺固定 SLA。报告被确认后，维护者会在适当范围内告知处理状态，并在修复发布后决定是否公开安全说明。

对于来自 ESP-IDF、LVGL、`xiaozhi-esp32`、QWeather 或其它第三方依赖的问题，维护者可能要求同时向相应上游项目报告。请遵循上游项目的安全政策，不要把尚未修复的第三方漏洞提前公开。

## English

### Supported Scope

Security fixes primarily target the `main` branch and the latest stable release. Older releases may not receive individual backports. If an issue affects only an older release, maintainers may ask you to upgrade and verify the latest version first.

Security issues include, but are not limited to:

- Authentication or integrity problems in the provisioning portal, network requests, OTA manifests, or firmware downloads.
- Credential exposure involving Wi-Fi, weather services, host applications, or custom service configuration.
- Unauthorized reading, replacement, or deletion of sensitive NVS configuration.
- Remotely triggered crashes, memory corruption, arbitrary code execution, or persistent denial of service.
- Private endpoints, tokens, API keys, or absolute local paths accidentally included in logs, releases, build artifacts, or public source.

Use a regular issue for ordinary functional bugs, compatibility problems, and crashes that do not cross a security boundary.

### Reporting a Vulnerability Privately

Please use **Report a vulnerability** on this repository's **Security** page whenever available. Do not open a public issue, disclose exploit details, or distribute directly usable proof-of-concept code before a fix is available and coordinated disclosure is complete.

If private vulnerability reporting is unavailable, contact the maintainer through a public contact method listed on the repository owner's GitHub profile and clearly mark the message as a security report. Do not paste vulnerability details into public comments.

A useful report should include:

- The affected version, commit, and relevant hardware conditions.
- The issue type, impact, and required attack conditions.
- Minimal reproduction steps or a proof of concept.
- Sanitized logs, requests, and responses.
- Any mitigations you have already tested.

Never expose Wi-Fi passwords, QWeather API keys, private API Host configuration, tokens, NVS images, device private keys, absolute local paths, private service endpoints, or other user data in issues, pull requests, comments, logs, screenshots, or commits. Use fictional values or redact sensitive fields when describing a format, and inspect screenshots and serial logs before upload. If a secret is committed accidentally, revoke or rotate it immediately and notify maintainers through private vulnerability reporting, because deleting the current file does not remove the value from Git history.

### Handling and Disclosure

Maintainers will make a reasonable effort to acknowledge the report, assess the impact, and coordinate a fix. Response time depends on complexity and hardware reproduction requirements, so no fixed SLA is promised. Once confirmed, maintainers will share status when practical and decide whether to publish a security advisory after the fix is released.

For vulnerabilities originating in ESP-IDF, LVGL, `xiaozhi-esp32`, QWeather, or another third-party dependency, maintainers may ask you to report the issue to the relevant upstream project as well. Follow the upstream security policy and avoid disclosing an unpatched third-party vulnerability prematurely.
