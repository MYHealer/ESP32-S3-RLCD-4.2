// 验证 HTTP client 作用域守卫的初始化与单次清理语义。
#include "scoped_http_client.h"

#include <cassert>

struct HostHttpClient {
    int marker = 0;
};

namespace {
HostHttpClient s_client;
int s_init_calls = 0;
int s_cleanup_calls = 0;
bool s_init_succeeds = true;
} // namespace

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config)
{
    ++s_init_calls;
    if (!s_init_succeeds || !config) {
        return nullptr;
    }
    s_client.marker = config->marker;
    return &s_client;
}

void esp_http_client_cleanup(esp_http_client_handle_t client)
{
    assert(client == &s_client);
    ++s_cleanup_calls;
}

static void reset_state(bool init_succeeds = true)
{
    s_init_calls = 0;
    s_cleanup_calls = 0;
    s_init_succeeds = init_succeeds;
    s_client = {};
}

static void test_null_config_skips_initialization()
{
    reset_state();
    {
        ScopedHttpClient client(nullptr);
        assert(!client);
        assert(client.get() == nullptr);
    }
    assert(s_init_calls == 0);
    assert(s_cleanup_calls == 0);
}

static void test_failed_initialization_skips_cleanup()
{
    reset_state(false);
    esp_http_client_config_t config = {};
    {
        ScopedHttpClient client(&config);
        assert(!client);
    }
    assert(s_init_calls == 1);
    assert(s_cleanup_calls == 0);
}

static void test_successful_initialization_cleans_up_once()
{
    reset_state();
    esp_http_client_config_t config = {};
    config.marker = 42;
    {
        ScopedHttpClient client(&config);
        assert(client);
        assert(client.get() == &s_client);
        assert(client.get()->marker == config.marker);
        assert(s_cleanup_calls == 0);
    }
    assert(s_init_calls == 1);
    assert(s_cleanup_calls == 1);
}

int main()
{
    test_null_config_skips_initialization();
    test_failed_initialization_skips_cleanup();
    test_successful_initialization_cleans_up_once();
    return 0;
}
