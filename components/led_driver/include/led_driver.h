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
 * Thread-safe when led_driver_init() has been called (internal mutex).
 *
 * @file led_driver.h
 * @defgroup led_driver LED Driver
 * @{
 */

/**
 * @brief Initialise LED driver (RMT + encoder + mutex).
 * @return ESP_OK on success, error code on RMT/encoder init failure.
 */
esp_err_t led_driver_init(void);

/**
 * @brief Deinitialise LED driver and free resources.
 */
void led_driver_deinit(void);

/**
 * @brief Set pixel color (GRB order).
 * @param index Pixel index (0 to LED_MATRIX_LEN-1).
 * @param color RGB color value.
 * @return ESP_OK on success.
 */
esp_err_t led_driver_set_pixel(uint8_t index, rgb_t color);

/**
 * @brief Flush pixel buffer to LEDs via RMT.
 * @return ESP_OK on success.
 */
esp_err_t led_driver_flush(void);

/**
 * @brief Clear all pixels (set to black).
 */
void led_driver_clear(void);

/** @} */

#ifdef __cplusplus
}
#endif
