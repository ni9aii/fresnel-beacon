#include "config_manager.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "config_manager";

static runtime_config_t s_config;
static SemaphoreHandle_t s_config_mutex = NULL;

/* Defaults */
#define DEFAULT_SPEED_RPM  8.0f
#define DEFAULT_MODE       0
#define DEFAULT_BRIGHTNESS 1.0f
#define DEFAULT_COLOR_RGB  0xFFA028 /* warm amber */

#define NVS_NAMESPACE "fresnel"

esp_err_t config_manager_init(void) {
    if (s_config_mutex == NULL) {
        s_config_mutex = xSemaphoreCreateMutex();
        if (s_config_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create config mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    s_config.speed_rpm = DEFAULT_SPEED_RPM;
    s_config.mode = DEFAULT_MODE;
    s_config.brightness = DEFAULT_BRIGHTNESS;
    s_config.color_rgb = DEFAULT_COLOR_RGB;
    s_config.wifi_ssid[0] = '\0';
    s_config.wifi_pass[0] = '\0';

    ESP_LOGI(TAG, "Runtime config initialized (speed=%.1f rpm, brightness=%.2f, color=0x%06X)",
             s_config.speed_rpm, s_config.brightness, (unsigned int) s_config.color_rgb);
    return ESP_OK;
}

esp_err_t config_manager_get(runtime_config_t *out_cfg) {
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

esp_err_t config_manager_set(const runtime_config_t *in_cfg) {
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

esp_err_t config_manager_set_speed(float rpm) {
    if (s_config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    // Basic validation: speed must be positive
    if (rpm <= 0.0f) {
        xSemaphoreGive(s_config_mutex);
        return ESP_ERR_INVALID_ARG;
    }
    s_config.speed_rpm = rpm;
    xSemaphoreGive(s_config_mutex);
    return ESP_OK;
}

esp_err_t config_manager_set_speed_sec(float sec) {
    // Convert seconds per rotation to RPM
    if (sec <= 0.0f) return ESP_ERR_INVALID_ARG;
    float rpm = 60.0f / sec;
    return config_manager_set_speed(rpm);
}

esp_err_t config_manager_get_speed_sec(float *out_sec) {
    if (out_sec == NULL) return ESP_ERR_INVALID_ARG;
    if (s_config_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_TIMEOUT;
    float rpm = s_config.speed_rpm;
    xSemaphoreGive(s_config_mutex);
    *out_sec = (rpm <= 0.0f) ? 1.0f : (60.0f / rpm);
    return ESP_OK;
}

esp_err_t config_manager_set_mode(int32_t mode) {
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

esp_err_t config_manager_set_brightness(float brightness) {
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

esp_err_t config_manager_set_color(uint32_t rgb) {
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

/* ---------- NVS read/write ---------- */
/* NVS API is thread-safe for internal state, but s_config struct is not */

esp_err_t config_manager_load_from_nvs(void) {
    if (s_config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed (%s), keeping defaults", esp_err_to_name(err));
        xSemaphoreGive(s_config_mutex);
        return ESP_OK; /* non-fatal: fall back to defaults */
    }

    /* speed_rpm (float) */
    union {
        uint32_t u;
        float f;
    } conv;
    conv.f = DEFAULT_SPEED_RPM;
    err = nvs_get_u32(handle, "speed_rpm", &conv.u);
    if (err == ESP_OK) {
        s_config.speed_rpm = conv.f;
    } else {
        ESP_LOGW(TAG, "NVS key speed_rpm missing, using default");
    }

    /* mode (uint8_t stored as u8) */
    uint8_t mode_u8 = (uint8_t) DEFAULT_MODE;
    err = nvs_get_u8(handle, "mode", &mode_u8);
    if (err == ESP_OK) {
        s_config.mode = (int32_t) mode_u8;
    } else {
        ESP_LOGW(TAG, "NVS key mode missing, using default");
    }

    /* brightness (float stored as u32) */
    conv.f = DEFAULT_BRIGHTNESS;
    err = nvs_get_u32(handle, "brightness", &conv.u);
    if (err == ESP_OK) {
        s_config.brightness = conv.f;
    } else {
        ESP_LOGW(TAG, "NVS key brightness missing, using default");
    }

    /* color_r, color_g, color_b (uint8_t each) */
    uint8_t r = 0xFF, g = 0xA0, b = 0x28;
    err = nvs_get_u8(handle, "color_r", &r);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS key color_r missing, using default");
    }
    err = nvs_get_u8(handle, "color_g", &g);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS key color_g missing, using default");
    }
    err = nvs_get_u8(handle, "color_b", &b);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS key color_b missing, using default");
    }
    s_config.color_rgb = ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b;

    /* wifi_ssid (string, max 31 chars + null) */
    size_t ssid_len = sizeof(s_config.wifi_ssid);
    err = nvs_get_str(handle, "wifi_ssid", s_config.wifi_ssid, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS key wifi_ssid missing or truncated");
        s_config.wifi_ssid[0] = '\0';
    }

    /* wifi_pass (string, max 63 chars + null) */
    size_t pass_len = sizeof(s_config.wifi_pass);
    err = nvs_get_str(handle, "wifi_pass", s_config.wifi_pass, &pass_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS key wifi_pass missing or truncated");
        s_config.wifi_pass[0] = '\0';
    }

    nvs_close(handle);

    xSemaphoreGive(s_config_mutex);

    ESP_LOGI(TAG, "Config loaded from NVS (speed=%.1f, mode=%ld, brightness=%.2f, color=0x%06X)",
             s_config.speed_rpm, (long) s_config.mode, s_config.brightness,
             (unsigned int) s_config.color_rgb);
    return ESP_OK;
}

esp_err_t config_manager_save_to_nvs(void) {
    if (s_config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open (RW) failed (%s), config not saved", esp_err_to_name(err));
        xSemaphoreGive(s_config_mutex);
        return err;
    }

    /* speed_rpm */
    union {
        uint32_t u;
        float f;
    } conv;
    conv.f = s_config.speed_rpm;
    err = nvs_set_u32(handle, "speed_rpm", conv.u);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS set speed_rpm failed: %s", esp_err_to_name(err));
    }

    /* mode */
    err = nvs_set_u8(handle, "mode", (uint8_t) s_config.mode);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS set mode failed: %s", esp_err_to_name(err));
    }

    /* brightness */
    conv.f = s_config.brightness;
    err = nvs_set_u32(handle, "brightness", conv.u);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS set brightness failed: %s", esp_err_to_name(err));
    }

    /* color components */
    err = nvs_set_u8(handle, "color_r", (uint8_t) ((s_config.color_rgb >> 16) & 0xFF));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS set color_r failed: %s", esp_err_to_name(err));
    }
    err = nvs_set_u8(handle, "color_g", (uint8_t) ((s_config.color_rgb >> 8) & 0xFF));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS set color_g failed: %s", esp_err_to_name(err));
    }
    err = nvs_set_u8(handle, "color_b", (uint8_t) (s_config.color_rgb & 0xFF));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS set color_b failed: %s", esp_err_to_name(err));
    }

    /* wifi credentials */
    err = nvs_set_str(handle, "wifi_ssid", s_config.wifi_ssid);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS set wifi_ssid failed: %s", esp_err_to_name(err));
    }
    err = nvs_set_str(handle, "wifi_pass", s_config.wifi_pass);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS set wifi_pass failed: %s", esp_err_to_name(err));
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS commit failed: %s", esp_err_to_name(err));
        nvs_close(handle);
        xSemaphoreGive(s_config_mutex);
        return err;
    }
    nvs_close(handle);

    xSemaphoreGive(s_config_mutex);

    ESP_LOGI(TAG, "Config saved to NVS");
    return ESP_OK;
}
