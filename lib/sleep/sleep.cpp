#include "sleep.h"
#include "sound.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

static const char* TAG = "SleepManager";

SleepManager& SleepManager::getInstance(int darkThreshold, gpio_num_t buttonPin)
{
    static SleepManager instance(darkThreshold, buttonPin);
    return instance;
}

SleepManager::SleepManager(int darkThreshold, gpio_num_t buttonPin)
    : _dark_threshold(darkThreshold),
      _button_pin(buttonPin),
      _adc_handle(nullptr),
      _task_handle(nullptr)
{}

void SleepManager::init()
{
    ESP_LOGI(TAG, "Initialising LDR ADC on channel %d ...", LDR_ADC_CHANNEL);

    adc_oneshot_unit_init_cfg_t unit_cfg = {};
    unit_cfg.unit_id = LDR_ADC_UNIT;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    chan_cfg.atten    = ADC_ATTEN_DB_12;
    ESP_ERROR_CHECK(adc_oneshot_config_channel(_adc_handle, LDR_ADC_CHANNEL, &chan_cfg));

    // Configure wake-up sources ONCE here, not inside the sleep loop.
    // Timer: wake every 5 sec
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(5ULL * 1000000ULL));

    // Button: wake on falling edge
    if (_button_pin != GPIO_NUM_NC) {
        ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
        ESP_ERROR_CHECK(gpio_wakeup_enable(_button_pin, GPIO_INTR_LOW_LEVEL));
        ESP_LOGI(TAG, "Button wake-up enabled on GPIO %d", _button_pin);
    }

    ESP_LOGI(TAG, "SleepManager initialised (dark threshold: %d)", _dark_threshold);
}

void SleepManager::start()
{
    if (_task_handle != nullptr) {
        ESP_LOGW(TAG, "start() called more than once — ignoring");
        return;
    }

    BaseType_t result = xTaskCreate(
        SleepManager::sleepTaskEntry,
        "sleep_mgr",
        4096,
        this,
        /* priority */ 3,
        &_task_handle
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sleep task!");
    } else {
        ESP_LOGI(TAG, "Sleep manager task started");
    }
}

void SleepManager::sleepTaskEntry(void* arg)
{
    SleepManager* self = static_cast<SleepManager*>(arg);
    while (true) {
        self->checkAndSleep();
        // When it's light, poll every 5 s — cheap and harmless.
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void SleepManager::checkAndSleep()
{
    if (_adc_handle == nullptr) {
        ESP_LOGE(TAG, "ADC not initialised — call init() first!");
        return;
    }

    int ldr_val = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(_adc_handle, LDR_ADC_CHANNEL, &ldr_val));
    ESP_LOGI(TAG, "LDR value: %d (threshold: %d)", ldr_val, _dark_threshold);

    if (ldr_val >= _dark_threshold) {
        return; // It's light — nothing to do.
    }

    // ── It's dark: stop sound and enter the light-sleep loop ──────────────────
    ESP_LOGW(TAG, "Dark detected — entering light-sleep loop");
    stop_sound();
    vTaskDelay(pdMS_TO_TICKS(50)); // Let audio turn off

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10)); // Let the UART flush before sleeping

        esp_light_sleep_start(); // sleeping

        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

        if (cause == ESP_SLEEP_WAKEUP_GPIO) {
            ESP_LOGI(TAG, "Woken by button (GPIO %d)", _button_pin);

            // Debounce
            if (_button_pin != GPIO_NUM_NC) {
                while (gpio_get_level(_button_pin) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            }
        }

        // Read LDR after every wake-up
        ESP_ERROR_CHECK(adc_oneshot_read(_adc_handle, LDR_ADC_CHANNEL, &ldr_val));

        if (ldr_val >= _dark_threshold) {
            ESP_LOGI(TAG, "Light restored (LDR: %d) — resuming normal operation", ldr_val);
            break; //the task loop will re-check in 5 s
        }
    }
}