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

typedef struct {
    uint8_t pixels[RENDERER_MAX_COLS * RENDERER_MAX_ROWS * 3]; // GRB buffer
    uint8_t cols;
    uint8_t rows;
} framebuffer_t;

typedef struct renderer renderer_t;

struct renderer {
    const char *name;
    esp_err_t (*init)(renderer_t *self, uint8_t cols, uint8_t rows);
    void (*clear)(renderer_t *self);
    void (*set_pixel)(renderer_t *self, uint8_t index, rgb_t color);
    esp_err_t (*flush)(renderer_t *self);
    void (*deinit)(renderer_t *self);
    framebuffer_t fb;
};

// Built-in renderers
extern renderer_t led_renderer;  // Uses led_driver
extern renderer_t mock_renderer; // For host tests

esp_err_t renderer_init(renderer_t *r, uint8_t cols, uint8_t rows);
void renderer_clear(renderer_t *r);
void renderer_set_pixel(renderer_t *r, uint8_t index, rgb_t color);
esp_err_t renderer_flush(renderer_t *r);
void renderer_deinit(renderer_t *r);

#ifdef __cplusplus
}
#endif
