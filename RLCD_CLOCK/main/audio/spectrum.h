// 频谱分析：12 频段能量 + peak hold，用于 MiPlay 播放器 UI 可视化。
#pragma once

#include <cstddef>
#include <cstdint>

constexpr int kSpectrumBands = 12;

// 频谱数据（0.0~1.0 归一化），由 audio_services 更新，UI 读取。
struct SpectrumData {
    float bands[kSpectrumBands];
};

// 从音频回调推送 PCM 样本（降混为单声道，取最新 256 样本）。
void spectrum_push_samples(const int16_t *stereo, size_t frame_count);

// 读取当前频谱数据（线程安全，简单拷贝）。
SpectrumData spectrum_read(void);

// 读取指定频段的 peak hold 值（0.0~1.0）。
float spectrum_peak_hold(int band);

// 清零频谱数据（停止播放时调用，避免残影）。
void spectrum_reset(void);
