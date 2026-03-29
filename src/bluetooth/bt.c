//
// Created by Christian ten Brinke on 29/03/2026.
//

#include "bt.h"

#include <esp_ble_mesh_config_model_api.h>
#include <esp_ble_mesh_health_model_api.h>
#include <esp_bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <string.h>
#include <inttypes.h>

#include "bt_provisioner.h"
#include "bt_node.h"

#include "esp_ble_mesh_defs.h"

#define CID_ESP         0x02E5



/* ---------- UUID ---------- */
static uint8_t dev_uuid[16] = { 0xcd, 0xcd };

void ble_mesh_get_dev_uuid(uint8_t *dev_uuid_out)
{
    if (!dev_uuid_out) return;
    memcpy(dev_uuid_out + 2, esp_bt_dev_get_address(), BD_ADDR_LEN);
}

/* ---------- Config Server ---------- */
 esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
    .default_ttl = 7,
};

/* ---------- Bluetooth init ----------------------------------------------- */
esp_err_t bluetooth_init(void)
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret) return ret;

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) return ret;

    esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&cfg);
    if (ret) return ret;

    return esp_bluedroid_enable();
}

/* ---------- Mesh init ---------------------------------------------------- */
esp_err_t ble_mesh_init( enum BLE_ROLE role ) {

    ble_mesh_get_dev_uuid(dev_uuid);
    switch (role) {
        case PROV: return init_prov(dev_uuid);
        case NODE:return init_node(dev_uuid);
    }

    return ESP_OK;

}

