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
    xTaskCreate(beacon_animation_task, "beacon", stack_size, NULL, 5, NULL);
    ESP_LOGI(TAG, "beacon_animation_task created with stack size: %u", stack_size);
}
