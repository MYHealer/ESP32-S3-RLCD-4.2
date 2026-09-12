// 定义每日文字单次同步的内容筛选和服务故障重试边界。
#pragma once

enum class DailySayingAttemptOutcome {
    kAccepted,
    kHttpFailure,
    kTerminalHttpFailure,
    kParseFailure,
    kTooLong,
};

inline constexpr int kDailySayingMaxAttempts = 8;
inline constexpr int kDailySayingMaxConsecutiveServiceFailures = 3;

class DailySayingRetryPolicy {
public:
    void record(DailySayingAttemptOutcome outcome)
    {
        if (outcome == DailySayingAttemptOutcome::kHttpFailure ||
            outcome == DailySayingAttemptOutcome::kParseFailure) {
            ++consecutive_service_failures_;
        } else {
            consecutive_service_failures_ = 0;
        }
    }

    [[nodiscard]] bool should_retry(int completed_attempts,
                                    DailySayingAttemptOutcome outcome) const
    {
        if (outcome == DailySayingAttemptOutcome::kAccepted ||
            outcome == DailySayingAttemptOutcome::kTerminalHttpFailure ||
            completed_attempts >= kDailySayingMaxAttempts) {
            return false;
        }
        if ((outcome == DailySayingAttemptOutcome::kHttpFailure ||
             outcome == DailySayingAttemptOutcome::kParseFailure) &&
            consecutive_service_failures_ >=
                kDailySayingMaxConsecutiveServiceFailures) {
            return false;
        }
        return true;
    }

    [[nodiscard]] int consecutive_service_failures() const
    {
        return consecutive_service_failures_;
    }

private:
    int consecutive_service_failures_ = 0;
};

static_assert(kDailySayingMaxAttempts > 0,
              "daily saying total attempt count must be positive");
static_assert(kDailySayingMaxConsecutiveServiceFailures > 0,
              "daily saying service failure limit must be positive");
static_assert(kDailySayingMaxConsecutiveServiceFailures <
                  kDailySayingMaxAttempts,
              "service failure limit must save requests before the total limit");
