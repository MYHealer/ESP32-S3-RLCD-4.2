// 声明小智 WebSocket 入站文本事件的只读 JSON 分类对象。
#pragma once

#include "xiaozhi_json_owner.h"

#include <stddef.h>
#include <string.h>

enum class XiaozhiIncomingEventType {
    kUnknown,
    kTtsStart,
    kTtsStop,
    kTtsSentenceStart,
    kStt,
    kLlm,
};

class XiaozhiIncomingEvent {
public:
    XiaozhiIncomingEvent() = default;
    ~XiaozhiIncomingEvent()
    {
        reset();
    }

    XiaozhiIncomingEvent(const XiaozhiIncomingEvent &) = delete;
    XiaozhiIncomingEvent &operator=(const XiaozhiIncomingEvent &) = delete;

    bool parse(const char *json, size_t json_len)
    {
        reset();
        root_.reset(cJSON_ParseWithLength(json, json_len));
        if (!root_) {
            return false;
        }

        const char *type = json_string_value(cJSON_GetObjectItem(root_.get(), "type"));
        const char *state = json_string_value(cJSON_GetObjectItem(root_.get(), "state"));
        text_ = json_string_value(cJSON_GetObjectItem(root_.get(), "text"));
        if (type && strcmp(type, "tts") == 0 && state) {
            if (strcmp(state, "start") == 0) {
                type_ = XiaozhiIncomingEventType::kTtsStart;
            } else if (strcmp(state, "stop") == 0) {
                type_ = XiaozhiIncomingEventType::kTtsStop;
            } else if (strcmp(state, "sentence_start") == 0 && text_) {
                type_ = XiaozhiIncomingEventType::kTtsSentenceStart;
            }
        } else if (type && strcmp(type, "stt") == 0 && text_) {
            type_ = XiaozhiIncomingEventType::kStt;
        } else if (type && strcmp(type, "llm") == 0) {
            type_ = XiaozhiIncomingEventType::kLlm;
            emotion_ = json_string_value(cJSON_GetObjectItem(root_.get(), "emotion"));
        }
        return true;
    }

    XiaozhiIncomingEventType type() const
    {
        return type_;
    }

    const char *text() const
    {
        return text_;
    }

    const char *emotion() const
    {
        return emotion_;
    }

private:
    static const char *json_string_value(const cJSON *item)
    {
        return cJSON_IsString(item) ? item->valuestring : nullptr;
    }

    void reset()
    {
        root_.reset();
        type_ = XiaozhiIncomingEventType::kUnknown;
        text_ = nullptr;
        emotion_ = nullptr;
    }

    XiaozhiJsonOwner root_;
    XiaozhiIncomingEventType type_ = XiaozhiIncomingEventType::kUnknown;
    const char *text_ = nullptr;
    const char *emotion_ = nullptr;
};
