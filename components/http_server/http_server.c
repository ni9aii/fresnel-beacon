#include "http_server.h"
#include "config_manager.h"
#include "ipc.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_err.h"

#ifndef __linux__
#include "esp_http_server.h"
#include "esp_wifi.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "http_server";
static httpd_handle_t s_server = NULL;

/* ---------- helpers ---------- */

static int get_rssi(void)
{
    if (wifi_manager_get_status() == WIFI_STATUS_CONNECTED) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            return ap_info.rssi;
        }
    }
    return 0;
}

static const char *status_str(void)
{
    switch (wifi_manager_get_status()) {
    case WIFI_STATUS_CONNECTED: return "connected";
    case WIFI_STATUS_AP_MODE:   return "ap_mode";
    default:                    return "disconnected";
    }
}

/* ---------- GET /api/status ---------- */

static esp_err_t api_status_get_handler(httpd_req_t *req)
{
    runtime_config_t cfg;
    if (config_manager_get(&cfg) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "config error");
        return ESP_FAIL;
    }

    char json[384];
    snprintf(json, sizeof(json),
             "{"
             "\"status\":\"ok\","
             "\"wifi\":\"%s\","
             "\"ip\":\"%s\","
             "\"rssi\":%d,"
             "\"beacon\":{"
             "\"speed_rpm\":%.1f,"
             "\"mode\":%ld,"
             "\"brightness\":%.2f,"
             "\"color\":\"0x%06X\""
             "}"
             "}",
             status_str(),
             wifi_manager_get_ip(),
             get_rssi(),
             cfg.speed_rpm,
             (long)cfg.mode,
             cfg.brightness,
             (unsigned int)cfg.color_rgb);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ---------- POST /api/config ---------- */

static esp_err_t api_config_post_handler(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid content length");
        return ESP_FAIL;
    }

    char *buf = malloc(req->content_len + 1);
    if (buf == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
        return ESP_ERR_NO_MEM;
    }

    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "receive error");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    /* Simple JSON parser: scan for known keys using sscanf */
    float speed_rpm = -1.0f;
    float brightness = -1.0f;
    int32_t mode = -1;
    unsigned int color = 0xFFFFFFFF;

    if (strstr(buf, "\"speed_rpm\"")) {
        sscanf(strstr(buf, "\"speed_rpm\""), "\"speed_rpm\":%f", &speed_rpm);
    }
    if (strstr(buf, "\"brightness\"")) {
        sscanf(strstr(buf, "\"brightness\""), "\"brightness\":%f", &brightness);
    }
    if (strstr(buf, "\"mode\"")) {
        sscanf(strstr(buf, "\"mode\""), "\"mode\":%ld", &mode);
    }
    if (strstr(buf, "\"color\"")) {
        char *color_ptr = strstr(buf, "\"color\"");
        if (color_ptr) {
            /* Try hex string form first: "0xFFA028" or "#FFA028" */
            char hex_str[16] = {0};
            if (sscanf(color_ptr, "\"color\":\"%15[^\"]\"", hex_str) == 1) {
                if (hex_str[0] == '#' && strlen(hex_str) == 7) {
                    color = (unsigned int)strtol(hex_str + 1, NULL, 16);
                } else if (strlen(hex_str) > 2 && (hex_str[1] == 'x' || hex_str[1] == 'X')) {
                    color = (unsigned int)strtol(hex_str + 2, NULL, 16);
                } else {
                    color = (unsigned int)strtol(hex_str, NULL, 16);
                }
            } else {
                /* Try numeric form */
                sscanf(color_ptr, "\"color\":%u", &color);
            }
        }
    }

    free(buf);

    /* Validate and apply */
    if (speed_rpm >= 0.0f) {
        if (speed_rpm > 60.0f) speed_rpm = 60.0f;
        ipc_cmd_t cmd = { .type = IPC_CMD_SET_SPEED, .data.speed_rpm = speed_rpm };
        if (xQueueSend(ipc_queue, &cmd, 0) != pdTRUE) {
            ESP_LOGW(TAG, "IPC queue full (speed)");
        }
    }
    if (brightness >= 0.0f) {
        if (brightness > 1.0f) brightness = 1.0f;
        ipc_cmd_t cmd = { .type = IPC_CMD_SET_BRIGHTNESS, .data.brightness = brightness };
        if (xQueueSend(ipc_queue, &cmd, 0) != pdTRUE) {
            ESP_LOGW(TAG, "IPC queue full (brightness)");
        }
    }
    if (mode >= 0) {
        ipc_cmd_t cmd = { .type = IPC_CMD_SET_MODE, .data.mode = mode };
        if (xQueueSend(ipc_queue, &cmd, 0) != pdTRUE) {
            ESP_LOGW(TAG, "IPC queue full (mode)");
        }
    }
    if (color != 0xFFFFFFFF) {
        ipc_cmd_t cmd = { .type = IPC_CMD_SET_COLOR, .data.color_rgb = (uint32_t)color };
        if (xQueueSend(ipc_queue, &cmd, 0) != pdTRUE) {
            ESP_LOGW(TAG, "IPC queue full (color)");
        }
    }

    /* Save to NVS */
    esp_err_t err = config_manager_save_to_nvs();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS save failed: %s", esp_err_to_name(err));
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ---------- GET / ---------- */

static const char s_index_html[] =
    "<!DOCTYPE html>"
    "<html><head><title>Fresnel Beacon</title></head>"
    "<body><h1>Fresnel Beacon</h1>"
    "<p>Web UI placeholder. Use /api/status and /api/config.</p>"
    "</body></html>";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, s_index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ---------- URI table ---------- */

static const httpd_uri_t uri_status = {
    .uri      = "/api/status",
    .method   = HTTP_GET,
    .handler  = api_status_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_config = {
    .uri      = "/api/config",
    .method   = HTTP_POST,
    .handler  = api_config_post_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_root = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = root_get_handler,
    .user_ctx = NULL
};

/* ---------- public API ---------- */

esp_err_t http_server_init(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    httpd_register_uri_handler(s_server, &uri_root);
    httpd_register_uri_handler(s_server, &uri_status);
    httpd_register_uri_handler(s_server, &uri_config);

    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}

void http_server_stop(void)
{
    if (s_server != NULL) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

#else /* __linux__ */

/* Stub implementations for host builds / unit tests */
esp_err_t http_server_init(void) { return ESP_OK; }
void http_server_stop(void) { }

#endif /* __linux__ */
