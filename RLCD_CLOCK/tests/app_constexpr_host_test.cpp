// 验证公共数组、固定 C 字符串和饱和计数 helper 的边界行为。
#include "app_constexpr.h"

#include <assert.h>

int main()
{
    constexpr int numbers[] = {1, 2, 3};
    constexpr const char *valid_texts[] = {"alpha", "beta", "gamma"};
    constexpr const char *empty_texts[] = {"alpha", "", "gamma"};
    constexpr const char *duplicate_texts[] = {"alpha", "beta", "alpha"};

    static_assert(array_count(numbers) == 3);
    static_assert(array_count(valid_texts) == 3);
    static_assert(cstr_nonempty("x"));
    static_assert(!cstr_nonempty(""));
    static_assert(!cstr_nonempty(nullptr));
    static_assert(cstr_equal("same", "same"));
    static_assert(!cstr_equal("same", "different"));
    static_assert(!cstr_equal(nullptr, nullptr));
    static_assert(cstr_length(nullptr) == 0);
    static_assert(cstr_length("") == 0);
    static_assert(cstr_length("length") == 6);
    static_assert(saturating_increment_u8(0) == 1);
    static_assert(saturating_increment_u8(UINT8_MAX - 1) == UINT8_MAX);
    static_assert(saturating_increment_u8(UINT8_MAX) == UINT8_MAX);
    static_assert(cstr_array_nonempty(valid_texts));
    static_assert(!cstr_array_nonempty(empty_texts));
    static_assert(cstr_array_contains(valid_texts, "beta"));
    static_assert(!cstr_array_contains(valid_texts, "missing"));
    static_assert(cstr_array_unique(valid_texts));
    static_assert(!cstr_array_unique(duplicate_texts));

    assert(cstr_or_empty(nullptr)[0] == '\0');
    assert(cstr_equal(cstr_or_empty("value"), "value"));
    return 0;
}
