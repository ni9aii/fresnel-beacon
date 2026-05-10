#include "ipc.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "ipc";

QueueHandle_t ipc_queue = NULL;
SemaphoreHandle_t ipc_commit_sem = NULL;

esp_err_t ipc_init(void)
{
    if (ipc_queue == NULL) {
        ipc_queue = xQueueCreate(32, sizeof(ipc_cmd_t));
        if (ipc_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create IPC queue");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "IPC queue created (size 32)");
    }

    if (ipc_commit_sem == NULL) {
        ipc_commit_sem = xSemaphoreCreateBinary();
        if (ipc_commit_sem == NULL) {
            ESP_LOGE(TAG, "Failed to create IPC commit semaphore");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "IPC commit semaphore created");
    }

    return ESP_OK;
}

/**
 * @brief Wait for an IPC commit signal.
 *
 * @param timeout_ms Maximum time to wait in milliseconds.
 *                   0 means non-blocking (returns immediately).
 *                   Use portMAX_DELAY for infinite wait.
 * @return pdTRUE if the semaphore was taken, pdFALSE on timeout or if not initialised.
 */
BaseType_t ipc_wait_commit(uint32_t timeout_ms)
{
    if (ipc_commit_sem == NULL) {
        return pdFALSE;
    }
    return xSemaphoreTake(ipc_commit_sem, pdMS_TO_TICKS(timeout_ms));
}

void ipc_signal_commit(void)
{
    if (ipc_commit_sem != NULL) {
        xSemaphoreGive(ipc_commit_sem);
    }
}
