//
// Created by Christian ten Brinke on 25/03/2026.
//

#ifndef VOGELVERSCHIKKER_BLUETOOTH_H
#define VOGELVERSCHIKKER_BLUETOOTH_H
#include <esp_err.h>

esp_err_t bluetooth_init(void);
esp_err_t ble_mesh_init(void);

#endif //VOGELVERSCHIKKER_BLUETOOTH_H