#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs2.h>

#define TAG "NVS"

esp_err_t nvs2_init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition problem, erasing all data and retrying...");
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed (%s)", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}
