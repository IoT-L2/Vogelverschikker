#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_attr.h"
#include "sound.h"
#include "sleep.h"

#define BUTTON_PIN GPIO_NUM_41 // Zelfde pin voor wake-up EN geluid

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

    while (1)
    {
        // Check de LDR en ga in sleep als het donker is
        sleepCtrl.checkAndSleep();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}