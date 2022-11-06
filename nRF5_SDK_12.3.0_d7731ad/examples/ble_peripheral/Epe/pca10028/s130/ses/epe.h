#ifndef _EPE_SERVICE_H_
#define _EPE_SERVICE_H_

#include "ble.h"
#include "ble_srv_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_UUID_EPE_SERVICE { 0xab, 0x51, 0xaf, 0xaf, 0xa1, 0x4d, 0x4f, 0xa3, 0xcf, 0x45, 0x78, 0xaf, 0xdd, 0xb6, 0x7a, 0x1c}
#define BLE_EPE_MAX_DATA_LEN (GATT_MTU_SIZE_DEFAULT - 3) /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */

#define EPE_SERVICE_UUID  0xFF00

/* Forward declaration of the ble_nus_t type. */
typedef struct ble_epe_s ble_epe_t;

/**@brief Nordic UART Service event handler type. */
typedef void (*ble_epe_data_handler_t) (ble_epe_t * p_epe, uint8_t * p_data, uint16_t length);


typedef void (*ble_epe_tx_complete_handler_t) (void);

/**@brief Nordic UART Service initialization structure.
 *
 * @details This structure contains the initialization information for the service. The application
 * must fill this structure and pass it to the service using the @ref ble_nus_init
 *          function.
 */
typedef struct
{
    ble_epe_data_handler_t data_handler; /**< Event handler to be called for handling received data. */
    ble_epe_tx_complete_handler_t tx_complete_handler;     
} ble_epe_init_t;

/**@brief Epe Service structure.
 *
 * @details This structure contains status information related to the service.
 */
struct ble_epe_s
{
    uint8_t                  uuid_type;               /**< UUID type for Nordic UART Service Base UUID. */
    uint16_t                 service_handle;          /**< Handle of Nordic UART Service (as provided by the SoftDevice). */
    ble_gatts_char_handles_t tx_handles;              /**< Handles related to the TX characteristic (as provided by the SoftDevice). */
    ble_gatts_char_handles_t rx_handles;              /**< Handles related to the RX characteristic (as provided by the SoftDevice). */
    uint16_t                 conn_handle;             /**< Handle of the current connection (as provided by the SoftDevice). BLE_CONN_HANDLE_INVALID if not in a connection. */
    bool                     is_notification_enabled; /**< Variable to indicate if the peer has enabled notification of the RX characteristic.*/
    ble_epe_data_handler_t   data_handler;            /**< Event handler to be called for handling received data. */
    ble_epe_tx_complete_handler_t tx_complete_handler;                  
};

/**@brief Function for initializing the Nordic UART Service.
 *
 * @param[out] p_epe      Nordic UART Service structure. This structure must be supplied
 *                        by the application. It is initialized by this function and will
 *                        later be used to identify this particular service instance.
 * @param[in] p_epe_init  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was successfully initialized. Otherwise, an error code is returned.
 * @retval NRF_ERROR_NULL If either of the pointers p_epe or p_epe_init is NULL.
 */
uint32_t ble_epe_init(ble_epe_t * p_epe, const ble_epe_init_t * p_epe_init);

/**@brief Function for handling the Nordic UART Service's BLE events.
 *
 * @details The Nordic UART Service expects the application to call this function each time an
 * event is received from the SoftDevice. This function processes the event if it
 * is relevant and calls the Nordic UART Service event handler of the
 * application if necessary.
 *
 * @param[in] p_epe       Epee Service
 * @param[in] p_ble_evt   Event received from the SoftDevice.
 */
void ble_epe_on_ble_evt(ble_epe_t * p_epe, ble_evt_t * p_ble_evt);

/**@brief Function for sending a string to the peer.
 *
 * @details This function sends the input string as an RX characteristic notification to the
 *          peer.
 *
 * @param[in] p_epe       Pointer to the Nordic UART Service structure.
 * @param[in] p_string    String to be sent.
 * @param[in] length      Length of the string.
 *
 * @retval NRF_SUCCESS If the string was sent successfully. Otherwise, an error code is returned.
 */
uint32_t ble_epe_string_send(ble_epe_t * p_epe, uint8_t * p_string, uint16_t length);

uint32_t ble_epe_send_file(ble_epe_t * p_epe, uint8_t * p_data, uint32_t data_length, uint32_t max_packet_length);

uint32_t epe_characteristiv_value_update(ble_epe_t * p_epe, uint8_t * custom_value, uint8_t length);


#ifdef __cplusplus
}
#endif


#endif // _EPE_SERVICE_H_