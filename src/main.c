#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sound.h"

// Vul hier de GPIO pin in waar je sensor/knop op is aangesloten
#define SENSOR_PIN GPIO_NUM_4

// De interrupt functie
static void IRAM_ATTR sensor_isr_handler(void *arg)
{
    // Wakker de audiotaak aan!
    trigger_sound_from_isr();
}

void app_main(void)
{
    // 1. Initialiseer I2C, SPIFFS en de audiotaak
    init_sound();

    // 2. Configureer de sensor pin als een interrupt
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE; // Triggert als het signaal LOW wordt (aarde raakt)
    io_conf.pin_bit_mask = (1ULL << SENSOR_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // 3. Koppel de interrupt aan de pin
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SENSOR_PIN, sensor_isr_handler, NULL);

    ESP_LOGI("MAIN", "Systeem gestart! Verbind GPIO 4 met GND om geluid te testen.");

    // Hieronder kun je later je BLE Mesh code toevoegen.
}