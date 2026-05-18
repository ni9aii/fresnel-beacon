#include "auth.h"
#include "credentials.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "auth";

/* Base64 decode table */
static const unsigned char base64_table[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 64, 0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64};

static int base64_decode(const char *in, unsigned char *out, int out_len) {
    int i, j, v;
    int in_len = (int) strlen(in);
    int olen = 0;

    for (i = 0, j = 0; i < in_len; i += 4, j += 3) {
        v = base64_table[(unsigned char) in[i]] << 18;
        v += base64_table[(unsigned char) in[i + 1]] << 12;
        v += base64_table[(unsigned char) in[i + 2]] << 6;
        v += base64_table[(unsigned char) in[i + 3]];

        if (j < out_len)
            out[j] = (unsigned char) ((v >> 16) & 0xFF);
        if (j + 1 < out_len)
            out[j + 1] = (unsigned char) ((v >> 8) & 0xFF);
        if (j + 2 < out_len)
            out[j + 2] = (unsigned char) (v & 0xFF);
        olen += 3;
    }

    /* Adjust for padding */
    if (in_len > 0 && in[in_len - 1] == '=')
        olen--;
    if (in_len > 1 && in[in_len - 2] == '=')
        olen--;

    return olen > out_len ? out_len : olen;
}

esp_err_t auth_validate(httpd_req_t *req) {
    char auth_header[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) !=
        ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *prefix = "Basic ";
    if (strncmp(auth_header, prefix, strlen(prefix)) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *b64 = auth_header + strlen(prefix);
    unsigned char decoded[64] = {0};
    int decoded_len = base64_decode(b64, decoded, sizeof(decoded) - 1);
    if (decoded_len <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    decoded[decoded_len] = '\0';

    /* Expected format: username:password */
    const char *expected = ADMIN_USERNAME ":" ADMIN_PASSWORD;
    if (strcmp((const char *) decoded, expected) != 0) {
        ESP_LOGW(TAG, "Invalid credentials");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

void auth_send_401(httpd_req_t *req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Fresnel Beacon\"");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"error\":\"unauthorized\"}", HTTPD_RESP_USE_STRLEN);
}
