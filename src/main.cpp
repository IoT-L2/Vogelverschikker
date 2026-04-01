#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sound.h"
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <freertos/semphr.h>

#include "bluetooth/bt_mesh.h"


#define TAG          "MAIN"
#define PROV_WAIT_MS 10000
#define BUTTON_PIN GPIO_NUM_41

static void IRAM_ATTR button_isr_handler(void *arg)
{
    trigger_sound_from_isr();
}

static SemaphoreHandle_t s_node_ready;

void ble_mesh_on_node_configured(void)
{
    xSemaphoreGive(s_node_ready);
}

extern "C" void app_main(void)
{
    ESP_LOGI("MAIN", "Systeem start op!");

    init_sound();

    gpio_config_t btn_config = {};
    btn_config.pin_bit_mask = (1ULL << BUTTON_PIN);
    btn_config.mode = GPIO_MODE_INPUT;
    btn_config.pull_up_en = GPIO_PULLUP_ENABLE;
    btn_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn_config.intr_type = GPIO_INTR_NEGEDGE;
    gpio_config(&btn_config);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);

    ESP_LOGI("MAIN", "Druk op knop (GPIO41 → GND)");

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
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Node fully configured - starting broadcast loop");

        while (1) {
            ble_mesh_broadcast_int(42);
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}