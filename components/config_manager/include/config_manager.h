#pragma once

#include <stdint.h>
#include "ipc.h"

/**
 * @brief Runtime configuration structure.
 */
typedef struct {
    float    speed_rpm;   // beacon rotation speed (RPM)
    int32_t  mode;        // animation mode identifier
    float    brightness;  // global brightness factor 0.0..1.0
    uint32_t color_rgb;   // beam color as 0xRRGGBB
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
esp_err_t config_manager_set_mode(int32_t mode);
esp_err_t config_manager_set_brightness(float brightness);
esp_err_t config_manager_set_color(uint32_t rgb);
