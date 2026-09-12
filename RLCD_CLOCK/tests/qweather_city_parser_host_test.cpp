// 验证 QWeather 城市首项必填、可选字段和短路复制语义。
#include "qweather_city_parser.h"

#include "cJSON.h"

#include <assert.h>
#include <string.h>

namespace {
cJSON *parse_json(const char *text)
{
    cJSON *root = cJSON_Parse(text);
    assert(root != nullptr);
    return root;
}
} // namespace

int main()
{
    char city_id[16] = "id-old";
    char city_name[32] = "name-old";
    char latitude[16] = "lat-old";
    char longitude[16] = "lon-old";
    assert(!parse_qweather_city_location(nullptr,
                                         city_id,
                                         sizeof(city_id),
                                         city_name,
                                         sizeof(city_name),
                                         latitude,
                                         sizeof(latitude),
                                         longitude,
                                         sizeof(longitude)));

    cJSON *location = parse_json("[]");
    assert(!parse_qweather_city_location(location,
                                         city_id,
                                         sizeof(city_id),
                                         city_name,
                                         sizeof(city_name),
                                         latitude,
                                         sizeof(latitude),
                                         longitude,
                                         sizeof(longitude)));
    cJSON_Delete(location);

    location = parse_json(
        "{\"id\":\"101210101\",\"name\":\"杭州\",\"lat\":\"30.28746\",\"lon\":\"120.15358\"}");
    assert(parse_qweather_city_location(location,
                                        city_id,
                                        sizeof(city_id),
                                        city_name,
                                        sizeof(city_name),
                                        latitude,
                                        sizeof(latitude),
                                        longitude,
                                        sizeof(longitude)));
    assert(strcmp(city_id, "101210101") == 0);
    assert(strcmp(city_name, "杭州") == 0);
    assert(strcmp(latitude, "30.28746") == 0);
    assert(strcmp(longitude, "120.15358") == 0);
    cJSON_Delete(location);

    strlcpy(city_id, "id-old", sizeof(city_id));
    strlcpy(city_name, "name-old", sizeof(city_name));
    strlcpy(latitude, "lat-old", sizeof(latitude));
    strlcpy(longitude, "lon-old", sizeof(longitude));
    location = parse_json("{\"id\":\"101020100\",\"lat\":\"31.23\",\"lon\":\"121.47\"}");
    assert(!parse_qweather_city_location(location,
                                         city_id,
                                         sizeof(city_id),
                                         city_name,
                                         sizeof(city_name),
                                         latitude,
                                         sizeof(latitude),
                                         longitude,
                                         sizeof(longitude)));
    assert(strcmp(city_id, "id-old") == 0);
    assert(strcmp(city_name, "name-old") == 0);
    assert(strcmp(latitude, "lat-old") == 0);
    assert(strcmp(longitude, "lon-old") == 0);
    cJSON_Delete(location);

    strlcpy(city_id, "id-old", sizeof(city_id));
    strlcpy(city_name, "name-old", sizeof(city_name));
    location = parse_json("{\"name\":\"上海\"}");
    assert(!parse_qweather_city_location(location,
                                         city_id,
                                         sizeof(city_id),
                                         city_name,
                                         sizeof(city_name),
                                         nullptr,
                                         0,
                                         nullptr,
                                         0));
    assert(strcmp(city_id, "id-old") == 0);
    assert(strcmp(city_name, "name-old") == 0);
    cJSON_Delete(location);

    location = parse_json("{\"id\":101010100,\"name\":\"北京\"}");
    assert(!parse_qweather_city_location(location,
                                         city_id,
                                         sizeof(city_id),
                                         city_name,
                                         sizeof(city_name),
                                         latitude,
                                         sizeof(latitude),
                                         longitude,
                                         sizeof(longitude)));
    assert(strcmp(city_id, "id-old") == 0);
    assert(strcmp(city_name, "name-old") == 0);
    assert(strcmp(latitude, "lat-old") == 0);
    assert(strcmp(longitude, "lon-old") == 0);
    cJSON_Delete(location);

    strlcpy(latitude, "lat-old", sizeof(latitude));
    strlcpy(longitude, "lon-old", sizeof(longitude));
    location = parse_json("{\"id\":\"101190101\",\"name\":\"南京\"}");
    assert(parse_qweather_city_location(location,
                                        city_id,
                                        sizeof(city_id),
                                        city_name,
                                        sizeof(city_name),
                                        latitude,
                                        sizeof(latitude),
                                        longitude,
                                        sizeof(longitude)));
    assert(strcmp(city_id, "101190101") == 0);
    assert(strcmp(city_name, "南京") == 0);
    assert(strcmp(latitude, "lat-old") == 0);
    assert(strcmp(longitude, "lon-old") == 0);
    cJSON_Delete(location);

    location = parse_json("{\"id\":\"101280101\",\"name\":\"广州\",\"lat\":\"23.13\",\"lon\":\"113.26\"}");
    char short_latitude[4] = {};
    assert(parse_qweather_city_location(location,
                                        city_id,
                                        sizeof(city_id),
                                        city_name,
                                        sizeof(city_name),
                                        short_latitude,
                                        sizeof(short_latitude),
                                        nullptr,
                                        0));
    assert(strcmp(short_latitude, "23.") == 0);
    cJSON_Delete(location);

    location = parse_json("{\"id\":\"101010100\",\"name\":\"北京\"}");
    assert(!parse_qweather_city_location(location,
                                         nullptr,
                                         0,
                                         city_name,
                                         sizeof(city_name),
                                         nullptr,
                                         0,
                                         nullptr,
                                         0));
    cJSON_Delete(location);

    return 0;
}
