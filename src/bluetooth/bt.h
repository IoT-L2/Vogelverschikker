//
// Created by Christian ten Brinke on 29/03/2026.
//

#ifndef VOGELVERSCHIKKER_BT_H
#define VOGELVERSCHIKKER_BT_H
#include <esp_ble_mesh_config_model_api.h>
#include <esp_ble_mesh_health_model_api.h>
extern  esp_ble_mesh_cfg_srv_t config_server;

/* ---------- Role ---------- */
enum BLE_ROLE {
    PROV,
    NODE
};

esp_err_t bluetooth_init(void);
esp_err_t ble_mesh_init( enum BLE_ROLE role );

#endif //VOGELVERSCHIKKER_BT_H