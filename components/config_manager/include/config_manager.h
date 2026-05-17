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
// clang-format off
#define CONFIG_MANAGER_DEFAULTS() \
    (runtime_config_t){          \
        .speed_rpm = 8.0f, .mode = 0, .brightness = 1.0f, .color_rgb = 0xFFA028}
// clang-format on

/**
 * @brief WiFi credentials stored in NVS (separate from animation config).
 */
typedef struct {
    char ssid[32];
    char pass[64];
} wifi_credentials_t;

/**
 * @brief Runtime configuration structure (animation settings only).
 */
typedef struct {
    float speed_rpm;    // beacon rotation speed (RPM)
    int32_t mode;       // animation mode identifier
    float brightness;   // global brightness factor 0.0..1.0
    uint32_t color_rgb; // beam color as 0xRRGGBB
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

/**
 * @brief Trigger async NVS save via background task.
 *
 * Enqueues save request and returns immediately. HTTP handlers
 * should call this instead of the synchronous version to avoid
 * blocking the HTTP worker thread.
 *
 * @return ESP_OK if save request enqueued, ESP_ERR_NO_MEM if queue full.
 */
esp_err_t config_manager_save_to_nvs_async(void);

/* WiFi credential management (separate from animation config) */

/**
 * @brief Get WiFi credentials (mutex-protected).
 */
esp_err_t config_manager_get_wifi_credentials(wifi_credentials_t *out_cred);

/**
 * @brief Set WiFi credentials (mutex-protected).
 */
esp_err_t config_manager_set_wifi_credentials(const wifi_credentials_t *in_cred);

/**
 * @brief Save WiFi credentials to NVS (async, via background task).
 */
esp_err_t config_manager_save_wifi_credentials_async(void);

#ifdef __cplusplus
}
#endif
