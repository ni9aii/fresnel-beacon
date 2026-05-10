#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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
    IPC_CMD_COMMIT,
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
 *        Created by ipc_init(), size = 32.
 */
extern QueueHandle_t ipc_queue;

/**
 * @brief Binary semaphore used to signal that IPC_CMD_COMMIT has been
 *        processed by the animation task.
 */
extern SemaphoreHandle_t ipc_commit_sem;

/**
 * @brief Initialise IPC queue and commit semaphore.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM if allocation fails.
 */
esp_err_t ipc_init(void);

/**
 * @brief Wait for the animation task to signal that IPC_CMD_COMMIT was processed.
 *
 * @param timeout_ms Timeout in milliseconds.
 * @return pdTRUE if signaled, pdFALSE on timeout.
 */
BaseType_t ipc_wait_commit(uint32_t timeout_ms);

/**
 * @brief Signal that IPC_CMD_COMMIT has been processed.
 */
void ipc_signal_commit(void);

#ifdef __cplusplus
}
#endif
