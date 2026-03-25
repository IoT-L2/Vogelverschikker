//
// Created by Christian ten Brinke on 25/03/2026.
//

#include "bluetooth.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_health_model_api.h"
#include "esp_gap_ble_api.h"

#include "board.h"

#define TAG "BLE_MESH"
#define CID_ESP 0x02E5

/* ---------- UUID ---------- */
static uint8_t dev_uuid[16] = { 0xdd, 0xdd };

/* ---------- Provisioning ---------- */
static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid,
};

/* ---------- Config Server ---------- */
static esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
    .default_ttl = 7,
};

/* ---------- OnOff Model ---------- */
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub, 2 + 3, ROLE_NODE);

static esp_ble_mesh_gen_onoff_srv_t onoff_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    },
};

/* ---------- Composition ---------- */
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub, &onoff_server),
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
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name("Christian+Daniel"));
    ESP_ERROR_CHECK(esp_ble_mesh_set_unprovisioned_device_name("Christian+Daniel"));
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

/* ---------- Generic server callback ---------- */
static void gen_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                          esp_ble_mesh_generic_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT) {
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {

            uint8_t onoff = param->value.state_change.onoff_set.onoff;

            esp_ble_mesh_model_publish(param->model,
                                       ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                                       sizeof(onoff),
                                       &onoff,
                                       ROLE_NODE);
        }
    }
}

/* ---------- Mesh init ---------- */
esp_err_t ble_mesh_init(void)
{
    esp_ble_mesh_register_prov_callback(prov_cb);
    esp_ble_mesh_register_generic_server_callback(gen_server_cb);

    esp_err_t err = esp_ble_mesh_init(&provision, &composition);
    if (err) return err;

    err = esp_ble_mesh_node_prov_enable(
    (esp_ble_mesh_prov_bearer_t)(
        ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT
    )
);

    return err;
}

