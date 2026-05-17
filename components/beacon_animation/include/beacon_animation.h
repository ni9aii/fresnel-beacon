#pragma once

#include "esp_err.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Global beacon rotation speed (seconds per full rotation).
 * Write via config_manager or IPC; read-only here for animation loop.
 */
extern float g_beacon_speed;

typedef struct {
    renderer_t *renderer;
} animation_config_t;

esp_err_t beacon_animation_init(void);
void beacon_animation_task(void *arg);

#ifdef __cplusplus
}
#endif
