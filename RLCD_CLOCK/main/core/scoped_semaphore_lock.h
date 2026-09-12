// 为普通任务上下文提供常驻静态 mutex 所有权和 semaphore 作用域释放保护。
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class StaticTaskMutex {
public:
    StaticTaskMutex() = default;

    bool init()
    {
        if (handle_) {
            return true;
        }
        handle_ = xSemaphoreCreateMutexStatic(&storage_);
        return handle_ != nullptr;
    }

    SemaphoreHandle_t handle() const
    {
        return handle_;
    }

    StaticTaskMutex(const StaticTaskMutex &) = delete;
    StaticTaskMutex &operator=(const StaticTaskMutex &) = delete;

private:
    StaticSemaphore_t storage_ = {};
    SemaphoreHandle_t handle_ = nullptr;
};

class ScopedSemaphoreLock {
public:
    explicit ScopedSemaphoreLock(SemaphoreHandle_t semaphore,
                                 TickType_t wait_ticks = portMAX_DELAY)
        : semaphore_(semaphore),
          locked_(semaphore_ &&
                  xSemaphoreTake(semaphore_, wait_ticks) == pdTRUE)
    {
    }

    explicit ScopedSemaphoreLock(const StaticTaskMutex &mutex,
                                 TickType_t wait_ticks = portMAX_DELAY)
        : ScopedSemaphoreLock(mutex.handle(), wait_ticks)
    {
    }

    ~ScopedSemaphoreLock()
    {
        if (locked_) {
            xSemaphoreGive(semaphore_);
        }
    }

    explicit operator bool() const
    {
        return locked_;
    }

    ScopedSemaphoreLock(const ScopedSemaphoreLock &) = delete;
    ScopedSemaphoreLock &operator=(const ScopedSemaphoreLock &) = delete;

private:
    SemaphoreHandle_t semaphore_;
    bool locked_;
};
