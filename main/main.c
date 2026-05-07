#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_driver.h"
#include "beacon_animation.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Fresnel Beacon starting");

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
