#include "http_server.h"
#include "config_manager.h"
#include "ipc.h"
#include "wifi_manager.h"
#include "ota_manager.h"
#include "auth.h"
#include "esp_log.h"
#include "esp_err.h"

#ifndef __linux__
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static const char *TAG = "http_server";
static httpd_handle_t s_server = NULL;

/* ---------- authentication ---------- */

/* Simple token auth: compare against hardcoded token derived from MAC.
 * In production, use NVS-encrypted storage or device certificate. */
static char s_auth_token[33] = {0};

static void init_auth_token(void) {
    if (s_auth_token[0] != '\0') {
        return; /* already initialised */
    }
    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK ||
        esp_wifi_get_mac(WIFI_IF_AP, mac) == ESP_OK) {
        snprintf(s_auth_token, sizeof(s_auth_token), "%02x%02x%02x%02x%02x%02x%08x", mac[0], mac[1],
                 mac[2], mac[3], mac[4], mac[5], (unsigned) esp_random());
    } else {
        /* Fallback: random token if MAC unavailable */
        snprintf(s_auth_token, sizeof(s_auth_token), "fresnel%08x", (unsigned) esp_random());
    }
    ESP_LOGI(TAG, "Auth token initialised (print on first boot for setup)");
}

static bool check_auth(httpd_req_t *req) {
    char auth_header[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) !=
        ESP_OK) {
        return false;
    }
    /* Expect "Bearer <token>" */
    const char *prefix = "Bearer ";
    size_t prefix_len = strlen(prefix);
    if (strncasecmp(auth_header, prefix, prefix_len) != 0) {
        return false;
    }
    return strcmp(auth_header + prefix_len, s_auth_token) == 0;
}

/* ---------- rate limiting ---------- */

static uint32_t s_request_count = 0;
static uint32_t s_last_reset_ticks = 0;

static bool check_rate_limit(void) {
    uint32_t now = xTaskGetTickCount();
    if (now - s_last_reset_ticks > pdMS_TO_TICKS(60000)) {
        s_request_count = 0;
        s_last_reset_ticks = now;
    }
    /* Atomic increment to prevent data race across httpd worker tasks */
    uint32_t count = __atomic_fetch_add(&s_request_count, 1, __ATOMIC_RELAXED);
    if (count > 30) {
        return false;
    }
    return true;
}

/* ---------- helpers ---------- */

static int get_rssi(void) {
    if (wifi_manager_get_status() == WIFI_STATUS_CONNECTED) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            return ap_info.rssi;
        }
    }
    return 0;
}

static const char *status_str(void) {
    switch (wifi_manager_get_status()) {
    case WIFI_STATUS_CONNECTED:
        return "connected";
    case WIFI_STATUS_AP_MODE:
        return "ap_mode";
    default:
        return "disconnected";
    }
}

/* ---------- GET /api/status ---------- */

static esp_err_t api_status_get_handler(httpd_req_t *req) {
    if (!check_rate_limit()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Rate limit exceeded");
        return ESP_FAIL;
    }

    runtime_config_t cfg;
    if (config_manager_get(&cfg) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "config error");
        return ESP_FAIL;
    }

    float speed_sec;
    config_manager_get_speed_sec(&speed_sec);

    /* Build JSON response dynamically with cJSON to avoid stack buffer overflow */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");
    cJSON_AddStringToObject(root, "wifi", status_str());
    char ip_str[16] = "0.0.0.0";
    wifi_manager_get_ip(ip_str, sizeof(ip_str));
    cJSON_AddStringToObject(root, "ip", ip_str);
    cJSON_AddNumberToObject(root, "rssi", get_rssi());

    cJSON *beacon = cJSON_CreateObject();
    cJSON_AddNumberToObject(beacon, "speed_rpm", cfg.speed_rpm);
    cJSON_AddNumberToObject(beacon, "speed_sec", speed_sec);
    cJSON_AddNumberToObject(beacon, "mode", cfg.mode);
    cJSON_AddNumberToObject(beacon, "brightness", cfg.brightness);

    char color_str[16];
    snprintf(color_str, sizeof(color_str), "0x%06X", (unsigned int) cfg.color_rgb);
    cJSON_AddStringToObject(beacon, "color", color_str);

    cJSON_AddItemToObject(root, "beacon", beacon);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encode error");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ESP_OK;
}

/* ---------- POST /api/config ---------- */

static esp_err_t api_config_post_handler(httpd_req_t *req) {
    if (auth_validate(req) != ESP_OK) {
        auth_send_401(req);
        return ESP_FAIL;
    }
    if (!check_rate_limit()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Rate limit exceeded");
        return ESP_FAIL;
    }

    /* Require authentication for config changes */
    if (!check_auth(req)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid or missing token");
        return ESP_FAIL;
    }

    /* Validate Content-Type */
    char ct_buf[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Content-Type", ct_buf, sizeof(ct_buf)) != ESP_OK ||
        strncasecmp(ct_buf, "application/json", 16) != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content-Type must be application/json");
        return ESP_FAIL;
    }

    if (req->content_len <= 0 || req->content_len > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid content length");
        return ESP_FAIL;
    }

    char *buf = malloc(req->content_len + 1);
    if (buf == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
        return ESP_ERR_NO_MEM;
    }

    /* Loop recv until full body received (handles partial reads) */
    size_t received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buf + received, req->content_len - received);
        if (ret <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "receive error");
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[received] = '\0';

    /* Parse JSON using cJSON */
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (root == NULL) {
        ESP_LOGW(TAG, "JSON parse error");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }

    float speed_rpm = -1.0f;
    float speed_sec = -1.0f;
    float brightness = -1.0f;
    int32_t mode = -1;
    unsigned int color = 0xFFFFFFFF;

    cJSON *item = cJSON_GetObjectItem(root, "speed_rpm");
    if (item != NULL && cJSON_IsNumber(item)) {
        speed_rpm = (float) item->valuedouble;
    }
    item = cJSON_GetObjectItem(root, "speed_sec");
    if (item != NULL && cJSON_IsNumber(item)) {
        speed_sec = (float) item->valuedouble;
    }
    item = cJSON_GetObjectItem(root, "brightness");
    if (item != NULL && cJSON_IsNumber(item)) {
        brightness = (float) item->valuedouble;
    }
    item = cJSON_GetObjectItem(root, "mode");
    if (item != NULL && cJSON_IsNumber(item)) {
        mode = (int32_t) item->valueint;
    }
    item = cJSON_GetObjectItem(root, "color");
    if (item != NULL) {
        if (cJSON_IsString(item)) {
            const char *hex_str = item->valuestring;
            char *endptr = NULL;
            unsigned long val = 0;
            errno = 0;
            if (hex_str[0] == '#' && strlen(hex_str) == 7) {
                val = strtoul(hex_str + 1, &endptr, 16);
            } else if (strlen(hex_str) > 2 && (hex_str[1] == 'x' || hex_str[1] == 'X')) {
                val = strtoul(hex_str + 2, &endptr, 16);
            } else {
                val = strtoul(hex_str, &endptr, 16);
            }
            if (endptr == NULL || *endptr != '\0' || errno != 0) {
                ESP_LOGW(TAG, "Invalid color string: %s", hex_str);
            } else if (val > 0xFFFFFF) {
                ESP_LOGW(TAG, "Color out of range: 0x%lX", val);
            } else {
                color = (unsigned int) val;
            }
        } else if (cJSON_IsNumber(item)) {
            unsigned int color_tmp = (unsigned int) item->valueint;
            if (color_tmp <= 0xFFFFFF) {
                color = color_tmp;
            } else {
                ESP_LOGW(TAG, "Color out of range: %u", color_tmp);
            }
        }
    }

    cJSON_Delete(root);

    /* Validate and apply */
    if (speed_sec >= 0.0f) {
        // Convert seconds per rotation to RPM and send via IPC
        if (speed_sec > 20.0f)
            speed_sec = 20.0f;
        if (speed_sec < 0.5f)
            speed_sec = 0.5f;
        float rpm = 60.0f / speed_sec;
        ipc_cmd_t cmd = {.type = IPC_CMD_SET_SPEED, .data.speed_rpm = rpm};
        if (xQueueSend(ipc_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "IPC queue full (speed)");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "IPC queue full");
            return ESP_FAIL;
        }
    }
    if (speed_rpm >= 0.0f) {
        if (speed_rpm > 60.0f)
            speed_rpm = 60.0f;
        ipc_cmd_t cmd = {.type = IPC_CMD_SET_SPEED, .data.speed_rpm = speed_rpm};
        if (xQueueSend(ipc_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "IPC queue full (speed)");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "IPC queue full");
            return ESP_FAIL;
        }
    }
    if (brightness >= 0.0f) {
        if (brightness > 1.0f)
            brightness = 1.0f;
        ipc_cmd_t cmd = {.type = IPC_CMD_SET_BRIGHTNESS, .data.brightness = brightness};
        if (xQueueSend(ipc_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "IPC queue full (brightness)");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "IPC queue full");
            return ESP_FAIL;
        }
    }
    if (mode >= 0 && mode <= 3 && mode != -1) {
        ipc_cmd_t cmd = {.type = IPC_CMD_SET_MODE, .data.mode = mode};
        if (xQueueSend(ipc_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "IPC queue full (mode)");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "IPC queue full");
            return ESP_FAIL;
        }
    } else if (mode > 3 || mode == -1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid mode");
        return ESP_FAIL;
    }
    if (color != 0xFFFFFFFF) {
        ipc_cmd_t cmd = {.type = IPC_CMD_SET_COLOR, .data.color_rgb = (uint32_t) color};
        if (xQueueSend(ipc_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "IPC queue full (color)");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "IPC queue full");
            return ESP_FAIL;
        }
    }

    /* Send IPC_CMD_COMMIT and wait for animation task to process it */
    ipc_cmd_t commit_cmd = {.type = IPC_CMD_COMMIT};
    if (xQueueSend(ipc_queue, &commit_cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "IPC queue full (commit)");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "IPC queue full");
        return ESP_FAIL;
    }

    BaseType_t sem_ret = ipc_wait_commit(500);
    if (sem_ret != pdTRUE) {
        ESP_LOGW(TAG, "Commit timeout: animation task did not signal");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "commit timeout");
        return ESP_FAIL;
    }

    /* Save to NVS after commit acknowledged (async, non-blocking) */
    esp_err_t err = config_manager_save_to_nvs_async();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS async save failed: %s", esp_err_to_name(err));
        /* Don't fail the request — save is best-effort */
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t api_ota_post_handler(httpd_req_t *req) {
    if (auth_validate(req) != ESP_OK) {
        auth_send_401(req);
        return ESP_FAIL;
    }

    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        ESP_LOGW(TAG, "OTA request body empty");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        ESP_LOGW(TAG, "OTA JSON parse error");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }

    cJSON *url_item = cJSON_GetObjectItem(root, "url");
    if (url_item == NULL || !cJSON_IsString(url_item)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing 'url' field");
        return ESP_FAIL;
    }

    const char *url = url_item->valuestring;
    cJSON_Delete(root);

    esp_err_t err = ota_manager_start(url);
    if (err != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"OTA failed\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"OTA started\"}", HTTPD_RESP_USE_STRLEN);
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
    "body{font-family:system-ui,-apple-system,sans-serif;background:#0f1115;color:#e4e6eb;padding:"
    "1rem;line-height:1.5}"
    ".card{background:#181b21;border:1px solid "
    "#23262d;border-radius:12px;padding:1rem;margin-bottom:1rem}"
    "h1{font-size:1.25rem;margin-bottom:.5rem;color:#fff}"
    ".status{display:flex;gap:.5rem;align-items:center;margin-bottom:1rem}"
    ".dot{width:10px;height:10px;border-radius:50%;background:#888}"
    ".dot.ok{background:#22c55e}"
    ".dot.err{background:#ef4444}"
    ".row{display:flex;gap:1rem;flex-wrap:wrap}"
    ".col{flex:1;min-width:260px}"
    "label{display:block;font-size:.85rem;color:#9aa0a6;margin-bottom:.35rem}"
    "input[type=range]{width:100%;margin-bottom:.25rem}"
    "input[type=color]{width:100%;height:40px;border:none;border-radius:6px;cursor:pointer;"
    "background:none}"
    "select{width:100%;padding:.5rem;border-radius:6px;border:1px solid "
    "#333;background:#0f1115;color:#e4e6eb}"
    ".presets{display:flex;gap:.5rem;flex-wrap:wrap;margin-top:.5rem}"
    ".preset{width:32px;height:32px;border-radius:50%;border:2px solid #333;cursor:pointer}"
    ".preset:hover{border-color:#fff}"
    ".val{font-size:.8rem;color:#9aa0a6;text-align:right}"
    ".info{font-size:.8rem;color:#9aa0a6;margin-top:.25rem}"
    "button{width:100%;padding:.65rem;border:none;border-radius:8px;background:#2563eb;color:#fff;"
    "font-size:.95rem;cursor:pointer;margin-top:.5rem}"
    "button:hover{background:#1d4ed8}"
    "#connStatus{font-size:.8rem;color:#9aa0a6;margin-top:.25rem}"
    ".preset-btn{flex:1;min-width:60px;padding:.4rem;border:none;border-radius:6px;background:#"
    "23262d;color:#e4e6eb;font-size:.8rem;cursor:pointer}"
    ".preset-btn:hover{background:#2563eb}"
    "#preview{width:100%;height:120px;border-radius:8px;background:#000;margin-top:.5rem}"
    "@media(max-width:480px){.row{flex-direction:column}.col{min-width:auto}}"
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
    "<option value=\"3\">Off</option>"
    "</select>"
    "<div class=\"presets\" style=\"margin-top:.5rem\">"
    "<button class=\"preset-btn\" data-mode=\"0\" data-speed=\"10\" data-bright=\"1\" "
    "data-color=\"#ff5500\">Beacon</button>"
    "<button class=\"preset-btn\" data-mode=\"1\" data-speed=\"30\" data-bright=\"1\" "
    "data-color=\"#ffffff\">Strobe</button>"
    "<button class=\"preset-btn\" data-mode=\"2\" data-speed=\"5\" data-bright=\"0.3\" "
    "data-color=\"#00aaff\">Ambient</button>"
    "<button class=\"preset-btn\" data-mode=\"3\" data-speed=\"0\" data-bright=\"0\" "
    "data-color=\"#000000\">Off</button>"
    "</div>"
    "</div>"
    "<div class=\"card\">"
    "<label>Speed (RPM)</label>"
    "<input type=\"range\" id=\"speed\" min=\"0.5\" max=\"20\" step=\"0.1\" value=\"1.0\">"
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
    "<label>Live Preview</label>"
    "<canvas id=\"preview\"></canvas>"
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
    "const preview=$('preview');"
    "let pending=false;"
    "function drawPreview(color,mode,brightness){"
    "const ctx=preview.getContext('2d');"
    "const w=preview.width=preview.offsetWidth;"
    "const h=preview.height=preview.offsetHeight;"
    "ctx.fillStyle='#000';ctx.fillRect(0,0,w,h);"
    "if(mode==3||brightness<=0)return;"
    "const rgb=parseInt(color.replace('#',''),16);"
    "const r=(rgb>>16)&0xFF,g=(rgb>>8)&0xFF,b=rgb&0xFF;"
    "const a=brightness;"
    "if(mode==1){"
    "ctx.fillStyle=`rgba(${r},${g},${b},${a})`;"
    "ctx.beginPath();ctx.arc(w/2,h/2,Math.min(w,h)*0.35,0,Math.PI*2);ctx.fill();"
    "}else if(mode==2){"
    "const grd=ctx.createRadialGradient(w/2,h/2,0,w/2,h/2,Math.min(w,h)*0.45);"
    "grd.addColorStop(0,`rgba(${r},${g},${b},${a})`);"
    "grd.addColorStop(1,`rgba(${r},${g},${b},0)`);"
    "ctx.fillStyle=grd;ctx.fillRect(0,0,w,h);"
    "}else{"
    "const grd=ctx.createRadialGradient(w/2,h/2,0,w/2,h/2,Math.min(w,h)*0.4);"
    "grd.addColorStop(0,`rgba(${r},${g},${b},${a})`);"
    "grd.addColorStop(1,`rgba(${r},${g},${b},${a*0.3})`);"
    "ctx.fillStyle=grd;ctx.fillRect(0,0,w,h);"
    "}"
    "}"
    "function setConn(ok,msg){"
    "connDot.className='dot '+(ok?'ok':'err');"
    "connText.textContent=msg;"
    "}"
    "function updateUI(d){"
    "if(d.beacon){"
    "let b=d.beacon;"
    "let displaySec = (typeof b.speed_sec === 'number' && b.speed_sec > 0) ? "
    "b.speed_sec.toFixed(2) : (typeof b.speed_rpm === 'number' && b.speed_rpm > 0 ? (60.0 / "
    "b.speed_rpm).toFixed(2) : '1.00');"
    "speed.value=displaySec; speedVal.textContent=displaySec;"
    "bright.value=b.brightness; brightVal.textContent=Number(b.brightness).toFixed(2);"
    "mode.value=b.mode;"
    "if(b.color){color.value=b.color.replace('0x','#').replace('0X','#');}"
    "}"
    "wifiInfo.textContent='WiFi: '+(d.wifi||'--');"
    "ipInfo.textContent='IP: '+(d.ip||'--');"
    "rssiInfo.textContent='RSSI: '+(d.rssi!=null?d.rssi+' dBm':'--');"
    "if(d.beacon){drawPreview(d.beacon.color||'#ff5500',d.beacon.mode||0,d.beacon.brightness||0.5);"
    "}"
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
    "let r=await "
    "fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/"
    "json'},body:JSON.stringify(body)});"
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
    "speed.addEventListener('input',()=>{speedVal.textContent=parseFloat(speed.value).toFixed(1);})"
    ";"
    "bright.addEventListener('input',()=>{brightVal.textContent=parseFloat(bright.value).toFixed(2)"
    ";});"
    "document.querySelectorAll('.preset').forEach(p=>{"
    "p.addEventListener('click',()=>{color.value=p.dataset.c;post();});"
    "});"
    "document.querySelectorAll('.preset-btn').forEach(b=>{"
    "b.addEventListener('click',()=>{"
    "mode.value=b.dataset.mode;"
    "speed.value=60.0/parseFloat(b.dataset.speed);"
    "bright.value=b.dataset.bright;"
    "color.value=b.dataset.color;"
    "post();"
    "});"
    "});"
    "$('saveBtn').addEventListener('click',post);"
    "[speed,bright,mode,color].forEach(el=>el.addEventListener('change',post));"
    "fetchStatus();"
    "setInterval(fetchStatus,2000);"
    "})();"
    "</script>"
    "</body>"
    "</html>";

static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, s_index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ---------- URI table ---------- */

static const httpd_uri_t uri_status = {
    .uri = "/api/status", .method = HTTP_GET, .handler = api_status_get_handler, .user_ctx = NULL};

static const httpd_uri_t uri_config = {.uri = "/api/config",
                                       .method = HTTP_POST,
                                       .handler = api_config_post_handler,
                                       .user_ctx = NULL};

static const httpd_uri_t uri_ota = {
    .uri = "/api/ota", .method = HTTP_POST, .handler = api_ota_post_handler, .user_ctx = NULL};

static const httpd_uri_t uri_root = {
    .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL};

/* ---------- public API ---------- */

esp_err_t http_server_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    err = httpd_register_uri_handler(s_server, &uri_root);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register root handler: %s", esp_err_to_name(err));
    }
    err = httpd_register_uri_handler(s_server, &uri_status);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register status handler: %s", esp_err_to_name(err));
    }
    err = httpd_register_uri_handler(s_server, &uri_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register config handler: %s", esp_err_to_name(err));
    }
    err = httpd_register_uri_handler(s_server, &uri_ota);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register OTA handler: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "HTTP server started");
    init_auth_token();
    ESP_LOGI(TAG, "Auth token: %s", s_auth_token);
    return ESP_OK;
}

void http_server_stop(void) {
    if (s_server != NULL) {
        esp_err_t err = httpd_stop(s_server);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "httpd_stop failed: %s", esp_err_to_name(err));
        }
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

#else /* __linux__ */

/* Stub implementations for host builds / unit tests */
esp_err_t http_server_init(void) {
    return ESP_OK;
}
void http_server_stop(void) {}

#endif /* __linux__ */
