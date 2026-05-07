#include "ipc.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "ipc";

QueueHandle_t ipc_queue = NULL;
SemaphoreHandle_t led_mutex = NULL;

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

    if (led_mutex == NULL) {
        led_mutex = xSemaphoreCreateMutex();
        if (led_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create LED mutex");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "LED mutex created");
    }

    return ESP_OK;
}
