#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "led_driver.h"
#include "beacon_animation.h"
#include "ipc.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "nvs_flash.h"

static const char *TAG = "main";

static void wifi_monitor_task(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        if (wifi_manager_get_status() == WIFI_STATUS_CONNECTED) {
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                ESP_LOGI(TAG, "WiFi RSSI: %d dBm", ap_info.rssi);
            } else {
                ESP_LOGW(TAG, "WiFi RSSI unavailable");
            }
        }
    }
}

static void log_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "ESP-IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Chip model: %s, cores: %d, revision: %d",
             (chip_info.model == CHIP_ESP32) ? "ESP32" :
             (chip_info.model == CHIP_ESP32S2) ? "ESP32-S2" :
             (chip_info.model == CHIP_ESP32S3) ? "ESP32-S3" :
             (chip_info.model == CHIP_ESP32C3) ? "ESP32-C3" :
             (chip_info.model == CHIP_ESP32C6) ? "ESP32-C6" :
             (chip_info.model == CHIP_ESP32H2) ? "ESP32-H2" : "Unknown",
             chip_info.cores, chip_info.revision);
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());

    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi MAC: " MACSTR, MAC2STR(mac));
    } else {
        ESP_LOGW(TAG, "WiFi MAC unavailable");
    }

    runtime_config_t cfg;
    if (config_manager_get(&cfg) == ESP_OK) {
        ESP_LOGI(TAG, "Config — speed: %.1f rpm, mode: %ld, brightness: %.2f, color: 0x%06X",
                 cfg.speed_rpm, (long)cfg.mode, cfg.brightness, (unsigned int)cfg.color_rgb);
    } else {
        ESP_LOGW(TAG, "Config unavailable at startup");
    }
}

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

    ESP_ERROR_CHECK(wifi_manager_init());

    /* Brief delay to let WiFi settle before starting the HTTP server */
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_ERROR_CHECK(http_server_init());

    led_driver_init();
    ESP_LOGI(TAG, "led_driver_init complete");

    log_system_info();

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

    task_created = xTaskCreate(wifi_monitor_task, "wifi_mon", 4096, NULL, 3, NULL);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create wifi_monitor_task (ret=%d)", task_created);
    } else {
        ESP_LOGI(TAG, "wifi_monitor_task created with stack size: 4096");
    }
}
