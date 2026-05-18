#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OTA status enum */
typedef enum {
    OTA_STATUS_IDLE = 0,
    OTA_STATUS_IN_PROGRESS = 1,
    OTA_STATUS_SUCCESS = 2,
    OTA_STATUS_FAILED = 3,
} ota_status_t;

/**
 * @brief Initialize OTA manager.
 *
 * Must be called before any OTA operations.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t ota_manager_init(void);

/**
 * @brief Start OTA update from HTTPS URL.
 *
 * Validates URL (HTTPS only), downloads firmware, verifies signature,
 * and reboots on success.
 *
 * @param url HTTPS URL to firmware image.
 * @return ESP_OK if update started successfully, error code otherwise.
 */
esp_err_t ota_manager_start(const char *url);

/**
 * @brief Get current OTA status.
 *
 * @param[out] progress Percentage complete (0-100), or -1 if not running.
 * @param[out] message Human-readable status message.
 * @return ESP_OK on success.
 */
esp_err_t ota_manager_get_status(int *progress, const char **message);

#ifdef __cplusplus
}
#endif
