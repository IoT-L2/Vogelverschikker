//
// Created by Christian ten Brinke on 27/03/2026.
//

#ifndef VOGELVERSCHIKKER_BT_PROVISIONER_H
#define VOGELVERSCHIKKER_BT_PROVISIONER_H
#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_health_model_api.h>

esp_err_t init_prov(const uint8_t *dev_uuid);
#endif //VOGELVERSCHIKKER_BT_PROVISIONER_H