#include "led_driver.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"
#include "rom/ets_sys.h"

static const char *TAG = "led_driver";

// 20 MHz resolution → 50 ns per tick
#define RMT_RESOLUTION_HZ   20000000

// WS2812B bit timing (datasheet values, 50 ns/tick)
// T0H = 400 ns → 8 ticks,  T0L = 850 ns → 17 ticks
// T1H = 800 ns → 16 ticks, T1L = 450 ns →  9 ticks
static const rmt_bytes_encoder_config_t s_encoder_cfg = {
    .bit0 = { .level0 = 1, .duration0 = 8,
               .level1 = 0, .duration1 = 17 },
    .bit1 = { .level0 = 1, .duration0 = 16,
               .level1 = 0, .duration1 =  9 },
    .flags.msb_first = 1,
};

static rmt_channel_handle_t s_led_chan;
static rmt_encoder_handle_t s_led_encoder;

// GRB byte order as required by WS2812B
// Double-buffer: front buffer written by set_pixel/clear, back buffer handed to RMT
static uint8_t s_pixels_front[LED_MATRIX_LEN * 3];
static uint8_t s_pixels_back[LED_MATRIX_LEN * 3];

SemaphoreHandle_t led_mutex = NULL;

static esp_err_t led_mutex_init(void)
{
    if (led_mutex == NULL) {
        led_mutex = xSemaphoreCreateMutex();
        if (led_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create LED mutex");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "LED mutex created");
    }
    return ESP_OK;
}

void led_driver_init(void)
{
    ESP_LOGI(TAG, "init RMT on GPIO %d, %d LEDs", LED_MATRIX_GPIO, LED_MATRIX_LEN);

    rmt_tx_channel_config_t chan_cfg = {
        .gpio_num          = LED_MATRIX_GPIO,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 128,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &s_led_chan));
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&s_encoder_cfg, &s_led_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_led_chan));

    ESP_ERROR_CHECK(led_mutex_init());

    memset(s_pixels_front, 0, sizeof(s_pixels_front));
    memset(s_pixels_back,  0, sizeof(s_pixels_back));
    ESP_ERROR_CHECK(led_driver_flush());
}

esp_err_t led_driver_set_pixel(uint8_t index, rgb_t color)
{
    if (index >= LED_MATRIX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    if (led_mutex != NULL) {
        if (xSemaphoreTake(led_mutex, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "set_pixel: mutex take failed");
            return ESP_ERR_TIMEOUT;
        }
    }

    // WS2812B expects GRB, not RGB
    s_pixels_front[index * 3 + 0] = color.g;
    s_pixels_front[index * 3 + 1] = color.r;
    s_pixels_front[index * 3 + 2] = color.b;

    if (led_mutex != NULL) {
        xSemaphoreGive(led_mutex);
    }
    return ESP_OK;
}

void led_driver_clear(void)
{
    if (led_mutex != NULL) {
        if (xSemaphoreTake(led_mutex, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "clear: mutex take failed");
            return;
        }
    }

    memset(s_pixels_front, 0, sizeof(s_pixels_front));

    if (led_mutex != NULL) {
        xSemaphoreGive(led_mutex);
    }
}

esp_err_t led_driver_flush(void)
{
    if (led_mutex != NULL) {
        if (xSemaphoreTake(led_mutex, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "flush: mutex take failed");
            return ESP_ERR_TIMEOUT;
        }
    }

    // Wait for any previous transmission before writing new frame
    esp_err_t ret = rmt_tx_wait_all_done(s_led_chan, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_tx_wait_all_done failed: %s", esp_err_to_name(ret));
        if (led_mutex != NULL) {
            xSemaphoreGive(led_mutex);
        }
        return ret;
    }

    // Copy front buffer to back buffer under mutex, then release
    memcpy(s_pixels_back, s_pixels_front, sizeof(s_pixels_front));

    if (led_mutex != NULL) {
        xSemaphoreGive(led_mutex);
    }

    // Hand the stable back buffer to RMT (non-blocking DMA read)
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    ret = rmt_transmit(s_led_chan, s_led_encoder, s_pixels_back, sizeof(s_pixels_back), &tx_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_transmit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // WS2812B reset: >50 us LOW between frames
    esp_rom_delay_us(60);
    return ESP_OK;
}

void led_driver_deinit(void)
{
    if (s_led_encoder != NULL) {
        ESP_ERROR_CHECK(rmt_del_encoder(s_led_encoder));
        s_led_encoder = NULL;
    }
    if (s_led_chan != NULL) {
        ESP_ERROR_CHECK(rmt_disable(s_led_chan));
        ESP_ERROR_CHECK(rmt_del_channel(s_led_chan));
        s_led_chan = NULL;
    }
    led_driver_clear();
}
