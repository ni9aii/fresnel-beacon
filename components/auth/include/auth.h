#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate HTTP Basic Auth header.
 *
 * @param req HTTP request.
 * @return ESP_OK if valid, ESP_ERR_INVALID_ARG if missing/invalid.
 */
esp_err_t auth_validate(httpd_req_t *req);

/**
 * @brief Send 401 Unauthorized response.
 *
 * @param req HTTP request.
 */
void auth_send_401(httpd_req_t *req);

#ifdef __cplusplus
}
#endif
