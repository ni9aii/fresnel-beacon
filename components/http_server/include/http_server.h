#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
 * @brief Start the HTTP server.
 *
 * Registers URI handlers for REST API and Web UI.
 *
 * @return ESP_OK on success.
 */
esp_err_t http_server_init(void);

/**
 * @brief Stop the HTTP server.
 */
void http_server_stop(void);

#ifdef __cplusplus
}
#endif
