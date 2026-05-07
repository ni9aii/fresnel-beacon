#pragma once
#include <stdint.h>
#include <math.h>
#include "led_driver_types.h"

// Waveshare ESP32-S3-Matrix wires rows in serpentine order
static inline uint8_t pixel_index(int x, int y)
{
    return (y % 2 == 0)
        ? (uint8_t)(y * LED_MATRIX_COLS + x)
        : (uint8_t)(y * LED_MATRIX_COLS + (LED_MATRIX_COLS - 1 - x));
}

// Angular distance of pixel_angle behind beam_angle, normalised to (-pi, pi]
static inline float angle_diff(float beam_angle, float pixel_angle)
{
    float diff = beam_angle - pixel_angle;
    if      (diff >  (float)M_PI) diff -= 2.0f * (float)M_PI;
    else if (diff < -(float)M_PI) diff += 2.0f * (float)M_PI;
    return diff;
}
