/*
 * Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is confidential property of Nordic Semiconductor. The use,
 * copying, transfer or disclosure of such information is prohibited except by express written
 * agreement with Nordic Semiconductor.
 *
 */

/**@cond To Make Doxygen skip documentation generation for this file.
 * @{
 */

#include <stdint.h>
#include <string.h>

#include "ble_uart_c.h"
#include "ble_db_discovery.h"
#include "ble_types.h"
#include "ble_srv_common.h"
#include "nordic_common.h"
#include "nrf_error.h"
#include "ble_gattc.h"
#include "app_util.h"
#include "app_trace.h"

#define LOG                    app_trace_log         /**< Debug logger macro that will be used in this file to do logging of important information over UART. */

#define TX_BUFFER_MASK         0x07                  /**< TX Buffer mask, must be a mask of continuous zeroes, followed by continuous sequence of ones: 000...111. */
#define TX_BUFFER_SIZE         (TX_BUFFER_MASK + 1)  /**< Size of send buffer, which is 1 higher than the mask. */

#define WRITE_MESSAGE_LENGTH   20//BLE_CCCD_VALUE_LEN    /**< Length of the write message for CCCD. */

typedef enum
{
    READ_REQ,  /**< Type identifying that this tx_message is a read request. */
    WRITE_REQ  /**< Type identifying that this tx_message is a write request. */
} tx_request_t;

/**@brief Structure for writing a message to the peer, i.e. CCCD.
 */
typedef struct
{
    uint8_t                  gattc_value[WRITE_MESSAGE_LENGTH];  /**< The message to write. */
    ble_gattc_write_params_t gattc_params;                       /**< GATTC parameters for this message. */
} write_params_t;

/**@brief Structure for holding data to be transmitted to the connected central.
 */
typedef struct
{
    uint16_t     conn_handle;  /**< Connection handle to be used when transmitting this message. */
    tx_request_t type;         /**< Type of this message, i.e. read or write message. */
    union
    {
        uint16_t       read_handle;  /**< Read request message. */
        write_params_t write_req;    /**< Write request message. */
    } req;
} tx_message_t;


static ble_uart_c_t * mp_ble_uart_c;                 /**< Pointer to the current instance of the uart Client module. The memory for this provided by the application.*/
static tx_message_t  m_tx_buffer[TX_BUFFER_SIZE];  /**< Transmit buffer for messages to be transmitted to the central. */
static uint32_t      m_tx_insert_index = 0;        /**< Current index in the transmit buffer where the next message should be inserted. */
static uint32_t      m_tx_index = 0;               /**< Current index in the transmit buffer from where the next message to be transmitted resides. */
static  ble_uuid_t uart_uuid;

/**@brief Function for passing any pending request from the buffer to the stack.
 */
static void tx_buffer_process(void)
{
    if (m_tx_index != m_tx_insert_index)
    {
        uint32_t err_code;

        if (m_tx_buffer[m_tx_index].type == READ_REQ)
        {
            err_code = sd_ble_gattc_read(m_tx_buffer[m_tx_index].conn_handle,
                                         m_tx_buffer[m_tx_index].req.read_handle,
                                         0);
        }
        else
        {
            err_code = sd_ble_gattc_write(m_tx_buffer[m_tx_index].conn_handle,
                                          &m_tx_buffer[m_tx_index].req.write_req.gattc_params);
        }
        if (err_code == NRF_SUCCESS)
        {
            LOG("[uart_C]: SD Read/Write API returns Success..\r\n");
            m_tx_index++;
            m_tx_index &= TX_BUFFER_MASK;
        }
        else
        {
            LOG("[uart_C]: SD Read/Write API returns error. This message sending will be "
                "attempted again..\r\n");
        }
    }
}


/**@brief     Function for handling write response events.
 *
 * @param[in] p_ble_uart_c Pointer to the UART Client structure.
 * @param[in] p_ble_evt   Pointer to the BLE event received.
 */
static void on_write_rsp(ble_uart_c_t * p_ble_uart_c, const ble_evt_t * p_ble_evt)
{
    // Check if there is any message to be sent across to the peer and send it.
    tx_buffer_process();
}


/**@brief     Function for handling Handle Value Notification received from the SoftDevice.
 *
 * @details   This function will uses the Handle Value Notification received from the SoftDevice
 *            and checks if it is a notification of the NUS RX data from the peer. If it is,
 *            this function will send the RX data to the application.
 *
 * @param[in] p_ble_uart_c Pointer to the NUS Client structure.
 * @param[in] p_ble_evt   Pointer to the BLE event received.
 */
static void on_hvx(ble_uart_c_t * p_ble_uart_c, const ble_evt_t * p_ble_evt)
{
    // Check if this is an RX data notification.
    if (p_ble_evt->evt.gattc_evt.params.hvx.handle == p_ble_uart_c->RX_handle)
    {
        ble_uart_c_evt_t ble_uart_c_evt;

        ble_uart_c_evt.evt_type = BLE_UART_C_EVT_RX_DATA_NOTIFICATION;
				memcpy(ble_uart_c_evt.params.uart.rx_data,p_ble_evt->evt.gattc_evt.params.hvx.data,p_ble_evt->evt.gattc_evt.params.hvx.len);
				ble_uart_c_evt.params.uart.len = p_ble_evt->evt.gattc_evt.params.hvx.len;
        p_ble_uart_c->evt_handler(p_ble_uart_c, &ble_uart_c_evt);
    }
}


/**@brief     Function for handling events from the database discovery module.
 *
 * @details   This function will handle an event from the database discovery module, and determine
 *            if it relates to the discovery of nordic uart service at the peer. If so, it will
 *            call the application's event handler indicating that the nordic uart service has been
 *            discovered at the peer. It also populates the event with the service related
 *            information before providing it to the application.
 *
 * @param[in] p_evt Pointer to the event received from the database discovery module.
 *
 */
static void db_discover_evt_handler(ble_db_discovery_evt_t * p_evt)
{
    // Check if the Nordic UART Service was discovered.
    if (p_evt->evt_type == BLE_DB_DISCOVERY_COMPLETE &&
        p_evt->params.discovered_db.srv_uuid.uuid == BLE_UUID_NUS_SERVICE &&
        p_evt->params.discovered_db.srv_uuid.type == uart_uuid.type)
    {
        mp_ble_uart_c->conn_handle = p_evt->conn_handle;

        // Find the CCCD Handles of the TX/RX data characteristics.
        uint32_t i;

        for (i = 0; i < p_evt->params.discovered_db.char_count; i++)
        {
            if ((p_evt->params.discovered_db.charateristics[i].characteristic.uuid.uuid == BLE_UUID_NUS_RX_CHARACTERISTIC)
            	&&(p_evt->params.discovered_db.charateristics[i].characteristic.uuid.type==uart_uuid.type))
                
            {
                // Found RX data characteristic. Store CCCD handle .
                mp_ble_uart_c->RX_cccd_handle =
                    p_evt->params.discovered_db.charateristics[i].cccd_handle;
                mp_ble_uart_c->RX_handle      =
                    p_evt->params.discovered_db.charateristics[i].characteristic.handle_value;
               
            }
		if ((p_evt->params.discovered_db.charateristics[i].characteristic.uuid.uuid == BLE_UUID_NUS_TX_CHARACTERISTIC)
			&&(p_evt->params.discovered_db.charateristics[i].characteristic.uuid.type==uart_uuid.type))
            {
                // Found TX data characteristic. Store CCCD handle .
                mp_ble_uart_c->TX_handle      =
                    p_evt->params.discovered_db.charateristics[i].characteristic.handle_value;
               
            }
						
        }

        LOG("[uart_C]: Nordic UART service (NUS) discovered at peer.\r\n");

        ble_uart_c_evt_t evt;

        evt.evt_type = BLE_UART_C_EVT_DISCOVERY_COMPLETE;

        mp_ble_uart_c->evt_handler(mp_ble_uart_c, &evt);
    }
}


uint32_t ble_uart_c_init(ble_uart_c_t * p_ble_uart_c, ble_uart_c_init_t * p_ble_uart_c_init)
    {
    ble_uuid128_t   nus_base_uuid = {{0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x00, 0x00, 0x40, 0x6E}};
    uint32_t        err_code;
																		 
    if ((p_ble_uart_c == NULL) || (p_ble_uart_c_init == NULL))
    {
        return NRF_ERROR_NULL;
    }


		
    err_code = sd_ble_uuid_vs_add(&nus_base_uuid, &uart_uuid.type);
    //NOTE: after this, uart_uuid.type will hold the index of the NUS 128bit base UUID in the UUID database. 
    //Store and use this to distinguise between characteristics have different 128bit base UUIDs. 		
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
  
    uart_uuid.uuid = BLE_UUID_NUS_SERVICE;

    mp_ble_uart_c = p_ble_uart_c;

    mp_ble_uart_c->evt_handler    = p_ble_uart_c_init->evt_handler;
    mp_ble_uart_c->conn_handle    = BLE_CONN_HANDLE_INVALID;
    mp_ble_uart_c->RX_cccd_handle = BLE_GATT_HANDLE_INVALID;

    return ble_db_discovery_evt_register(&uart_uuid, db_discover_evt_handler);
}


void ble_uart_c_on_ble_evt(ble_uart_c_t * p_ble_uart_c, const ble_evt_t * p_ble_evt)
{
    if ((p_ble_uart_c == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            p_ble_uart_c->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;

        case BLE_GATTC_EVT_HVX:
            on_hvx(p_ble_uart_c, p_ble_evt);
            break;

        case BLE_GATTC_EVT_WRITE_RSP:
            on_write_rsp(p_ble_uart_c, p_ble_evt);
            break;

        default:
            break;
    }
}


/**@brief Function for creating a message for writing to the CCCD.
 */
static uint32_t cccd_configure(uint16_t conn_handle, uint16_t handle_cccd, bool enable)
{
    LOG("[uart_C]: Configuring CCCD. CCCD Handle = %d, Connection Handle = %d\r\n",
        handle_cccd,conn_handle);

    tx_message_t * p_msg;
    uint16_t       cccd_val = enable ? BLE_GATT_HVX_NOTIFICATION : 0;

    p_msg              = &m_tx_buffer[m_tx_insert_index++];
    m_tx_insert_index &= TX_BUFFER_MASK;

    p_msg->req.write_req.gattc_params.handle   = handle_cccd;
    p_msg->req.write_req.gattc_params.len      = 2;//WRITE_MESSAGE_LENGTH;
    p_msg->req.write_req.gattc_params.p_value  = p_msg->req.write_req.gattc_value;
    p_msg->req.write_req.gattc_params.offset   = 0;
    p_msg->req.write_req.gattc_params.write_op = BLE_GATT_OP_WRITE_REQ;
    p_msg->req.write_req.gattc_value[0]        = LSB(cccd_val);
    p_msg->req.write_req.gattc_value[1]        = MSB(cccd_val);
    p_msg->conn_handle                         = conn_handle;
    p_msg->type                                = WRITE_REQ;

    tx_buffer_process();
    return NRF_SUCCESS;
}


///**@brief Function for creating a message for writing to the CCCD.
// */
//static uint32_t write_char(uint16_t conn_handle, uint16_t char_handle, uint8_t *data, uint8_t len)
//{
//    LOG("[uart_C]: Writing to characteristic Handle = %d, Connection Handle = %d\r\n",
//        char_handle,conn_handle);

//    tx_message_t * p_msg;
//   
//    p_msg              = &m_tx_buffer[m_tx_insert_index++];
//    m_tx_insert_index &= TX_BUFFER_MASK;

//    p_msg->req.write_req.gattc_params.handle   = char_handle;
//    p_msg->req.write_req.gattc_params.len      = WRITE_MESSAGE_LENGTH;
//    p_msg->req.write_req.gattc_params.p_value  = p_msg->req.write_req.gattc_value;
//    p_msg->req.write_req.gattc_params.offset   = 0;
//    p_msg->req.write_req.gattc_params.write_op = BLE_GATT_OP_WRITE_REQ;
//    memcpy(p_msg->req.write_req.gattc_value,data,len);
//   
//    p_msg->conn_handle                         = conn_handle;
//    p_msg->type                                = WRITE_REQ;

//    tx_buffer_process();
//    return NRF_SUCCESS;
//}
 uint32_t ble_uart_c_write_string(ble_uart_c_t * p_ble_uart_c, const uint8_t * p_str, uint16_t p_str_len)
 {
    if (p_ble_uart_c->conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return NRF_ERROR_INVALID_STATE;
    }
    LOG("[uart_C]: Writing to characteristic Handle = %d, Connection Handle = %d\r\n",
        p_ble_uart_c->TX_handle,p_ble_uart_c->conn_handle);

    tx_message_t * p_msg;
   
    p_msg              = &m_tx_buffer[m_tx_insert_index++];
    m_tx_insert_index &= TX_BUFFER_MASK;

    p_msg->req.write_req.gattc_params.handle   = p_ble_uart_c->TX_handle;
    p_msg->req.write_req.gattc_params.len      = p_str_len;
    p_msg->req.write_req.gattc_params.p_value  = p_msg->req.write_req.gattc_value;
    p_msg->req.write_req.gattc_params.offset   = 0;
    p_msg->req.write_req.gattc_params.write_op = BLE_GATT_OP_WRITE_REQ;
    memcpy(p_msg->req.write_req.gattc_value,p_str,p_str_len);
   
    p_msg->conn_handle                         = p_ble_uart_c->conn_handle;
    p_msg->type                                = WRITE_REQ;

    tx_buffer_process();
     
    return NRF_SUCCESS;
 }
uint32_t ble_uart_c_rx_notif_enable(ble_uart_c_t * p_ble_uart_c)
{
    if (p_ble_uart_c == NULL)
    {
        return NRF_ERROR_NULL;
    }

    return cccd_configure(p_ble_uart_c->conn_handle, p_ble_uart_c->RX_cccd_handle, true);
}

/** @}
 *  @endcond
 */
