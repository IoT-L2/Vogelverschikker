#include "sound.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "rom/ets_sys.h"


static const char *TAG = "AUDIO";

#define I2C_MASTER_SDA_IO 4
#define I2C_MASTER_SCL_IO 5
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 1000000
#define MCP4725_ADDR 0x60

#define WAV_HEADER_SIZE 44
#define AUDIO_BUFFER_SIZE 2048
#define SAMPLE_DELAY_US 45

#define MAX_FILENAME_LEN 32

static TaskHandle_t audio_task_handle = NULL;
static QueueHandle_t audio_queue = NULL;
static volatile bool is_playing = false;

// ---------------- I2C ----------------
static void init_i2c(void)
{
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// ---------------- DAC ----------------
static inline void mcp4725_set_voltage(uint16_t value)
{
    uint8_t data[2];
    data[0] = (value >> 8) & 0x0F;
    data[1] = value & 0xFF;

    i2c_master_write_to_device(I2C_MASTER_NUM, MCP4725_ADDR, data, 2, pdMS_TO_TICKS(10));
}

// ---------------- WAV ----------------
static void play_wav(const char *filename)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/spiffs/%s", filename);

    FILE *f = fopen(filepath, "rb");
    if (!f)
    {
        ESP_LOGE(TAG, "Kan bestand niet openen: %s", filepath);
        is_playing = false;
        return;
    }

    fseek(f, WAV_HEADER_SIZE, SEEK_SET);

    uint8_t buffer[AUDIO_BUFFER_SIZE];
    size_t bytes_read;

    ESP_LOGI(TAG, "Start afspelen: %s", filename);

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0)
    {
        for (size_t i = 0; i < bytes_read; i++)
        {
            if (!is_playing) {
                fclose(f);
                mcp4725_set_voltage(0);
                is_playing = false;
                return;
            }
            uint16_t sample = buffer[i] << 4;
            mcp4725_set_voltage(sample);
            ets_delay_us(SAMPLE_DELAY_US);

            // Yield elke ~200 samples (~9ms) zodat de WDT niet afloopt
            if ((i & 0xFF) == 0)
            {
                taskYIELD();
            }
        }
        // vTaskDelay(pdMS_TO_TICKS(1)); ← dit mag weg, taskYIELD hierboven is voldoende
    }

    fclose(f);
    mcp4725_set_voltage(0);
    is_playing = false;

    ESP_LOGI(TAG, "Klaar met afspelen.");
}


// ---------------- TASK ----------------
static void audio_task(void *pvParameters)
{
    char filename[MAX_FILENAME_LEN];

    while (1)
    {
        if (xQueueReceive(audio_queue, &filename, portMAX_DELAY))
        {
            play_wav(filename);
        }
    }
}

// ---------------- INIT ----------------
void init_sound(void)
{
    ESP_LOGI(TAG, "Init sound...");

    init_i2c();

    audio_queue = xQueueCreate(1, MAX_FILENAME_LEN);

    xTaskCreatePinnedToCore(audio_task, "audio_task", 8192, NULL, 8, &audio_task_handle, 1);
}

// ---------------- PLAY FUNCTION ----------------
void play_sound(const char *filename)
{
    if (audio_queue == NULL) return;

    if (is_playing)
    {
        ESP_LOGI(TAG, "Sound bezig, trigger genegeerd");
        return;
    }

    is_playing = true;

    char buffer[MAX_FILENAME_LEN];
    strncpy(buffer, filename, MAX_FILENAME_LEN - 1);
    buffer[MAX_FILENAME_LEN - 1] = '\0';

    xQueueReset(audio_queue);
    xQueueSend(audio_queue, &buffer, pdMS_TO_TICKS(100));
}

bool is_sound_playing(void)
{
    return is_playing;
}

// sound.cpp
void stop_sound(void)
{
    if (!is_playing) return;
    is_playing = false;
    xQueueReset(audio_queue);
}
