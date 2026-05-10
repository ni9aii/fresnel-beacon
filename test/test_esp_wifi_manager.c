#include "unity.h"
#include "wifi_manager.h"

TEST_CASE("wifi_manager header compiles and enum values are correct", "[wifi_manager]") {
    /* Compile-time / link-time check */
    TEST_ASSERT_EQUAL(0, WIFI_STATUS_DISCONNECTED);
    TEST_ASSERT_EQUAL(1, WIFI_STATUS_CONNECTING);
    TEST_ASSERT_EQUAL(2, WIFI_STATUS_CONNECTED);
    TEST_ASSERT_EQUAL(3, WIFI_STATUS_AP_MODE);
}

TEST_CASE("wifi_manager function symbols are declared", "[wifi_manager]") {
    esp_err_t (*init_fn)(void) = wifi_manager_init;
    wifi_manager_status_t (*status_fn)(void) = wifi_manager_get_status;
    const char *(*ip_fn)(void) = wifi_manager_get_ip;
    esp_err_t (*ap_fn)(void) = wifi_manager_start_ap;

    TEST_ASSERT_NOT_NULL(init_fn);
    TEST_ASSERT_NOT_NULL(status_fn);
    TEST_ASSERT_NOT_NULL(ip_fn);
    TEST_ASSERT_NOT_NULL(ap_fn);
}
