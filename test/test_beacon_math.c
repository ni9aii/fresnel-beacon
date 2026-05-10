/*
 * Host unit tests for beacon_math: pixel_index and angle_diff.
 *
 * Compile: gcc -I. test/test_beacon_math.c -lm -o test_beacon_math && ./test_beacon_math
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "components/beacon_animation/include/beacon_math.h"

#define LED_MATRIX_ROWS 8
#define LED_MATRIX_COLS 8

static int failures = 0;

#define ASSERT_EQ(got, expected)                                                                   \
    do {                                                                                           \
        if ((got) != (expected)) {                                                                 \
            fprintf(stderr, "FAIL %s:%d  pixel_index: got %d, expected %d\n", __FILE__, __LINE__,  \
                    (int) (got), (int) (expected));                                                \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

#define ASSERT_NEAR(got, expected, eps)                                                            \
    do {                                                                                           \
        if (fabsf((got) - (expected)) > (eps)) {                                                   \
            fprintf(stderr, "FAIL %s:%d  angle_diff: got %.6f, expected %.6f\n", __FILE__,         \
                    __LINE__, (float) (got), (float) (expected));                                  \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static void test_pixel_index_even_rows_left_to_right(void) {
    /* Even rows: left-to-right mapping */
    ASSERT_EQ(pixel_index(0, 0), 0);
    ASSERT_EQ(pixel_index(1, 0), 1);
    ASSERT_EQ(pixel_index(7, 0), 7);

    ASSERT_EQ(pixel_index(0, 2), 16);
    ASSERT_EQ(pixel_index(4, 2), 20);
    ASSERT_EQ(pixel_index(7, 2), 23);

    ASSERT_EQ(pixel_index(0, 4), 32);
    ASSERT_EQ(pixel_index(7, 4), 39);

    ASSERT_EQ(pixel_index(0, 6), 48);
    ASSERT_EQ(pixel_index(7, 6), 55);
}

static void test_pixel_index_odd_rows_right_to_left(void) {
    /* Odd rows: right-to-left (serpentine) */
    ASSERT_EQ(pixel_index(0, 1), 15); /* 1*8 + (8-1-0) = 15 */
    ASSERT_EQ(pixel_index(7, 1), 8);  /* 1*8 + (8-1-7) = 8  */
    ASSERT_EQ(pixel_index(3, 1), 12); /* 1*8 + (8-1-3) = 12 */

    ASSERT_EQ(pixel_index(0, 3), 31); /* 3*8 + (8-1-0) = 31 */
    ASSERT_EQ(pixel_index(7, 3), 24); /* 3*8 + (8-1-7) = 24 */

    ASSERT_EQ(pixel_index(0, 5), 47);
    ASSERT_EQ(pixel_index(7, 5), 40);

    ASSERT_EQ(pixel_index(0, 7), 63); /* last pixel */
    ASSERT_EQ(pixel_index(7, 7), 56);
}

static void test_pixel_index_contiguous(void) {
    /* Verify no gaps or overlaps: all 64 indices are unique */
    uint8_t seen[64] = {0};
    for (int y = 0; y < LED_MATRIX_ROWS; y++) {
        for (int x = 0; x < LED_MATRIX_COLS; x++) {
            uint8_t idx = pixel_index(x, y);
            ASSERT_EQ(idx < 64, 1);
            ASSERT_EQ(seen[idx], 0);
            seen[idx] = 1;
        }
    }
}

static void test_angle_diff_no_wrap(void) {
    const float eps = 1e-6f;
    ASSERT_NEAR(angle_diff(0.5f, 0.0f), 0.5f, eps);
    ASSERT_NEAR(angle_diff(0.0f, 0.0f), 0.0f, eps);
    ASSERT_NEAR(angle_diff(0.0f, 0.5f), -0.5f, eps);
    ASSERT_NEAR(angle_diff(1.0f, 0.5f), 0.5f, eps);
    ASSERT_NEAR(angle_diff(0.5f, 1.0f), -0.5f, eps);
}

static void test_angle_diff_wrap_positive(void) {
    const float pi = (float) M_PI;
    const float eps = 1e-5f;

    /* diff just above +pi wraps to negative */
    ASSERT_NEAR(angle_diff(pi + 0.1f, 0.0f), -(pi - 0.1f), eps);

    /* Large positive diff wraps correctly */
    ASSERT_NEAR(angle_diff(pi + 1.0f, -0.5f), -(pi - 1.5f), eps);
}

static void test_angle_diff_wrap_negative(void) {
    const float pi = (float) M_PI;
    const float eps = 1e-5f;

    /* diff just below -pi wraps to positive */
    ASSERT_NEAR(angle_diff(-(pi + 0.1f), 0.0f), pi - 0.1f, eps);

    /* Large negative diff wraps correctly */
    ASSERT_NEAR(angle_diff(-(pi + 1.0f), 0.5f), pi - 1.5f, eps);
}

static void test_angle_diff_boundary_pi(void) {
    const float pi = (float) M_PI;
    const float eps = 1e-5f;

    /* Exact +pi should stay +pi (not wrap to -pi) */
    ASSERT_NEAR(angle_diff(pi, 0.0f), pi, eps);

    /* Exact -pi from opposite direction */
    ASSERT_NEAR(angle_diff(-pi, 0.0f), -pi, eps);
}

static void test_angle_diff_symmetry(void) {
    const float eps = 1e-5f;

    /* angle_diff(a, b) ≈ -angle_diff(b, a) for most cases */
    float cases[] = {0.3f, 1.5f, 2.8f, -0.7f, -2.1f};
    int n = (int) (sizeof(cases) / sizeof(cases[0]));

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            float ab = angle_diff(cases[i], cases[j]);
            float ba = angle_diff(cases[j], cases[i]);
            ASSERT_NEAR(ab, -ba, eps);
        }
    }
}

int main(void) {
    printf("--- pixel_index ---\n");
    test_pixel_index_even_rows_left_to_right();
    test_pixel_index_odd_rows_right_to_left();
    test_pixel_index_contiguous();

    printf("--- angle_diff ---\n");
    test_angle_diff_no_wrap();
    test_angle_diff_wrap_positive();
    test_angle_diff_wrap_negative();
    test_angle_diff_boundary_pi();
    test_angle_diff_symmetry();

    if (failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }
    fprintf(stderr, "%d test(s) failed.\n", failures);
    return 1;
}
