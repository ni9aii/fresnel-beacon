/*
 * Host unit tests for led_driver logic (mocked).
 *
 * We can't link the real led_driver (needs RMT peripheral), but we can
 * verify the GRB byte-order logic and bounds checking by including a
 * minimal mock of the pixel buffer.
 *
 * Compile: gcc -I. test/test_led_driver.c -o test_led_driver && ./test_led_driver
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Pull in shared types and constants (no ESP-IDF dependencies) */
#include "components/led_driver/include/led_driver_types.h"

/* ---- mock: led_driver internals ---- */

/* Replicate the GRB buffer from led_driver.c */
static uint8_t s_pixels[LED_MATRIX_LEN * 3];

static void mock_clear(void) {
    memset(s_pixels, 0, sizeof(s_pixels));
}

/* Replicate the GRB byte-order logic from led_driver_set_pixel */
static int mock_set_pixel(uint8_t index, rgb_t color) {
    if (index >= LED_MATRIX_LEN) {
        return -1; /* ESP_ERR_INVALID_ARG */
    }
    s_pixels[index * 3 + 0] = color.g;
    s_pixels[index * 3 + 1] = color.r;
    s_pixels[index * 3 + 2] = color.b;
    return 0;
}

/* ---- test framework ---- */

static int failures = 0;

#define ASSERT_EQ(got, expected)                                                                   \
    do {                                                                                           \
        if ((got) != (expected)) {                                                                 \
            fprintf(stderr, "FAIL %s:%d  got %d, expected %d\n", __FILE__, __LINE__, (int) (got),  \
                    (int) (expected));                                                             \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

#define ASSERT_ZERO(expr)                                                                          \
    do {                                                                                           \
        if ((expr) != 0) {                                                                         \
            fprintf(stderr, "FAIL %s:%d  expected 0, got %d\n", __FILE__, __LINE__, (int) (expr)); \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

#define ASSERT_NONZERO(expr)                                                                       \
    do {                                                                                           \
        if ((expr) == 0) {                                                                         \
            fprintf(stderr, "FAIL %s:%d  expected nonzero\n", __FILE__, __LINE__);                 \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/* ---- tests ---- */

static void test_grb_byte_order(void) {
    mock_clear();
    rgb_t c = {.r = 10, .g = 20, .b = 30};
    ASSERT_ZERO(mock_set_pixel(0, c));

    /* WS2812B expects GRB: byte 0=G, byte 1=R, byte 2=B */
    ASSERT_EQ(s_pixels[0], 20); /* G */
    ASSERT_EQ(s_pixels[1], 10); /* R */
    ASSERT_EQ(s_pixels[2], 30); /* B */
}

static void test_pixel_indices(void) {
    mock_clear();
    rgb_t r = {.r = 255, .g = 0, .b = 0};

    /* First pixel */
    ASSERT_ZERO(mock_set_pixel(0, r));
    ASSERT_EQ(s_pixels[1], 255);

    /* Last pixel (index 63) */
    ASSERT_ZERO(mock_set_pixel(63, r));
    ASSERT_EQ(s_pixels[63 * 3 + 1], 255);
}

static void test_bounds_reject(void) {
    rgb_t c = {.r = 1, .g = 1, .b = 1};
    ASSERT_NONZERO(mock_set_pixel(LED_MATRIX_LEN, c)); /* 64 — out of range */
    ASSERT_NONZERO(mock_set_pixel(255, c));            /* far out of range */
}

static void test_clear_zeros_buffer(void) {
    rgb_t c = {.r = 0xFF, .g = 0xFF, .b = 0xFF};
    mock_set_pixel(0, c);
    mock_set_pixel(63, c);

    mock_clear();

    for (int i = 0; i < (int) sizeof(s_pixels); i++) {
        ASSERT_EQ(s_pixels[i], 0);
    }
}

static void test_overwrite_pixel(void) {
    mock_clear();
    rgb_t a = {.r = 100, .g = 150, .b = 200};
    rgb_t b = {.r = 10, .g = 20, .b = 30};

    mock_set_pixel(5, a);
    mock_set_pixel(5, b);

    /* Should contain the second write, not the first */
    ASSERT_EQ(s_pixels[5 * 3 + 0], 20); /* G */
    ASSERT_EQ(s_pixels[5 * 3 + 1], 10); /* R */
    ASSERT_EQ(s_pixels[5 * 3 + 2], 30); /* B */
}

int main(void) {
    test_grb_byte_order();
    test_pixel_indices();
    test_bounds_reject();
    test_clear_zeros_buffer();
    test_overwrite_pixel();

    if (failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }
    fprintf(stderr, "%d test(s) failed.\n", failures);
    return 1;
}
