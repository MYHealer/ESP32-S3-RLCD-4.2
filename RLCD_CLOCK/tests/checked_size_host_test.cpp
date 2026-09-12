// 验证公共缓冲区尺寸乘法在零值、正常值和溢出边界上的行为。
#include "checked_size.h"

#include <assert.h>
#include <stdint.h>

int main()
{
    size_t bytes = 123;
    assert(app_memory::checked_size_multiply(0, sizeof(uint16_t), &bytes));
    assert(bytes == 0);

    assert(app_memory::checked_size_multiply(960, sizeof(int16_t), &bytes));
    assert(bytes == 1920);

    assert(app_memory::checked_size_multiply(SIZE_MAX / 2, 2, &bytes));
    assert(bytes == SIZE_MAX - 1);

    bytes = 123;
    assert(!app_memory::checked_size_multiply(SIZE_MAX / 2 + 1, 2, &bytes));
    assert(bytes == 0);

    assert(!app_memory::checked_size_multiply(1, 1, nullptr));

    int converted = -1;
    assert(app_memory::checked_size_to_int(0, &converted));
    assert(converted == 0);
    assert(app_memory::checked_size_to_int(static_cast<size_t>(INT_MAX), &converted));
    assert(converted == INT_MAX);

    converted = -1;
    assert(!app_memory::checked_size_to_int(static_cast<size_t>(INT_MAX) + 1U,
                                            &converted));
    assert(converted == 0);
    assert(!app_memory::checked_size_to_int(1, nullptr));
    return 0;
}
