#include "unity.h"
#include "http_server.h"

TEST_CASE("http_server header compiles and function symbols are declared", "[http_server]") {
    /* Compile-time / link-time check */
    esp_err_t (*init_fn)(void) = http_server_init;
    void (*stop_fn)(void) = http_server_stop;

    TEST_ASSERT_NOT_NULL(init_fn);
    TEST_ASSERT_NOT_NULL(stop_fn);
}
