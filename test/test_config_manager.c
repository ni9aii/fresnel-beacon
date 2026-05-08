#include "unity.h"
#include "config_manager.h"

TEST_CASE("config_manager_init sets defaults", "[config_manager]")
{
    esp_err_t err = config_manager_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    runtime_config_t cfg;
    err = config_manager_get(&cfg);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    TEST_ASSERT_EQUAL_FLOAT(8.0f, cfg.speed_rpm);
    TEST_ASSERT_EQUAL(0, cfg.mode);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, cfg.brightness);
    TEST_ASSERT_EQUAL_UINT32(0xFFA028, cfg.color_rgb);
    TEST_ASSERT_EQUAL('\0', cfg.wifi_ssid[0]);
    TEST_ASSERT_EQUAL('\0', cfg.wifi_pass[0]);
}

TEST_CASE("config_manager_set and get roundtrip", "[config_manager]")
{
    config_manager_init();

    runtime_config_t in = {
        .speed_rpm  = 15.5f,
        .mode       = 2,
        .brightness = 0.75f,
        .color_rgb  = 0x00FF00,
        .wifi_ssid  = "test_ssid",
        .wifi_pass  = "test_pass",
    };

    esp_err_t err = config_manager_set(&in);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    runtime_config_t out;
    err = config_manager_get(&out);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    TEST_ASSERT_EQUAL_FLOAT(in.speed_rpm, out.speed_rpm);
    TEST_ASSERT_EQUAL(in.mode, out.mode);
    TEST_ASSERT_EQUAL_FLOAT(in.brightness, out.brightness);
    TEST_ASSERT_EQUAL_UINT32(in.color_rgb, out.color_rgb);
    TEST_ASSERT_EQUAL_STRING(in.wifi_ssid, out.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING(in.wifi_pass, out.wifi_pass);
}

TEST_CASE("config_manager individual setters work", "[config_manager]")
{
    config_manager_init();

    TEST_ASSERT_EQUAL(ESP_OK, config_manager_set_speed(12.0f));
    TEST_ASSERT_EQUAL(ESP_OK, config_manager_set_mode(1));
    TEST_ASSERT_EQUAL(ESP_OK, config_manager_set_brightness(0.5f));
    TEST_ASSERT_EQUAL(ESP_OK, config_manager_set_color(0x123456));

    runtime_config_t cfg;
    TEST_ASSERT_EQUAL(ESP_OK, config_manager_get(&cfg));
    TEST_ASSERT_EQUAL_FLOAT(12.0f, cfg.speed_rpm);
    TEST_ASSERT_EQUAL(1, cfg.mode);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, cfg.brightness);
    TEST_ASSERT_EQUAL_UINT32(0x123456, cfg.color_rgb);
}
