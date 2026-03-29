//
// bt_mesh.h — BLE Mesh init & role management
//

#pragma once

#include <esp_err.h>
#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_health_model_api.h>
#include <esp_ble_mesh_config_model_api.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Initialize the Bluetooth controller + Bluedroid stack.
     * Call once before any mesh init.
     */
    esp_err_t bluetooth_init(void);

    /**
     * Initialize the mesh stack as an unprovisioned node.
     * Advertises and waits to be provisioned by a provisioner.
     */
    esp_err_t ble_mesh_init_node(void);

    /**
     * Upgrade a running node to provisioner role without reinitializing
     * the mesh stack. Safe to call after ble_mesh_init_node().
     *
     * Sets up the config client callback, enables provisioner scanning,
     * sets the UUID prefix filter, and installs the app key.
     */
    esp_err_t ble_mesh_upgrade_to_provisioner(void);

    /**
     * Returns true if this device has been provisioned onto a network.
     * Use this to decide whether to upgrade to provisioner.
     */
    bool ble_mesh_is_provisioned(void);

#ifdef __cplusplus
}
#endif