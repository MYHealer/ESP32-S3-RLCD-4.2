# MiPlay 遥控接收端组件。
#
# 依赖说明：
#   mdns      — 服务注册与发现
#   lwip      — socket / UDP / TCP
#   esp_netif — 取 STA 网卡 IP
#   mbedtls   — SHA256/MD5（身份派生）与后续的 AES/HMAC（会话握手）
#   esp_wifi  — 网络栈依赖
idf_component_register(
    SRCS
        "miplay_identity.cpp"
        "miplay_discovery.cpp"
        "miplay_proto.cpp"
        "miplay_session.cpp"
        "miplay_safety.cpp"
        "miplay_control.cpp"
        "miplay_media.cpp"
        "miplay_remote.cpp"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "."
    REQUIRES
        mdns
        lwip
        esp_netif
        esp_wifi
        mbedtls
        esp_timer
        nvs_flash
    PRIV_REQUIRES
        efuse
        esp_audio_codec)
