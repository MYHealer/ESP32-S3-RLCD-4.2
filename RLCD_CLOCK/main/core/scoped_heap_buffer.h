// 提供默认堆、PSRAM 优先或 PSRAM 必需分配，并兼容 heap_caps 接管与 free 的不可复制字节缓冲区所有权。
#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif

enum class HeapBufferInit {
    kUninitialized,
    kZeroed,
    kCString,
};

enum class HeapBufferStorage {
    kDefault,
    kPsramPreferred,
    kPsramRequired,
};

template <typename Byte>
class ScopedHeapBuffer {
public:
    explicit ScopedHeapBuffer(size_t size,
                              HeapBufferInit init = HeapBufferInit::kUninitialized,
                              HeapBufferStorage storage = HeapBufferStorage::kDefault)
        : data_(allocate(size, init, storage)),
          size_(size)
    {
        static_assert(sizeof(Byte) == 1, "ScopedHeapBuffer only owns byte-sized elements");
        if (data_ && size_ > 0 && init == HeapBufferInit::kCString) {
            data_[0] = Byte{};
        }
    }

    // 接管由 malloc/calloc/heap_caps_* 分配且可由 free() 释放的既有缓冲区。
    ScopedHeapBuffer(Byte *data, size_t size)
        : data_(data),
          size_(size)
    {
        static_assert(sizeof(Byte) == 1, "ScopedHeapBuffer only owns byte-sized elements");
    }

    ~ScopedHeapBuffer()
    {
        reset();
    }

    ScopedHeapBuffer(const ScopedHeapBuffer &) = delete;
    ScopedHeapBuffer &operator=(const ScopedHeapBuffer &) = delete;

    Byte *data() const
    {
        return data_;
    }

    Byte *get() const
    {
        return data_;
    }

    size_t size() const
    {
        return size_;
    }

    Byte *release()
    {
        Byte *data = data_;
        data_ = nullptr;
        size_ = 0;
        return data;
    }

    void reset()
    {
        free(data_);
        data_ = nullptr;
        size_ = 0;
    }

    void clear() const
    {
        if (data_) {
            memset(data_, 0, size_);
        }
    }

    explicit operator bool() const
    {
        return data_ != nullptr;
    }

private:
    static Byte *allocate(size_t size,
                          HeapBufferInit init,
                          HeapBufferStorage storage)
    {
        if (size == 0) {
            return nullptr;
        }
        void *memory = nullptr;
#if defined(ESP_PLATFORM)
        if (storage == HeapBufferStorage::kPsramPreferred ||
            storage == HeapBufferStorage::kPsramRequired) {
            memory = init == HeapBufferInit::kZeroed
                         ? heap_caps_calloc(size,
                                            sizeof(Byte),
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                         : heap_caps_malloc(size * sizeof(Byte),
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!memory && storage == HeapBufferStorage::kPsramRequired) {
                return nullptr;
            }
        }
#else
        (void)storage;
#endif
        if (!memory) {
            memory = init == HeapBufferInit::kZeroed
                         ? calloc(size, sizeof(Byte))
                         : malloc(size * sizeof(Byte));
        }
        return static_cast<Byte *>(memory);
    }

    Byte *data_ = nullptr;
    size_t size_ = 0;
};
