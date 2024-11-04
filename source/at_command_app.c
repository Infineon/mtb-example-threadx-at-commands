/******************************************************************************
 * File Name:   at_command_app.c
 *
 * Description: This file contains declaration of task related to AT commands
 * operation.
 *
 * Related Document: See README.md
 *
 *
 *******************************************************************************
 * Copyright 2019-2023, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 *******************************************************************************/

/* Header file includes. */
#include "at_command_app.h"

#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/* Standard C header file. */
#include <string.h>

/* Cypress secure socket header file. */
#include "cy_secure_sockets.h"

/* Wi-Fi connection manager header files. */
#include "cy_wcm.h"
#include "cy_wcm_error.h"
#include "at_command_parser.h"
/* TCP client task header file. */
#include "cy_nw_helper.h"
#include "at_cmd_refapp.h"

/* Standard C header files */
#include <inttypes.h>

#if defined(SDIO_HM_AT_CMD)
#include "sdio_at_cmd_support.h"
#endif

/*******************************************************************************
 * Function Prototypes
 ********************************************************************************/

static at_cmd_msg_base_t *cmd_callback_wcm_cmd(uint32_t cmd_id, uint32_t serial, uint32_t cmd_args_len, uint8_t *cmd_args)
{

    at_cmd_msg_base_t *at_cmd_msg;
    //  AT_CMD_REFAPP_LOG_MSG(("cmd_callback_wcm_cmd: cmd_id %lu, serial %lu, args %s\n", cmd_id, serial, (char *)cmd_args));
    at_cmd_msg = at_cmd_refapp_parse_wcm_cmd(cmd_id, serial, cmd_args_len, (char *)cmd_args);
    if (at_cmd_msg == NULL)
    {
        AT_CMD_REFAPP_LOG_MSG(("at_cmd_refapp_parse_wcm_cmd failed\n"));
    }
    return (at_cmd_msg_base_t *)at_cmd_msg;
}

static at_cmd_msg_base_t *cmd_callback_mqtt_cmd(uint32_t cmd_id, uint32_t serial, uint32_t cmd_args_len, uint8_t *cmd_args)
{
    at_cmd_msg_base_t *at_cmd_msg;
    //  AT_CMD_REFAPP_LOG_MSG(("cmd_callback_mqtt_cmd: cmd_id %lu, serial %lu\n", cmd_id, serial));
    at_cmd_msg = at_cmd_refapp_parse_mqtt_cmd(cmd_id, serial, cmd_args_len, (char *)cmd_args);
    if (at_cmd_msg == NULL)
    {
        AT_CMD_REFAPP_LOG_MSG(("at_cmd_refapp_parse_wcm_cmd failed\n"));
    }
    else
    {
        AT_CMD_REFAPP_LOG_MSG(("%s exit at_cmd_msg->cmd_id:%ld\n", __func__, at_cmd_msg->cmd_id));
    }
    return (at_cmd_msg_base_t *)at_cmd_msg;
}

static at_cmd_def_t at_cmd_refapp_wcm_cmd_table[] =
    {
        {"WCM_APConnect", CMD_ID_AP_CONNECT, cmd_callback_wcm_cmd},
        {"WCM_APDisconnect", CMD_ID_AP_DISCONNECT, cmd_callback_wcm_cmd},
        {"WCM_APGetInfo", CMD_ID_AP_GET_INFO, cmd_callback_wcm_cmd},
        {"WCM_GetIPAddress", CMD_ID_GET_IP_ADDRESS, cmd_callback_wcm_cmd},
        {"WCM_GetIPV4Info", CMD_ID_GET_IPv4_ADDRESS, cmd_callback_wcm_cmd},
        {"WCM_Ping", CMD_ID_PING, cmd_callback_wcm_cmd},
        {"WCM_ScanStart", CMD_ID_SCAN_START, cmd_callback_wcm_cmd},
        {"WCM_ScanStop", CMD_ID_SCAN_STOP, cmd_callback_wcm_cmd},
        {"MQTT_DefineBroker", CMD_ID_MQTT_DEFINE_BROKER, cmd_callback_mqtt_cmd},
        {"MQTT_GetBroker", CMD_ID_MQTT_GET_BROKER, cmd_callback_mqtt_cmd},
        {"MQTT_DeleteBroker", CMD_ID_MQTT_DELETE_BROKER, cmd_callback_mqtt_cmd},
        {"MQTT_ConnectBroker", CMD_ID_MQTT_CONNECT_BROKER, cmd_callback_mqtt_cmd},
        {"MQTT_DisconnectBroker", CMD_ID_MQTT_DISCONNECT_BROKER, cmd_callback_mqtt_cmd},
        {"MQTT_Subscribe", CMD_ID_MQTT_SUBSCRIBE, cmd_callback_mqtt_cmd},
        {"MQTT_Unsubscribe", CMD_ID_MQTT_UNSUBSCRIBE, cmd_callback_mqtt_cmd},
        {"MQTT_Publish", CMD_ID_MQTT_PUBLISH, cmd_callback_mqtt_cmd},
        {NULL, CMD_ID_INVALID, cmd_callback_wcm_cmd}

};

static cy_queue_t msgq;
at_cmd_result_data_t result_str;

bool at_cmd_refapp_transport_is_data_ready(void *opaque)
{
#if defined(SDIO_HM_AT_CMD)
    return sdio_cmd_at_is_data_ready();
#else
    volatile bool is_transport_ready = false;
    uint32_t num_bytes = 0;
    num_bytes = cyhal_uart_readable(&cy_retarget_io_uart_obj);
    if (num_bytes > 0)
        is_transport_ready = true;

    return is_transport_ready;
#endif /* AT_CMD_OVER_SDIO */
}

/**
 * transport read callback
 */
static uint32_t transport_read_data(uint8_t *buffer, uint32_t size, void *opaque)
{
#if defined(SDIO_HM_AT_CMD)
    return sdio_cmd_at_read_data(buffer, size);
#else
    size_t len = cyhal_uart_readable(&cy_retarget_io_uart_obj);

    cyhal_uart_read(&cy_retarget_io_uart_obj, buffer, &len);

    return (uint32_t)len;
#endif /* AT_CMD_OVER_SDIO */
}

/**
 * transport write callback
 */
static cy_rslt_t transport_write_data(uint8_t *buffer, uint32_t length, void *opaque)
{
#if defined(SDIO_HM_AT_CMD)
    return sdio_cmd_at_write_data(buffer, length);
#else
    for (int i = 0; i < length; i++)
    {
        cyhal_uart_putc(&cy_retarget_io_uart_obj, buffer[i]);
    }

    return CY_RSLT_SUCCESS;
#endif /* AT_CMD_OVER_SDIO */
}

/*******************************************************************************
 * Function Name: client_task
 *******************************************************************************
 * Summary:
 *
 * Parameters:
 *  void *args : Task parameter defined during task creation (unused).
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void client_task(cy_thread_arg_t arg)
{
    cy_rslt_t result;
    at_cmd_params_t params;
    at_cmd_msg_queue_t msg_queue_entry;
    at_cmd_ref_app_mqtt_disconnect_event_t *mqtt_async_disconnect_event = NULL;
    at_cmd_ref_app_mqtt_publish_t *async_subscriber_event = NULL;

    cy_wcm_config_t wifi_config = {.interface = CY_WCM_INTERFACE_TYPE_STA};

    result = cy_rtos_queue_init(&msgq, AT_CMD_REF_APP_NUM_CMD_QUEUE_MSGS, sizeof(at_cmd_msg_queue_t));

    memset(&params, 0, sizeof(params));
    params.cmd_msg_queue = &msgq;
    params.is_data_ready = at_cmd_refapp_transport_is_data_ready;
    params.read_data = transport_read_data;
    params.write_data = transport_write_data;

    result = at_cmd_parser_init(&params);

    result = at_cmd_parser_register_commands(at_cmd_refapp_wcm_cmd_table, sizeof(at_cmd_refapp_wcm_cmd_table) / sizeof(at_cmd_def_t));

    /* Initialize Wi-Fi connection manager. */
    result = cy_wcm_init(&wifi_config);

    if (result != CY_RSLT_SUCCESS)
    {
        printf("Wi-Fi Connection Manager initialization failed! Error code: 0x%08" PRIx32 "\n", (uint32_t)result);
        CY_ASSERT(0);
    }
    printf("Wi-Fi Connection Manager initialized.\r\n");

    result = at_cmd_refapp_mqtt_init();
    if (result != CY_RSLT_SUCCESS)
    {
        AT_CMD_REFAPP_LOG_MSG(("Error initializing MQTT \n"));
    }

    printf("MQTT library  initialized.\r\n");

    for (;;)
    {

        memset(&msg_queue_entry, 0, sizeof(msg_queue_entry));
        memset(&result_str, 0, sizeof(result_str));
        result = cy_rtos_queue_get(&msgq, &msg_queue_entry, AT_CMD_REF_APP_WAITFOREVER);
        if (result == CY_RSLT_SUCCESS)
        {
            at_cmd_msg_base_t *cmd = (at_cmd_msg_base_t *)msg_queue_entry.msg;
            if ((cmd == NULL))
            {
                AT_CMD_REFAPP_LOG_MSG(("NULL buffer: received ignore and drop\n"));
                continue;
            }
            if (cmd->cmd_id > CMD_ID_INVALID)
            {
                AT_CMD_REFAPP_LOG_MSG(("invalid cmd received continue cmd_id:%ld\n", cmd->cmd_id));
                continue;
            }
            AT_CMD_REFAPP_LOG_MSG(("\nReceived command message - cmd_id: %lu, serial: %lu\n",
                                   cmd->cmd_id, cmd->serial));
            switch (cmd->cmd_id)
            {
            case CMD_ID_AP_CONNECT:
            case CMD_ID_AP_DISCONNECT:
            case CMD_ID_AP_GET_INFO:
            case CMD_ID_SCAN_START:
            case CMD_ID_SCAN_STOP:
            case CMD_ID_GET_IP_ADDRESS:
            case CMD_ID_GET_IPv4_ADDRESS:
            case CMD_ID_PING:
                at_cmd_refapp_build_wcm_json_text_to_host(cmd->cmd_id, cmd->serial, cmd, &result_str);
                at_cmd_parser_send_cmd_response(cmd->serial, result_str.result_status, result_str.result_text);
                break;
            case CMD_ID_WCM_NETWORK_CHANGE_NOTIFICATION:
            case CMD_ID_HOST_WCM_SCAN_INFO:
                at_cmd_refapp_build_wcm_json_text_to_host(cmd->cmd_id, cmd->serial, cmd, &result_str);
                at_cmd_parser_send_cmd_response(cmd->serial, result_str.result_status, result_str.result_text);
                break;
            case CMD_ID_MQTT_DEFINE_BROKER:
            case CMD_ID_MQTT_GET_BROKER:
            case CMD_ID_MQTT_DELETE_BROKER:
            case CMD_ID_MQTT_CONNECT_BROKER:
            case CMD_ID_MQTT_DISCONNECT_BROKER:
            case CMD_ID_MQTT_SUBSCRIBE:
            case CMD_ID_MQTT_UNSUBSCRIBE:
            case CMD_ID_MQTT_PUBLISH:
                at_cmd_refapp_build_mqtt_json_text_to_host(cmd->cmd_id, cmd->serial, cmd, &result_str);
                at_cmd_parser_send_cmd_response(cmd->serial, result_str.result_status, result_str.result_text);
                break;
            case CMD_ID_MQTT_ASYNC_DISCONNECT_EVENT:
                mqtt_async_disconnect_event = (at_cmd_ref_app_mqtt_disconnect_event_t *)cmd;
                at_cmd_refapp_mqtt_event_callback(cmd->cmd_id, (at_cmd_msg_base_t *)mqtt_async_disconnect_event, &result_str);
                at_cmd_parser_send_cmd_async_response(cmd->serial, result_str.result_text);
                break;
            case CMD_ID_MQTT_ASYNC_SUBSCRIPTION_EVENT:
                async_subscriber_event = (at_cmd_ref_app_mqtt_publish_t *)cmd;
                at_cmd_refapp_mqtt_event_callback(cmd->cmd_id, (at_cmd_msg_base_t *)async_subscriber_event, &result_str);
                at_cmd_parser_send_cmd_async_response(cmd->serial, result_str.result_text);
                break;
            default:
                AT_CMD_REFAPP_LOG_MSG(("unknown command received cmd_id:%ld \n", cmd->cmd_id));
                break;
            } /* end of switch */
            free(cmd);
        }
        else
        {
            AT_CMD_REFAPP_LOG_MSG(("Message queue timeout.. \n"));
        }
    }
}

void at_cmd_refapp_build_wcm_json_text_to_host(uint32_t cmd_id, uint32_t serial, at_cmd_msg_base_t *cmd, at_cmd_result_data_t *result_str)
{
    at_cmd_msg_base_t *host_resp_msg = NULL;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    switch (cmd_id)
    {
    case CMD_ID_AP_CONNECT:
    case CMD_ID_AP_GET_INFO:
    case CMD_ID_AP_DISCONNECT:
    case CMD_ID_SCAN_START:
    case CMD_ID_SCAN_STOP:
    case CMD_ID_GET_IP_ADDRESS:
    case CMD_ID_GET_IPv4_ADDRESS:
    case CMD_ID_PING:
    case CMD_ID_WCM_NETWORK_CHANGE_NOTIFICATION:
    case CMD_ID_HOST_WCM_SCAN_INFO:
        host_resp_msg = at_cmd_refapp_wcm_process_message((at_cmd_msg_base_t *)cmd, result_str);
        if (host_resp_msg != NULL)
        {
            /* process host message */
            result = at_cmd_refapp_process_wcm_host_msg(cmd_id, host_resp_msg, result_str->result_text, sizeof(result_str->result_text));
            if (result != CY_RSLT_SUCCESS)
            {
                AT_CMD_REFAPP_LOG_MSG(("at_cmd_refapp_process_wcm_host_msg failed result:%ld\n", result));
            }
        }
        break;

    default:
        AT_CMD_REFAPP_LOG_MSG(("unknown command received cmd_id:%ld\n", cmd_id));
        break;
    }
}

/**
 * send message to AT command reference app queue
 */
cy_rslt_t at_cmd_refapp_send_message(at_cmd_msg_base_t *msg)
{
    at_cmd_msg_queue_t msg_queue_entry;
    msg_queue_entry.msg = (at_cmd_msg_base_t *)msg;
    cy_rslt_t result;

    result = cy_rtos_put_queue(&msgq, &msg_queue_entry, 0, true);
    if (result != CY_RSLT_SUCCESS)
    {
        AT_CMD_REFAPP_LOG_MSG(("unable to put msg on queue\n"));
    }
    return result;
}

/* [] END OF FILE */
