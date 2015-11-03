/*
 * Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is confidential property of Nordic Semiconductor. The use,
 * copying, transfer or disclosure of such information is prohibited except by express written
 * agreement with Nordic Semiconductor.
 *
 */

/**@file
 *
 * @defgroup ble_sdk_srv_uart_c   Nordic UART Service (NUS) Client
 * @{
 * @ingroup  ble_sdk_srv
 * @brief    Nordic UART Service (NUS) Client module.
 *
 * @details  This module contains the APIs and types exposed by the Nordic UART Service (NUS) Client
 *           module. These APIs and types can be used by the application to perform discovery of
 *           Nordic UART Service (NUS) at the peer and interact with it.
 *
 * @note     The application must propagate BLE stack events to this module by calling
 *           ble_uart_c_on_ble_evt().
 *
 */

#ifndef BLE_UART_C_H__
#define BLE_UART_C_H__
#define BLE_UUID_NUS_SERVICE            0x0001                       /**< The UUID of the Nordic UART Service. */
#define BLE_UUID_NUS_TX_CHARACTERISTIC  0x0002                       /**< The UUID of the TX Characteristic. */
#define BLE_UUID_NUS_RX_CHARACTERISTIC  0x0003                       /**< The UUID of the RX Characteristic. */

#define BLE_NUS_MAX_DATA_LEN (GATT_MTU_SIZE_DEFAULT - 3) /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */

#include <stdint.h>
#include "ble.h"

/**
 * @defgroup uart_c_enums Enumerations
 * @{
 */

/**@brief uart Client event type. */
typedef enum
{
    BLE_UART_C_EVT_DISCOVERY_COMPLETE = 1,  /**< Event indicating that the Nordic UART Service (NUS) has been discovered at the peer. */
    BLE_UART_C_EVT_RX_DATA_NOTIFICATION     /**< Event indicating that a notification of the NUS RX data characteristic has been received from the peer. */
} ble_uart_c_evt_type_t;

/** @} */

/**
 * @defgroup uart_c_structs Structures
 * @{
 */

/**@brief Structure containing the NUS RX data received from the peer. */
typedef struct
{
    uint8_t rx_data[20];  /**< RX Value. */
    uint8_t len; 
} ble_uart_t;

/**@brief NUS Event structure. */
typedef struct
{
    ble_uart_c_evt_type_t evt_type;  /**< Type of the event. */
   union
	 {
		 
			ble_uart_t 						uart;  /**< UART measurement received. This will be filled if the evt_type is @ref BLE_UART_C_EVT_HRM_NOTIFICATION. */
   } params;
} ble_uart_c_evt_t;

/** @} */

/**
 * @defgroup uart_c_types Types
 * @{
 */

// Forward declaration of the ble_bas_t type.
typedef struct ble_uart_c_s ble_uart_c_t;

/**@brief   Event handler type.
 *
 * @details This is the type of the event handler that should be provided by the application
 *          of this module in order to receive events.
 */
typedef void (* ble_uart_c_evt_handler_t) (ble_uart_c_t * p_ble_uart_c, ble_uart_c_evt_t * p_evt);

/** @} */

/**
 * @addtogroup uart_c_structs
 * @{
 */

/**@brief UART Client structure.
 */
typedef struct ble_uart_c_s
{
    uint16_t                conn_handle;      /**< Connection handle as provided by the SoftDevice. */
    uint16_t                RX_cccd_handle;  /**< Handle of the CCCD of the RX characteristic. */
    uint16_t                RX_handle;       /**< Handle of the RX characteristic as provided by the SoftDevice. */
	uint16_t                TX_handle;       /**< Handle of the TX characteristic as provided by the SoftDevice. */
    ble_uart_c_evt_handler_t evt_handler;      /**< Application event handler to be called when there is an event related to the UART service. */
} ble_uart_c_t;

/**@brief UART Client initialization structure.
 */
typedef struct
{
    ble_uart_c_evt_handler_t evt_handler;  /**< Event handler to be called by the UART Client module whenever there is an event related to the UART Service. */
} ble_uart_c_init_t;

/** @} */

/**
 * @defgroup uart_c_functions Functions
 * @{
 */

/**@brief   Function for writing data to the peer TX Characetistic.
 *
 *
 * @param   p_ble_uart_c Pointer to the UART client structure.
 *
 * @retval  NRF_SUCCESS If the SoftDevice has been requested to write to the TX Characteristic of the peer.
 *                      Otherwise, an error code. This function propagates the error code returned 
 *                      by the SoftDevice API @ref sd_ble_gattc_write.
 */
uint32_t ble_uart_c_write_string(ble_uart_c_t * p_ble_uart_c, const uint8_t * p_str, uint16_t p_str_len);

/* write a dummy data */


/**@brief     Function for initializing the UART client module.
 *
 * @details   This function will register with the DB Discovery module. There it
 *            registers for the UART Service. Doing so will make the DB Discovery
 *            module look for the presence of a UART Service instance at the peer when a
 *            discovery is started.
 *
 * @param[in] p_ble_uart_c      Pointer to the UART client structure.
 * @param[in] p_ble_uart_c_init Pointer to the UART initialization structure containing the
 *                             initialization information.
 *
 * @retval    NRF_SUCCESS On successful initialization. Otherwise an error code. This function
 *                        propagates the error code returned by the Database Discovery module API
 *                        @ref ble_db_discovery_evt_register.
 */
uint32_t ble_uart_c_init(ble_uart_c_t * p_ble_uart_c, ble_uart_c_init_t * p_ble_uart_c_init);

/**@brief     Function for handling BLE events from the SoftDevice.
 *
 * @details   This function will handle the BLE events received from the SoftDevice. If a BLE
 *            event is relevant to the UART Client module, then it uses it to update
 *            interval variables and, if necessary, send events to the application.
 *
 * @param[in] p_ble_uart_c Pointer to the UART client structure.
 * @param[in] p_ble_evt   Pointer to the BLE event.
 */
void ble_uart_c_on_ble_evt(ble_uart_c_t * p_ble_uart_c, const ble_evt_t * p_ble_evt);


/**@brief   Function for requesting the peer to start sending notification of RX characteristic.
 *
 * @details This function will enable to notification of the RX characteristic at the peer
 *          by writing to the CCCD of the UART RX Characteristic.
 *
 * @param   p_ble_uart_c Pointer to the UART client structure.
 *
 * @retval  NRF_SUCCESS If the SoftDevice has been requested to write to the CCCD of the peer.
 *                      Otherwise, an error code. This function propagates the error code returned 
 *                      by the SoftDevice API @ref sd_ble_gattc_write.
 */
uint32_t ble_uart_c_rx_notif_enable(ble_uart_c_t * p_ble_uart_c);

/** @} */ // End tag for Function group.

#endif // BLE_UART_C_H__



/** @} */ // End tag for the file.
