//
// Created by Christian ten Brinke on 27/03/2026.
//

#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_config_model_api.h>
#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_provisioning_api.h>

#include "bt.h"
#include "component/board.h"


#define TAG "BLE_MESH_NODE"
#define CID_ESP         0x02E5

/* ---------- Provisioning ---------- */

static uint8_t dev_uuid_node[16];

static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid_node,
};

/* ---------- Provisioning callback ---------- */
static void prov_cb(esp_ble_mesh_prov_cb_event_t event,
                    esp_ble_mesh_prov_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT) {
        ESP_LOGI(TAG, "Provisioning complete");
        board_led_operation(LED_G, LED_OFF);
    }
}

/* ---------- Health Server ---------- */
static const uint8_t test_ids[] = { 0x00 };

static const esp_ble_mesh_health_test_t health_test = {
    .id_count = 1,
    .test_ids = test_ids,
    .company_id = CID_ESP,
};

esp_ble_mesh_health_srv_t health_srv_n = {
    .health_test = health_test,
};

ESP_BLE_MESH_HEALTH_PUB_DEFINE(health_pub, 1, 0);


/* Composition */
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_HEALTH_SRV(&health_srv_n, &health_pub),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = {
    .cid           = CID_ESP,
    .element_count = 1,
    .elements      = elements,
};



/* ---------- Init node ---------- */
esp_err_t init_node(
    const uint8_t *dev_uuid
) {

    memcpy(dev_uuid_node, dev_uuid, 16);
    esp_ble_mesh_register_prov_callback(prov_cb);
    esp_err_t err = esp_ble_mesh_init(&provision, &composition);
    if (err) {
        ESP_LOGE(TAG, "Mesh init failed (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_node_prov_enable(
       ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT
   );

    if (err) {
        ESP_LOGE(TAG, "Failed to enable provisioner (err %d)", err);
        return err;
    }


    return ESP_OK;
}