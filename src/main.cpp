#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"

#include "sleep.h"
#include "board/board.h"
#include "bluetooth/bt_handler.h"

#define TAG        "MAIN"
#define BUTTON_PIN GPIO_NUM_41

static void nvs_init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated - erasing and re-initialising");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    esp_err_t result = esp_vfs_spiffs_register(&conf);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%)", esp_err_to_name(result));
        return;
    }
    size_t total = 0, used = 0;
    result = esp_spiffs_info(conf.partition_label, &total, &used);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get partition info (%s)", esp_err_to_name(result));
    }else{
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}


extern "C" void app_main()
{
    ESP_LOGI(TAG, "System starting up");

    board_init();
    nvs_init();
    spiffs_init();

    // Init and start the sleep monitor as an FreeRTOS task.
    SleepManager::getInstance(700, BUTTON_PIN).init();
    SleepManager::getInstance().start();

    init_ble_mesh_config();
}