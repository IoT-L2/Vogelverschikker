#pragma once

#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C"
{
#endif
class SleepManager
{
public:
    /**
     * Get the singleton instance.
     * Parameters are only applied on the FIRST call
     * return the already constructed instance and ignore the arguments.
     *
     * @param darkThreshold  value below which we consider it "dark"  (default 700)
     * @param buttonPin      GPIO used as wake-up button                  (default GPIO_NUM_NC)
     */
    static SleepManager& getInstance(int darkThreshold = 700,
                                     gpio_num_t buttonPin = GPIO_NUM_NC);

    /**
     * Initialise the LDR and configure light-sleep wake-up sources.
     * Call once from app_main before start().
     */
    void init();

    /** start the background FreeRTOS task that monitors light and sleeps.*/
    void start();

    /** Read LDR once and, if dark, enter the light-sleep loop.*/
    void checkAndSleep();

private:
    SleepManager(int darkThreshold, gpio_num_t buttonPin);

    static void sleepTaskEntry(void* arg);

    static const adc_channel_t LDR_ADC_CHANNEL = ADC_CHANNEL_5; // GPIO 6
    static const adc_unit_t    LDR_ADC_UNIT     = ADC_UNIT_1;

    int                       _dark_threshold;
    gpio_num_t                _button_pin;
    adc_oneshot_unit_handle_t _adc_handle;
    TaskHandle_t              _task_handle;
};
#ifdef __cplusplus
}
#endif