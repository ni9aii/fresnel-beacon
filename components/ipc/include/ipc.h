#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

/**
 * @brief IPC command types for runtime configuration updates.
 */
typedef enum {
    IPC_CMD_SET_SPEED,
    IPC_CMD_SET_COLOR,
    IPC_CMD_SET_MODE,
    IPC_CMD_SET_BRIGHTNESS,
} ipc_cmd_type_t;

/**
 * @brief IPC command payload.
 */
typedef struct {
    ipc_cmd_type_t type;
    union {
        float    speed_rpm;
        uint32_t color_rgb;   // 0xRRGGBB
        int32_t  mode;
        float    brightness;  // 0.0 .. 1.0
    } data;
} ipc_cmd_t;

/**
 * @brief Global FreeRTOS queue handle for IPC commands.
 *        Created by ipc_init(), size = 10.
 */
extern QueueHandle_t ipc_queue;

/**
 * @brief Initialise IPC queue.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM if allocation fails.
 */
esp_err_t ipc_init(void);
