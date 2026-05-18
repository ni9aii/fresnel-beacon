#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start mDNS service.
 *
 * Advertises fresnel-beacon.local and _http._tcp on port 80.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t mdns_service_init(void);

/**
 * @brief Stop mDNS service.
 */
void mdns_service_deinit(void);

#ifdef __cplusplus
}
#endif
