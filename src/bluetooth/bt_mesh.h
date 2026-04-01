#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//
// bt_mesh.h - BLE Mesh public API
//

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Initialize the Bluetooth controller and Bluedroid stack.
 * Call once before any BLE Mesh API.
 */
esp_err_t bluetooth_init(void);

/**
 * Initialize as a mesh node and start advertising for provisioning.
 */
esp_err_t ble_mesh_init_node(void);

/**
 * Upgrade from node to provisioner role.
 * If the device was already provisioned it resets local state first.
 * Begins scanning for unprovisioned devices with the matching UUID prefix
 * and runs the full config sequence on each one automatically.
 */
esp_err_t ble_mesh_upgrade_to_provisioner(void);

/**
 * Returns true if this device has been provisioned into a mesh network.
 */
bool ble_mesh_is_provisioned(void);

/**
 * Broadcast an int32_t to all nodes subscribed to the mesh group address.
 * Works from both node and provisioner roles once the device is configured.
 */
esp_err_t ble_mesh_broadcast_int(int32_t value);


__attribute__((weak)) void ble_mesh_on_node_configured(void);

#ifdef __cplusplus
}
#endif