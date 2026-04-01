#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>

    /**
    * Determine what to do based on the value received.
    * @param value the value that's been received by  bluetooth
    */
    void on_bt_received(int32_t value);

    /**
     * initialize the BLE_MESH start config and sequence
     */
    void init_ble_mesh_config();
#ifdef __cplusplus
}
#endif