// 提供缓冲区元素数量与单元素大小相乘时的无溢出尺寸计算。
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

namespace app_memory {

constexpr bool checked_size_multiply(size_t count,
                                     size_t element_size,
                                     size_t *bytes)
{
    if (!bytes) {
        return false;
    }
    *bytes = 0;
    if (count != 0 && element_size > SIZE_MAX / count) {
        return false;
    }
    *bytes = count * element_size;
    return true;
}

constexpr bool checked_size_to_int(size_t value, int *converted)
{
    if (!converted) {
        return false;
    }
    *converted = 0;
    if (value > static_cast<size_t>(INT_MAX)) {
        return false;
    }
    *converted = static_cast<int>(value);
    return true;
}

} // namespace app_memory
