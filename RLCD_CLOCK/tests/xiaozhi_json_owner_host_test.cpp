// 验证小智共享 cJSON 所有权对象的析构、重置和同指针保护。
#include "xiaozhi_json_owner.h"

#include <assert.h>

namespace {
int g_delete_calls = 0;
cJSON *g_last_deleted = nullptr;
}

extern "C" void cJSON_Delete(cJSON *item)
{
    if (item) {
        ++g_delete_calls;
        g_last_deleted = item;
    }
}

int main()
{
    cJSON first = {};
    cJSON second = {};

    {
        XiaozhiJsonOwner owner;
        assert(!owner);
        assert(owner.get() == nullptr);
        owner.reset(&first);
        assert(owner && owner.get() == &first);
        owner.reset(&first);
        assert(g_delete_calls == 0);
        owner.reset(&second);
        assert(g_delete_calls == 1 && g_last_deleted == &first);
        assert(owner.get() == &second);
        owner.reset();
        assert(g_delete_calls == 2 && g_last_deleted == &second);
        assert(!owner);
    }
    assert(g_delete_calls == 2);

    {
        XiaozhiJsonOwner owner(&first);
        const XiaozhiJsonOwner &read_only = owner;
        assert(read_only.get() == &first);
    }
    assert(g_delete_calls == 3 && g_last_deleted == &first);
    return 0;
}
