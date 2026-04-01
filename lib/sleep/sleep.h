#pragma once

#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"

class SleepManager
{
public:
    // Constructor waar je eventueel de drempelwaarde kan meegeven (standaard 1000)
    SleepManager(int darkThreshold = 1000);

    // Initialiseert de ADC (LDR op pin 6) en de knop (pin 41)
    void init();

    // Controleert het licht en gaat in slaap indien nodig
    void checkAndSleep();

private:
    int _dark_threshold;
    adc_oneshot_unit_handle_t _adc_handle;

    // Pin definities
    static const adc_channel_t LDR_ADC_CHANNEL = ADC_CHANNEL_5; // GPIO 6
    static const adc_unit_t LDR_ADC_UNIT = ADC_UNIT_1;
    static const gpio_num_t BUTTON_PIN = GPIO_NUM_41; // GPIO 41
};