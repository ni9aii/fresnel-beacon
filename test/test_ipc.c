#include "unity.h"
#include "ipc.h"

TEST_CASE("ipc_init creates queue and semaphore", "[ipc]") {
    /* Reset static handles so we can test init on a clean slate */
    extern QueueHandle_t ipc_queue;
    extern SemaphoreHandle_t ipc_commit_sem;
    ipc_queue = NULL;
    ipc_commit_sem = NULL;

    esp_err_t err = ipc_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_NOT_NULL(ipc_queue);
    TEST_ASSERT_NOT_NULL(ipc_commit_sem);
}

TEST_CASE("ipc_cmd_t size is 8 bytes", "[ipc]") {
    /* Compile-time sanity check for IPC struct layout */
    TEST_ASSERT_EQUAL(8, sizeof(ipc_cmd_t));
}

TEST_CASE("ipc_cmd_t union stores speed_rpm correctly", "[ipc]") {
    ipc_cmd_t cmd;
    cmd.type = IPC_CMD_SET_SPEED;
    cmd.data.speed_rpm = 42.0f;
    TEST_ASSERT_EQUAL(IPC_CMD_SET_SPEED, cmd.type);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, cmd.data.speed_rpm);
}

TEST_CASE("ipc_cmd_t union stores color_rgb correctly", "[ipc]") {
    ipc_cmd_t cmd;
    cmd.type = IPC_CMD_SET_COLOR;
    cmd.data.color_rgb = 0xFFA028;
    TEST_ASSERT_EQUAL(IPC_CMD_SET_COLOR, cmd.type);
    TEST_ASSERT_EQUAL_UINT32(0xFFA028, cmd.data.color_rgb);
}
