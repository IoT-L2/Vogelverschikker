#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_attr.h"
#include "sound.h"
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <freertos/semphr.h>

#include "bluetooth/bt_mesh.h"
#include "sleep.h"


#define TAG          "MAIN"
#define PROV_WAIT_MS 10000
#define BUTTON_PIN GPIO_NUM_41

// Variabele om bij te houden wanneer de knop voor het laatst is ingedrukt (Debounce)
static uint32_t last_isr_time = 0;

static void IRAM_ATTR button_isr_handler(void *arg)
{
    // Haal de huidige tijd op (in milliseconden) via een veilige ISR functie
    uint32_t current_time = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

    // Check of er minstens 250ms verstreken zijn sinds de laatste druk.
    // Dit filtert het "stotteren" (bouncen) van de fysieke knop weg!
    if (current_time - last_isr_time > 250)
    {
        last_isr_time = current_time;
        trigger_sound_from_isr();
    }
}

static SemaphoreHandle_t s_node_ready;

void ble_mesh_on_node_configured(void)
{
    xSemaphoreGive(s_node_ready);
}

extern "C" void app_main(void)
{
    ESP_LOGI("MAIN", "Systeem start op!");

    // 1. Initialiseer geluid
    init_sound();

    // 2. Configureer de knop (GPIO 41) voor BEIDE: Interrupt (geluid) én Wake-up (slaap)
    gpio_config_t btn_config = {};
    btn_config.pin_bit_mask = (1ULL << BUTTON_PIN);
    btn_config.mode = GPIO_MODE_INPUT;
    btn_config.pull_up_en = GPIO_PULLUP_ENABLE;
    btn_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn_config.intr_type = GPIO_INTR_NEGEDGE; // Trigger op indrukken (HOOG naar LAAG)
    ESP_ERROR_CHECK(gpio_config(&btn_config));

    // Koppel de ISR (voor het geluid)
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL));

    // Activeer Light Sleep wake-up voor dezelfde pin (op een LAAG niveau)
    ESP_ERROR_CHECK(gpio_wakeup_enable(BUTTON_PIN, GPIO_INTR_LOW_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

    // 3. Initialiseer de Sleep Manager
    SleepManager sleepCtrl(2500); // 2500 is de drempelwaarde voor donker
    sleepCtrl.init();

    ESP_LOGI("MAIN", "Systeem is klaar. Druk op knop (GPIO41 → GND) voor geluid.");

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
            sleepCtrl.checkAndSleep();
            ble_mesh_broadcast_int(counter++);
        }
    } else {
        ESP_LOGI(TAG, "Provisioned - waiting for full config from provisioner...");
        xSemaphoreTake(s_node_ready, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Node fully configured - starting broadcast loop");

        while (1) {
            ble_mesh_broadcast_int(42);
            sleepCtrl.checkAndSleep();
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}