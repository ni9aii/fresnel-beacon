#pragma once
#include "esp_err.h"

esp_err_t mdns_service_init(const char *hostname, const char *instance_name);
void mdns_service_stop(void);
