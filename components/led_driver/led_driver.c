#include "led_driver.h"
#include <string.h>
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"

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
static uint8_t s_pixels[LED_MATRIX_LEN * 3];

esp_err_t led_driver_init(void)
{
    ESP_LOGI(TAG, "init RMT on GPIO %d, %d LEDs", LED_MATRIX_GPIO, LED_MATRIX_LEN);

    rmt_tx_channel_config_t chan_cfg = {
        .gpio_num          = LED_MATRIX_GPIO,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &s_led_chan));
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&s_encoder_cfg, &s_led_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_led_chan));

    led_driver_clear();
    return led_driver_flush();
}

esp_err_t led_driver_set_pixel(uint8_t index, rgb_t color)
{
    if (index >= LED_MATRIX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    // WS2812B expects GRB, not RGB
    s_pixels[index * 3 + 0] = color.g;
    s_pixels[index * 3 + 1] = color.r;
    s_pixels[index * 3 + 2] = color.b;
    return ESP_OK;
}

void led_driver_clear(void)
{
    memset(s_pixels, 0, sizeof(s_pixels));
}

esp_err_t led_driver_flush(void)
{
    // Wait for any previous transmission before writing new frame
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_led_chan, portMAX_DELAY));

    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    return rmt_transmit(s_led_chan, s_led_encoder, s_pixels, sizeof(s_pixels), &tx_cfg);
}
