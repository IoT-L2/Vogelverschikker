#include "sound.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "esp_spiffs.h"

static const char *TAG = "AUDIO";

// I2C
#define I2C_MASTER_SDA_IO 4
#define I2C_MASTER_SCL_IO 5
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 1000000
#define MCP4725_ADDR 0x60

// Audio
#define WAV_HEADER_SIZE 44
#define AUDIO_BUFFER_SIZE 2048

// 🔥 FIX: 125us is de correcte pauze voor een 8000 Hz wav bestand.
// Als je audio te traag klinkt, zet dit op 60 (voor 16kHz). Als het te snel klinkt, zet op 250 (voor 4kHz).
#define SAMPLE_DELAY_US 45

static TaskHandle_t audio_task_handle = NULL;

// ---------------- SPIFFS ----------------
static void init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    esp_vfs_spiffs_register(&conf);
}

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
    // MCP4725 "Fast Write" mode: De eerste 4 bits zijn 0, gevolgd door 12 bits data
    data[0] = (value >> 8) & 0x0F;
    data[1] = value & 0xFF;

    // 🔥 FIX: Dit is een veel efficiëntere, moderne ESP-IDF functie!
    // Hierdoor raakt de processor niet overbelast.
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
        return;
    }

    fseek(f, WAV_HEADER_SIZE, SEEK_SET);

    uint8_t buffer[AUDIO_BUFFER_SIZE];
    size_t bytes_read;

    ESP_LOGI(TAG, "Start afspelen...");

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0)
    {
        for (size_t i = 0; i < bytes_read; i++)
        {
            uint16_t sample = buffer[i] << 4;

            mcp4725_set_voltage(sample);

            // Busy-wait voor de sample frequentie
            ets_delay_us(SAMPLE_DELAY_US);
        }

        // 🔥 FIX 1: Geef de watchdog van Core 1 even ademruimte na elk datablok!
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    fclose(f);
    mcp4725_set_voltage(0);

    ESP_LOGI(TAG, "Klaar met afspelen.");
}

// ---------------- TASK ----------------
static void audio_task(void *pvParameters)
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        play_wav("tetrismusic.wav");
    }
}

// ---------------- INIT ----------------
void init_sound(void)
{
    ESP_LOGI(TAG, "Init sound...");

    init_spiffs();
    init_i2c();

    // 🔥 FIX 2: Pin de taak vast aan Core 1 in plaats van willekeurig!
    // Parameter 1 (helemaal achteraan) is de Core ID.
    xTaskCreatePinnedToCore(audio_task, "audio_task", 8192, NULL, 5, &audio_task_handle, 1);
}

// ---------------- TRIGGER ----------------
void trigger_sound(void)
{
    if (audio_task_handle != NULL)
    {
        xTaskNotifyGive(audio_task_handle);
    }
}

// ---------------- ISR ----------------
void trigger_sound_from_isr(void)
{
    if (audio_task_handle != NULL)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(audio_task_handle, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR();
        }
    }
}