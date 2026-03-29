//
// Created by Christian ten Brinke on 29/03/2026.
//

#include "bt_mesh.h"
#include <string.h>
#include <inttypes.h>

#include <esp_bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_log.h>

#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_provisioning_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_config_model_api.h>
#include <esp_ble_mesh_health_model_api.h>

#include "component/board.h"

/* -------- Constants -------- */
#define TAG             "BLE_MESH"
#define CID_ESP         0x02E5
#define NET_KEY_IDX     0x0000
#define APP_KEY_IDX     0x0000
#define PROV_FLAGS      0x00
#define PROV_IV_INDEX   0x00

/* -------- UUID -------- */
static const uint8_t UUID_PREFIX[2] = {0xcd, 0xcd};
static uint8_t dev_uuid[16];

static void build_dev_uuid(void) {
    dev_uuid[0] = UUID_PREFIX[0];
    dev_uuid[1] = UUID_PREFIX[1];
    memcpy(dev_uuid + 2, esp_bt_dev_get_address(), BD_ADDR_LEN);
}

/* -------- Keys -------- */
static const uint8_t APP_KEY[16] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
    0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc,
    0xdd, 0xee, 0xff, 0x00
};

/* -------- Config server -------- */
static esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
    .default_ttl = 7,
};

/* -------- Config client --------*/
static esp_ble_mesh_client_t config_client;

/*-------- Health server -------- */
static const uint8_t health_test_ids[] = {0x00};
static const esp_ble_mesh_health_test_t health_test = {
    .id_count = 1,
    .test_ids = health_test_ids,
    .company_id = CID_ESP,
};
static esp_ble_mesh_health_srv_t health_srv = {
    .health_test = health_test,
};
ESP_BLE_MESH_HEALTH_PUB_DEFINE(health_pub, 1, 0);

/* -------- Models -------- */
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_CFG_CLI(&config_client),
    ESP_BLE_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = ARRAY_SIZE(elements),
    .elements = elements,
};

/* -------- Node Callbacks -------- */
static void node_prov_cb(esp_ble_mesh_prov_cb_event_t event,
                         esp_ble_mesh_prov_cb_param_t *param) {
    switch (event) {
        case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
            ESP_LOGI(TAG, "[node] mesh registered (err %d)",
                     param->prov_register_comp.err_code);
            break;

        case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
            ESP_LOGI(TAG, "[node] provisioning complete");
            board_led_operation(LED_G, LED_OFF);
            break;

        default:
            break;
    }
}

/* -------- Provisioner Callbacks -------- */
static void prov_send_app_key(uint16_t unicast_addr) {
    esp_ble_mesh_client_common_param_t common = {
        .opcode = ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD,
        .model = &root_models[1], /* CFG_CLI */
        .ctx.net_idx = NET_KEY_IDX,
        .ctx.app_idx = APP_KEY_IDX,
        .ctx.addr = unicast_addr,
        .ctx.send_ttl = 7,
        .msg_timeout = 4000,
        .msg_role = ROLE_PROVISIONER,
    };

    esp_ble_mesh_cfg_app_key_add_t add_key = {
        .net_idx = NET_KEY_IDX,
        .app_idx = APP_KEY_IDX,
    };
    memcpy(add_key.app_key, APP_KEY, sizeof(APP_KEY));

    esp_err_t err = esp_ble_mesh_config_client_set_state(
        &common, (esp_ble_mesh_cfg_client_set_state_t *) &add_key
    );
    if (err) {
        ESP_LOGE(TAG, "[prov] AppKey Add failed (err %d)", err);
    }
}

static void provisioner_prov_cb(esp_ble_mesh_prov_cb_event_t event,
                                esp_ble_mesh_prov_cb_param_t *param) {
    switch (event) {
        case ESP_BLE_MESH_PROVISIONER_PROV_ENABLE_COMP_EVT:
            ESP_LOGI(TAG, "[prov] provisioner enabled (err %d)",
                     param->provisioner_prov_enable_comp.err_code);
            break;

        case ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT: {
            const uint8_t *u = param->provisioner_recv_unprov_adv_pkt.dev_uuid;
            ESP_LOGI(TAG, "[prov] unprovisioned device: "
                     "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7],
                     u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);

            esp_ble_mesh_unprov_dev_add_t add_dev = {0};
            memcpy(add_dev.addr, param->provisioner_recv_unprov_adv_pkt.addr, BD_ADDR_LEN);
            add_dev.addr_type = param->provisioner_recv_unprov_adv_pkt.addr_type;
            memcpy(add_dev.uuid, u, ESP_BLE_MESH_OCTET16_LEN);
            add_dev.oob_info = param->provisioner_recv_unprov_adv_pkt.oob_info;
            add_dev.bearer = ESP_BLE_MESH_PROV_ADV;

            esp_err_t err = esp_ble_mesh_provisioner_add_unprov_dev(
                &add_dev,
                ADD_DEV_RM_AFTER_PROV_FLAG | ADD_DEV_START_PROV_NOW_FLAG
            );
            if (err) {
                ESP_LOGE(TAG, "[prov] add device failed (err %d)", err);
            }
            break;
        }

        case ESP_BLE_MESH_PROVISIONER_PROV_LINK_OPEN_EVT:
            ESP_LOGI(TAG, "[prov] link opened (bearer %d)",
                     param->provisioner_prov_link_open.bearer);
            break;

        case ESP_BLE_MESH_PROVISIONER_PROV_LINK_CLOSE_EVT:
            ESP_LOGI(TAG, "[prov] link closed (reason 0x%02x)",
                     param->provisioner_prov_link_close.reason);
            break;

        case ESP_BLE_MESH_PROVISIONER_PROV_COMPLETE_EVT:
            ESP_LOGI(TAG, "[prov] node provisioned - unicast 0x%04x",
                     param->provisioner_prov_complete.unicast_addr);
            board_led_operation(LED_G, LED_ON);
            prov_send_app_key(param->provisioner_prov_complete.unicast_addr);
            break;

        default:
            ESP_LOGD(TAG, "[prov] unhandled event %d", event);
            break;
    }
}

static void config_client_cb(esp_ble_mesh_cfg_client_cb_event_t event,
                             esp_ble_mesh_cfg_client_cb_param_t *param) {
    ESP_LOGI(TAG, "[prov] config client event %d opcode 0x%04" PRIx32,
             event, param->params->opcode);

    switch (event) {
        case ESP_BLE_MESH_CFG_CLIENT_SET_STATE_EVT:
            if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD) {
                ESP_LOGI(TAG, "[prov] app key bound to 0x%04x - node ready",
                         param->params->ctx.addr);
            }
            break;

        case ESP_BLE_MESH_CFG_CLIENT_TIMEOUT_EVT:
            ESP_LOGW(TAG, "[prov] config client timeout (opcode 0x%04" PRIx32 ")",
                     param->params->opcode);
            break;

        default:
            break;
    }
}

/* -------- Bluetooth -------- */
esp_err_t bluetooth_init(void) {
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


static esp_ble_mesh_prov_t prov = {
    .uuid = dev_uuid,
    .prov_unicast_addr = 0x0001,
    .prov_start_address = 0x0005,
    .prov_attention = 0,
    .prov_pub_key_oob = 0,
    .flags = PROV_FLAGS,
    .iv_index = PROV_IV_INDEX,
};

esp_err_t ble_mesh_init_node(void) {
    build_dev_uuid();

    esp_ble_mesh_register_prov_callback(node_prov_cb);

    esp_err_t err = esp_ble_mesh_init(&prov, &composition);
    if (err) {
        ESP_LOGE(TAG, "[node] mesh init failed (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_node_prov_enable(
        ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT
    );
    if (err) {
        ESP_LOGE(TAG, "[node] prov enable failed (err %d)", err);
        return err;
    }

    ESP_LOGI(TAG, "[node] initialized - waiting to be provisioned");
    return ESP_OK;
}

/* -------- Provision upgrade -------- */
esp_err_t ble_mesh_upgrade_to_provisioner(void) {
    esp_err_t err;

    err = esp_ble_mesh_node_prov_disable(
        ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT
    );
    if (err) {
        ESP_LOGE(TAG, "[prov] node disable failed (err %d)", err);
        return err;
    }

    esp_ble_mesh_register_prov_callback(provisioner_prov_cb);
    esp_ble_mesh_register_config_client_callback(config_client_cb);

    err = esp_ble_mesh_provisioner_prov_enable(
        ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT
    );
    if (err) {
        ESP_LOGE(TAG, "[prov] enable failed (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_provisioner_set_dev_uuid_match(
        UUID_PREFIX, sizeof(UUID_PREFIX), 0x00, false
    );
    if (err) {
        ESP_LOGE(TAG, "[prov] UUID match failed (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_provisioner_add_local_app_key(
        APP_KEY, NET_KEY_IDX, APP_KEY_IDX
    );
    if (err) {
        ESP_LOGE(TAG, "[prov] add app key failed (err %d)", err);
        return err;
    }

    ESP_LOGI(TAG, "[prov] upgraded - scanning for unprovisioned devices");
    return ESP_OK;
}

bool ble_mesh_is_provisioned(void) {
    return esp_ble_mesh_node_is_provisioned();
}
