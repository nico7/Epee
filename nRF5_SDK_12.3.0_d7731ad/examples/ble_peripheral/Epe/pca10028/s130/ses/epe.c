
#include "epe.h"

/**
 * Copyright (c) 2012 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
#include "sdk_common.h"

#include "ble_srv_common.h"

#define NRF_LOG_MODULE_NAME "BLE_EPE"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#define BLE_UUID_EPE_TX_CHARACTERISTIC 0x0002                      /**< The UUID of the TX Characteristic. */
#define BLE_UUID_EPE_RX_CHARACTERISTIC 0x0003                      /**< The UUID of the RX Characteristic. */

#define BLE_EPE_MAX_RX_CHAR_LEN        BLE_EPE_MAX_DATA_LEN        /**< Maximum length of the RX Characteristic (in bytes). */
#define BLE_EPE_MAX_TX_CHAR_LEN        BLE_EPE_MAX_DATA_LEN        /**< Maximum length of the TX Characteristic (in bytes). */



#define EPE_MAX_DATA_LENGTH           0x100

volatile uint32_t file_size = 0, file_pos = 0, m_max_data_length = BLE_EPE_MAX_TX_CHAR_LEN;
static volatile bool nrf_error_resources = false;

uint8_t * file_data;
ble_epe_t * mp_epe;

static uint16_t m_tx_packet_count = 0;


/**@brief Function for handling the @ref BLE_GAP_EVT_CONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_epe     Nordic UART Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_connect(ble_epe_t * p_epe, ble_evt_t * p_ble_evt)
{
    p_epe->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Function for handling the @ref BLE_GAP_EVT_DISCONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_epe     Nordic UART Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_disconnect(ble_epe_t * p_epe, ble_evt_t * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_epe->conn_handle = BLE_CONN_HANDLE_INVALID;
}


/**@brief Function for handling the @ref BLE_GATTS_EVT_WRITE event from the S110 SoftDevice.
 *
 * @param[in] p_epe     Nordic UART Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(ble_epe_t * p_epe, ble_evt_t * p_ble_evt)
{
    ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (
        (p_evt_write->handle == p_epe->rx_handles.cccd_handle)
        &&
        (p_evt_write->len == 2)
       )
    {
        if (ble_srv_is_notification_enabled(p_evt_write->data))
        {
            p_epe->is_notification_enabled = true;
            NRF_LOG_INFO("BLE EPE : Notification is Enabled\n");
        }
        else
        {
            p_epe->is_notification_enabled = false;
        }
    }
    else if (
             (p_evt_write->handle == p_epe->tx_handles.value_handle)
             &&
             (p_epe->data_handler != NULL)
            )
    {
        p_epe->data_handler(p_epe, p_evt_write->data, p_evt_write->len);
    }
    else
    {
        // Do Nothing. This event is not relevant for this service.
    }
}


/**@brief Function for adding RX characteristic.
 *
 * @param[in] p_epe       Nordic UART Service structure.
 * @param[in] p_epe_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t rx_char_add(ble_epe_t * p_epe, const ble_epe_init_t * p_epe_init)
{
    /**@snippet [Adding proprietary characteristic to S110 SoftDevice] */
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&cccd_md, 0, sizeof(cccd_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

    cccd_md.vloc = BLE_GATTS_VLOC_STACK;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.notify = 1;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = &cccd_md;
    char_md.p_sccd_md         = NULL;

    ble_uuid.type = p_epe->uuid_type;
    ble_uuid.uuid = BLE_UUID_EPE_RX_CHARACTERISTIC;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(uint8_t);
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = BLE_EPE_MAX_RX_CHAR_LEN;

    return sd_ble_gatts_characteristic_add(p_epe->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_epe->rx_handles);
    /**@snippet [Adding proprietary characteristic to S110 SoftDevice] */
}


/**@brief Function for adding TX characteristic.
 *
 * @param[in] p_epe       Nordic UART Service structure.
 * @param[in] p_epe_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t tx_char_add(ble_epe_t * p_epe, const ble_epe_init_t * p_epe_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.write         = 1;
    char_md.char_props.read          = 1;
    char_md.char_props.notify        = 1;
    char_md.char_props.write_wo_resp = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_epe->uuid_type;
    ble_uuid.uuid = BLE_UUID_EPE_TX_CHARACTERISTIC;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = 1;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = BLE_EPE_MAX_TX_CHAR_LEN;

    return sd_ble_gatts_characteristic_add(p_epe->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_epe->tx_handles);
}


static uint32_t push_data_packets(void)
{
        uint32_t return_code = NRF_SUCCESS;
        uint32_t packet_length = m_max_data_length;
        uint32_t packet_size = 0;

        while(return_code == NRF_SUCCESS)
        {
                //printf("file_pos = %d, packet_size = %d\n", file_pos, packet_size);
                //printf("\nb %d, %d, %d , %d\n", file_size, file_pos, packet_size, packet_length);

                if((file_size - file_pos) > packet_length)
                {
                        packet_size = packet_length;
                }
                else if((file_size - file_pos) >= 0)
                {
                        packet_size = file_size - file_pos;
                }
                else
                {
                        packet_size = 0;
                }

                if(packet_size > 0)
                {
                        return_code = ble_epe_string_send(mp_epe, &file_data[file_pos], packet_size);
                        if(return_code == NRF_SUCCESS)
                        {
                                file_pos += packet_size;
                        }
                }
                else
                {
                        file_size = 0;
                        break;
                }
        }
        return return_code;
}

void ble_epe_on_ble_evt(ble_epe_t * p_epe, ble_evt_t * p_ble_evt)
{
    if ((p_epe == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_epe, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_epe, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_epe, p_ble_evt);
            break;

        case BLE_EVT_TX_COMPLETE:
        {
                NRF_LOG_DEBUG("Complete m_tx_packet_count = %d\n", m_tx_packet_count);
                if(file_size > 0)
                {
                        push_data_packets();
                }
                nrf_error_resources = false;
                if( m_tx_packet_count > 0)
                {
                    m_tx_packet_count--;
                }


                if (m_tx_packet_count == 0)
                {
                      if (p_epe->tx_complete_handler != NULL)
                      {
                          p_epe->tx_complete_handler();
                      }
                }
        } break;
        default:
            // No implementation needed.
            break;
    }
}

uint32_t ble_epe_init(ble_epe_t * p_epe, const ble_epe_init_t * p_epe_init)
{
    uint32_t      err_code;
    ble_uuid_t    ble_uuid;
    ble_uuid128_t epe_base_uuid = BLE_UUID_EPE_SERVICE;

    VERIFY_PARAM_NOT_NULL(p_epe);
    VERIFY_PARAM_NOT_NULL(p_epe_init);

    // Initialize the service structure.
    p_epe->conn_handle             = BLE_CONN_HANDLE_INVALID;
    p_epe->data_handler            = p_epe_init->data_handler;
    p_epe->is_notification_enabled = false;

    p_epe->tx_complete_handler     = p_epe_init->tx_complete_handler;

    /**@snippet [Adding proprietary Service to S130 SoftDevice] */
    // Add a custom base UUID.
    err_code = sd_ble_uuid_vs_add(&epe_base_uuid, &p_epe->uuid_type);
    VERIFY_SUCCESS(err_code);

    ble_uuid.type = p_epe->uuid_type;
    ble_uuid.uuid = EPE_SERVICE_UUID;

    // Add the service.
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_epe->service_handle);
    /**@snippet [Adding proprietary Service to S110 SoftDevice] */
    VERIFY_SUCCESS(err_code);

    // Add the RX Characteristic.
   // err_code = rx_char_add(p_epe, p_epe_init);
    //VERIFY_SUCCESS(err_code);

    // Add the TX Characteristic.
    err_code = tx_char_add(p_epe, p_epe_init);
    VERIFY_SUCCESS(err_code);

    return NRF_SUCCESS;
}


uint32_t ble_epe_string_send(ble_epe_t * p_epe, uint8_t * p_string, uint16_t length)
{
    ble_gatts_hvx_params_t hvx_params;
        uint32_t err_code;

        if(nrf_error_resources)
        {
                return NRF_ERROR_RESOURCES;
        }

    VERIFY_PARAM_NOT_NULL(p_epe);

    if ((p_epe->conn_handle == BLE_CONN_HANDLE_INVALID) || (!p_epe->is_notification_enabled))
    {
        return NRF_ERROR_INVALID_STATE;
    }

    if (length > BLE_EPE_MAX_DATA_LEN)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_epe->rx_handles.value_handle;
    hvx_params.p_data = p_string;
    hvx_params.p_len  = &length;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;

        err_code = sd_ble_gatts_hvx(p_epe->conn_handle, &hvx_params);

        if(err_code == NRF_ERROR_RESOURCES)
        {
                nrf_error_resources = true;
        }
        return err_code;
}


uint32_t ble_epe_send_file(ble_epe_t * p_epe, uint8_t * p_data, uint32_t data_length, uint32_t max_packet_length)
{
        uint32_t err_code;

        if ((p_epe->conn_handle == BLE_CONN_HANDLE_INVALID) || (!p_epe->is_notification_enabled))
        {
                err_code = NRF_ERROR_INVALID_STATE;
                VERIFY_SUCCESS(err_code);
                return NRF_ERROR_INVALID_STATE;
        }

        if(file_size != 0)
        {
                return NRF_ERROR_BUSY;
        }

        if (data_length > EPE_MAX_DATA_LENGTH)
        {
                err_code = NRF_ERROR_INVALID_PARAM;
                VERIFY_SUCCESS(err_code);
                return NRF_ERROR_INVALID_PARAM;
        }

        file_size = data_length;
        file_pos = 0;
        file_data = p_data;
        m_max_data_length = max_packet_length;
        mp_epe = p_epe;

  
        m_tx_packet_count = (data_length / max_packet_length) + 1;

        if (data_length % max_packet_length == 0)
        {
              m_tx_packet_count++;
        }
        NRF_LOG_DEBUG("m_tx_packet_count = %d", m_tx_packet_count);

        err_code = push_data_packets();

        if(err_code != NRF_ERROR_RESOURCES)
                return NRF_SUCCESS;

        return err_code;
}


uint32_t epe_characteristiv_value_update(ble_epe_t * p_epe, uint8_t * custom_value, uint8_t length)
{

    if (p_epe == NULL)
    {
        return NRF_ERROR_NULL;
    }

    uint32_t err_code = NRF_SUCCESS;
    ble_gatts_value_t gatts_value;

    // Initialize value structure.
    memset(&gatts_value, 0, sizeof(uint8_t)); // at least make this a 2, but idk 

    gatts_value.len = length;
    gatts_value.offset = 0;
    gatts_value.p_value = custom_value;

    // Update database.
    err_code = sd_ble_gatts_value_set(p_epe->conn_handle,
                                      p_epe->tx_handles.value_handle,
                                      &gatts_value);

    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
   
    // Send value if connected and notifying.
    if((p_epe->conn_handle != BLE_CONN_HANDLE_INVALID))
    {
        ble_gatts_hvx_params_t hvx_params;

        memset(&hvx_params, 0, sizeof(hvx_params));

        hvx_params.handle = p_epe->tx_handles.value_handle;
        hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;//BLE_GATT_HVX_INDICATION;
        hvx_params.offset = gatts_value.offset;
        hvx_params.p_len  = &gatts_value.len;
        hvx_params.p_data = gatts_value.p_value;
        

        err_code = sd_ble_gatts_hvx(p_epe->conn_handle, &hvx_params);
        
        if(err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING)
        {
            err_code = sd_ble_gatts_sys_attr_set(p_epe->conn_handle, NULL, 0, BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS);
        }
    
    }
    else
    {
        err_code = NRF_ERROR_INVALID_STATE;
    }

    return err_code;



}