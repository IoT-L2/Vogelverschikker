#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sound.h"

extern "C" void app_main(void)
{
    ESP_LOGI("MAIN", "Systeem start op!");

    // 1. Initialiseer SPIFFS en I2C
    init_sound();

    // 2. Test Loop: Roep het geluid elke 5 seconden automatisch aan
    while (1)
    {
        ESP_LOGI("MAIN", "Stuur signaal om vogel af te spelen...");
        trigger_sound();

        // Wacht 5 seconden
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}