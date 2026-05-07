#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_driver.h"
#include "beacon_animation.h"
#include "ipc.h"
#include "config_manager.h"
#include "nvs_flash.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Fresnel Beacon starting");

    /* Initialise NVS before config manager so we can restore saved settings */
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS init error (%s), attempting erase and re-init", esp_err_to_name(nvs_ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    if (nvs_ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS unavailable (%s), continuing with RAM defaults", esp_err_to_name(nvs_ret));
    }

    ESP_ERROR_CHECK(ipc_init());
    ESP_ERROR_CHECK(config_manager_init());
    ESP_ERROR_CHECK(config_manager_load_from_nvs());

    led_driver_init();

    const size_t stack_size = 4096;
    BaseType_t task_created = xTaskCreate(beacon_animation_task, "beacon", stack_size, NULL, 5, NULL);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create beacon_animation_task (ret=%d)", task_created);
        /* Graceful halt: do not reboot so the error is visible over serial */
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "beacon_animation_task created with stack size: %u", stack_size);
}
