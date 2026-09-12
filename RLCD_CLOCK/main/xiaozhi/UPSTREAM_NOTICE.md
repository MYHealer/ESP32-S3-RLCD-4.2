# Xiaozhi protocol reference

This directory adapts the activation and WebSocket/Opus protocol documented by
[`78/xiaozhi-esp32`](https://github.com/78/xiaozhi-esp32), revision
`7b190b78e4f8dfef14126f6cd478c134b3cd3cd8`.
This is an upstream development revision 57 commits after the `v2.2.6` tag;
the full commit hash, rather than a moving branch or approximate release name,
is the reproducible porting baseline.

## Upstream review status

The immutable porting baseline above is separate from the most recently
reviewed upstream state. On 2026-08-31, this port was reviewed against the
formal `v2.4.2` release (`e8d8a4010788afd60f0c8aa3b2e3d0a7bb8f02e5`) and
upstream `main` revision `5df5b7fb4da2b4d80e2b7f87285ec1f8a9ca565c`.

The review found no mandatory WebSocket or MCP compatibility change. Upstream's
playback-drain protection is already covered here by the local final-frame
grace, playback-tail settling, and delayed listening resume. The upstream
single-`AudioEngine`/full-duplex AFE architecture, ESP-IDF 6 migration, dynamic
glyph delivery, streamed notification/Ogg playback, and MQTT/UDP hardening are
not copied because they either do not apply to this WebSocket-only port or
would change the validated RLCD audio lifecycle.

Production remains on ESP-IDF 5.5.3 and ESP-SR 2.3.1. ESP-SR 2.4.7 is retained
only as a possible isolated hardware A/B experiment for full-duplex AEC; it
must not be adopted without repeated wake, long-TTS, page-lifecycle, memory,
power, and codec regression testing. ESP-SR 2.5.x is not a production upgrade
candidate while upstream itself remains on 2.4.7.

The upstream project is MIT licensed. This firmware does not copy its board,
Wi-Fi provisioning, display, OTA, remote-control, MCP server or task framework. It
implements the interoperable activation, binding and audio session messages
against the existing RLCD services. Official `llm.emotion` names are accepted
and rendered as lightweight monochrome faces by the native RLCD UI; upstream
emoji image assets and display code are not copied. The small wake acknowledgement asset
`assets/common/popup.ogg` is reused from that revision and converted at build
time to 16 kHz mono PCM as `components/port_bsp/pcm/xiaozhi_binding/popup.pcm`.

The Xiaozhi-only codec path also follows the upstream Waveshare
ESP32-S3-RLCD-4.2 topology: standard TX, four-slot TDM RX with microphone and
playback-reference selection, device-side AEC, and realtime listening. Wake
listening uses an SR AFE with the model's default WakeNet threshold; after a
wake event it is released before a separate VC AFE with VOIP AEC is created for
the realtime conversation. Only one AFE pipeline is alive at a time. The
existing RLCD network, power, audio-hardware ownership and non-AI playback
services remain authoritative, but the two ESP-SR algorithm pipelines are not
merged merely to reuse code.

The firmware also implements a small independent MCP JSON-RPC adapter for the
official WebSocket envelope. Only local sensor/battery status and speaker
volume are exposed. Page switching, reminder switches, offline mode, remote
restart and display control are deliberately not exposed. Alarm and countdown
handler interfaces are reserved for future local business modules and are not
advertised until a handler is registered.
