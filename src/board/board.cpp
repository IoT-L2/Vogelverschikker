//
// Created by Christian ten Brinke on 02/04/2026.
//

#include "board.h"

#include <FreeRTOSConfig.h>
#include <portmacro.h>
#include <freertos/FreeRTOS.h>
#include <freertos/projdefs.h>

#include "sound.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "bluetooth/bt_mesh.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"

#define TAG "BOARD"

static TaskHandle_t s_button_task_handle = NULL;
static volatile uint32_t last_isr_time = 0;

#define BUTTON_PIN GPIO_NUM_41

static void IRAM_ATTR button_isr_handler(void *arg)
{
    uint32_t now = (uint32_t)xTaskGetTickCountFromISR();
    if (now - last_isr_time < pdMS_TO_TICKS(500)) return;
    last_isr_time = now;

    gpio_isr_handler_remove(BUTTON_PIN);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(s_button_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void button_task(void *arg)
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (gpio_get_level(BUTTON_PIN) == 0)
            vTaskDelay(pdMS_TO_TICKS(10));
        vTaskDelay(pdMS_TO_TICKS(50));

        ble_mesh_broadcast_int(1);

        gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);
    }
}

void board_init()
{
    // Geluid
    init_sound();

    // GPIO knop
    gpio_config_t btn_config = {};
    btn_config.pin_bit_mask = (1ULL << BUTTON_PIN);
    btn_config.mode         = GPIO_MODE_INPUT;
    btn_config.pull_up_en   = GPIO_PULLUP_ENABLE;
    btn_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn_config.intr_type    = GPIO_INTR_POSEDGE;
    ESP_ERROR_CHECK(gpio_config(&btn_config));

    // Sleep wakeup op dezelfde pin
    ESP_ERROR_CHECK(gpio_wakeup_enable(BUTTON_PIN, GPIO_INTR_LOW_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

    // Queue en tasks
    xTaskCreate(button_task,    "button_task",    4096, NULL, 10, &s_button_task_handle);
    // ISR service + handler
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL));

    ESP_LOGI(TAG, "Board geïnitialiseerd.");
}
