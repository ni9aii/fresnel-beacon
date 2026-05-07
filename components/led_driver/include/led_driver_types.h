#pragma once

#include <stdint.h>

/* Shared types and constants for the LED driver.
 * This header is ESP-IDF-free so it can be included in host unit tests. */

// Waveshare ESP32-S3-Matrix: GPIO39, 8x8 WS2812B
#define LED_MATRIX_GPIO   39
#define LED_MATRIX_COLS   8
#define LED_MATRIX_ROWS   8
#define LED_MATRIX_LEN    (LED_MATRIX_COLS * LED_MATRIX_ROWS)

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;
