#include "sleep.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "SleepManager";

SleepManager::SleepManager(int darkThreshold) : _dark_threshold(darkThreshold), _adc_handle(NULL)
{
}

void SleepManager::init()
{
    ESP_LOGI(TAG, "Initialiseren van LDR (ADC)...");

    // We configureren hier ALLEEN de LDR, de knop is al in main.cpp gedaan!
    adc_oneshot_unit_init_cfg_t init_config1 = {};
    init_config1.unit_id = LDR_ADC_UNIT;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &_adc_handle));

    adc_oneshot_chan_cfg_t adc_config = {};
    adc_config.bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_config.atten = ADC_ATTEN_DB_12;
    ESP_ERROR_CHECK(adc_oneshot_config_channel(_adc_handle, LDR_ADC_CHANNEL, &adc_config));
}

void SleepManager::checkAndSleep()
{
    if (_adc_handle == NULL)
    {
        ESP_LOGE(TAG, "ADC is niet geïnitialiseerd!");
        return;
    }

    int ldr_val = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(_adc_handle, LDR_ADC_CHANNEL, &ldr_val));

    ESP_LOGI(TAG, "LDR Waarde: %d", ldr_val);

    // Als het donker is, starten we de slaap-loop
    if (ldr_val < _dark_threshold)
    {
        ESP_LOGW(TAG, "Het is donker! Slaapcyclus gestart...");

        // Vertel de ESP32 dat hij ook wakker mag worden van een timer (elke 5 seconden)
        esp_sleep_enable_timer_wakeup(5 * 1000000ULL); // 5.000.000 microseconden = 5 sec

        bool is_donker = true;

        // Blijf in deze loop hangen zolang het donker is
        while (is_donker)
        {
            vTaskDelay(pdMS_TO_TICKS(100)); // Geef de seriële monitor tijd om te printen

            // Start Light Sleep. De processor pauzeert hier!
            esp_light_sleep_start();

            // --- DE ESP32 ONTWAAKT HIER (door knop óf door 5-sec timer) ---
            esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

            if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO)
            {
                ESP_LOGI(TAG, "Wakker door KNOP! Geluid wordt afgespeeld...");

                // Wacht tot de knop wordt losgelaten
                while (gpio_get_level(BUTTON_PIN) == 0)
                {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }

                // Belangrijk: Blijf even 8 seconden geforceerd wakker.
                // Dit geeft de audio-taak op de achtergrond de tijd om je geluid volledig
                // af te spelen voordat we de processor weer in slaap laten vallen!
                vTaskDelay(pdMS_TO_TICKS(8000));
            }
            else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
            {
                // Stille timer wakeup elke 5 seconden. We printen niks om de log schoon te houden.
            }

            // Nu we toch wakker zijn (door knop of timer), meten we direct de LDR opnieuw
            ESP_ERROR_CHECK(adc_oneshot_read(_adc_handle, LDR_ADC_CHANNEL, &ldr_val));

            if (ldr_val >= _dark_threshold)
            {
                ESP_LOGI(TAG, "Het is weer licht (Waarde: %d)! Ik blijf helemaal wakker.", ldr_val);
                is_donker = false; // Dit verbreekt de while-loop, we gaan terug naar main.cpp
            }
            else
            {
                if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO)
                {
                    ESP_LOGI(TAG, "Muziek is klaar, maar het is nog donker. Verder slapen...");
                }
            }
        }
    }
}