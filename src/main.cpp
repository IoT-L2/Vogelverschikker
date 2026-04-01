#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "sleep.h"
#include "board/board.h"
#include "bluetooth/bt_handler.h"

#define TAG        "MAIN"
#define BUTTON_PIN GPIO_NUM_41

static void nvs_init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated — erasing and re-initialising");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "System starting up");

    board_init();
    nvs_init();

    // Init and start the sleep monitor as an FreeRTOS task.
    SleepManager::getInstance(700, BUTTON_PIN).init();
    SleepManager::getInstance().start();

    init_ble_mesh_config();
}