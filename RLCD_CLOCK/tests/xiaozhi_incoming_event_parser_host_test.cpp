// 验证小智 WebSocket TTS、STT、LLM 入站文本事件的分类语义。
#include "xiaozhi_incoming_event_parser.h"

#include <assert.h>
#include <string.h>

int main()
{
    XiaozhiIncomingEvent event;

    assert(!event.parse(nullptr, 0));
    assert(!event.parse("{", 1));
    assert(event.type() == XiaozhiIncomingEventType::kUnknown);
    assert(event.text() == nullptr && event.emotion() == nullptr);

    const char tts_start[] = "{\"type\":\"tts\",\"state\":\"start\"}";
    assert(event.parse(tts_start, sizeof(tts_start) - 1));
    assert(event.type() == XiaozhiIncomingEventType::kTtsStart);
    assert(event.text() == nullptr && event.emotion() == nullptr);

    const char tts_stop[] = "{\"type\":\"tts\",\"state\":\"stop\"}";
    assert(event.parse(tts_stop, sizeof(tts_stop) - 1));
    assert(event.type() == XiaozhiIncomingEventType::kTtsStop);

    const char sentence[] =
        "{\"type\":\"tts\",\"state\":\"sentence_start\",\"text\":\"你好\"}";
    assert(event.parse(sentence, sizeof(sentence) - 1));
    assert(event.type() == XiaozhiIncomingEventType::kTtsSentenceStart);
    assert(strcmp(event.text(), "你好") == 0);

    const char sentence_without_text[] =
        "{\"type\":\"tts\",\"state\":\"sentence_start\"}";
    assert(event.parse(sentence_without_text, sizeof(sentence_without_text) - 1));
    assert(event.type() == XiaozhiIncomingEventType::kUnknown);

    const char stt[] = "{\"type\":\"stt\",\"text\":\"退下吧\"}";
    assert(event.parse(stt, sizeof(stt) - 1));
    assert(event.type() == XiaozhiIncomingEventType::kStt);
    assert(strcmp(event.text(), "退下吧") == 0);
    assert(event.emotion() == nullptr);

    const char stt_without_text[] = "{\"type\":\"stt\"}";
    assert(event.parse(stt_without_text, sizeof(stt_without_text) - 1));
    assert(event.type() == XiaozhiIncomingEventType::kUnknown);

    const char llm[] = "{\"type\":\"llm\",\"emotion\":\"happy\"}";
    assert(event.parse(llm, sizeof(llm) - 1));
    assert(event.type() == XiaozhiIncomingEventType::kLlm);
    assert(strcmp(event.emotion(), "happy") == 0);

    const char llm_without_emotion[] = "{\"type\":\"llm\"}";
    assert(event.parse(llm_without_emotion, sizeof(llm_without_emotion) - 1));
    assert(event.type() == XiaozhiIncomingEventType::kLlm);
    assert(event.emotion() == nullptr);

    const char unknown_state[] = "{\"type\":\"tts\",\"state\":\"unknown\"}";
    assert(event.parse(unknown_state, sizeof(unknown_state) - 1));
    assert(event.type() == XiaozhiIncomingEventType::kUnknown);

    const char unknown_type[] = "{\"type\":\"custom\",\"text\":\"ignored\"}";
    assert(event.parse(unknown_type, sizeof(unknown_type) - 1));
    assert(event.type() == XiaozhiIncomingEventType::kUnknown);
    assert(strcmp(event.text(), "ignored") == 0);
    return 0;
}
