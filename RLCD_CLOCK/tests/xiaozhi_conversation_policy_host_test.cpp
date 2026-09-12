// 验证小智 TTS 全程不会因 WakeNet/扬声器回声误触发而中断回复。
#include "xiaozhi_conversation_policy.h"

#include <cassert>

int main()
{
    assert(!xiaozhi_wake_interrupt_allowed(false, false, true, 5000));
    assert(!xiaozhi_wake_interrupt_allowed(true, false, false, 5000));
    assert(!xiaozhi_wake_interrupt_allowed(true,
                                           false,
                                           true,
                                           kXiaozhiWakeInterruptArmDelayMs - 1));
    assert(!xiaozhi_wake_interrupt_allowed(true,
                                           false,
                                           true,
                                           kXiaozhiWakeInterruptArmDelayMs));
    assert(!xiaozhi_wake_interrupt_allowed(true, false, true, 5000));
    assert(!xiaozhi_wake_interrupt_allowed(true, true, true, 5000));

    assert(xiaozhi_microphone_uplink_allowed(false, false));
    assert(!xiaozhi_microphone_uplink_allowed(true, false));
    assert(!xiaozhi_microphone_uplink_allowed(false, true));
    assert(!xiaozhi_microphone_uplink_allowed(true, true));

    static_assert(kXiaozhiEmptyReplyContinuationMs > 0);
    assert(!xiaozhi_turn_reply_is_empty(false, false, false));
    assert(xiaozhi_turn_reply_is_empty(true, false, false));
    assert(!xiaozhi_turn_reply_is_empty(true, true, false));
    assert(!xiaozhi_turn_reply_is_empty(true, false, true));

    constexpr uint32_t kGraceTicks = kXiaozhiTtsFinalFrameGraceMs;
    constexpr uint32_t kTailTicks = 120;
    static_assert(kXiaozhiTtsFinalFrameGraceMs == 700);
    assert(!xiaozhi_tts_final_frames_settled(1699, 1000, 0, kGraceTicks, kTailTicks));
    assert(xiaozhi_tts_final_frames_settled(1700, 1000, 0, kGraceTicks, kTailTicks));
    assert(!xiaozhi_tts_final_frames_settled(2000, 1000, 1881, kGraceTicks, kTailTicks));
    assert(xiaozhi_tts_final_frames_settled(2000, 1000, 1880, kGraceTicks, kTailTicks));
    assert(xiaozhi_tts_final_frames_settled(2000, 0, 0, kGraceTicks, kTailTicks));
    assert(!xiaozhi_tts_final_frames_settled(20,
                                             UINT32_MAX - 300,
                                             0,
                                             kGraceTicks,
                                             kTailTicks));
    assert(xiaozhi_tts_final_frames_settled(400,
                                            UINT32_MAX - 300,
                                            UINT32_MAX - 50,
                                            kGraceTicks,
                                            kTailTicks));

    assert(!xiaozhi_user_requested_exit(nullptr));
    assert(!xiaozhi_user_requested_exit(""));
    assert(xiaozhi_user_requested_exit("关闭小智吧"));
    assert(xiaozhi_user_requested_exit("请停止对话"));
    assert(xiaozhi_user_requested_exit("退下吧。"));
    assert(xiaozhi_user_requested_exit("你退下吧！"));
    assert(!xiaozhi_user_requested_exit("关闭闹钟"));
    assert(!xiaozhi_user_requested_exit("停止番茄钟"));
    assert(!xiaozhi_user_requested_exit("结束今天的闹钟"));
    assert(!xiaozhi_user_requested_exit("退出设置页面"));
    return 0;
}
