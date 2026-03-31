#include "sound.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "esp_spiffs.h"
#include "esp_task_wdt.h" // 🔥 Toegevoegd voor de watchdog

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

// 🔥 tuning (pas aan indien nodig)
#define SAMPLE_DELAY_US 1

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
    uint8_t msb = (value >> 8) & 0x0F;
    uint8_t lsb = value & 0xFF;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP4725_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, msb, true);
    i2c_master_write_byte(cmd, lsb, true);
    i2c_master_stop(cmd);

    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, portMAX_DELAY);

    i2c_cmd_link_delete(cmd);
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
            ets_delay_us(SAMPLE_DELAY_US);
        }

        // 🔥 Vertel de watchdog dat we nog in leven zijn
        esp_task_wdt_reset();

        // Korte adempauze voor andere processen
        vTaskDelay(1);
    }

    fclose(f);
    mcp4725_set_voltage(0);

    ESP_LOGI(TAG, "Klaar met afspelen.");
}

// ---------------- TASK ----------------
static void audio_task(void *pvParameters)
{
    // 🔥 Abonneer deze specifieke taak op de Task Watchdog
    esp_task_wdt_add(NULL);

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        play_wav("tetrismusic.wav");

        // Ook even resetten als we wachten op de volgende notificatie
        esp_task_wdt_reset();
    }
}

// ---------------- INIT ----------------
void init_sound(void)
{
    ESP_LOGI(TAG, "Init sound...");

    init_spiffs();
    init_i2c();

    xTaskCreate(audio_task, "audio_task", 8192, NULL, 5, &audio_task_handle);
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