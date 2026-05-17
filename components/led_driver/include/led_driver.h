#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"
#include "led_driver_types.h"

/**
 * @brief LED driver for WS2812B matrix on Waveshare ESP32-S3-Matrix.
 *
 * @note Thread-safe when led_driver_init() has been called (internal mutex).
 */

void led_driver_init(void);
void led_driver_deinit(void);
esp_err_t led_driver_set_pixel(uint8_t index, rgb_t color);
esp_err_t led_driver_flush(void);
void led_driver_clear(void);

#ifdef __cplusplus
}
#endif
