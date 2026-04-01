#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sound.h"

#define BUTTON_PIN GPIO_NUM_41 // 🔥 FIX: andere pin!

static void IRAM_ATTR button_isr_handler(void *arg)
{
    trigger_sound_from_isr();
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

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}