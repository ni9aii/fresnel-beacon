#pragma once

#include "esp_err.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    renderer_t *renderer;
} animation_config_t;

esp_err_t beacon_animation_init(void);
void beacon_animation_task(void *arg);

#ifdef __cplusplus
}
#endif
