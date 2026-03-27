#include <esp_err.h>
#include <esp_log.h>
#include <nvs.h>
#include <esp_random.h>

#define TAG "MAIN"
#include <esp_ble_mesh_defs.h>
#include <esp_bt_device.h>
#include <nvs_flash.h>

#include "component/board.h"
#include "bluetooth/bluetooth.h"


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



    err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }



    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err) {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
    }
}