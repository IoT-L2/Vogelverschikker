#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_attr.h"

#include "sleep.h"
#include "board/board.h"
#include "bluetooth/bt_handler.h"
#include "nvs2.h"
#include "simpelconfig.h"
#include "spiffs.h"

#define TAG        "MAIN"
#define BUTTON_PIN GPIO_NUM_41

SimpleConfig audio_cfg("audio_cfg");
SimpleConfig detections("detections");

extern "C" void app_main()
{
    ESP_LOGI(TAG, "System starting up");

    board_init();
    nvs2_init();
    spiffs_init();




    // Init and start the sleep monitor as an FreeRTOS task.
    SleepManager::getInstance(700, BUTTON_PIN).init();
    SleepManager::getInstance().start();

    init_ble_mesh_config();
}