#include <esp_err.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_random.h>
#include <esp_ble_mesh_defs.h>
#include <esp_bt_device.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "bluetooth/bt_mesh.h"
#include "component/board.h"


#define TAG          "MAIN"
#define PROV_WAIT_MS 10000

static SemaphoreHandle_t s_node_ready;

void ble_mesh_on_node_configured(void)
{
    xSemaphoreGive(s_node_ready);
}

void app_main(void)
{
    s_node_ready = xSemaphoreCreateBinary();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(bluetooth_init());
    ESP_ERROR_CHECK(ble_mesh_init_node());

    ESP_LOGI(TAG, "Waiting %d ms to be provisioned...", PROV_WAIT_MS);
    vTaskDelay(pdMS_TO_TICKS(PROV_WAIT_MS));

    if (!ble_mesh_is_provisioned()) {
        ESP_LOGI(TAG, "Not provisioned - becoming provisioner");
        ESP_ERROR_CHECK(ble_mesh_upgrade_to_provisioner());

        int32_t counter = 0;
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            ble_mesh_broadcast_int(counter++);
        }
    } else {
        ESP_LOGI(TAG, "Provisioned - waiting for full config from provisioner...");
        xSemaphoreTake(s_node_ready, portMAX_DELAY);
        ESP_LOGI(TAG, "Node fully configured - starting broadcast loop");

        while (1) {
            ble_mesh_broadcast_int(42);
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}