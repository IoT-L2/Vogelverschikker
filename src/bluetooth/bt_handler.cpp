#include "bt_handler.h"

#include "blepacket.h"
#include "bt_mesh.h"
#include "sound.h"

#include "esp_log.h"
#include "main.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define TAG          "BT_HANDLER"
#define PROV_WAIT_MS 10000

bool is_provsioner = false;

void handle_play_sound() {
    char filename[64];
    if (audio_cfg.getLine(0, filename, sizeof(filename))) {
        ESP_LOGI(TAG, "Audio file: %s", filename);
    } else {
        //default
        strncpy(filename, "default.mp3", sizeof(filename));
    }
    play_sound(filename);
}

void send_data_packet() {
    char count[16];
    int number = 0;
    if (detections.getLine(0, count, sizeof(count))) {
        number = atoi(count);
    } else {
        number = 0;
        detections.setLine(0, "0");
    }
    ble_mesh_broadcast_int(ble_data_packet(get_uuid()[7],number, sound_get_current()));
}

void handle_data_recieved(uint8_t node_id, uint16_t total, SoundIndex sound) {
    if (!is_provsioner) return;
    uint8_t node_mac = node_id;
    uint16_t total_from_node = total;
    //TODO: time = get current time
    SoundIndex sound_from_node = sound;
    //TODO: send to api
    ESP_LOGI(TAG, "Received data from node %i , %i , %i", node_id,total_from_node,sound_from_node);
}
extern "C" void on_bt_received(int32_t value)
{
    BlePacket p = blepacket_unpack(value);
    ESP_LOGI(TAG, "[vendor] received value: %" PRId32, value);
    switch (p.code) {
        case BLE_CODE_PLAY_SOUND:
            handle_play_sound();
            break;
        case BLE_CODE_CHANGE_SOUND:
            sound_switch((SoundIndex)p.arg1);
            break;
        case BLE_CODE_DATA: {
            handle_data_recieved(p.arg1, (uint16_t)p.arg3, (SoundIndex)p.arg2);
            break;
        }
    }
}

static SemaphoreHandle_t s_node_ready;

void ble_mesh_on_node_configured(void)
{
    xSemaphoreGive(s_node_ready);
}

static void node_loop()
{
    while (true) {
        // any periodic provisioner/node work here.
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void init_ble_mesh_config()
{
    s_node_ready = xSemaphoreCreateBinary();
    configASSERT(s_node_ready);

    ESP_ERROR_CHECK(bluetooth_init());
    ESP_ERROR_CHECK(ble_mesh_init_node());

    ESP_LOGI(TAG, "Waiting %d ms to be provisioned ...", PROV_WAIT_MS);
    vTaskDelay(pdMS_TO_TICKS(PROV_WAIT_MS));

    if (!ble_mesh_is_provisioned()) {
        ESP_LOGI(TAG, "Not provisioned — becoming provisioner");
        ESP_ERROR_CHECK(ble_mesh_upgrade_to_provisioner());
        //can add a while loop here?
        is_provsioner = true;
    } else {
        ESP_LOGI(TAG, "Provisioned — waiting for full config from provisioner ...");
        xSemaphoreTake(s_node_ready, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Node fully configured — starting node loop");
        node_loop(); // loops forever;
    }
}