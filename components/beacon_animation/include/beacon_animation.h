#pragma once

#include "esp_err.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Animation modes */
typedef enum {
    ANIM_MODE_BEACON = 0,  /* Rotating beacon (default) */
    ANIM_MODE_STROBE = 1,  /* Flashing strobe */
    ANIM_MODE_AMBIENT = 2, /* Static ambient color */
    ANIM_MODE_OFF = 3,     /* All LEDs off */
    ANIM_MODE_COUNT = 4    /* Total modes for validation */
} animation_mode_t;

typedef struct {
    renderer_t *renderer;
} animation_config_t;

esp_err_t beacon_animation_init(void);
void beacon_animation_task(void *arg);

#ifdef __cplusplus
}
#endif