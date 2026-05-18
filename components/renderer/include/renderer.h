#pragma once

#include "led_driver_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef ESP_PLATFORM
#include "esp_err.h"
#else
/* Host-test fallback so this header is includable without ESP-IDF */
#ifndef ESP_OK
typedef int esp_err_t;
#define ESP_OK              0
#define ESP_ERR_INVALID_ARG -1
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RENDERER_MAX_COLS 16
#define RENDERER_MAX_ROWS 16

/**
 * @brief Framebuffer for LED matrix.
 */
typedef struct {
    uint8_t pixels[RENDERER_MAX_COLS * RENDERER_MAX_ROWS * 3]; /**<< GRB buffer */
    uint8_t cols;                                              /**<< Number of columns */
    uint8_t rows;                                              /**<< Number of rows */
} framebuffer_t;

typedef struct renderer renderer_t;

/**
 * @brief Renderer vtable (strategy pattern).
 *
 * Allows switching between LED driver (hardware) and mock (host tests).
 */
struct renderer {
    const char *name;                                                /**<< Renderer name */
    esp_err_t (*init)(renderer_t *self, uint8_t cols, uint8_t rows); /**<< Initialise */
    void (*clear)(renderer_t *self);                                 /**<< Clear framebuffer */
    void (*set_pixel)(renderer_t *self, uint8_t index, rgb_t color); /**<< Set pixel */
    esp_err_t (*flush)(renderer_t *self);                            /**<< Flush to hardware */
    void (*deinit)(renderer_t *self);                                /**<< Cleanup */
    framebuffer_t fb;                                                /**<< Internal framebuffer */
};

// Built-in renderers
extern renderer_t led_renderer;  /**<< Hardware LED renderer */
extern renderer_t mock_renderer; /**<< Host test renderer (no-op) */

/**
 * @brief Initialise renderer with given dimensions.
 */
esp_err_t renderer_init(renderer_t *r, uint8_t cols, uint8_t rows);

/**
 * @brief Clear framebuffer.
 */
void renderer_clear(renderer_t *r);

/**
 * @brief Set pixel color.
 */
void renderer_set_pixel(renderer_t *r, uint8_t index, rgb_t color);

/**
 * @brief Flush framebuffer to hardware.
 */
esp_err_t renderer_flush(renderer_t *r);

/**
 * @brief Deinitialise renderer.
 */
void renderer_deinit(renderer_t *r);

#ifdef __cplusplus
}
#endif
