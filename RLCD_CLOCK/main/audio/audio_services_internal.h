// 声明提醒编排调用的音频生命周期内部接口，不作为业务公共 API。
#pragma once

class CodecPort;

bool audio_try_mark_playing();
void audio_clear_playing();
CodecPort *audio_prepare_codec_for_playback();
void audio_finish_playback();
void park_unused_audio_peripherals();
