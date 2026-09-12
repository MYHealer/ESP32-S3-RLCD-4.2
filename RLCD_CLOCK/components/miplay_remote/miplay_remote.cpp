// MiPlay 遥控接收端入口：身份初始化、mDNS 注册、发现层与 8899 控制口启动。
#include "miplay_remote.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "miplay_control.h"
#include "miplay_remote_internal.h"

static const char *TAG = "miplay_remote";

// 记录任务是否使用了 PSRAM 栈，决定删除时调用哪个 API。
// 键为任务名指针，值为栈内存的 caps。
namespace {
constexpr int kMaxTrackedTasks = 8;
struct TaskCapsEntry {
    TaskHandle_t handle;
    bool used_psram;
};
TaskCapsEntry s_task_caps[kMaxTrackedTasks];
}  // namespace

int miplay_create_task(void (*task)(void *), const char *name,
                       uint32_t stack_bytes, void *arg, unsigned priority,
                       void **handle, int core)
{
    TaskHandle_t h = nullptr;
    if (handle) {
        *(TaskHandle_t *)handle = nullptr;
    }

    // 优先用 PSRAM 栈：本项目内部 SRAM 被 UI 与音频占用较多，
    // 大栈放内部容易失败。
    const uint32_t stack_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(
        (TaskFunction_t)task, name, stack_bytes, arg, (UBaseType_t)priority, &h,
        (BaseType_t)core, stack_caps);

    bool used_psram = (ret == pdPASS);
    if (ret != pdPASS) {
        const size_t internal_free =
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        ESP_LOGW(TAG, "task %s PSRAM stack failed (internal free=%u), retrying",
                 name, (unsigned)internal_free);
        ret = xTaskCreatePinnedToCore((TaskFunction_t)task, name, stack_bytes,
                                      arg, (UBaseType_t)priority, &h,
                                      (BaseType_t)core);
    }
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "task %s creation failed (stack=%u)", name,
                 (unsigned)stack_bytes);
        return ret;
    }

    // 登记栈类型，供删除时选择正确的释放 API。
    if (h) {
        int slot = -1;
        for (int i = 0; i < kMaxTrackedTasks; i++) {
            if (s_task_caps[i].handle == nullptr) {
                slot = i;
                break;
            }
        }
        if (slot >= 0) {
            s_task_caps[slot].handle = h;
            s_task_caps[slot].used_psram = used_psram;
        }
    }

    ESP_LOGI(TAG, "task %s created (%u bytes, %s stack)", name,
             (unsigned)stack_bytes, used_psram ? "PSRAM" : "internal");
    if (handle) {
        *(TaskHandle_t *)handle = h;
    }
    return ret;
}

void miplay_delete_current_task(void)
{
    const TaskHandle_t current = xTaskGetCurrentTaskHandle();
    bool used_psram = false;
    for (int i = 0; i < kMaxTrackedTasks; i++) {
        if (s_task_caps[i].handle == current) {
            used_psram = s_task_caps[i].used_psram;
            s_task_caps[i].handle = nullptr;
            break;
        }
    }

    // WithCaps 任务必须用 WithCaps 删除，否则 PSRAM 栈不会被释放。
    // 参考实现用 xTaskGetStaticBuffers 判断，但那只对 xTaskCreateStatic
    // 有效，对 WithCaps 永远返回 false，导致栈内存持续泄漏。
    if (used_psram) {
        vTaskDeleteWithCaps(nullptr);
    } else {
        vTaskDelete(nullptr);
    }
}

esp_err_t miplay_remote_start(void)
{
    if (s_running) {
        ESP_LOGD(TAG, "already started");
        return ESP_OK;
    }

    miplay_init_device_identity();

    // 身份里的 device_id 是空串说明 efuse 读取失败，继续跑也没有意义。
    if (s_device_id[0] == '\0') {
        ESP_LOGE(TAG, "identity init failed, abort");
        return ESP_FAIL;
    }

    s_running = true;

    // mDNS 注册失败不阻断整体启动：UDP 5355 那条发现路径独立于 mDNS，
    // 仍可能让手机发现设备。但必须把错误打出来，否则会变成静默失败。
    const esp_err_t mdns_err = miplay_register_mdns_services();
    if (mdns_err != ESP_OK) {
        ESP_LOGE(TAG, "mdns registration failed: %s (UDP 5355 still active)",
                 esp_err_to_name(mdns_err));
    }
    miplay_start_discovery();

    // 8899 控制口。放在发现层之后：手机查到设备信息会立刻连这个端口，
    // 端口没起来会被判为设备不可用。
    const esp_err_t ctrl_err = miplay_control_start();
    if (ctrl_err != ESP_OK) {
        ESP_LOGE(TAG, "control port %d start failed: %s", MIPLAY_CONTROL_PORT,
                 esp_err_to_name(ctrl_err));
    }

    ESP_LOGI(TAG, "started: control port %d, lan discovery %d",
             MIPLAY_CONTROL_PORT, MIPLAY_LAN_DISCOVERY_PORT);
    return ESP_OK;
}

void miplay_remote_stop(void)
{
    if (!s_running) {
        return;
    }
    s_running = false;
    miplay_control_stop();
    // 任务在各自的主循环里检测到 s_running 为假后自行退出并释放资源，
    // 这里不强制删除，避免正在使用的 socket / 内存被提前回收。
    ESP_LOGI(TAG, "stopping");
}

bool miplay_remote_is_connected(void)
{
    return miplay_control_is_connected();
}
