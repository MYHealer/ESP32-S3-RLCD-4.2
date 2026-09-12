// 验证账户专属 QWeather Host 的系统证书包优先和旧证书兜底策略。
#include "http_tls_retry_policy.h"

#include <assert.h>

int main()
{
    assert(http_tls_attempt_count(false) == 1U);
    assert(http_tls_attempt_count(true) == 2U);

    assert(http_tls_trust_mode(false, 0) ==
           HttpTlsTrustMode::kCertificateBundle);
    assert(http_tls_trust_mode(true, 0) ==
           HttpTlsTrustMode::kCertificateBundle);
    assert(http_tls_trust_mode(true, 1) ==
           HttpTlsTrustMode::kQweatherLegacyCa);

    assert(!http_tls_should_retry(false, 0, true));
    assert(!http_tls_should_retry(true, 0, false));
    assert(http_tls_should_retry(true, 0, true));
    assert(!http_tls_should_retry(true, 1, true));
    return 0;
}
