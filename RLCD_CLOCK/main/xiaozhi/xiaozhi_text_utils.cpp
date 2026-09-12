// 实现小智协议和页面快照共用的 UTF-8 有界复制规则。
#include "xiaozhi_text_utils.h"

#include <string.h>

namespace xiaozhi_protocol {
namespace {
size_t utf8_character_size(const unsigned char *text, size_t remaining)
{
    if (!text || remaining == 0) {
        return 0;
    }
    size_t size = 1;
    if (text[0] < 0x80) {
        return 1;
    }
    if ((text[0] & 0xe0) == 0xc0) {
        size = 2;
    } else if ((text[0] & 0xf0) == 0xe0) {
        size = 3;
    } else if ((text[0] & 0xf8) == 0xf0) {
        size = 4;
    } else {
        return 0;
    }
    if (size > remaining) {
        return 0;
    }
    for (size_t index = 1; index < size; ++index) {
        if ((text[index] & 0xc0) != 0x80) {
            return 0;
        }
    }
    return size;
}
} // namespace

bool output_buffer_available(char *out, size_t out_len)
{
    return out && out_len > 0;
}

void utf8_safe_copy(char *out, size_t out_len, const char *text)
{
    if (!output_buffer_available(out, out_len)) {
        return;
    }
    out[0] = '\0';
    if (!text) {
        return;
    }
    const unsigned char *source = reinterpret_cast<const unsigned char *>(text);
    size_t source_len = strlen(text);
    size_t source_offset = 0;
    size_t output_offset = 0;
    while (source_offset < source_len && output_offset + 1 < out_len) {
        size_t character_size = utf8_character_size(source + source_offset,
                                                    source_len - source_offset);
        if (character_size == 0) {
            out[output_offset++] = '?';
            ++source_offset;
            continue;
        }
        if (output_offset + character_size >= out_len) {
            break;
        }
        memcpy(out + output_offset, source + source_offset, character_size);
        output_offset += character_size;
        source_offset += character_size;
    }
    out[output_offset] = '\0';
}

} // namespace xiaozhi_protocol
