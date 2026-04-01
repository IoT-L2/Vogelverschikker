//
// Created by Christian ten Brinke on 01/04/2026.
//
#include "bt_handler.h"
#include <esp_log.h>
#include "sound.h"


#define TAG "BT_HANDLER"
extern "C" void on_bt_received(int32_t value)
{
    ESP_LOGI(TAG, "[vendor] handling value: %" PRId32, value);
    switch (value) {
        case 1: play_sound("tetrismusic.wav"); break;
        default: break;
    }
}
