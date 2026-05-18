#include "renderer.h"
#include <string.h>

#ifdef ESP_PLATFORM
#include "led_driver.h"
#include "esp_log.h"
#endif

/* ---- public dispatch wrappers ---- */

esp_err_t renderer_init(renderer_t *r, uint8_t cols, uint8_t rows) {
    if (r == NULL || r->init == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return r->init(r, cols, rows);
}

void renderer_clear(renderer_t *r) {
    if (r != NULL && r->clear != NULL) {
        r->clear(r);
    }
}

void renderer_set_pixel(renderer_t *r, uint8_t index, rgb_t color) {
    if (r != NULL && r->set_pixel != NULL) {
        r->set_pixel(r, index, color);
    }
}

esp_err_t renderer_flush(renderer_t *r) {
    if (r == NULL || r->flush == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return r->flush(r);
}

void renderer_deinit(renderer_t *r) {
    if (r != NULL && r->deinit != NULL) {
        r->deinit(r);
    }
}

/* ---- mock_renderer: framebuffer-only, no hardware ---- */

static esp_err_t mock_renderer_init(renderer_t *self, uint8_t cols, uint8_t rows) {
    if (cols > RENDERER_MAX_COLS || rows > RENDERER_MAX_ROWS) {
        return ESP_ERR_INVALID_ARG;
    }
    self->fb.cols = cols;
    self->fb.rows = rows;
    memset(self->fb.pixels, 0, sizeof(self->fb.pixels));
    return ESP_OK;
}

static void mock_renderer_clear(renderer_t *self) {
    memset(self->fb.pixels, 0, sizeof(self->fb.pixels));
}

static void mock_renderer_set_pixel(renderer_t *self, uint8_t index, rgb_t color) {
    uint16_t max = (uint16_t) self->fb.cols * self->fb.rows;
    if (index >= max) {
        return;
    }
    // WS2812B expects GRB
    self->fb.pixels[index * 3 + 0] = color.g;
    self->fb.pixels[index * 3 + 1] = color.r;
    self->fb.pixels[index * 3 + 2] = color.b;
}

static esp_err_t mock_renderer_flush(renderer_t *self) {
    (void) self;
    return ESP_OK;
}

static void mock_renderer_deinit(renderer_t *self) {
    memset(self->fb.pixels, 0, sizeof(self->fb.pixels));
    self->fb.cols = 0;
    self->fb.rows = 0;
}

renderer_t mock_renderer = {
    .name = "mock",
    .init = mock_renderer_init,
    .clear = mock_renderer_clear,
    .set_pixel = mock_renderer_set_pixel,
    .flush = mock_renderer_flush,
    .deinit = mock_renderer_deinit,
    .fb = {{0}, 0, 0},
};

/* ---- led_renderer: wraps led_driver (ESP-IDF target only) ---- */

#ifdef ESP_PLATFORM

static esp_err_t led_renderer_init(renderer_t *self, uint8_t cols, uint8_t rows) {
    if (cols > RENDERER_MAX_COLS || rows > RENDERER_MAX_ROWS) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = led_driver_init();
    if (err != ESP_OK) {
        ESP_LOGE("renderer", "led_driver_init failed: %s", esp_err_to_name(err));
        return err;
    }

    self->fb.cols = cols;
    self->fb.rows = rows;
    memset(self->fb.pixels, 0, sizeof(self->fb.pixels));
    return ESP_OK;
}

static void led_renderer_clear(renderer_t *self) {
    led_driver_clear();
    memset(self->fb.pixels, 0, sizeof(self->fb.pixels));
}

static void led_renderer_set_pixel(renderer_t *self, uint8_t index, rgb_t color) {
    uint16_t max = (uint16_t) self->fb.cols * self->fb.rows;
    if (index < max) {
        self->fb.pixels[index * 3 + 0] = color.g;
        self->fb.pixels[index * 3 + 1] = color.r;
        self->fb.pixels[index * 3 + 2] = color.b;
    }
    esp_err_t err = led_driver_set_pixel(index, color);
    if (err != ESP_OK) {
        ESP_LOGW("renderer", "led_driver_set_pixel(%u) failed: %s", index, esp_err_to_name(err));
    }
}

static esp_err_t led_renderer_flush(renderer_t *self) {
    (void) self;
    return led_driver_flush();
}

static void led_renderer_deinit(renderer_t *self) {
    led_driver_deinit();
    memset(self->fb.pixels, 0, sizeof(self->fb.pixels));
    self->fb.cols = 0;
    self->fb.rows = 0;
}

renderer_t led_renderer = {
    .name = "led",
    .init = led_renderer_init,
    .clear = led_renderer_clear,
    .set_pixel = led_renderer_set_pixel,
    .flush = led_renderer_flush,
    .deinit = led_renderer_deinit,
    .fb = {{0}, 0, 0},
};

#endif /* ESP_PLATFORM */
