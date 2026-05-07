#include "wifi_manager.h"
#include "config_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "wifi_manager";

static wifi_manager_status_t s_status = WIFI_STATUS_DISCONNECTED;
static char s_ip_str[16] = "0.0.0.0";
static int s_retry_count = 0;
static const int MAX_RETRIES = 5;

static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;

static void update_status(wifi_manager_status_t status)
{
    if (s_status != status) {
        s_status = status;
        switch (status) {
        case WIFI_STATUS_DISCONNECTED:
            ESP_LOGI(TAG, "Status: DISCONNECTED");
            break;
        case WIFI_STATUS_CONNECTING:
            ESP_LOGI(TAG, "Status: CONNECTING");
            break;
        case WIFI_STATUS_CONNECTED:
            ESP_LOGI(TAG, "Status: CONNECTED");
            break;
        case WIFI_STATUS_AP_MODE:
            ESP_LOGI(TAG, "Status: AP_MODE");
            break;
        }
    }
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "STA started, connecting...");
        update_status(WIFI_STATUS_CONNECTING);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "STA disconnected, reason=%d", disconn->reason);
        if (s_retry_count < MAX_RETRIES) {
            s_retry_count++;
            ESP_LOGI(TAG, "Retry %d/%d", s_retry_count, MAX_RETRIES);
            update_status(WIFI_STATUS_CONNECTING);
            esp_wifi_connect();
        } else {
            ESP_LOGW(TAG, "Max retries reached, falling back to AP mode");
            update_status(WIFI_STATUS_DISCONNECTED);
            wifi_manager_start_ap();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", s_ip_str);
        s_retry_count = 0;
        update_status(WIFI_STATUS_CONNECTED);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *conn = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "AP: station " MACSTR " joined, AID=%d",
                 MAC2STR(conn->mac), conn->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *disconn = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "AP: station " MACSTR " left, AID=%d",
                 MAC2STR(disconn->mac), disconn->aid);
    }
}

esp_err_t wifi_manager_start_ap(void)
{
    ESP_LOGI(TAG, "Starting AP mode");

    if (s_sta_netif) {
        esp_wifi_stop();
        esp_wifi_set_mode(WIFI_MODE_NULL);
        esp_netif_destroy(s_sta_netif);
        s_sta_netif = NULL;
    }

    if (s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
    }

    wifi_config_t wifi_config = {0};
    uint8_t mac[6];
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_AP, mac);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP MAC: %s", esp_err_to_name(err));
        return err;
    }

    snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid),
             "Fresnel-Beacon-%02X%02X", mac[4], mac[5]);
    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode(AP) failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config(AP) failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start() failed: %s", esp_err_to_name(err));
        return err;
    }

    update_status(WIFI_STATUS_AP_MODE);
    ESP_LOGI(TAG, "AP started: SSID=%s, channel=%d", wifi_config.ap.ssid, wifi_config.ap.channel);
    return ESP_OK;
}

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initialising WiFi manager");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create STA netif");
        return ESP_ERR_NO_MEM;
    }

    runtime_config_t cfg_rt;
    esp_err_t err = config_manager_get(&cfg_rt);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read config: %s", esp_err_to_name(err));
        cfg_rt.wifi_ssid[0] = '\0';
        cfg_rt.wifi_pass[0] = '\0';
    }

    wifi_config_t wifi_config = {0};
    if (cfg_rt.wifi_ssid[0] != '\0') {
        strlcpy((char *)wifi_config.sta.ssid, cfg_rt.wifi_ssid, sizeof(wifi_config.sta.ssid));
        if (cfg_rt.wifi_pass[0] != '\0') {
            strlcpy((char *)wifi_config.sta.password, cfg_rt.wifi_pass, sizeof(wifi_config.sta.password));
        }
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    if (cfg_rt.wifi_ssid[0] == '\0') {
        ESP_LOGW(TAG, "No WiFi credentials configured, starting AP fallback immediately");
        wifi_manager_start_ap();
    } else {
        update_status(WIFI_STATUS_CONNECTING);
    }

    return ESP_OK;
}

wifi_manager_status_t wifi_manager_get_status(void)
{
    return s_status;
}

const char *wifi_manager_get_ip(void)
{
    return s_ip_str;
}
