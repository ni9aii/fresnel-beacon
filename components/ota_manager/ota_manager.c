#include "ota_manager.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_system.h"
#include <string.h>

static const char *TAG = "ota_manager";

static volatile int s_progress = -1;
static volatile ota_status_t s_status = OTA_STATUS_IDLE;

esp_err_t ota_manager_init(void) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Running partition: %s", running->label);
    return ESP_OK;
}

static bool is_https_url(const char *url) {
    return (strncmp(url, "https://", 8) == 0);
}

esp_err_t ota_manager_start(const char *url) {
    if (url == NULL || url[0] == '\0') {
        ESP_LOGE(TAG, "URL is null or empty");
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_https_url(url)) {
        ESP_LOGE(TAG, "URL must use HTTPS (rejecting: %s)", url);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting OTA from: %s", url);
    s_status = OTA_STATUS_IN_PROGRESS;
    s_progress = 0;

    esp_https_ota_config_t ota_config = {
        .http_config =
            &(esp_http_client_config_t){
                .url = url,
                .timeout_ms = 30000,
                .cert_pem = NULL, /* Server certificate required for production */
            },
    };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA successful, rebooting...");
        s_status = OTA_STATUS_SUCCESS;
        s_progress = 100;
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
        s_status = OTA_STATUS_FAILED;
        s_progress = -1;
    }

    return ret;
}

esp_err_t ota_manager_get_status(int *progress, const char **message) {
    if (progress == NULL || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *progress = s_progress;

    switch (s_status) {
    case OTA_STATUS_IDLE:
        *message = "Idle";
        break;
    case OTA_STATUS_IN_PROGRESS:
        *message = "In progress";
        break;
    case OTA_STATUS_SUCCESS:
        *message = "Success (rebooting)";
        break;
    case OTA_STATUS_FAILED:
        *message = "Failed";
        break;
    default:
        *message = "Unknown";
        break;
    }

    return ESP_OK;
}
