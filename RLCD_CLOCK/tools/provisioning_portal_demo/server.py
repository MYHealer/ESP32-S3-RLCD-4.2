#!/usr/bin/env python3
"""在电脑端模拟设备配网页及其保存反馈状态。"""

from __future__ import annotations

import argparse
import html
import re
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse


DEMO_DIR = Path(__file__).resolve().parent
REPO_ROOT = DEMO_DIR.parents[2]
ASSET_HEADER = REPO_ROOT / "RLCD_CLOCK/main/network/wifi_portal_ui_assets.h"

SCENARIOS = {
    "form": ("", "", ""),
    "validating": (
        "pending",
        "正在验证网络配置",
        "设备正在连接 Wi-Fi，并验证天气 API 密钥、API Host 和天气城市，请稍候。",
    ),
    "success": (
        "success",
        "网络连接成功",
        "天气时钟已连接到 Wi-Fi 网络。",
    ),
    "wifi-failed": (
        "",
        "Wi-Fi 连接失败",
        "设备未能连接到该 Wi-Fi。请检查密码、信号和路由器状态后重新填写。",
    ),
    "api-failed": (
        "",
        "天气 API 验证失败",
        "Wi-Fi 已连接，但和风天气验证失败。请检查 API 密钥和账号专属 API Host 后重新填写。",
    ),
    "city-failed": (
        "",
        "天气城市无效",
        "Wi-Fi 与 API 密钥可用，但和风天气无法识别该城市。请修改城市，或留空使用自动定位。",
    ),
}

DEMO_CSS = """
body{padding-bottom:66px}
.demo-toolbar{
  position:fixed;z-index:20;left:50%;bottom:10px;transform:translateX(-50%);
  width:min(calc(100% - 20px),720px);padding:7px;
  display:flex;align-items:center;gap:6px;overflow-x:auto;
  border:1px solid #83909c;border-radius:8px;background:rgba(23,32,42,.96);
  box-shadow:0 8px 24px rgba(0,0,0,.2);color:#fff;
}
.demo-toolbar strong{flex:0 0 auto;padding:0 7px;font-size:12px}
.demo-toolbar a{
  flex:0 0 auto;padding:7px 9px;border:1px solid #66737e;border-radius:5px;
  color:#fff;text-decoration:none;font-size:12px;line-height:1;background:#25313b;
}
.demo-toolbar a.active{border-color:#fff;background:#fff;color:#17202a;font-weight:800}
"""

DEMO_WIFI_NETWORKS = (
    ("Redmi_8FA2", -31),
    ("家庭网络 5G", -45),
    ("WeatherClock-Lab-Long-SSID", -61),
    ("访客网络", -73),
)


def extract_raw_literal(name: str) -> str:
    source = ASSET_HEADER.read_text(encoding="utf-8")
    pattern = (
        rf'inline constexpr char {re.escape(name)}\[\] = '
        r'R"PORTAL\((.*?)\)PORTAL";'
    )
    match = re.search(pattern, source, flags=re.DOTALL)
    if not match:
        raise RuntimeError(f"无法从 {ASSET_HEADER} 读取 {name}")
    return match.group(1)


COMMON_CSS = extract_raw_literal("kCommonCss")
COMMON_SCRIPT = extract_raw_literal("kCommonScript")
FORM_HTML = extract_raw_literal("kFormHtml")


def toolbar(active: str) -> str:
    links = (
        ("form", "表单"),
        ("validating", "验证中"),
        ("success", "成功"),
        ("wifi-failed", "Wi-Fi 失败"),
        ("api-failed", "API 失败"),
        ("city-failed", "城市无效"),
        ("offline", "离线结果"),
    )
    items = ["<nav class='demo-toolbar' aria-label='Demo 状态'><strong>预览状态</strong>"]
    for state, label in links:
        href = "/" if state == "form" else f"/?state={state}"
        active_class = " class='active'" if state == active else ""
        items.append(
            f"<a{active_class} href='{href}'>{html.escape(label)}</a>"
        )
    items.append("</nav>")
    return "".join(items)


def feedback_html(state: str) -> str:
    style, title, body = SCENARIOS.get(state, SCENARIOS["form"])
    if not title:
        return ""
    role = "status" if state in {"validating", "success"} else "alert"
    class_name = f"feedback {style}".strip()
    return (
        f"<div class='{class_name}' role='{role}'>"
        f"<strong>{html.escape(title)}</strong>{html.escape(body)}</div>"
    )


def wifi_list_html() -> str:
    buttons = []
    for ssid, rssi in DEMO_WIFI_NETWORKS:
        safe_ssid = html.escape(ssid, quote=True)
        buttons.append(
            "<button type='button' class='wifi' "
            f"data-ssid='{safe_ssid}' onclick='pick(this.dataset.ssid)'>"
            f"<span>{safe_ssid}</span><b>{rssi} dBm</b></button>"
        )
    return (
        "<section class='wifi-section portal-panel'><div class='portal-panel-body'>"
        "<div class='section-title'><span>附近的 Wi-Fi</span>"
        "<a href='/'>重新扫描</a></div><div class='wifi-list'>"
        + "".join(buttons)
        + "</div></div></section>"
    )


def render_form(state: str = "form") -> str:
    if state == "offline":
        return render_offline_result(True)
    if state not in SCENARIOS:
        state = "form"
    form = FORM_HTML % ("Redmi_8FA2", "备用热点", "杭州")
    return (
        "<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>天气时钟配网 Demo</title>"
        f"<style>{COMMON_CSS}{DEMO_CSS}</style>"
        f"<script>{COMMON_SCRIPT}</script></head><body>"
        f"{toolbar(state)}"
        "<main class='portal-shell'><header class='portal-header'>"
        "<div class='brand-lockup'><div class='brand-mark'>42</div>"
        "<div class='brand-copy'><h1>天气时钟</h1><p>网络、天气与离线时间设置</p></div></div>"
        "<div class='ap-meta'><span>设备热点</span><strong>WeatherClock-Demo</strong></div>"
        "</header><section class='portal-form-shell'>"
        f"{feedback_html(state)}{form}</section>{wifi_list_html()}</main>"
        "</body></html>"
    )


def render_result(state: str) -> str:
    states = {
        "validating": (
            "验证中",
            "正在验证网络配置",
            "设备正在连接 Wi-Fi，并验证天气服务，请稍候。",
        ),
        "success": (
            "已连接",
            "网络连接成功",
            "验证通过，设备即将进入工作状态。",
        ),
        "wifi-failed": (
            "失败",
            "Wi-Fi 连接失败",
            "请检查密码、信号和路由器状态后重新填写。",
        ),
        "api-failed": (
            "失败",
            "天气 API 验证失败",
            "请检查 API 密钥和账号专属 API Host 后重新填写。",
        ),
    }
    badge, title, body = states.get(state, states["validating"])
    poll_script = ""
    if state == "validating":
        poll_script = """
<script>
function poll(){
  fetch("/status",{cache:"no-store"}).then(function(response){
    if(response.status===200){location.replace("/result?state=success");return;}
    if(response.status===409){location.replace("/?state=api-failed");return;}
    setTimeout(poll,700);
  }).catch(function(){setTimeout(poll,900);});
}
setTimeout(poll,600);
</script>
"""
    return (
        "<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>天气时钟配网结果 Demo</title>"
        f"<style>{COMMON_CSS}{DEMO_CSS}</style>{poll_script}</head><body>"
        f"{toolbar(state)}"
        "<main class='result-shell'><section class='portal-panel result-panel'>"
        f"<div class='result-state'>{html.escape(badge)}</div>"
        f"<h1>{html.escape(title)}</h1><p>{html.escape(body)}</p>"
        "<div class='meta'>主 Wi-Fi：Redmi_8FA2<br>备用 Wi-Fi：备用热点<br>API Host：已保存<br>"
        "天气城市：杭州<br>最近一次 Wi-Fi 断开原因：0</div>"
        "<a class='primary-link' href='/'>返回配网页</a>"
        "</section></main></body></html>"
    )


def render_offline_result(saved: bool) -> str:
    badge = "已开启" if saved else "提示"
    title = "离线模式已开启" if saved else "日期或时间无效"
    body = (
        "天气时钟将使用 RTC 时间，并停止所有网络更新。"
        if saved
        else "请输入有效日期和时间，或者填写联网配置。"
    )
    return (
        "<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>天气时钟离线模式 Demo</title>"
        f"<style>{COMMON_CSS}{DEMO_CSS}</style></head><body>{toolbar('offline')}"
        "<main class='result-shell'><section class='portal-panel result-panel'>"
        f"<div class='result-state'>{badge}</div><h1>{title}</h1><p>{body}</p>"
        "<a class='primary-link' href='/'>返回配网页</a>"
        "</section></main></body></html>"
    )


class PortalDemoHandler(BaseHTTPRequestHandler):
    validation_started_at = 0.0

    def log_message(self, fmt: str, *args: object) -> None:
        print(f"[portal-demo] {self.address_string()} {fmt % args}")

    def send_html(self, content: str, status: HTTPStatus = HTTPStatus.OK) -> None:
        body = content.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        request = urlparse(self.path)
        query = parse_qs(request.query)
        if request.path == "/":
            self.send_html(render_form(query.get("state", ["form"])[0]))
            return
        if request.path == "/result":
            self.send_html(render_result(query.get("state", ["validating"])[0]))
            return
        if request.path == "/status":
            ready = time.monotonic() - self.validation_started_at >= 2.4
            self.send_response(HTTPStatus.OK if ready else HTTPStatus.NO_CONTENT)
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        self.send_response(HTTPStatus.FOUND)
        self.send_header("Location", "/")
        self.end_headers()

    def do_POST(self) -> None:
        if urlparse(self.path).path != "/save":
            self.send_error(HTTPStatus.NOT_FOUND)
            return
        content_length = int(self.headers.get("Content-Length", "0"))
        form = parse_qs(self.rfile.read(content_length).decode("utf-8"))
        ssid = form.get("ssid", [""])[0].strip()
        manual_time = form.get("manual_time", [""])[0].strip()
        if not ssid:
            self.send_html(render_offline_result(bool(manual_time)))
            return
        type(self).validation_started_at = time.monotonic()
        self.send_html(render_result("validating"))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="天气时钟配网页电脑调试 Demo")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8096)
    parser.add_argument(
        "--render",
        choices=tuple(SCENARIOS) + ("offline",),
        help="输出指定状态 HTML 后退出，供自动化测试使用",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.render:
        print(render_form(args.render))
        return 0
    server = ThreadingHTTPServer((args.host, args.port), PortalDemoHandler)
    print(f"配网页 Demo 已启动：http://{args.host}:{args.port}/")
    print("按 Ctrl+C 停止。")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
