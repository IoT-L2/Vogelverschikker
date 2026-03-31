//
// bt_mesh.c - BLE Mesh: node init + provisioner upgrade + vendor broadcast model
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

#define TAG             "BLE_MESH"
#define CID_ESP         0x02E5
#define NET_KEY_IDX     0x0000
#define APP_KEY_IDX     0x0000
#define PROV_FLAGS      0x00
#define PROV_IV_INDEX   1
#define MAX_IV_UPDATE   5

/* -------- Vendor model opcodes -------- */
#define VENDOR_MODEL_ID         0x0001
#define OP_BROADCAST_SET        ESP_BLE_MESH_MODEL_OP_3(0x00, CID_ESP)

#define BROADCAST_GROUP_ADDR    0xC000

/* -------- Provisioning state machine -------- */
typedef enum {
    PROV_STEP_APP_KEY_ADD = 0,
    PROV_STEP_APP_KEY_BIND,
    PROV_STEP_PUB_SET,
    PROV_STEP_SUB_ADD,
    PROV_STEP_RELAY_SET,
    PROV_STEP_DONE,
} prov_step_t;

static uint16_t s_target_unicast = 0;
static uint16_t s_own_unicast_addr = 0;
static uint32_t s_iv_index = 1;
static bool s_network_formed = false;
static bool s_bearer_enabled = false;
static prov_step_t s_prov_step   = PROV_STEP_APP_KEY_ADD;

static const uint8_t NET_KEY[16] = {
    0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
    0x66, 0x77, 0x88, 0x99
};

static const uint8_t APP_KEY[16] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
    0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc,
    0xdd, 0xee, 0xff, 0x00
};

/* -------- UUID -------- */
static const uint8_t UUID_PREFIX[2] = { 0xcd, 0xcd };
static uint8_t dev_uuid[16];

static void build_dev_uuid(void)
{
    dev_uuid[0] = UUID_PREFIX[0];
    dev_uuid[1] = UUID_PREFIX[1];
    memcpy(dev_uuid + 2, esp_bt_dev_get_address(), BD_ADDR_LEN);
}


/* -------- Vendor model -------- */
static esp_ble_mesh_model_op_t vendor_ops[] = {
    ESP_BLE_MESH_MODEL_OP(OP_BROADCAST_SET, 4),
    ESP_BLE_MESH_MODEL_OP_END,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(vendor_pub, 7, ROLE_NODE);

/* -------- Models -------- */
static esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit     = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay            = ESP_BLE_MESH_RELAY_ENABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon           = ESP_BLE_MESH_BEACON_ENABLED,
    .default_ttl      = 7,
};

static esp_ble_mesh_client_t config_client;

static const uint8_t health_test_ids[] = { 0x00 };
static const esp_ble_mesh_health_test_t health_test = {
    .id_count   = 1,
    .test_ids   = health_test_ids,
    .company_id = CID_ESP,
};
static esp_ble_mesh_health_srv_t health_srv = { .health_test = health_test };
ESP_BLE_MESH_HEALTH_PUB_DEFINE(health_pub, 1, 0);

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_CFG_CLI(&config_client),
    ESP_BLE_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
};

static esp_ble_mesh_model_t vendor_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, VENDOR_MODEL_ID,
                              vendor_ops, &vendor_pub, NULL),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vendor_models),
};

static esp_ble_mesh_comp_t composition = {
    .cid           = CID_ESP,
    .element_count = ARRAY_SIZE(elements),
    .elements      = elements,
};

static esp_ble_mesh_prov_t prov = {
    .uuid               = dev_uuid,
    .prov_unicast_addr  = 0x0001,
    .prov_start_address = 0x0005,
    .prov_attention     = 0,
    .prov_pub_key_oob   = 0,
    .flags              = PROV_FLAGS,
    .iv_index           = PROV_IV_INDEX,
    .static_val         = NULL,
    .static_val_len     = 0,
};

/* -------- Helpers -------- */
static void fill_common(esp_ble_mesh_client_common_param_t *c, uint32_t opcode,
                        uint16_t addr)
{
    memset(c, 0, sizeof(*c));
    c->opcode       = opcode;
    c->model        = &root_models[1];
    c->ctx.net_idx  = NET_KEY_IDX;
    c->ctx.app_idx  = APP_KEY_IDX;
    c->ctx.addr     = addr;
    c->ctx.send_ttl = 7;
    c->msg_timeout  = 4000;
}

/* -------- Provisioning config chain -------- */
static void prov_send_app_key(uint16_t addr)
{
    ESP_LOGI(TAG, "[prov] >>> sending AppKey Add to 0x%04x", addr);
    esp_ble_mesh_client_common_param_t common;
    fill_common(&common, ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD, addr);

    esp_ble_mesh_cfg_app_key_add_t msg = {
        .net_idx = NET_KEY_IDX,
        .app_idx = APP_KEY_IDX,
    };
    memcpy(msg.app_key, APP_KEY, sizeof(APP_KEY));

    esp_err_t err = esp_ble_mesh_config_client_set_state(
        &common, (esp_ble_mesh_cfg_client_set_state_t *)&msg);
    if (err) ESP_LOGE(TAG, "[prov] AppKey Add failed (err %d)", err);
}

static void prov_send_app_key_bind(uint16_t addr)
{
    esp_ble_mesh_client_common_param_t common;
    fill_common(&common, ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND, addr);

    esp_ble_mesh_cfg_model_app_bind_t msg = {
        .element_addr  = addr,
        .model_app_idx = APP_KEY_IDX,
        .model_id      = VENDOR_MODEL_ID,
        .company_id    = CID_ESP,
    };

    esp_err_t err = esp_ble_mesh_config_client_set_state(
        &common, (esp_ble_mesh_cfg_client_set_state_t *)&msg);
    if (err) ESP_LOGE(TAG, "[prov] App Key Bind failed (err %d)", err);
}

static void prov_send_pub_set(uint16_t addr)
{
    esp_ble_mesh_client_common_param_t common;
    fill_common(&common, ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET, addr);

    esp_ble_mesh_cfg_model_pub_set_t msg = {
        .element_addr       = addr,
        .publish_addr       = BROADCAST_GROUP_ADDR,
        .publish_app_idx    = APP_KEY_IDX,
        .cred_flag          = false,
        .publish_ttl        = 7,
        .publish_period     = 0,
        .publish_retransmit = ESP_BLE_MESH_TRANSMIT(0, 0),
        .model_id           = VENDOR_MODEL_ID,
        .company_id         = CID_ESP,
    };

    esp_err_t err = esp_ble_mesh_config_client_set_state(
        &common, (esp_ble_mesh_cfg_client_set_state_t *)&msg);
    if (err) ESP_LOGE(TAG, "[prov] Pub Set failed (err %d)", err);
}

static void prov_send_sub_add(uint16_t addr)
{
    esp_ble_mesh_client_common_param_t common;
    fill_common(&common, ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD, addr);

    esp_ble_mesh_cfg_model_sub_add_t msg = {
        .element_addr = addr,
        .sub_addr     = BROADCAST_GROUP_ADDR,
        .model_id     = VENDOR_MODEL_ID,
        .company_id   = CID_ESP,
    };

    esp_err_t err = esp_ble_mesh_config_client_set_state(
        &common, (esp_ble_mesh_cfg_client_set_state_t *)&msg);
    if (err) ESP_LOGE(TAG, "[prov] Sub Add failed (err %d)", err);
}

static void prov_send_relay_set(uint16_t addr)
{
    esp_ble_mesh_client_common_param_t common;
    fill_common(&common, ESP_BLE_MESH_MODEL_OP_RELAY_SET, addr);

    esp_ble_mesh_cfg_relay_set_t msg = {
        .relay            = ESP_BLE_MESH_RELAY_ENABLED,
        .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    };

    esp_err_t err = esp_ble_mesh_config_client_set_state(
        &common, (esp_ble_mesh_cfg_client_set_state_t *)&msg);
    if (err) ESP_LOGE(TAG, "[prov] Relay Set failed (err %d)", err);
}

/* -------- Node callbacks -------- */
static void node_prov_cb(esp_ble_mesh_prov_cb_event_t event,
                          esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "[node] mesh registered (err %d)",
                 param->prov_register_comp.err_code);
        break;

    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        s_own_unicast_addr = param->node_prov_complete.addr;
        s_iv_index = param->node_prov_complete.iv_index;
        ESP_LOGI(TAG, "[node] provisioned - unicast 0x%04x, iv_index %lu",
                 param->node_prov_complete.addr, s_iv_index);
        ESP_LOGI(TAG, "[node] mesh network formed");
        s_network_formed = true;
        board_led_operation(LED_G, LED_OFF);
        break;

    default:
        break;
    }
}

/* -------- Vendor model receive callback -------- */
static void vendor_model_cb(esp_ble_mesh_model_cb_event_t event,
                            esp_ble_mesh_model_cb_param_t *param)
{
    if (event != ESP_BLE_MESH_MODEL_OPERATION_EVT) return;
    if (param->model_operation.opcode != OP_BROADCAST_SET) return;
    if (s_own_unicast_addr != 0 &&
        param->model_operation.ctx->addr == s_own_unicast_addr) return;
    if (param->model_operation.length < sizeof(int32_t)) return;

    int32_t value;
    memcpy(&value, param->model_operation.msg, sizeof(value));

    ESP_LOGI(TAG, "[vendor] broadcast received: %" PRId32
             " (from 0x%04x)", value, param->model_operation.ctx->addr);

    static bool s_node_ready_signalled = false;
    if (!s_node_ready_signalled) {
        s_node_ready_signalled = true;
        ble_mesh_on_node_configured();
    }

    /* ---- application logic here ---- */
}

/* -------- Provisioner callbacks -------- */
static void provisioner_prov_cb(esp_ble_mesh_prov_cb_event_t event,
                                esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROVISIONER_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(TAG, "[prov] provisioner enabled (err %d)",
                 param->provisioner_prov_enable_comp.err_code);
        s_bearer_enabled = true;
        break;

    case ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT: {
        const uint8_t *u = param->provisioner_recv_unprov_adv_pkt.dev_uuid;
        ESP_LOGI(TAG, "[prov] unprovisioned: "
                 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 u[0],u[1],u[2],u[3], u[4],u[5], u[6],u[7],
                 u[8],u[9], u[10],u[11],u[12],u[13],u[14],u[15]);

        esp_ble_mesh_unprov_dev_add_t add = { 0 };
        memcpy(add.addr, param->provisioner_recv_unprov_adv_pkt.addr, BD_ADDR_LEN);
        add.addr_type = param->provisioner_recv_unprov_adv_pkt.addr_type;
        memcpy(add.uuid, u, ESP_BLE_MESH_OCTET16_LEN);
        add.oob_info  = param->provisioner_recv_unprov_adv_pkt.oob_info;
        add.bearer    = ESP_BLE_MESH_PROV_ADV;

        esp_err_t err = esp_ble_mesh_provisioner_add_unprov_dev(
            &add, ADD_DEV_RM_AFTER_PROV_FLAG | ADD_DEV_START_PROV_NOW_FLAG);
        if (err) ESP_LOGE(TAG, "[prov] add device failed (err %d)", err);
        break;
    }

    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "[prov] link opened (bearer %d)",
                 param->provisioner_prov_link_open.bearer);
        s_bearer_enabled = true;
        break;

    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "[prov] link closed (reason 0x%02x)",
                 param->provisioner_prov_link_close.reason);
        break;

    case ESP_BLE_MESH_PROVISIONER_PROV_COMPLETE_EVT:
        s_target_unicast = param->provisioner_prov_complete.unicast_addr;
        s_prov_step      = PROV_STEP_APP_KEY_ADD;
        ESP_LOGI(TAG, "[prov] node provisioned - unicast 0x%04x, starting config",
                 s_target_unicast);
        board_led_operation(LED_G, LED_ON);
        prov_send_app_key(s_target_unicast);
        s_network_formed = true;
        break;

    default:
        ESP_LOGD(TAG, "[prov] unhandled event %d", event);
        break;
    }
}

static void config_client_cb(esp_ble_mesh_cfg_client_cb_event_t event,
                             esp_ble_mesh_cfg_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "[prov] config client event %d opcode 0x%04" PRIx32,
             event, param->params->opcode);

    switch (event) {
    case ESP_BLE_MESH_CFG_CLIENT_SET_STATE_EVT:
        switch (param->params->opcode) {

        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            ESP_LOGI(TAG, "[prov] AppKey added to 0x%04x -> binding model",
                     param->params->ctx.addr);
            s_prov_step = PROV_STEP_APP_KEY_BIND;
            prov_send_app_key_bind(s_target_unicast);
            break;

        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(TAG, "[prov] App key bound on 0x%04x -> setting publication",
                     param->params->ctx.addr);
            s_prov_step = PROV_STEP_PUB_SET;
            prov_send_pub_set(s_target_unicast);
            break;

        case ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET:
            ESP_LOGI(TAG, "[prov] Publication set on 0x%04x -> adding subscription",
                     param->params->ctx.addr);
            s_prov_step = PROV_STEP_SUB_ADD;
            prov_send_sub_add(s_target_unicast);
            break;

        case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
            ESP_LOGI(TAG, "[prov] Subscription added on 0x%04x -> enabling relay",
                     param->params->ctx.addr);
            s_prov_step = PROV_STEP_RELAY_SET;
            prov_send_relay_set(s_target_unicast);
            break;

        case ESP_BLE_MESH_MODEL_OP_RELAY_SET:
            ESP_LOGI(TAG, "[prov] Relay enabled on 0x%04x - node fully configured",
                     param->params->ctx.addr);
            s_prov_step = PROV_STEP_DONE;
            break;

        default:
            break;
        }
        break;

    case ESP_BLE_MESH_CFG_CLIENT_TIMEOUT_EVT:
        ESP_LOGW(TAG, "[prov] config timeout (opcode 0x%04" PRIx32
                 ") step %d - retrying", param->params->opcode, s_prov_step);
        switch (s_prov_step) {
        case PROV_STEP_APP_KEY_ADD:  prov_send_app_key(s_target_unicast);      break;
        case PROV_STEP_APP_KEY_BIND: prov_send_app_key_bind(s_target_unicast); break;
        case PROV_STEP_PUB_SET:      prov_send_pub_set(s_target_unicast);      break;
        case PROV_STEP_SUB_ADD:      prov_send_sub_add(s_target_unicast);      break;
        case PROV_STEP_RELAY_SET:    prov_send_relay_set(s_target_unicast);    break;
        default: break;
        }
        break;

    default:
        break;
    }
}

/* -------- Public API -------- */
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

esp_err_t ble_mesh_init_node(void)
{
    build_dev_uuid();

    esp_ble_mesh_register_prov_callback(node_prov_cb);
    esp_ble_mesh_register_custom_model_callback(vendor_model_cb);

    esp_err_t err = esp_ble_mesh_init(&prov, &composition);
    if (err) {
        ESP_LOGE(TAG, "[node] mesh init failed (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_node_prov_enable(
        ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err) {
        ESP_LOGE(TAG, "[node] prov enable failed (err %d)", err);
        return err;
    }

    s_network_formed = true;
    ESP_LOGI(TAG, "[node] initialized - waiting to be provisioned");
    return ESP_OK;
}

esp_err_t ble_mesh_upgrade_to_provisioner(void)
{
    esp_err_t err;

    s_own_unicast_addr = prov.prov_unicast_addr;

    if (esp_ble_mesh_node_is_provisioned()) {
        ESP_LOGI(TAG, "[prov] resetting provisioned node state");
        err = esp_ble_mesh_node_local_reset();
        if (err) {
            ESP_LOGE(TAG, "[prov] local reset failed (err %d)", err);
            return err;
        }
    } else {
        err = esp_ble_mesh_node_prov_disable(
            ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
        if (err) {
            ESP_LOGE(TAG, "[prov] node disable failed (err %d)", err);
            return err;
        }
    }

    esp_ble_mesh_register_prov_callback(provisioner_prov_cb);
    esp_ble_mesh_register_config_client_callback(config_client_cb);
    esp_ble_mesh_register_custom_model_callback(vendor_model_cb);

    err = esp_ble_mesh_provisioner_prov_enable(
        ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err) {
        ESP_LOGE(TAG, "[prov] enable failed (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_provisioner_set_dev_uuid_match(
        UUID_PREFIX, sizeof(UUID_PREFIX), 0x00, false);
    if (err) {
        ESP_LOGE(TAG, "[prov] UUID match failed (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_provisioner_add_local_net_key(NET_KEY, NET_KEY_IDX);
    if (err) {
        ESP_LOGW(TAG, "[prov] add net key: %d (may already exist)", err);
    }

    err = esp_ble_mesh_provisioner_add_local_app_key(APP_KEY, NET_KEY_IDX, APP_KEY_IDX);
    if (err) {
        ESP_LOGW(TAG, "[prov] add app key: %d (may already exist)", err);
    }

    err = esp_ble_mesh_provisioner_bind_app_key_to_local_model(
        elements[0].element_addr, APP_KEY_IDX,
        VENDOR_MODEL_ID, CID_ESP);
    if (err) {
        ESP_LOGW(TAG, "[prov] local model bind: %d", err);
    }

    esp_ble_mesh_model_t *model = &vendor_models[0];
    bool subscribed = false;
    for (int i = 0; i < CONFIG_BLE_MESH_MODEL_GROUP_COUNT; i++) {
        if (model->groups[i] == ESP_BLE_MESH_ADDR_UNASSIGNED) {
            model->groups[i] = BROADCAST_GROUP_ADDR;
            ESP_LOGI(TAG, "[prov] subscribed local vendor model to 0x%04x",
                     BROADCAST_GROUP_ADDR);
            subscribed = true;
            break;
        }
    }
    if (!subscribed) {
        ESP_LOGW(TAG, "[prov] no free group slots - increase CONFIG_BLE_MESH_MODEL_GROUP_COUNT");
    }

    model->pub->publish_addr = BROADCAST_GROUP_ADDR;
    model->pub->app_idx      = APP_KEY_IDX;
    model->pub->ttl          = 7;

    ESP_LOGI(TAG, "[prov] upgraded - scanning for unprovisioned devices");
    return ESP_OK;
}

bool ble_mesh_is_provisioned(void)
{
    return esp_ble_mesh_node_is_provisioned();
}

esp_err_t ble_mesh_broadcast_int(int32_t value)
{
    esp_ble_mesh_model_t *model = &vendor_models[0];

    if (model->pub == NULL) {
        ESP_LOGE(TAG, "[vendor] broadcast failed - no publish set");
        return ESP_ERR_INVALID_ARG;
    }

    model->pub->publish_addr = BROADCAST_GROUP_ADDR;
    model->pub->app_idx      = APP_KEY_IDX;
    model->pub->ttl          = 7;

    uint8_t buf[sizeof(int32_t)];
    memcpy(buf, &value, sizeof(buf));

    esp_err_t err = esp_ble_mesh_model_publish(
        model, OP_BROADCAST_SET, sizeof(buf), buf, ROLE_NODE);
    if (err) {
        ESP_LOGE(TAG, "[vendor] broadcast failed (err %d)", err);
    } else {
        ESP_LOGI(TAG, "[vendor] broadcast sent: %" PRId32, value);
    }
    return err;
}

void ble_mesh_get_unicast_addr(uint16_t *dst_addr)
{
    if (dst_addr != NULL) {
        *dst_addr = s_own_unicast_addr;
    }
}

void ble_mesh_get_iv_index(uint32_t *dst_index)
{
    if (dst_index != NULL) {
        *dst_index = s_iv_index;
    }
}
