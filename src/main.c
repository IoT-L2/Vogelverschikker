#include <esp_err.h>
#include <esp_log.h>
#include <nvs.h>
#include <esp_random.h>

#define TAG "MAIN"
#include <esp_ble_mesh_defs.h>
#include <esp_bt_device.h>
#include <nvs_flash.h>


#include "bluetooth/bt_mesh.h"
#include "component/board.h"


#define TAG             "MAIN"
#define PROV_WAIT_MS    10000   /* how long to wait before becoming provisioner */


/* ---------- App entry ---------- */
void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Initializing...");

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(bluetooth_init());
    ESP_ERROR_CHECK(ble_mesh_init_node());

    ESP_LOGI(TAG, "Waiting %d ms to be provisioned...", PROV_WAIT_MS);
    vTaskDelay(pdMS_TO_TICKS(PROV_WAIT_MS));

    if (!ble_mesh_is_provisioned()) {
        ESP_LOGI(TAG, "Not provisioned — becoming provisioner");
        ESP_ERROR_CHECK(ble_mesh_upgrade_to_provisioner());
    } else {
        ESP_LOGI(TAG, "Provisioned — running as node");
    }
}