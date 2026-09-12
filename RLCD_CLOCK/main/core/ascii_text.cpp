// 实现两类既有 ASCII 首尾空白裁剪语义。
#include "ascii_text.h"

#include <cctype>
#include <cstring>

namespace {
constexpr size_t kCStringTerminatorSize = 1;

bool c_whitespace(char ch)
{
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

bool line_whitespace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

void trim_with_predicate(char *text, bool (*is_whitespace)(char))
{
    if (!text || !is_whitespace) {
        return;
    }
    char *start = text;
    while (*start && is_whitespace(*start)) {
        ++start;
    }
    if (start != text) {
        std::memmove(text, start, std::strlen(start) + kCStringTerminatorSize);
    }
    size_t length = std::strlen(text);
    while (length > 0 && is_whitespace(text[length - 1])) {
        text[--length] = '\0';
    }
}
} // namespace

static_assert(kCStringTerminatorSize == 1,
              "ASCII text terminator reservation must be one byte");

void trim_ascii_whitespace(char *text)
{
    trim_with_predicate(text, c_whitespace);
}

void trim_ascii_line_whitespace(char *text)
{
    trim_with_predicate(text, line_whitespace);
}
