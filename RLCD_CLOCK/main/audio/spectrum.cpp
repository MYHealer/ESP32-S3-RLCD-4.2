// 频谱分析：256 点 radix-2 FFT + 12 频段对数映射 + 自动增益 + peak hold。
// 算法参考：
//   - 对数频率映射: SebastianS85/ST7789-audio-spectrum
//   - 自动增益 ExpFilter: zhujisheng/audio-reactive-led-strip
//   - Peak hold 衰减: s-marley/ESP32_FFT_VU
#include "spectrum.h"

#include <math.h>
#include <string.h>

// 256 点样本缓冲区（单声道环形）
static int16_t s_samples[256];
static int s_write_pos = 0;
static SpectrumData s_spectrum;
static float s_peak_hold[kSpectrumBands] = {};
static float s_auto_gain = 1.0f;

void spectrum_push_samples(const int16_t *stereo, size_t frame_count)
{
    if (!stereo || frame_count == 0) return;
    for (size_t i = 0; i < frame_count; i++) {
        int32_t mono = (int32_t)stereo[i * 2] + (int32_t)stereo[i * 2 + 1];
        s_samples[s_write_pos] = (int16_t)(mono >> 1);
        s_write_pos = (s_write_pos + 1) & 255;
    }
}

// 对数频率分布：40Hz~12kHz，每频段按指数增长分配 bin 范围。
// 256 点 FFT @44100Hz → 172Hz/bin，有效范围 bin 1~127 (40Hz~12kHz)。
static void compute_log_band_bins(uint8_t *out_start, uint8_t *out_end)
{
    constexpr float kMinHz = 40.0f;
    constexpr float kMaxHz = 12000.0f;
    constexpr float kBinHz = 44100.0f / 256.0f;  // ≈172.3Hz
    constexpr int kMaxBin = 127;
    constexpr int kBands = kSpectrumBands;

    float log_min = logf(kMinHz);
    float log_max = logf(kMaxHz);
    for (int b = 0; b < kBands; b++) {
        float f0 = expf(log_min + (log_max - log_min) * b / kBands);
        float f1 = expf(log_min + (log_max - log_min) * (b + 1) / kBands);
        int bin0 = (int)(f0 / kBinHz + 0.5f);
        int bin1 = (int)(f1 / kBinHz + 0.5f);
        if (bin0 < 1) bin0 = 1;
        if (bin1 > kMaxBin) bin1 = kMaxBin;
        if (bin1 <= bin0) bin1 = bin0 + 1;
        out_start[b] = (uint8_t)bin0;
        out_end[b] = (uint8_t)bin1;
    }
}

SpectrumData spectrum_read(void)
{
    // 对数频段 bin 范围（首次调用时计算，static 缓存）
    static uint8_t band_start[kSpectrumBands], band_end[kSpectrumBands];
    static bool band_init = false;
    if (!band_init) {
        compute_log_band_bins(band_start, band_end);
        band_init = true;
    }

    // 拷贝样本到连续缓冲（按时间顺序），归一化到 [-1, 1]
    static float re[256], im[256];
    for (int i = 0; i < 256; i++) {
        re[i] = (float)s_samples[(s_write_pos + i) & 255] * (1.0f / 32768.0f);
        im[i] = 0.0f;
    }

    // 256 点 radix-2 FFT（Cooley-Tukey）
    // 位反转置换
    for (int i = 1, j = 0; i < 256; i++) {
        int bit = 128;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    // 蝶形运算
    for (int len = 2; len <= 256; len <<= 1) {
        float ang = -2.0f * (float)M_PI / len;
        float w_re = cosf(ang), w_im = sinf(ang);
        for (int i = 0; i < 256; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int j = 0; j < len / 2; j++) {
                int u = i + j, v = u + len / 2;
                float tr = cr * re[v] - ci * im[v];
                float ti = cr * im[v] + ci * re[v];
                re[v] = re[u] - tr; im[v] = im[u] - ti;
                re[u] += tr;         im[u] += ti;
                float nr = cr * w_re - ci * w_im;
                ci = cr * w_im + ci * w_re;
                cr = nr;
            }
        }
    }

    // 12 频段：取每段均方根能量
    SpectrumData result;
    float raw_bands[kSpectrumBands];
    float raw_peak = 0.0f;
    for (int b = 0; b < kSpectrumBands; b++) {
        float energy = 0.0f;
        int count = band_end[b] - band_start[b];
        if (count < 1) count = 1;
        for (int k = band_start[b]; k < band_end[b]; k++) {
            float mag = re[k] * re[k] + im[k] * im[k];
            energy += mag;
        }
        float rms = sqrtf(energy / count);
        raw_bands[b] = rms;
        if (rms > raw_peak) raw_peak = rms;
    }

    // 自动增益：追踪全局峰值，慢衰减（ExpFilter style）
    // 峰值跟踪 → 归一化到 0~1，让小信号和大信号都能满幅
    if (raw_peak > s_auto_gain) {
        s_auto_gain = raw_peak;  // 快速上升
    } else {
        s_auto_gain = s_auto_gain * 0.998f + raw_peak * 0.002f;  // 极慢衰减
    }
    float inv_gain = (s_auto_gain > 0.001f) ? (1.0f / s_auto_gain) : 1.0f;

    for (int b = 0; b < kSpectrumBands; b++) {
        // 自动增益归一化
        float v = raw_bands[b] * inv_gain;
        if (v > 1.0f) v = 1.0f;
        // gamma 压缩（cbrt 让小信号更饱满）
        v = cbrtf(v);

        // 平滑：快速上升（75%新值），慢速下降（30%新值）
        float old = s_spectrum.bands[b];
        result.bands[b] = (v > old) ? (old * 0.25f + v * 0.75f)
                                     : (old * 0.7f + v * 0.3f);

        // Peak hold：高于当前值立即更新，否则缓慢衰减
        if (result.bands[b] > s_peak_hold[b]) {
            s_peak_hold[b] = result.bands[b];
        } else {
            s_peak_hold[b] *= 0.97f;  // 每帧衰减 3%
        }
        if (s_peak_hold[b] < result.bands[b]) {
            s_peak_hold[b] = result.bands[b];
        }
    }
    s_spectrum = result;
    return result;
}

float spectrum_peak_hold(int band)
{
    if (band < 0 || band >= kSpectrumBands) return 0.0f;
    return s_peak_hold[band];
}

void spectrum_reset(void)
{
    memset(s_samples, 0, sizeof(s_samples));
    s_write_pos = 0;
    s_spectrum = {};
    memset(s_peak_hold, 0, sizeof(s_peak_hold));
    s_auto_gain = 1.0f;
}
