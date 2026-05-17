#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default runtime configuration values.
 */
#define CONFIG_MANAGER_DEFAULTS()                          \
    (runtime_config_t){                                   \
        .speed_rpm = 8.0f, .mode = 0, .brightness = 1.0f, .color_rgb = 0xFFA028}

/**
 * @brief Runtime configuration structure.
 */
typedef struct {
    float speed_rpm;    // beacon rotation speed (RPM)
    int32_t mode;       // animation mode identifier
    float brightness;   // global brightness factor 0.0..1.0
    uint32_t color_rgb; // beam color as 0xRRGGBB
    /* Wi-Fi credentials stored in NVS but NOT exposed via IPC (security) */
    char wifi_ssid[32];
    char wifi_pass[64];
} runtime_config_t;

/**
 * @brief Initialise runtime config with defaults.
 *
 * @return ESP_OK on success.
 */
esp_err_t config_manager_init(void);

/**
 * @brief Get a copy of the current runtime config (mutex-protected).
 */
esp_err_t config_manager_get(runtime_config_t *out_cfg);

/**
 * @brief Update the runtime config (mutex-protected).
 */
esp_err_t config_manager_set(const runtime_config_t *in_cfg);

/**
 * @brief Individual field setters (mutex-protected).
 */
esp_err_t config_manager_set_speed(float rpm);
esp_err_t config_manager_set_speed_sec(float sec);
esp_err_t config_manager_get_speed_sec(float *out_sec);
esp_err_t config_manager_set_mode(int32_t mode);
esp_err_t config_manager_set_brightness(float brightness);
esp_err_t config_manager_set_color(uint32_t rgb);

/**
 * @brief Load all config fields from NVS namespace "fresnel".
 *
 * Falls back to defaults for any missing or corrupt keys.
 * NVS API is thread-safe; no extra mutex needed.
 *
 * @return ESP_OK on success, or warning-level error if NVS unavailable.
 */
esp_err_t config_manager_load_from_nvs(void);

/**
 * @brief Save all current config values to NVS namespace "fresnel".
 *
 * @return ESP_OK on success, or warning-level error if NVS unavailable.
 */
esp_err_t config_manager_save_to_nvs(void);

#ifdef __cplusplus
}
#endif
