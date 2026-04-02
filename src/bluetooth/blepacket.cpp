#include "blepacket.h"

int32_t blepacket_pack(BlePacket p)
{
    return ((int32_t)p.code << 24) |
           ((int32_t)p.arg1 << 16) |
           ((int32_t)p.arg2 << 8)  |
           ((int32_t)p.arg3);
}

BlePacket blepacket_unpack(int32_t value)
{
    BlePacket p;
    p.code = (value >> 24) & 0xFF;
    p.arg1 = (value >> 16) & 0xFF;
    p.arg2 = (value >> 8)  & 0xFF;
    p.arg3 = (value)       & 0xFF;
    return p;
}

int32_t ble_play_sound_packet()
{
    return blepacket_pack({BLE_CODE_PLAY_SOUND, 0, 0, 0});
}

int32_t ble_change_sound_packet(SoundIndex sound)
{
    return blepacket_pack({BLE_CODE_CHANGE_SOUND, (uint8_t)sound, 0, 0});
}

int32_t ble_data_packet(uint8_t node_id, uint16_t total, SoundIndex sound)
{
    return blepacket_pack({BLE_CODE_DATA, node_id, (uint8_t)sound, (uint8_t)total});
}