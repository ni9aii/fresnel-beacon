#include "unity.h"
#include "led_driver.h"

TEST_CASE("led_driver header compiles and types are correct", "[led_driver]") {
    /* Compile-time / link-time check that the header is usable */
    TEST_ASSERT_EQUAL(64, LED_MATRIX_LEN);
    TEST_ASSERT_EQUAL(8, LED_MATRIX_COLS);
    TEST_ASSERT_EQUAL(8, LED_MATRIX_ROWS);
    TEST_ASSERT_EQUAL(39, LED_MATRIX_GPIO);

    rgb_t color = {.r = 0xFF, .g = 0xA0, .b = 0x28};
    TEST_ASSERT_EQUAL_UINT8(0xFF, color.r);
    TEST_ASSERT_EQUAL_UINT8(0xA0, color.g);
    TEST_ASSERT_EQUAL_UINT8(0x28, color.b);
}

TEST_CASE("led_driver function symbols are declared", "[led_driver]") {
    /* Ensure function pointers are resolvable (link check) */
    esp_err_t (*init_fn)(void) = led_driver_init;
    esp_err_t (*set_fn)(uint8_t, rgb_t) = led_driver_set_pixel;
    esp_err_t (*flush_fn)(void) = led_driver_flush;
    void (*clear_fn)(void) = led_driver_clear;

    TEST_ASSERT_NOT_NULL(init_fn);
    TEST_ASSERT_NOT_NULL(set_fn);
    TEST_ASSERT_NOT_NULL(flush_fn);
    TEST_ASSERT_NOT_NULL(clear_fn);
}
