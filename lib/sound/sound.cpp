#include "sound.h" // pio run -t uploadfs upload het wav bestand naar internal memory important!
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "esp_spiffs.h" // Aangepast naar de juiste header!

static const char *TAG = "AUDIO";

// Jouw I2C pinnen
#define I2C_MASTER_SDA_IO 4
#define I2C_MASTER_SCL_IO 5
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 800000
#define MCP4725_ADDR 0x60 // Verander dit naar 0x60 als je niks hoort!

#define SAMPLE_RATE 8000
#define SAMPLE_PERIOD_US (1000000 / SAMPLE_RATE)
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
    // Fast Write Mode:
    // Byte 1: [0][0][D11][D10][D9][D8][D7][D6]
    // Byte 2: [D5][D4][D3][D2][D1][D0][x][x]

    uint8_t msb = (value >> 8) & 0x0F; // Bovenste 4 bits van je 12-bit waarde
    uint8_t lsb = value & 0xFF;        // Onderste 8 bits van je 12-bit waarde

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP4725_ADDR << 1) | I2C_MASTER_WRITE, true);

    // Alleen de 2 data-bytes sturen (Fast mode), geen command-byte!
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

    fseek(f, WAV_HEADER_SIZE, SEEK_SET);

    uint8_t buffer[AUDIO_BUFFER_SIZE];
    size_t bytes_read;

    ESP_LOGI(TAG, "Geluid aan het afspelen via MCP4725...");

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0)
    {
        for (size_t i = 0; i < bytes_read; i++)
        {
            int64_t start_time = esp_timer_get_time();

            // Zet 8-bit om naar 12-bit voor de DAC
            uint16_t sample_12bit = buffer[i] << 4;

            // Stuur het naar de DAC (nu 33% sneller!)
            mcp4725_set_voltage(sample_12bit);

            // Wacht de resterende tijd tot de volgende sample (125us)
            int64_t elapsed = esp_timer_get_time() - start_time;
            if (elapsed < SAMPLE_PERIOD_US)
            {
                esp_rom_delay_us(SAMPLE_PERIOD_US - elapsed);
            }
        }
    }

    fclose(f);
    mcp4725_set_voltage(0);
    ESP_LOGI(TAG, "Klaar met afspelen.");
}

static void audio_task(void *pvParameters)
{
    while (1)
    {
        // Taak slaapt totdat hij normaal wordt aangeroepen
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        play_wav("sound.wav");
    }
}

void init_sound(void)
{
    ESP_LOGI(TAG, "SPIFFS en I2C initialiseren...");
    init_spiffs();
    init_i2c();

    // Normale task creatie,
    xTaskCreate(audio_task, "audio_task", 4096, NULL, 5, &audio_task_handle);
}

void trigger_sound(void)
{
    if (audio_task_handle != NULL)
    {
        // Simpel signaal naar de taak (geen interrupt versie meer)
        xTaskNotifyGive(audio_task_handle);
    }
}

void trigger_sound_from_isr(void)
{
    if (audio_task_handle != NULL)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        // Gebruik de speciale FromISR functie!
        vTaskNotifyGiveFromISR(audio_task_handle, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR();
        }
    }
}