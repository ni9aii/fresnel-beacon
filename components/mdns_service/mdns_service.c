#include "mdns_service.h"
#include "mdns.h"
#include "esp_log.h"

static const char *TAG = "mdns";

esp_err_t mdns_service_init(const char *hostname, const char *instance_name) {
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_init failed: %s", esp_err_to_name(err));
        return err;
    }
    mdns_hostname_set(hostname);
    mdns_instance_name_set(instance_name);
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS: %s.local -> HTTP on port 80", hostname);
    return ESP_OK;
}

void mdns_service_stop(void) {
    mdns_free();
}
