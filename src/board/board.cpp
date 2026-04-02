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
#include "main.h"
#include "bluetooth/blepacket.h"
#include "bluetooth/bt_mesh.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"

#define TAG "BOARD"

static TaskHandle_t s_button_task_handle  = NULL;
static TaskHandle_t s_button2_task_handle = NULL;

static volatile uint32_t last_isr_time  = 0;
static volatile uint32_t last_isr2_time = 0;

#define BUTTON_PIN  GPIO_NUM_41
#define BUTTON2_PIN GPIO_NUM_42

// --- ISR handlers ---

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

static void IRAM_ATTR button2_isr_handler(void *arg)
{
    uint32_t now = (uint32_t)xTaskGetTickCountFromISR();
    if (now - last_isr2_time < pdMS_TO_TICKS(500)) return;
    last_isr2_time = now;

    gpio_isr_handler_remove(BUTTON2_PIN);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(s_button2_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// --- Tasks ---

static void button_task(void *arg)
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (gpio_get_level(BUTTON_PIN) == 0)
            vTaskDelay(pdMS_TO_TICKS(10));
        vTaskDelay(pdMS_TO_TICKS(50));

        char count[16];
        int number = 0;
        if (detections.getLine(0, count, sizeof(count))) {
            number = atoi(count);
            number += 1;

            char new_value[16];
            snprintf(new_value, sizeof(new_value), "%d", number);
            detections.setLine(0, new_value);
        } else {
            detections.setLine(0, "0");
        }
        ESP_LOGI(TAG, "Increased this nodes count by 1 now total: %i", number);
        ble_mesh_broadcast_int(ble_play_sound_packet());
        send_data_packet();
        gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);
    }
}

static void button2_task(void *arg)
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (gpio_get_level(BUTTON2_PIN) == 0)
            vTaskDelay(pdMS_TO_TICKS(10));
        vTaskDelay(pdMS_TO_TICKS(50));

        sound_cycle();
        ble_mesh_broadcast_int(ble_change_sound_packet(sound_get_current()));
        ESP_LOGI(TAG, "Sound cycled to next config");

        gpio_isr_handler_add(BUTTON2_PIN, button2_isr_handler, NULL);
    }
}

// --- Init ---

void board_init()
{
    // Geluid
    init_sound();

    // GPIO button 1 (GPIO 41)
    gpio_config_t btn_config = {};
    btn_config.pin_bit_mask = (1ULL << BUTTON_PIN);
    btn_config.mode         = GPIO_MODE_INPUT;
    btn_config.pull_up_en   = GPIO_PULLUP_ENABLE;
    btn_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn_config.intr_type    = GPIO_INTR_POSEDGE;
    ESP_ERROR_CHECK(gpio_config(&btn_config));

    // GPIO button 2 (GPIO 42)
    gpio_config_t btn2_config = {};
    btn2_config.pin_bit_mask = (1ULL << BUTTON2_PIN);
    btn2_config.mode         = GPIO_MODE_INPUT;
    btn2_config.pull_up_en   = GPIO_PULLUP_ENABLE;
    btn2_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn2_config.intr_type    = GPIO_INTR_POSEDGE;
    ESP_ERROR_CHECK(gpio_config(&btn2_config));

    // Sleep wakeup on button 1
    ESP_ERROR_CHECK(gpio_wakeup_enable(BUTTON_PIN, GPIO_INTR_LOW_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

    // Tasks
    xTaskCreate(button_task,  "button_task",  4096, NULL, 10, &s_button_task_handle);
    xTaskCreate(button2_task, "button2_task", 4096, NULL, 10, &s_button2_task_handle);

    // ISR service + handlers
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_PIN,  button_isr_handler,  NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON2_PIN, button2_isr_handler, NULL));

    ESP_LOGI(TAG, "Board geïnitialiseerd.");
}