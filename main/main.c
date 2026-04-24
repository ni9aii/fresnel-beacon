#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_driver.h"
#include "beacon_animation.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Fresnel Beacon starting");

    ESP_ERROR_CHECK(led_driver_init());

    xTaskCreate(beacon_animation_task, "beacon", 4096, NULL, 5, NULL);
}
