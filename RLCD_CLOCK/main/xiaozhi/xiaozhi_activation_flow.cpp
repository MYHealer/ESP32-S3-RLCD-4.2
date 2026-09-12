// 处理小智激活配置恢复、首次绑定和激活响应应用。
#include "xiaozhi_activation_flow.h"

#include "xiaozhi_activation_client.h"
#include "xiaozhi_activation_response_parser.h"
#include "xiaozhi_activation_storage.h"
#include "xiaozhi_binding_voice.h"
#include "xiaozhi_snapshot_state.h"

#include <esp_heap_caps.h>
#include <esp_log.h>

#include <atomic>
#include <cstring>

namespace {

constexpr const char *kTag = "WeatherClock";
constexpr const char *kActivatingStatus = "正在连接小智服务";
constexpr const char *kBindingStatus = "请绑定设备";
constexpr const char *kReadyStatus = "等待唤醒词";
constexpr const char *kErrorStatus = "小智服务不可用";
constexpr const char *kBoundDetail = "说出唤醒词即可开始对话";
constexpr const char *kActivationFailureDetail = "稍后将自动重试";
constexpr const char *kBindingFallbackDetail = "请在小智服务中输入绑定 ID";

std::atomic<bool> s_activation_scratch_in_use{false};
XiaozhiActivationScratch *s_activation_scratch = nullptr;

class ActivationScratchLease {
public:
    ActivationScratchLease()
    {
        bool expected = false;
        if (!s_activation_scratch_in_use.compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return;
        }
        if (!s_activation_scratch) {
            s_activation_scratch = static_cast<XiaozhiActivationScratch *>(heap_caps_calloc(
                1,
                sizeof(XiaozhiActivationScratch),
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        }
        if (!s_activation_scratch) {
            s_activation_scratch_in_use.store(false, std::memory_order_release);
            return;
        }
        std::memset(s_activation_scratch, 0, sizeof(XiaozhiActivationScratch));
        scratch_ = s_activation_scratch;
    }

    ~ActivationScratchLease()
    {
        reset();
    }

    ActivationScratchLease(const ActivationScratchLease &) = delete;
    ActivationScratchLease &operator=(const ActivationScratchLease &) = delete;

    XiaozhiActivationScratch *get() const
    {
        return scratch_;
    }

    explicit operator bool() const
    {
        return scratch_ != nullptr;
    }

    void reset()
    {
        if (!scratch_) {
            return;
        }
        std::memset(scratch_, 0, sizeof(XiaozhiActivationScratch));
        scratch_ = nullptr;
        s_activation_scratch_in_use.store(false, std::memory_order_release);
    }

private:
    XiaozhiActivationScratch *scratch_ = nullptr;
};

bool apply_activation_response(const XiaozhiActivationResponse &response)
{
    XiaozhiActivationResponseDocument document;
    if (!document.parse(response.data, response.len)) {
        return false;
    }
    const char *detail = document.message() ? document.message() : kBindingFallbackDetail;
    const char *binding_code = document.binding_code() ? document.binding_code() : "";
    const char *challenge_text = document.challenge() ? document.challenge() : "";
    // 未绑定响应可能同时携带 WebSocket 参数和绑定码。绑定码必须优先，
    // 否则把临时连接参数持久化后，下次启动会被误判成已经绑定。
    if (binding_code[0] != '\0') {
        xiaozhi_snapshot_set(kXiaozhiAiBinding,
                             kBindingStatus,
                             detail,
                             binding_code);
        xiaozhi_announce_binding_id_once(binding_code);
        ESP_LOGI(kTag, "xiaozhi device binding required");
        return true;
    }
    bool configured = xiaozhi_save_activation_config(document.websocket(),
                                                      challenge_text);
    if (configured) {
        xiaozhi_snapshot_set(kXiaozhiAiReady, kReadyStatus, kBoundDetail);
        ESP_LOGI(kTag, "xiaozhi device binding confirmed");
    }
    return configured;
}

} // namespace

void xiaozhi_activate_or_restore_session()
{
    char url[kXiaozhiWebsocketConfigFieldSize] = {};
    char token[kXiaozhiWebsocketConfigFieldSize] = {};
    int32_t version = 1;
    if (xiaozhi_load_websocket_config(url,
                                      sizeof(url),
                                      token,
                                      sizeof(token),
                                      &version)) {
        xiaozhi_snapshot_set(kXiaozhiAiReady, kReadyStatus, kBoundDetail);
        return;
    }
    xiaozhi_snapshot_set(kXiaozhiAiActivating,
                         kActivatingStatus,
                         "正在请求设备绑定信息");
    ActivationScratchLease scratch_lease;
    if (!scratch_lease) {
        xiaozhi_snapshot_set(kXiaozhiAiError,
                             kErrorStatus,
                             "小智内存不足，请稍后重试");
        return;
    }
    XiaozhiActivationScratch *scratch = scratch_lease.get();
    bool activated = xiaozhi_request_activation(scratch) &&
                     apply_activation_response(scratch->response);
    scratch_lease.reset();
    if (!activated) {
        xiaozhi_snapshot_set(kXiaozhiAiError,
                             kErrorStatus,
                             kActivationFailureDetail);
    }
}
