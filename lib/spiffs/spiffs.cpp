#include "spiffs.h"

#include <esp_log.h>
#include <esp_spiffs.h>


#define TAG "SPIFFS"

void spiffs_init()
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
