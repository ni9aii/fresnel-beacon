#include "mdns_service.h"
#include "esp_log.h"

static const char *TAG = "mdns_service";

esp_err_t mdns_service_init(void) {
    /* mDNS requires esp-mdns component from Espressif Component Registry.
     * Install with: idf.py add-dependency espressif/mdns
     * Then enable CONFIG_MDNS_ENABLED in sdkconfig.
     *
     * Until then, this is a stub that logs the requirement.
     */
    ESP_LOGW(TAG, "mDNS stub: install esp-mdns component to enable fresnel-beacon.local");
    ESP_LOGI(TAG, "Run: idf.py add-dependency espressif/mdns");
    return ESP_OK;
}

void mdns_service_deinit(void) {
    ESP_LOGI(TAG, "mDNS stub: nothing to stop");
}
