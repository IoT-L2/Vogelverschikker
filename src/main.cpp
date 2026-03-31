#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sound.h"

#define BUTTON_PIN GPIO_NUM_41

// De interrupt functie die wordt aangeroepen als de knop wordt ingedrukt
static void IRAM_ATTR button_isr_handler(void *arg)
{
    trigger_sound_from_isr();
}

extern "C" void app_main(void)
{
    ESP_LOGI("MAIN", "Systeem start op!");

    // 1. Initialiseer geluid (SPIFFS, I2C, Task)
    init_sound();

    // 2. Configureer Pin 41 als knop
    gpio_config_t btn_config = {};
    btn_config.pin_bit_mask = (1ULL << BUTTON_PIN);
    btn_config.mode = GPIO_MODE_INPUT;
    btn_config.pull_up_en = GPIO_PULLUP_ENABLE; // Zet interne weerstand naar 3.3V aan
    btn_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn_config.intr_type = GPIO_INTR_NEGEDGE; // Reageer als de pin naar GND (0V) gaat
    gpio_config(&btn_config);

    // 3. Koppel de interrupt aan de pin
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);

    ESP_LOGI("MAIN", "Klaar! Druk op de knop (verbind Pin 41 met GND) om af te spelen.");

    // De main loop doet nu niks meer behalve wachten.
    // De audio speelt volledig op de achtergrond via de interrupt!
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}