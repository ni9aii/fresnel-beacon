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
    "<html lang=\"en\">"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>Fresnel Beacon</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:system-ui,-apple-system,sans-serif;background:#0f1115;color:#e4e6eb;padding:1rem;line-height:1.5}"
    ".card{background:#181b21;border:1px solid #23262d;border-radius:12px;padding:1rem;margin-bottom:1rem}"
    "h1{font-size:1.25rem;margin-bottom:.5rem;color:#fff}"
    ".status{display:flex;gap:.5rem;align-items:center;margin-bottom:1rem}"
    ".dot{width:10px;height:10px;border-radius:50%;background:#888}"
    ".dot.ok{background:#22c55e}"
    ".dot.err{background:#ef4444}"
    ".row{display:flex;gap:1rem;flex-wrap:wrap}"
    ".col{flex:1;min-width:260px}"
    "label{display:block;font-size:.85rem;color:#9aa0a6;margin-bottom:.35rem}"
    "input[type=range]{width:100%;margin-bottom:.25rem}"
    "input[type=color]{width:100%;height:40px;border:none;border-radius:6px;cursor:pointer;background:none}"
    "select{width:100%;padding:.5rem;border-radius:6px;border:1px solid #333;background:#0f1115;color:#e4e6eb}"
    ".presets{display:flex;gap:.5rem;flex-wrap:wrap;margin-top:.5rem}"
    ".preset{width:32px;height:32px;border-radius:50%;border:2px solid #333;cursor:pointer}"
    ".preset:hover{border-color:#fff}"
    ".val{font-size:.8rem;color:#9aa0a6;text-align:right}"
    ".info{font-size:.8rem;color:#9aa0a6;margin-top:.25rem}"
    "button{width:100%;padding:.65rem;border:none;border-radius:8px;background:#2563eb;color:#fff;font-size:.95rem;cursor:pointer;margin-top:.5rem}"
    "button:hover{background:#1d4ed8}"
    "#connStatus{font-size:.8rem;color:#9aa0a6;margin-top:.25rem}"
    "</style>"
    "</head>"
    "<body>"
    "<h1>Fresnel Beacon</h1>"
    "<div class=\"status\">"
    "<div class=\"dot\" id=\"connDot\"></div>"
    "<span id=\"connText\">Connecting...</span>"
    "</div>"
    "<div class=\"row\">"
    "<div class=\"col\">"
    "<div class=\"card\">"
    "<label>Mode</label>"
    "<select id=\"mode\">"
    "<option value=\"0\">Beacon</option>"
    "<option value=\"1\">Strobe</option>"
    "<option value=\"2\">Ambient</option>"
    "</select>"
    "</div>"
    "<div class=\"card\">"
    "<label>Speed (RPM)</label>"
    "<input type=\"range\" id=\"speed\" min=\"0\" max=\"60\" step=\"0.5\" value=\"10\">"
    "<div class=\"val\" id=\"speedVal\">10.0</div>"
    "</div>"
    "<div class=\"card\">"
    "<label>Brightness</label>"
    "<input type=\"range\" id=\"brightness\" min=\"0\" max=\"1\" step=\"0.01\" value=\"0.5\">"
    "<div class=\"val\" id=\"brightVal\">0.50</div>"
    "</div>"
    "</div>"
    "<div class=\"col\">"
    "<div class=\"card\">"
    "<label>Color</label>"
    "<input type=\"color\" id=\"color\" value=\"#ff5500\">"
    "<div class=\"presets\">"
    "<div class=\"preset\" style=\"background:#ff0000\" data-c=\"#ff0000\"></div>"
    "<div class=\"preset\" style=\"background:#ff5500\" data-c=\"#ff5500\"></div>"
    "<div class=\"preset\" style=\"background:#ffaa00\" data-c=\"#ffaa00\"></div>"
    "<div class=\"preset\" style=\"background:#00ff00\" data-c=\"#00ff00\"></div>"
    "<div class=\"preset\" style=\"background:#00aaff\" data-c=\"#00aaff\"></div>"
    "<div class=\"preset\" style=\"background:#aa00ff\" data-c=\"#aa00ff\"></div>"
    "<div class=\"preset\" style=\"background:#ffffff\" data-c=\"#ffffff\"></div>"
    "</div>"
    "</div>"
    "<div class=\"card\">"
    "<label>Status</label>"
    "<div class=\"info\" id=\"wifiInfo\">WiFi: --</div>"
    "<div class=\"info\" id=\"ipInfo\">IP: --</div>"
    "<div class=\"info\" id=\"rssiInfo\">RSSI: -- dBm</div>"
    "</div>"
    "<button id=\"saveBtn\">Apply Changes</button>"
    "<div id=\"connStatus\"></div>"
    "</div>"
    "</div>"
    "<script>"
    "(function(){"
    "const $=id=>document.getElementById(id);"
    "const speed=$('speed'),bright=$('brightness'),mode=$('mode'),color=$('color');"
    "const speedVal=$('speedVal'),brightVal=$('brightVal');"
    "const connDot=$('connDot'),connText=$('connText'),connStatus=$('connStatus');"
    "const wifiInfo=$('wifiInfo'),ipInfo=$('ipInfo'),rssiInfo=$('rssiInfo');"
    "let pending=false;"
    "function setConn(ok,msg){"
    "connDot.className='dot '+(ok?'ok':'err');"
    "connText.textContent=msg;"
    "}"
    "function updateUI(d){"
    "if(d.beacon){"
    "let b=d.beacon;"
    "speed.value=b.speed_rpm; speedVal.textContent=Number(b.speed_rpm).toFixed(1);"
    "bright.value=b.brightness; brightVal.textContent=Number(b.brightness).toFixed(2);"
    "mode.value=b.mode;"
    "if(b.color){color.value=b.color.replace('0x','#').replace('0X','#');}"
    "}"
    "wifiInfo.textContent='WiFi: '+(d.wifi||'--');"
    "ipInfo.textContent='IP: '+(d.ip||'--');"
    "rssiInfo.textContent='RSSI: '+(d.rssi!=null?d.rssi+' dBm':'--');"
    "setConn(true,'Connected');"
    "}"
    "async function fetchStatus(){"
    "try{"
    "let r=await fetch('/api/status',{cache:'no-store'});"
    "if(!r.ok)throw new Error('HTTP '+r.status);"
    "let d=await r.json();"
    "updateUI(d);"
    "}catch(e){"
    "setConn(false,'Disconnected');"
    "connStatus.textContent='Error: '+e.message;"
    "}"
    "}"
    "async function sendConfig(body){"
    "try{"
    "let r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});"
    "if(!r.ok)throw new Error('HTTP '+r.status);"
    "connStatus.textContent='Saved.';"
    "setTimeout(()=>connStatus.textContent='',2000);"
    "}catch(e){"
    "connStatus.textContent='Save failed: '+e.message;"
    "}"
    "}"
    "function post(){"
    "let c=color.value.replace('#','');"
    "sendConfig({"
    "speed_rpm:parseFloat(speed.value),"
    "brightness:parseFloat(bright.value),"
    "mode:parseInt(mode.value,10),"
    "color:'#'+c"
    "});"
    "}"
    "speed.addEventListener('input',()=>{speedVal.textContent=parseFloat(speed.value).toFixed(1);});"
    "bright.addEventListener('input',()=>{brightVal.textContent=parseFloat(bright.value).toFixed(2);});"
    "document.querySelectorAll('.preset').forEach(p=>{"
    "p.addEventListener('click',()=>{color.value=p.dataset.c;post();});"
    "});"
    "$('saveBtn').addEventListener('click',post);"
    "[speed,bright,mode,color].forEach(el=>el.addEventListener('change',post));"
    "fetchStatus();"
    "setInterval(fetchStatus,2000);"
    "})();"
    "</script>"
    "</body>"
    "</html>";

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
