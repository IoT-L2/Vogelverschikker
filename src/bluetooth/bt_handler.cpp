#include "bt_handler.h"
#include "bt_mesh.h"
#include "sound.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define TAG          "BT_HANDLER"
#define PROV_WAIT_MS 10000

extern "C" void on_bt_received(int32_t value)
{
    ESP_LOGI(TAG, "[vendor] received value: %" PRId32, value);
    switch (value) {
        case 1:  play_sound("tetrismusic.wav"); break;
        default: break;
    }
}

static SemaphoreHandle_t s_node_ready;

void ble_mesh_on_node_configured(void)
{
    xSemaphoreGive(s_node_ready);
}

static void node_loop()
{
    while (true) {
        // any periodic provisioner/node work here.
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void init_ble_mesh_config()
{
    s_node_ready = xSemaphoreCreateBinary();
    configASSERT(s_node_ready);

    ESP_ERROR_CHECK(bluetooth_init());
    ESP_ERROR_CHECK(ble_mesh_init_node());

    ESP_LOGI(TAG, "Waiting %d ms to be provisioned ...", PROV_WAIT_MS);
    vTaskDelay(pdMS_TO_TICKS(PROV_WAIT_MS));

    if (!ble_mesh_is_provisioned()) {
        ESP_LOGI(TAG, "Not provisioned — becoming provisioner");
        ESP_ERROR_CHECK(ble_mesh_upgrade_to_provisioner());
        //can add a while loop here?

    } else {
        ESP_LOGI(TAG, "Provisioned — waiting for full config from provisioner ...");
        xSemaphoreTake(s_node_ready, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Node fully configured — starting node loop");
        node_loop(); // loops forever;
    }
}