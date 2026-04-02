#pragma once
#include <stdint.h>
#include "sound/soundlist.h"

// ── Codes ─────────────────────────────────────────────
#define BLE_CODE_PLAY_SOUND    0x01  // arg1=SoundIndex
#define BLE_CODE_CHANGE_SOUND  0x02  // arg1=SoundIndex
#define BLE_CODE_DATA          0x03  // arg1=node_id, arg2+arg3=total
#define BLE_CODE_ACK           0x04  // arg1=node_id

// ── Struct ─────────────────────────────────────────────
typedef struct {
    uint8_t code;
    uint8_t arg1;
    uint8_t arg2;
    uint8_t arg3;
} BlePacket;

int32_t  blepacket_pack(BlePacket p);
BlePacket blepacket_unpack(int32_t value);

int32_t ble_play_sound_packet();
int32_t ble_change_sound_packet(SoundIndex sound);
int32_t ble_data_packet(uint8_t node_id, uint16_t total, SoundIndex sound);