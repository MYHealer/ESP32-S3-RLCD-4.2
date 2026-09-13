// airkan 遥控接收端接入层实现。
//
// 按键语义映射（airkan keyCode → 本机动作）：
//   右(22) → 下一个工作页        左(21) → 上一个工作页
//   OK(23/66) → 进设置页         电源(26) → 无操作（待机不由遥控决定）
//   上(19)/下(20) 音+/- (24/25) → 预留：本机音量由旋钮管理，暂不接管
//
// 切页不直接操作 LVGL：只写 active_work_page_store() 再 notify_ui_task()，
// 由 UI 任务在自己的线程和时序里重建页面。这既避开了跨线程碰 LVGL 的数据
// 竞争，也保证本回调毫秒级返回（回调阻塞会拖住 airkan 的心跳/收包）。
#include "airkan_remote.h"

#include <cstdint>
#include <string.h>

#include "active_work_page_state.h"
#include "active_work_page_state_internal.h"
#include "airkan.h"
#include "atomic_ownership_gate.h"
#include "audio_services.h"
#include "chime_runtime_state.h"
#include "chime_runtime_state_internal.h"
#include "device_settings_persistence.h"
#include "alarm_services.h"
#include "audio_services.h"
#include "battery_runtime_state.h"
#include "chime_runtime_state.h"
#include "chime_runtime_state_internal.h"
#include "device_settings_persistence.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "network_diagnostics_state.h"
#include "ota_services.h"
#include "pomodoro_services.h"
#include "power_services.h"
#include "ui_info_page_state.h"
#include "ui_settings_activity_state.h"
#include "ui_settings_actions.h"
#include "ui_settings_feedback.h"
#include "ui_settings_navigation.h"
#include "ui_task_notify.h"
#include "ui_work_page_catalog.h"
#include "wifi_portal_state.h"

namespace {

constexpr char kTag[] = "airkan_remote";

// 任务栈：本工程内部 SRAM 紧张（UI + 音频占用多），统一走 PSRAM。
constexpr uint32_t kTaskStackBytes = 8192;
constexpr int kTaskPriority = 3;
// airkan 的两个服务 task 放 core 0：core 1 被 UI 任务占用。
constexpr int kTaskCore = 0;

airkan_t *s_airkan = nullptr;
bool s_started = false;
// 遥控服务运行期间持有的网络唤醒锁，阻止 Wi-Fi 射频被省电策略关闭。
bool s_net_lock_held = false;

// 左右切页消抖。手机方向键 down+up 成对下发，长按还会连发重复 DOWN 帧
// （airkan SDK 仅对 OK 键做消抖，方向键原样透传）。没有消抖时一次长按会
// 连跳 N 页。300ms 与 SDK 的 OK 键消抖语义一致：足以区分"连按"与"长按"，
// 又不牺牲连续点按的跟手性。
constexpr int kPageSwitchDebounceMs = 300;
// 设置项正在联网保存/OTA 时，移动选择要给用户可见反馈而非静默丢弃。
constexpr int kSettingsBusyFeedbackMs = 2000;
constexpr const char *kSettingsBusyFeedbackText = "请等待操作完成";
int64_t s_last_page_switch_ms = INT64_MIN;
// 设置页内移动选择单独计时：与切页共用同一个时间戳会让"切页后立刻进菜单"
// 的第一下移动被吞掉。
int64_t s_last_menu_move_ms = INT64_MIN;
// 遥控音量的 NVS 保存：单飞门控，防止连按创建多个保存任务。
AtomicOwnershipGate s_volume_save_gate;

// 沿工作页序表前进/后退一页。next_enabled_work_page() 在页表里循环，
// 后退用"遍历到回到起点"的方式实现，因为 SDK 没有 prev 接口。
// 音量步进（遥控音量键每次 ±5%），与整点提醒音量共用同一运行态。
constexpr int kRemoteVolumeStepPercent = 5;

void step_work_page(int direction)
{
    if (direction > 0) {
        const int next = next_enabled_work_page(active_work_page_load());
        active_work_page_store(next);
        return;
    }

    // 后退：从首页出发沿序表走到"下一站是当前页"的那一页。
    const int current = active_work_page_load();
    int cursor = first_enabled_work_page();
    for (int guard = 0; guard < 64; guard++) {
        const int following = next_enabled_work_page(cursor);
        if (following == current) {
            active_work_page_store(cursor);
            return;
        }
        if (following == first_enabled_work_page()) {
            // 绕回起点仍未命中（当前页不在序表里，如被禁用），退回首页。
            active_work_page_store(first_enabled_work_page());
            return;
        }
        cursor = following;
    }
}

// 遥控菜单键(82)与设备 KEY 键同语义：进设置主菜单。
// 复刻 input_tasks.cpp 的 enter_settings_primary_menu：只写状态 + 唤醒 UI，
// 不在本线程（airkan rc 任务）碰 LVGL。提醒音播放期间按键只负责停止音频。
void open_settings_from_remote(TickType_t now)
{
    if (alarm_stop_ringing_from_button() || pomodoro_stop_alert_from_button()) {
        return;
    }
    if (settings_page_requested()) {
        settings_activity_record(now);
        return;
    }
    if (info_page_requested() || network_diag_page_requested() ||
        setup_portal_active_load() || battery_low_mode_load()) {
        return;
    }
    info_page_clear();
    settings_page_request();
    enter_settings_primary_navigation();
    settings_activity_record(now);
    notify_ui_task();
}

// 遥控音量键：与设置页/小智共用 chime 运行态音量，步进后立即生效。
//
// NVS 保存**不能在本函数内调用**：本回调跑在 airkan 的 rc 任务里，而该任务
// 的栈在 PSRAM（为节省内部 SRAM）。任何 NVS 访问（连读都会）经
// spi_flash_disable_interrupts_caches_and_other_cpu() 禁用 cache，随后
// esp_task_stack_is_sane_cache_disabled() 对 PSRAM 栈断言失败 → 崩溃重启。
// （现场：coredump 2026-09-13, assert at cache_utils.c:127, task 'ak_rc',
//  回溯 #26 本函数 → save_hourly_chime_setting → esp_flash_read）
// 因此保存延后到专用的一次性内部栈任务；单飞门控避免连按时创建多个任务。
void volume_save_task(void *)
{
    if (!save_hourly_chime_setting()) {
        ESP_LOGW(kTag, "remote volume save failed");
    }
    s_volume_save_gate.release();
    vTaskDelete(nullptr);
}

// request_volume_save 移到匿名 namespace 外（line 425 之后），使其具有
// C 链接可见性，供 wifi_portal.cpp 的 MiPlay 音量回调调用。

void step_remote_volume(int direction)
{
    const int current = chime_runtime_volume_percent();
    int target = current + direction * kRemoteVolumeStepPercent;
    if (target < 0) {
        target = 0;
    } else if (target > 100) {
        target = 100;
    }
    if (target == current) {
        return;
    }
    chime_runtime_volume_percent_store(target);
    apply_codec_volume_direct(target);
    request_volume_save();
    // notify_ui_task() 不可省：UI 任务阻塞在 ulTaskNotifyTake 上，不唤醒它
    // 就不会走 update_settings_page()，表现为"声音界面按音量键数值不变，
    // 切到别的界面再回来才更新"（切页通知顺带唤醒了 UI 任务）。
    // 设置页内同时记一次活动，避免调音量途中页面超时自动退出。
    if (settings_page_requested()) {
        settings_activity_record(xTaskGetTickCount());
    }
    notify_ui_task();
    ESP_LOGI(kTag, "remote volume: %d -> %d%%", current, target);
}

// 遥控主页键(3)：回到主页。与 Android HOME 语义一致——清掉所有浮层
// （设置/信息/网络诊断），并回到首页工作页。
// UI 任务在浮层请求消失时会自动 restore_active_work_page_after_aux，
// 所以这里只负责清状态 + 置首页，不必碰 LVGL。
void return_home_from_remote()
{
    if (settings_page_requested()) {
        if (settings_navigation_snapshot().page_order_mode) {
            active_work_page_store(first_enabled_work_page());
        }
        reset_settings_navigation_state();
        settings_page_clear();
    }
    info_page_clear();
    network_diag_page_clear();
    clear_settings_feedback();
    active_work_page_store(first_enabled_work_page());
    settings_activity_record(xTaskGetTickCount());
    notify_ui_task();
    ESP_LOGI(kTag, "home key: return to home page");
}

// 设置页内移动光标（二维）。四个方向键都是"移动"，不执行任何动作：
//   上下 = 换行（网格里 ±2，单列 ±1）  左右 = 换列（±1）
// 选择/执行归 OK 键（23/66），与"按下才是选择"的遥控器语义一致。
void move_settings_focus_from_remote(int row_step, int col_step)
{
    const int64_t now_ms = esp_timer_get_time() / 1000;
    if (s_last_menu_move_ms != INT64_MIN &&
        now_ms - s_last_menu_move_ms < kPageSwitchDebounceMs) {
        return;  // 长按连发过滤
    }
    s_last_menu_move_ms = now_ms;

    if (!is_settings_sync_busy() && !ota_flow_active()) {
        move_settings_focus(row_step, col_step);
        return;
    }
    // 同步忙时给可见反馈，与设备 KEY 键行为一致，不静默丢弃。
    settings_activity_record(xTaskGetTickCount());
    set_settings_feedback(kSettingsBusyFeedbackText, kSettingsBusyFeedbackMs);
    notify_ui_task();
}

void on_key(airkan_t *handle, const airkan_event_t *ev, void *ctx)
{
    (void)handle;
    (void)ctx;

    // keyCode 3 主页：任何界面都回主页，优先于设置页的方向键接管。
    // SDK keymap 未映射 HOME（返回 AIRKAN_ACT_RAW），须先于 switch 判定。
    if (ev->key_code == 3 /* Android KEYCODE_HOME */) {
        return_home_from_remote();
        return;
    }

    // 设置页优先：菜单里的方向键一律是"移动光标"，不切页也不执行。
    // 这与设备 KEY 键的行为一致（进设置后按键改菜单而非切页）。
    if (settings_page_requested()) {
        switch (ev->action) {
        case AIRKAN_ACT_NEXT:  // keyCode 19：上 → 上一行
            move_settings_focus_from_remote(-1, 0);
            return;
        case AIRKAN_ACT_PREV:  // keyCode 20：下 → 下一行
            move_settings_focus_from_remote(+1, 0);
            return;
        case AIRKAN_ACT_SEEK_BACK:  // keyCode 21：左 → 左一列
            move_settings_focus_from_remote(0, -1);
            return;
        case AIRKAN_ACT_SEEK_FWD:  // keyCode 22：右 → 右一列
            move_settings_focus_from_remote(0, +1);
            return;
        case AIRKAN_ACT_PLAYPAUSE:  // keyCode 23/66：OK = 选择/执行
            // 与设备 BOOT 短按同义：推进动作序号，UI 任务轮询到后
            // 调 handle_settings_action 打开或切换当前项。
            settings_activity_record_action(xTaskGetTickCount());
            notify_ui_task();
            return;
        default:
            break;  // 音量、电源等走下面的通用分支
        }
    }

    // keyCode 82 菜单：绑定设备 KEY 键语义（进设置）。
    // SDK keymap 未映射 MENU（返回 AIRKAN_ACT_RAW），须先于 switch 判定。
    if (ev->key_code == AIRKAN_KEY_MENU) {
        open_settings_from_remote(xTaskGetTickCount());
        return;
    }
    // keyCode 4 返回：设置页内逐级返回（等效设备 KEY 长按），
    // 非设置页无动作。SDK 未映射 BACK，同样走 RAW 透传。
    if (ev->key_code == 4 /* Android KEYCODE_BACK */) {
        if (settings_page_requested()) {
            settings_activity_record(xTaskGetTickCount());
            handle_settings_key_long();
            notify_ui_task();
        }
        return;
    }

    const int page_before = active_work_page_load();
    switch (ev->action) {
    case AIRKAN_ACT_SEEK_FWD:  // keyCode 22：右
    case AIRKAN_ACT_SEEK_BACK: {  // keyCode 21：左
        // 浮层活跃时禁止切页，与设备 BOOT 键守卫一致
        // （input_tasks.cpp:252-256）。
        if (info_page_requested() || network_diag_page_requested() ||
            setup_portal_active_load() || battery_low_mode_load()) {
            return;
        }
        // 时间源用 esp_timer：本回调跑在 airkan 的 rc 任务里，
        // esp_timer_get_time 单调且线程安全。
        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (s_last_page_switch_ms != INT64_MIN &&
            now_ms - s_last_page_switch_ms < kPageSwitchDebounceMs) {
            ESP_LOGD(kTag, "page switch debounced (%lld ms since last)",
                     (long long)(now_ms - s_last_page_switch_ms));
            return;
        }
        s_last_page_switch_ms = now_ms;
        step_work_page(ev->action == AIRKAN_ACT_SEEK_FWD ? +1 : -1);
        break;
    }
    case AIRKAN_ACT_PLAYPAUSE:  // keyCode 23/66：OK = 确认键
        // 与设备 BOOT 短按同语义：设置页内触发确认/激活
        // （record_action 推进序号，UI 任务轮询到后调 handle_settings_action）。
        settings_activity_record_action(xTaskGetTickCount());
        notify_ui_task();
        return;
    case AIRKAN_ACT_VOL_UP:  // keyCode 24：音量 +
        step_remote_volume(+1);
        return;
    case AIRKAN_ACT_VOL_DOWN:  // keyCode 25：音量 -
        step_remote_volume(-1);
        return;
    case AIRKAN_ACT_NEXT:
    case AIRKAN_ACT_PREV:
    case AIRKAN_ACT_POWER:
        // 非设置页时上下键暂无绑定，电源不接管待机。
        ESP_LOGI(kTag, "key action=%d not mapped outside settings",
                 (int)ev->action);
        return;
    default:
        ESP_LOGI(kTag, "unhandled key code=%d action=%d", ev->key_code,
                 (int)ev->action);
        return;
    }

    const int page_after = active_work_page_load();
    if (page_after != page_before) {
        ESP_LOGI(kTag, "work page: %d -> %d", page_before, page_after);
        // 只置状态 + 唤醒 UI 任务，不在此线程碰 LVGL。
        notify_ui_task();
    }
}

void on_status(airkan_t *handle, airkan_status_t st, void *ctx)
{
    (void)handle;
    (void)ctx;
    switch (st) {
    case AIRKAN_STATUS_UP:
        ESP_LOGI(kTag, "phone remote connected");
        break;
    case AIRKAN_STATUS_DOWN:
        ESP_LOGI(kTag, "phone remote disconnected");
        break;
    case AIRKAN_STATUS_AUTH_OK:
        ESP_LOGI(kTag, "auth ok");
        break;
    case AIRKAN_STATUS_HTTP_ERR:
        ESP_LOGW(kTag, "http auth error");
        break;
    default:
        break;
    }
}

void sdk_log(int level, const char *fmt, va_list ap, void *ctx)
{
    (void)ctx;
    // SDK 自带日志走 esp_log 同级别输出，便于和本工程日志一起看时序。
    char buf[192];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    if (level <= 1) {
        ESP_LOGE(kTag, "%s", buf);
    } else if (level == 2) {
        ESP_LOGW(kTag, "%s", buf);
    } else if (level == 3) {
        ESP_LOGI(kTag, "%s", buf);
    } else {
        ESP_LOGD(kTag, "%s", buf);
    }
}

// SDK 的两个阻塞服务函数各占一个 task。
void http_task(void *arg)
{
    airkan_serve_http((airkan_t *)arg);
    vTaskDeleteWithCaps(nullptr);  // 栈来自 PSRAM，必须用 WithCaps 删除
}

void rc_task(void *arg)
{
    airkan_serve_rc((airkan_t *)arg);
    vTaskDeleteWithCaps(nullptr);
}

// 释放网络唤醒锁（幂等）。启动失败的各条回滚路径都要调用，避免泄漏。
void release_net_lock_if_held()
{
    if (s_net_lock_held) {
        release_network_awake_lock();
        s_net_lock_held = false;
    }
}

// 建 PSRAM 栈任务；失败回落内部栈。
bool create_service_task(TaskFunction_t fn, const char *name, void *arg)
{
    TaskHandle_t h = nullptr;
    BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(
        fn, name, kTaskStackBytes, arg, kTaskPriority, &h, kTaskCore,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ret != pdPASS) {
        ESP_LOGW(kTag, "task %s PSRAM stack failed, retrying internal", name);
        ret = xTaskCreatePinnedToCore(fn, name, kTaskStackBytes, arg,
                                      kTaskPriority, &h, kTaskCore);
    }
    if (ret != pdPASS) {
        ESP_LOGE(kTag, "task %s creation failed", name);
        return false;
    }
    return true;
}

}  // namespace

void request_volume_save()
{
    if (!s_volume_save_gate.try_acquire()) {
        return;  // 已有保存任务在跑，本次变更会被它一并落盘
    }
    // 内部 RAM 栈：flash 操作期间必须可访问，不能走 PSRAM 栈。
    if (xTaskCreate(volume_save_task, "ak_vol_save", 3072, nullptr,
                    kTaskPriority, nullptr) != pdPASS) {
        ESP_LOGW(kTag, "volume save task create failed");
        s_volume_save_gate.release();
    }
}

esp_err_t airkan_remote_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    // 遥控要求设备随时可被连接。本工程的 Wi-Fi 省电策略会在无网络活动时关闭
    // 射频（wifi_idle_stop_allowed 检查 network_awake_lock_active），射频一关
    // 手机就既发现不了也连不上。这里在服务运行期间持锁，让射频保持开启。
    // 代价是牺牲深度省电——需要随时可遥控的设备不能随时断网。
    if (acquire_network_awake_lock()) {
        s_net_lock_held = true;
    } else {
        ESP_LOGW(kTag, "network awake lock unavailable; radio may sleep");
    }

    airkan_config_t cfg = airkan_config_default();
    static char tv_id[40];
    airkan_plat_esp32_make_tvid(tv_id, sizeof(tv_id));

    // device_id 只作默认值：requestAuth 时 SDK 会原样回显手机传来的值。
    cfg.device_id = "rlcd42";
    cfg.tv_id = tv_id;
    cfg.version = AIRKAN_VERSION_DEFAULT;
    cfg.transport = airkan_transport_esp32();
    cfg.cb.on_key = on_key;
    cfg.cb.on_status = on_status;
    cfg.cb.log = sdk_log;

    s_airkan = airkan_create(&cfg);
    if (!s_airkan) {
        ESP_LOGE(kTag, "airkan_create failed");
        release_net_lock_if_held();
        return ESP_FAIL;
    }

    const airkan_err_t err = airkan_start(s_airkan);
    if (err != AIRKAN_OK) {
        ESP_LOGE(kTag, "airkan_start failed: %d", (int)err);
        airkan_destroy(s_airkan);
        s_airkan = nullptr;
        release_net_lock_if_held();
        return ESP_FAIL;
    }

    if (!create_service_task(http_task, "ak_http", s_airkan) ||
        !create_service_task(rc_task, "ak_rc", s_airkan)) {
        // 任务不足则服务无法工作，回滚以免留在半启动状态。
        airkan_stop(s_airkan);
        airkan_destroy(s_airkan);
        s_airkan = nullptr;
        release_net_lock_if_held();
        return ESP_FAIL;
    }

    s_started = true;
    ESP_LOGI(kTag, "started: http %d, rc %d, tv_id=%s", AIRKAN_HTTP_PORT_DEFAULT,
             AIRKAN_RC_PORT_DEFAULT, tv_id);
    return ESP_OK;
}

void airkan_remote_stop(void)
{
    if (!s_started || !s_airkan) {
        return;
    }
    airkan_stop(s_airkan);
    airkan_destroy(s_airkan);
    s_airkan = nullptr;
    s_started = false;
    release_net_lock_if_held();
    ESP_LOGI(kTag, "stopped");
}

bool airkan_remote_is_connected(void)
{
    if (!s_started || !s_airkan) {
        return false;
    }
    return airkan_is_controlable(s_airkan) != 0;
}
