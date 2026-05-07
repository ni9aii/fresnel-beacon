#include "config_manager.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "config_manager";

static runtime_config_t s_config;
static SemaphoreHandle_t s_config_mutex = NULL;

/* Defaults */
#define DEFAULT_SPEED_RPM    8.0f
#define DEFAULT_MODE         0
#define DEFAULT_BRIGHTNESS   1.0f
#define DEFAULT_COLOR_RGB    0xFFA028  /* warm amber */

esp_err_t config_manager_init(void)
{
    if (s_config_mutex == NULL) {
        s_config_mutex = xSemaphoreCreateMutex();
        if (s_config_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create config mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    s_config.speed_rpm   = DEFAULT_SPEED_RPM;
    s_config.mode          = DEFAULT_MODE;
    s_config.brightness    = DEFAULT_BRIGHTNESS;
    s_config.color_rgb     = DEFAULT_COLOR_RGB;

    ESP_LOGI(TAG, "Runtime config initialised (speed=%.1f rpm, brightness=%.2f, color=0x%06X)",
             s_config.speed_rpm, s_config.brightness, s_config.color_rgb);
    return ESP_OK;
}

esp_err_t config_manager_get(runtime_config_t *out_cfg)
{
    if (out_cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    *out_cfg = s_config;
    xSemaphoreGive(s_config_mutex);
    return ESP_OK;
}

esp_err_t config_manager_set(const runtime_config_t *in_cfg)
{
    if (in_cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_config = *in_cfg;
    xSemaphoreGive(s_config_mutex);
    return ESP_OK;
}

esp_err_t config_manager_set_speed(float rpm)
{
    if (s_config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_config.speed_rpm = rpm;
    xSemaphoreGive(s_config_mutex);
    return ESP_OK;
}

esp_err_t config_manager_set_mode(int32_t mode)
{
    if (s_config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_config.mode = mode;
    xSemaphoreGive(s_config_mutex);
    return ESP_OK;
}

esp_err_t config_manager_set_brightness(float brightness)
{
    if (s_config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_config.brightness = brightness;
    xSemaphoreGive(s_config_mutex);
    return ESP_OK;
}

esp_err_t config_manager_set_color(uint32_t rgb)
{
    if (s_config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_config.color_rgb = rgb;
    xSemaphoreGive(s_config_mutex);
    return ESP_OK;
}
