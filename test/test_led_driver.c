/*
 * Host unit tests for renderer_t mock implementation.
 *
 * Uses mock_renderer (framebuffer only, no RMT) to verify
 * GRB byte-order logic and bounds checking.
 *
 * Compile: gcc -I. -Icomponents/renderer/include test/test_led_driver.c -o test_led_driver && ./test_led_driver
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Pull in shared types and renderer (no ESP-IDF dependencies) */
#include "components/led_driver/include/led_driver_types.h"
#include "components/renderer/include/renderer.h"

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
    renderer_init(&mock_renderer, LED_MATRIX_COLS, LED_MATRIX_ROWS);
    rgb_t c = {.r = 10, .g = 20, .b = 30};
    renderer_set_pixel(&mock_renderer, 0, c);

    /* WS2812B expects GRB: byte 0=G, byte 1=R, byte 2=B */
    ASSERT_EQ(mock_renderer.fb.pixels[0], 20); /* G */
    ASSERT_EQ(mock_renderer.fb.pixels[1], 10); /* R */
    ASSERT_EQ(mock_renderer.fb.pixels[2], 30); /* B */
}

static void test_pixel_indices(void) {
    renderer_clear(&mock_renderer);
    rgb_t r = {.r = 255, .g = 0, .b = 0};

    /* First pixel */
    renderer_set_pixel(&mock_renderer, 0, r);
    ASSERT_EQ(mock_renderer.fb.pixels[1], 255);

    /* Last pixel (index 63) */
    renderer_set_pixel(&mock_renderer, 63, r);
    ASSERT_EQ(mock_renderer.fb.pixels[63 * 3 + 1], 255);
}

static void test_bounds_reject(void) {
    rgb_t c = {.r = 1, .g = 1, .b = 1};
    /* mock_renderer silently ignores out-of-bounds; verify no crash */
    renderer_set_pixel(&mock_renderer, LED_MATRIX_LEN, c); /* 64 — out of range */
    renderer_set_pixel(&mock_renderer, 255, c);          /* far out of range */
    /* If we get here without crash, bounds are handled */
}

static void test_clear_zeros_buffer(void) {
    rgb_t c = {.r = 0xFF, .g = 0xFF, .b = 0xFF};
    renderer_set_pixel(&mock_renderer, 0, c);
    renderer_set_pixel(&mock_renderer, 63, c);

    renderer_clear(&mock_renderer);

    for (int i = 0; i < LED_MATRIX_LEN * 3; i++) {
        ASSERT_EQ(mock_renderer.fb.pixels[i], 0);
    }
}

static void test_overwrite_pixel(void) {
    renderer_clear(&mock_renderer);
    rgb_t a = {.r = 100, .g = 150, .b = 200};
    rgb_t b = {.r = 10, .g = 20, .b = 30};

    renderer_set_pixel(&mock_renderer, 5, a);
    renderer_set_pixel(&mock_renderer, 5, b);

    /* Should contain the second write, not the first */
    ASSERT_EQ(mock_renderer.fb.pixels[5 * 3 + 0], 20); /* G */
    ASSERT_EQ(mock_renderer.fb.pixels[5 * 3 + 1], 10); /* R */
    ASSERT_EQ(mock_renderer.fb.pixels[5 * 3 + 2], 30); /* B */
}

int main(void) {
    renderer_init(&mock_renderer, LED_MATRIX_COLS, LED_MATRIX_ROWS);

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
