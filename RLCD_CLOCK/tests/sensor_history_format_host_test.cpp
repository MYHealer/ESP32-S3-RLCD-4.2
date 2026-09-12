// 验证温湿度小时历史 wire 格式和环形槽位换算保持兼容。
#include "sensor_history_format.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

int main()
{
    using namespace sensor_history_format;

    HourlySensorHistoryMeta meta = {};
    assert(hourly_meta_valid(meta, sizeof(meta)));
    assert(!hourly_meta_valid(meta, sizeof(meta) - 1));
    meta.magic = 0;
    assert(!hourly_meta_valid(meta, sizeof(meta)));
    meta = {};
    meta.version = kLegacyHourlyHistoryVersion;
    assert(!hourly_meta_valid(meta, sizeof(meta)));
    meta = {};
    meta.count = kHourlyHistoryCount - 1;
    assert(!hourly_meta_valid(meta, sizeof(meta)));

    LegacyHourlySensorHistoryBlob legacy = {};
    assert(legacy_history_valid(legacy, sizeof(legacy)));
    assert(!legacy_history_valid(legacy, sizeof(legacy) - 1));
    legacy.magic = 0;
    assert(!legacy_history_valid(legacy, sizeof(legacy)));
    legacy = {};
    legacy.version = kHourlyHistoryMetaVersion;
    assert(!legacy_history_valid(legacy, sizeof(legacy)));
    legacy = {};
    legacy.count = kLegacyHourlyHistoryCount - 1;
    assert(!legacy_history_valid(legacy, sizeof(legacy)));

    HourlySensorHistoryBlob initialized = {};
    memset(&initialized, 0xA5, sizeof(initialized));
    initialize_empty_hourly_history(&initialized);
    assert(initialized.magic == kHourlyHistoryMagic);
    assert(initialized.version == kLegacyHourlyHistoryVersion);
    assert(initialized.count == kHourlyHistoryCount);
    for (const HourlySensorSample &sample : initialized.samples) {
        assert(sample.timestamp == 0);
        assert(sample.temperature == 0.0f);
        assert(sample.humidity == 0.0f);
        assert(sample.valid == 0);
        for (uint8_t byte : sample.reserved) {
            assert(byte == 0);
        }
    }
    initialize_empty_hourly_history(nullptr);

    static_assert(offsetof(HourlySensorHistoryMeta, magic) == 0,
                  "hourly meta magic must remain first");
    static_assert(offsetof(HourlySensorHistoryMeta, last_saved_at) >= 8,
                  "hourly meta timestamp must follow the wire header");
    static_assert(offsetof(LegacyHourlySensorHistoryBlob, samples) == 8,
                  "legacy samples must follow the eight-byte wire header");

    assert(!hourly_index_valid(-1));
    assert(hourly_index_valid(0));
    assert(hourly_index_valid(kHourlyHistoryCount - 1));
    assert(!hourly_index_valid(kHourlyHistoryCount));

    constexpr time_t kSecondsPerHour = 3600;
    assert(hourly_slot_index_for_time(0) == 0);
    assert(hourly_slot_index_for_time(kSecondsPerHour) == 1);
    assert(hourly_slot_index_for_time((kHourlyHistoryCount - 1) * kSecondsPerHour) ==
           kHourlyHistoryCount - 1);
    assert(hourly_slot_index_for_time(kHourlyHistoryCount * kSecondsPerHour) == 0);
    assert(hourly_slot_index_for_time(-kSecondsPerHour) == kHourlyHistoryCount - 1);

    return 0;
}
