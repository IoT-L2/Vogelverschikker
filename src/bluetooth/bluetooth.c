//
// Created by Christian ten Brinke on 25/03/2026.
//

#include "bluetooth.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_health_model_api.h"
#include "esp_gap_ble_api.h"

#include "../component/board.h"

#define TAG "BLE_MESH"
#define CID_ESP 0x02E5

/* ---------- UUID ---------- */
static uint8_t dev_uuid[16] = { 0xdd, 0xdd };

/* ---------- Provisioning ---------- */
static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid,
};


/* ---------- Health Server ---------- */
static const uint8_t test_ids[] = { 0x00 };

static const esp_ble_mesh_health_test_t health_test = {
    .id_count = 1,
    .test_ids = test_ids,
    .company_id = CID_ESP,
};

static esp_ble_mesh_health_srv_t health_srv = {
    .health_test = health_test,
};

ESP_BLE_MESH_HEALTH_PUB_DEFINE(health_pub, 1, 0);


/* ---------- Config Server ---------- */
static esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
    .default_ttl = 7,
};

/* ---------- Composition ---------- */
static  esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = 1,
    .elements = elements,
};

/* ---------- UUID helper ---------- */
void ble_mesh_get_dev_uuid(uint8_t *dev_uuid_out)
{
    if (!dev_uuid_out) return;
    memcpy(dev_uuid_out + 2, esp_bt_dev_get_address(), BD_ADDR_LEN);
}

/* ---------- Bluetooth init ---------- */
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

    ret = esp_bluedroid_enable();
    return ret;
}

/* ---------- Provisioning callback ---------- */
static void prov_cb(esp_ble_mesh_prov_cb_event_t event,
                    esp_ble_mesh_prov_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT) {
        ESP_LOGI(TAG, "Provisioning complete");
        board_led_operation(LED_G, LED_OFF);
    }
}

/* ---------- Mesh init ---------- */
esp_err_t ble_mesh_init(void)
{
    esp_ble_mesh_register_prov_callback(prov_cb);

    ble_mesh_get_dev_uuid(dev_uuid);
    esp_err_t err = esp_ble_mesh_init(&provision, &composition);
    if (err) {
        ESP_LOGE(TAG, "Mesh init failed (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_node_prov_enable(
        ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT
    );

    return err;
}