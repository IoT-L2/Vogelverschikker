#include "sound.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "esp_spiffs.h"

static const char *TAG = "AUDIO_MCP4725";

// Pas deze pinnen aan naar hoe jij de MCP4725 hebt aangesloten op je S3
#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000 // 400kHz Fast Mode
#define MCP4725_ADDR 0x62

#define SAMPLE_RATE 8000
#define SAMPLE_PERIOD_US (1000000 / SAMPLE_RATE) // 125 microseconden per sample
#define WAV_HEADER_SIZE 44
#define AUDIO_BUFFER_SIZE 512

static TaskHandle_t audio_task_handle = NULL;

static void init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};
    esp_vfs_spiffs_register(&conf);
}

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

static inline void mcp4725_set_voltage(uint16_t value)
{
    uint8_t msb = (value >> 8) & 0x0F;
    uint8_t lsb = value & 0xFF;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP4725_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, msb, true);
    i2c_master_write_byte(cmd, lsb, true);
    i2c_master_stop(cmd);

    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
}

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

    // Sla de 44-byte WAV header over
    fseek(f, WAV_HEADER_SIZE, SEEK_SET);

    uint8_t buffer[AUDIO_BUFFER_SIZE];
    size_t bytes_read;
    int64_t next_sample_time = esp_timer_get_time();

    ESP_LOGI(TAG, "Geluid aan het afspelen via MCP4725...");

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0)
    {
        for (size_t i = 0; i < bytes_read; i++)
        {
            // Converteer 8-bit data naar 12-bit data voor de DAC
            uint16_t sample_12bit = buffer[i] << 4;

            mcp4725_set_voltage(sample_12bit);

            // Wacht exact lang genoeg voor 8000 Hz timing
            next_sample_time += SAMPLE_PERIOD_US;
            int64_t wait_time = next_sample_time - esp_timer_get_time();
            if (wait_time > 0)
            {
                esp_rom_delay_us(wait_time);
            }
        }
    }

    fclose(f);
    mcp4725_set_voltage(0); // Trek de speaker naar de nul-lijn om ruis te voorkomen
    ESP_LOGI(TAG, "Klaar met afspelen.");
}

// Achtergrondtaak die wacht op een seintje
static void audio_task(void *pvParameters)
{
    while (1)
    {
        // Taak slaapt totdat de interrupt hem wakker maakt (0% CPU gebruik)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        play_wav("bird.wav");
    }
}

void init_sound(void)
{
    ESP_LOGI(TAG, "SPIFFS en I2C initialiseren...");
    init_spiffs();
    init_i2c();

    // BELANGRIJK: We binden de audiotaak aan Core 1.
    // BLE Mesh draait standaard op Core 0, dus ze storen elkaar niet.
    xTaskCreatePinnedToCore(audio_task, "audio_task", 4096, NULL, configMAX_PRIORITIES - 1, &audio_task_handle, 1);
}

void trigger_sound_from_isr(void)
{
    if (audio_task_handle != NULL)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(audio_task_handle, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken == pdTRUE)
        {
            portYIELD_FROM_ISR();
        }
    }
}