#pragma once

#include "esp_err.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Animation modes for the beacon.
 */
typedef enum {
    ANIM_MODE_BEACON = 0,  /**<< Rotating beacon (default) */
    ANIM_MODE_STROBE = 1,  /**<< Flashing strobe */
    ANIM_MODE_AMBIENT = 2, /**<< Static ambient color */
    ANIM_MODE_OFF = 3,     /**<< All LEDs off */
    ANIM_MODE_COUNT = 4    /**<< Total modes for validation */
} animation_mode_t;

/**
 * @brief Animation configuration.
 */
typedef struct {
    renderer_t *renderer; /**<< Renderer instance (LED or mock) */
} animation_config_t;

/**
 * @brief Initialise animation subsystem.
 * @return ESP_OK on success.
 */
esp_err_t beacon_animation_init(void);

/**
 * @brief Main animation task (FreeRTOS task entry point).
 * @param arg Pointer to animation_config_t.
 */
void beacon_animation_task(void *arg);

#ifdef __cplusplus
}
#endif