#include "ipc.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "ipc";

QueueHandle_t ipc_queue = NULL;

esp_err_t ipc_init(void)
{
    if (ipc_queue == NULL) {
        ipc_queue = xQueueCreate(10, sizeof(ipc_cmd_t));
        if (ipc_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create IPC queue");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "IPC queue created (size 10)");
    }

    return ESP_OK;
}
