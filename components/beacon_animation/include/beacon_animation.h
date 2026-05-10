#pragma once

#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    renderer_t *renderer;
} animation_config_t;

void beacon_animation_task(void *arg);

#ifdef __cplusplus
}
#endif
