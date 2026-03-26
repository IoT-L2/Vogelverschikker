#include <esp_err.h>
#include <esp_log.h>
#include <nvs.h>

#define TAG "MAIN"
#include <nvs_flash.h>

#include "component/board.h"
#include "bluetooth/bluetooth.h"

/* ---------- App entry ---------- */
void app_main(void)
{
    ESP_LOGI(TAG, "Initializing...");

    /* NVS */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Board */
    board_init();

    /* Bluetooth stack */
    err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "Bluetooth init failed: %d", err);
        return;
    }

    /* UUID */


    /* Mesh */
    err = ble_mesh_init();
    if (err) {
        ESP_LOGE(TAG, "Mesh init failed: %d", err);
    }


}