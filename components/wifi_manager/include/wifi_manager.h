#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
 * @brief Wi-Fi connection status.
 */
typedef enum {
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_AP_MODE,
} wifi_manager_status_t;

/**
 * @brief Initialise Wi-Fi in STA mode.
 *
 * Reads credentials from config_manager, starts the Wi-Fi driver,
 * and attempts to connect. Falls back to AP mode on repeated failure.
 *
 * @return ESP_OK on success.
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Get current Wi-Fi status.
 */
wifi_manager_status_t wifi_manager_get_status(void);

/**
 * @brief Get current IP address as a string.
 *
 * @param buf  Buffer to store IP string.
 * @param len  Buffer size (must be >= 16).
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if buf is NULL or too small.
 */
esp_err_t wifi_manager_get_ip(char *buf, size_t len);

/**
 * @brief Start AP mode with SSID "Fresnel-Beacon-XXXX" (last 4 hex of MAC).
 *
 * @return ESP_OK on success.
 */
esp_err_t wifi_manager_start_ap(void);

#ifdef __cplusplus
}
#endif
