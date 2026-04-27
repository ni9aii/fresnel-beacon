#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "components/beacon_animation/include/beacon_math.h"

static int failures = 0;

#define ASSERT_EQ(got, expected) do { \
    if ((got) != (expected)) { \
        fprintf(stderr, "FAIL %s:%d  pixel_index: got %d, expected %d\n", \
                __FILE__, __LINE__, (int)(got), (int)(expected)); \
        failures++; \
    } \
} while (0)

#define ASSERT_NEAR(got, expected, eps) do { \
    if (fabsf((got) - (expected)) > (eps)) { \
        fprintf(stderr, "FAIL %s:%d  angle_diff: got %.5f, expected %.5f\n", \
                __FILE__, __LINE__, (float)(got), (float)(expected)); \
        failures++; \
    } \
} while (0)

static void test_pixel_index(void)
{
    // Even rows: left-to-right
    ASSERT_EQ(pixel_index(0, 0),  0);   // row 0, left edge
    ASSERT_EQ(pixel_index(7, 0),  7);   // row 0, right edge
    ASSERT_EQ(pixel_index(0, 2), 16);   // row 2, left edge
    ASSERT_EQ(pixel_index(7, 2), 23);   // row 2, right edge
    ASSERT_EQ(pixel_index(0, 6), 48);   // row 6, left edge
    ASSERT_EQ(pixel_index(7, 6), 55);   // row 6, right edge

    // Odd rows: right-to-left (serpentine)
    ASSERT_EQ(pixel_index(0, 1), 15);   // row 1: 1*8 + (8-1-0)
    ASSERT_EQ(pixel_index(7, 1),  8);   // row 1: 1*8 + (8-1-7)
    ASSERT_EQ(pixel_index(3, 1), 12);   // row 1: 1*8 + (8-1-3)
    ASSERT_EQ(pixel_index(0, 7), 63);   // row 7: 7*8 + (8-1-0)
    ASSERT_EQ(pixel_index(7, 7), 56);   // row 7: 7*8 + (8-1-7)
}

static void test_angle_diff(void)
{
    const float pi = (float)M_PI;
    const float eps = 1e-5f;

    // No wrap needed
    ASSERT_NEAR(angle_diff(0.5f,  0.0f),  0.5f, eps);
    ASSERT_NEAR(angle_diff(0.0f,  0.0f),  0.0f, eps);
    ASSERT_NEAR(angle_diff(0.0f,  0.5f), -0.5f, eps);

    // Wraps down: diff just above +pi -> negative
    ASSERT_NEAR(angle_diff(pi + 0.1f, 0.0f), -(pi - 0.1f), eps);

    // Wraps up: diff just below -pi -> positive
    ASSERT_NEAR(angle_diff(-(pi + 0.1f), 0.0f), pi - 0.1f, eps);

    // Result always in (-pi, pi]
    ASSERT_NEAR(angle_diff(pi, 0.0f), pi, eps);
}

int main(void)
{
    test_pixel_index();
    test_angle_diff();

    if (failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }
    fprintf(stderr, "%d test(s) failed.\n", failures);
    return 1;
}
