// 验证公共堆缓冲的分配模式、清零和基本所有权接口。
#include "scoped_heap_buffer.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>

#if defined(ESP_PLATFORM)
namespace {
bool s_psram_allocation_available = false;
size_t s_psram_allocation_attempts = 0;
}

void *heap_caps_malloc(size_t size, uint32_t caps)
{
    (void)caps;
    ++s_psram_allocation_attempts;
    return s_psram_allocation_available ? malloc(size) : nullptr;
}

void *heap_caps_calloc(size_t count, size_t size, uint32_t caps)
{
    (void)caps;
    ++s_psram_allocation_attempts;
    return s_psram_allocation_available ? calloc(count, size) : nullptr;
}
#endif

static_assert(!std::is_copy_constructible<ScopedHeapBuffer<char>>::value,
              "heap buffer ownership must not be copied");
static_assert(!std::is_copy_assignable<ScopedHeapBuffer<char>>::value,
              "heap buffer ownership must not be assigned");

int main()
{
    {
        ScopedHeapBuffer<uint8_t> buffer(16);
        assert(buffer);
        assert(buffer.data() == buffer.get());
        assert(buffer.size() == 16);
        memset(buffer.data(), 0x5a, buffer.size());
        buffer.clear();
        for (size_t i = 0; i < buffer.size(); ++i) {
            assert(buffer.data()[i] == 0);
        }
    }

    {
        ScopedHeapBuffer<char> buffer(12, HeapBufferInit::kZeroed);
        assert(buffer);
        assert(buffer.size() == 12);
        for (size_t i = 0; i < buffer.size(); ++i) {
            assert(buffer.data()[i] == '\0');
        }
    }

    {
        ScopedHeapBuffer<char> buffer(8, HeapBufferInit::kCString);
        assert(buffer);
        assert(buffer.data()[0] == '\0');
        memcpy(buffer.data(), "abc", 4);
        assert(strcmp(buffer.data(), "abc") == 0);
    }

    {
        ScopedHeapBuffer<char> buffer(12,
                                      HeapBufferInit::kZeroed,
                                      HeapBufferStorage::kPsramPreferred);
        assert(buffer);
        assert(buffer.size() == 12);
        for (size_t i = 0; i < buffer.size(); ++i) {
            assert(buffer.data()[i] == '\0');
        }
    }

#if defined(ESP_PLATFORM)
    {
        s_psram_allocation_available = false;
        s_psram_allocation_attempts = 0;
        ScopedHeapBuffer<char> buffer(12,
                                      HeapBufferInit::kZeroed,
                                      HeapBufferStorage::kPsramRequired);
        assert(!buffer);
        assert(s_psram_allocation_attempts == 1);
    }

    {
        s_psram_allocation_available = true;
        s_psram_allocation_attempts = 0;
        ScopedHeapBuffer<char> buffer(12,
                                      HeapBufferInit::kZeroed,
                                      HeapBufferStorage::kPsramRequired);
        assert(buffer);
        assert(buffer.size() == 12);
        assert(s_psram_allocation_attempts == 1);
        for (size_t i = 0; i < buffer.size(); ++i) {
            assert(buffer.data()[i] == '\0');
        }
    }
#endif

    {
        ScopedHeapBuffer<char> buffer(0, HeapBufferInit::kCString);
        assert(!buffer);
        assert(buffer.get() == nullptr);
        assert(buffer.size() == 0);
    }

    {
        char *owned = static_cast<char *>(calloc(8, sizeof(char)));
        assert(owned != nullptr);
        memcpy(owned, "owned", 6);
        ScopedHeapBuffer<char> buffer(owned, 8);
        assert(buffer);
        assert(buffer.data() == owned);
        assert(buffer.size() == 8);
        assert(strcmp(buffer.data(), "owned") == 0);
    }

    {
        ScopedHeapBuffer<char> buffer(8, HeapBufferInit::kZeroed);
        assert(buffer);
        char *released = buffer.release();
        assert(released != nullptr);
        assert(!buffer);
        assert(buffer.data() == nullptr);
        assert(buffer.size() == 0);
        free(released);
    }

    {
        ScopedHeapBuffer<uint8_t> buffer(8, HeapBufferInit::kZeroed);
        assert(buffer);
        buffer.reset();
        assert(!buffer);
        assert(buffer.data() == nullptr);
        assert(buffer.size() == 0);
        buffer.reset();
        assert(!buffer);
    }

    return 0;
}
