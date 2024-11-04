/*
 * Copyright 2023, Cypress Semiconductor Corporation or a subsidiary of
 * Cypress Semiconductor Corporation. All Rights Reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software"), is owned by Cypress Semiconductor Corporation
 * or one of its subsidiaries ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products. Any reproduction, modification, translation,
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
 */

/**
 * @file protocol_handler.c
 * @brief Main file for the CCM Protocol Handler.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "cy_retarget_io.h"
#include "cy_result.h"
#include "cyabs_rtos.h"
#include "at_command_parser.h"
#include "at_cmd_refapp.h"
#include "cy_wcm.h"

/******************************************************
 *               Static Function Declarations
 ******************************************************/
static void network_event_change_callback(cy_wcm_event_t event, cy_wcm_event_data_t *event_data);
static void wifi_scan_handler(cy_wcm_scan_result_t *result_ptr, void *user_data, cy_wcm_scan_status_t status);
/******************************************************
 *               Variable Definitions
 ******************************************************/
static at_cmd_security_def_t security_table[] =
    {
        {"open", CY_WCM_SECURITY_OPEN},
        {"wep-psk", CY_WCM_SECURITY_WEP_PSK},
        {"wep-shared", CY_WCM_SECURITY_WEP_SHARED},
        {"wpa-aes", CY_WCM_SECURITY_WPA_AES_PSK},
        {"wpa-tkip", CY_WCM_SECURITY_WPA_TKIP_PSK},
        {"wpa-mixed", CY_WCM_SECURITY_WPA_MIXED_PSK},
        {"wpa2-aes", CY_WCM_SECURITY_WPA2_AES_PSK},
        {"wpa2-tkip", CY_WCM_SECURITY_WPA2_TKIP_PSK},
        {"wpa2-mixed", CY_WCM_SECURITY_WPA2_MIXED_PSK},
        {"wpa3-wpa2", CY_WCM_SECURITY_WPA3_WPA2_PSK},
        {"wpa3", CY_WCM_SECURITY_WPA3_SAE},
        {NULL, CY_WCM_SECURITY_UNKNOWN}};

/******************************************************
 *               Function Definitions
 ******************************************************/

/**
 * Initialize WCM module and register callback notification for WCM events
 */
cy_rslt_t at_cmd_refapp_wcm_init(cy_queue_t *msgq)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_wcm_config_t config;

    AT_CMD_REFAPP_LOG_MSG(("before WCM function\n"));

    config.interface = CY_WCM_INTERFACE_TYPE_STA;
    result = cy_wcm_init(&config);
    if (result != CY_RSLT_SUCCESS)
    {
        AT_CMD_REFAPP_LOG_MSG(("WCM Initialization failed\n"));
        return CY_RSLT_AT_CMD_REF_APP_ERR;
    }

    AT_CMD_REFAPP_LOG_MSG(("inside WCM function\n"));

    /*
     * Register network event change callback with WCM library.
     */
    result = cy_wcm_register_event_callback(&network_event_change_callback);
    if (result != CY_RSLT_SUCCESS)
    {
        AT_CMD_REFAPP_LOG_MSG(("WCM callback registration failed\n"));
        return CY_RSLT_AT_CMD_REF_APP_ERR;
    }
    return result;
}

/**
 * Setup the WCM connect configuration parameters
 */
static cy_rslt_t setup_wcm_connect_config(at_cmd_ref_app_wcm_connect_specific_t *connect_config, cJSON *json)
{
    char *ssid;
    int idx;

    ssid = cJSON_GetObjectItem(json, WCM_TOKEN_SSID)->valuestring;

    if(strlen(ssid)>CY_WCM_MAX_SSID_LEN)
    {
            AT_CMD_REFAPP_LOG_MSG(("SSID too long\n"));
            return CY_RSLT_AT_CMD_REF_APP_ERR;
    }
    
    memcpy(connect_config->ssid, ssid, strlen(ssid));
    connect_config->ssid_length = strlen(ssid);

    idx = at_cmd_refapp_security_table_lookup_by_name(cJSON_GetObjectItem(json, WCM_TOKEN_SECURITY_TYPE)->valuestring, security_table);
    if (idx < 0)
    {
        AT_CMD_REFAPP_LOG_MSG(("Unknown security type: %s\n", cJSON_GetObjectItem(json, WCM_TOKEN_SECURITY_TYPE)->valuestring));
        return CY_RSLT_AT_CMD_REF_APP_ERR;
    }
    connect_config->security_type = security_table[idx].cmd_id;

    if (connect_config->security_type != CY_WCM_SECURITY_OPEN)
    {
        if (!cJSON_HasObjectItem(json, WCM_TOKEN_PASSWORD))
        {
            AT_CMD_REFAPP_LOG_MSG(("No password\n"));
            return CY_RSLT_AT_CMD_REF_APP_ERR;
        }

        if (strlen(cJSON_GetObjectItem(json, WCM_TOKEN_PASSWORD)->valuestring) > CY_WCM_MAX_PASSPHRASE_LEN)
        {
            AT_CMD_REFAPP_LOG_MSG(("password too long\n"));
            return CY_RSLT_AT_CMD_REF_APP_ERR;
        }

        strcpy(connect_config->password, cJSON_GetObjectItem(json, WCM_TOKEN_PASSWORD)->valuestring);
    }

    if (cJSON_HasObjectItem(json, WCM_TOKEN_MACADDR))
    {
        char *macaddr;

        macaddr = cJSON_GetObjectItem(json, WCM_TOKEN_MACADDR)->valuestring;

        sscanf(macaddr, "%x:%x:%x:%x:%x:%x", (unsigned int *)&connect_config->macaddr[0], (unsigned int *)&connect_config->macaddr[1], (unsigned int *)&connect_config->macaddr[2],
               (unsigned int *)&connect_config->macaddr[3], (unsigned int *)&connect_config->macaddr[4], (unsigned int *)&connect_config->macaddr[5]);
    }

    if (cJSON_HasObjectItem(json, WCM_TOKEN_BAND))
    {
        connect_config->band = cJSON_GetObjectItem(json, WCM_TOKEN_BAND)->valueint;
    }

    return CY_RSLT_SUCCESS;
}

/**
 * Parse the WCM commands and get individual elements of structure
 */
at_cmd_msg_base_t *at_cmd_refapp_parse_wcm_cmd(uint32_t cmd_id, uint32_t serial, uint32_t cmd_len, char *cmd)
{
    at_cmd_msg_base_t *msg = NULL;
    at_cmd_ref_app_wcm_connect_specific_t *connect_config;
    at_cmd_ref_app_wcm_get_ip_type_t *get_ip_config;
    at_cmd_ref_app_wcm_nw_change_notification_t *nw_change_notification_config;
    cy_rslt_t result;
    char *cmd_txt = cmd;
    cJSON *json;

    switch (cmd_id)
    {
    case CMD_ID_AP_DISCONNECT:
    case CMD_ID_SCAN_START:
    case CMD_ID_SCAN_STOP:
    case CMD_ID_AP_GET_INFO:
    case CMD_ID_GET_IPv4_ADDRESS:
    case CMD_ID_PING:
        /*
         * Commands with no arguments. We just need a basic config message structure.
         */
        msg = (at_cmd_msg_base_t *)malloc(sizeof(at_cmd_msg_base_t));
        if (msg == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("error allocating WCM config message\n"));
            break;
        }
        break;

    case CMD_ID_AP_CONNECT:

        json = cJSON_Parse(cmd_txt);
        if (!json)
        {
            AT_CMD_REFAPP_LOG_MSG(("error parsing the WCM connect specific\n"));
            break;
        }

        connect_config = malloc(sizeof(at_cmd_ref_app_wcm_connect_specific_t));
        if (connect_config == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("error allocating WCM connect specific message\n"));
            cJSON_Delete(json);
            break;
        }
        memset(connect_config, 0, sizeof(at_cmd_ref_app_wcm_connect_specific_t));

        result = setup_wcm_connect_config(connect_config, json);
        cJSON_Delete(json);

        if (result != CY_RSLT_SUCCESS)
        {
            free(connect_config);
            AT_CMD_REFAPP_LOG_MSG(("error WCM connect config setup\n"));
            break;
        }
        msg = (at_cmd_msg_base_t *)connect_config;

        break;

    case CMD_ID_GET_IP_ADDRESS:
        json = cJSON_Parse(cmd_txt);
        if (!json)
        {
            AT_CMD_REFAPP_LOG_MSG(("error parsing WCM get ip message \n"));
            break;
        }

        if (!cJSON_HasObjectItem(json, STR_TOKEN_ADDR_TYPE))
        {
            AT_CMD_REFAPP_LOG_MSG(("Invalid parameter set\n"));
            cJSON_Delete(json);
            break;
        }

        get_ip_config = (at_cmd_ref_app_wcm_get_ip_type_t *)malloc(sizeof(at_cmd_ref_app_wcm_get_ip_type_t));
        if (get_ip_config == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("error allocating WCM get ip config message \n"));
            cJSON_Delete(json);
            break;
        }
        memset(get_ip_config, 0, sizeof(at_cmd_ref_app_wcm_get_ip_type_t));

        get_ip_config->addr_type.version = cJSON_GetObjectItem(json, STR_TOKEN_ADDR_TYPE)->valueint;
        cJSON_Delete(json);

        if (get_ip_config->addr_type.version != CY_WCM_IP_VER_V4 && get_ip_config->addr_type.version != CY_WCM_IP_VER_V6)
        {
            AT_CMD_REFAPP_LOG_MSG(("Invalid IP address type\n"));
            free(get_ip_config);
            break;
        }
        msg = (at_cmd_msg_base_t *)get_ip_config;
        break;

    case CMD_ID_WCM_NETWORK_CHANGE_NOTIFICATION:
        json = cJSON_Parse(cmd_txt);
        if (!json)
        {
            AT_CMD_REFAPP_LOG_MSG(("error parsing WCM enable network change notification message \n"));
            break;
        }

        if (!cJSON_HasObjectItem(json, STR_TOKEN_ENABLE))
        {
            AT_CMD_REFAPP_LOG_MSG(("Invalid parameter set\n"));
            cJSON_Delete(json);
            break;
        }

        nw_change_notification_config = malloc(sizeof(at_cmd_ref_app_wcm_nw_change_notification_t));
        if (nw_change_notification_config == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("error allocating WCM network change notification message \n"));
            cJSON_Delete(json);
            break;
        }
        memset(nw_change_notification_config, 0, sizeof(at_cmd_ref_app_wcm_nw_change_notification_t));
        nw_change_notification_config->enable = cJSON_GetObjectItem(json, STR_TOKEN_ENABLE)->valueint;

        cJSON_Delete(json);

        msg = (at_cmd_msg_base_t *)nw_change_notification_config;
        break;

    default:
        AT_CMD_REFAPP_LOG_MSG(("Unimplemented cmd \n"));
        break;
    }

    if (msg)
    {
        /*
         * Set the message header fields.
         */
        msg->cmd_id = cmd_id;
        msg->serial = serial;
    }

    return msg;
}
/**
 * Process the command and create json text output
 */
cy_rslt_t at_cmd_refapp_process_wcm_host_msg(uint32_t cmd_id, at_cmd_msg_base_t *msg, char *buffer, uint32_t buflen)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    at_cmd_ref_app_host_ap_info_result_t *ap_info;
    at_cmd_ref_app_scan_result_t *scan;
    at_cmd_ref_app_wcm_get_ip_type_t *ip_msg;
    at_cmd_ref_host_ipv4_info_t *ip_info_msg;
    at_cmd_ref_app_network_change_t *nw_event_info_msg;
    at_cmd_ref_ping_ip_addr_t *ping_info;
    cJSON *cjson = NULL;
    char *json_text = NULL;
    char tmp_str[AT_CMD_REF_APP_IP_ADDR_STR_LEN];
    uint32_t len;
    int idx;

    /*
     * Create a JSON object to construct the output data.
     */
    cjson = cJSON_CreateObject();
    if (cjson == NULL)
    {
        return CY_RSLT_AT_CMD_REF_APP_ERR;
    }

    if (cmd_id == CMD_ID_HOST_WCM_SCAN_INFO)
    {
        scan = (at_cmd_ref_app_scan_result_t *)msg;
        if (scan->scan_complete)
        {
            cJSON_AddStringToObject(cjson, WCM_TOKEN_STATUS, WCM_TOKEN_COMPLETE);
        }
        else
        {
            /*
             * Create the JSON object with the results.
             */

            cJSON_AddItemToObject(cjson, WCM_TOKEN_SSID, cJSON_CreateString((const char *)scan->ssid));

            snprintf(tmp_str, sizeof(tmp_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                     scan->macaddr[0], scan->macaddr[1], scan->macaddr[2],
                     scan->macaddr[3], scan->macaddr[4], scan->macaddr[5]);

            cJSON_AddItemToObject(cjson, WCM_TOKEN_MACADDR, cJSON_CreateString((const char *)tmp_str));
            cJSON_AddNumberToObject(cjson, WCM_TOKEN_CHANNEL, scan->channel);
            cJSON_AddNumberToObject(cjson, WCM_TOKEN_BAND, scan->band);
            cJSON_AddNumberToObject(cjson, WCM_TOKEN_SIGNAL_STRENGTH, scan->signal_strength);

            idx = at_cmd_refapp_security_table_lookup_by_value(scan->security_type, security_table);
            cJSON_AddStringToObject(cjson, WCM_TOKEN_SECURITY_TYPE, idx >= 0 ? security_table[idx].cmd_name : WCM_TOKEN_UNKNOWN);
            cJSON_AddStringToObject(cjson, WCM_TOKEN_STATUS, WCM_TOKEN_INCOMPLETE);
        }
    }
    else if (cmd_id == CMD_ID_AP_GET_INFO)
    {
        ap_info = (at_cmd_ref_app_host_ap_info_result_t *)msg;

        /*
         * Create the JSON object with the results.
         */

        cJSON_AddItemToObject(cjson, WCM_TOKEN_SSID, cJSON_CreateString((const char *)ap_info->ssid));

        snprintf(tmp_str, sizeof(tmp_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 ap_info->bssid[0], ap_info->bssid[1], ap_info->bssid[2],
                 ap_info->bssid[3], ap_info->bssid[4], ap_info->bssid[5]);

        cJSON_AddItemToObject(cjson, WCM_TOKEN_BSSID, cJSON_CreateString((const char *)tmp_str));
        cJSON_AddNumberToObject(cjson, WCM_TOKEN_CHANNEL, ap_info->channel);
        cJSON_AddNumberToObject(cjson, WCM_TOKEN_CHANNEL_WIDTH, ap_info->channel_width);
        cJSON_AddNumberToObject(cjson, WCM_TOKEN_SIGNAL_STRENGTH, ap_info->signal_strength);

        idx = at_cmd_refapp_security_table_lookup_by_value(ap_info->security_type, security_table);
        AT_CMD_REFAPP_LOG_MSG(("CMD_ID_AP_GET_INFO idx:%d security_type:%llx\n", idx, ap_info->security_type));

        cJSON_AddStringToObject(cjson, WCM_TOKEN_SECURITY_TYPE, idx >= 0 ? security_table[idx].cmd_name : WCM_TOKEN_UNKNOWN);
    }
    else if (cmd_id == CMD_ID_GET_IP_ADDRESS)
    {
        ip_msg = (at_cmd_ref_app_wcm_get_ip_type_t *)msg;

        sprintf(tmp_str, "%d.%d.%d.%d", PRINT_IP(ip_msg->addr_type.ip.v4));
        cJSON_AddStringToObject(cjson, STR_TOKEN_IP_ADDRESS, tmp_str);
    }
    else if (cmd_id == CMD_ID_PING)
    {
        ping_info = (at_cmd_ref_ping_ip_addr_t *)msg;
        cJSON_AddNumberToObject(cjson, STR_TOKEN_ELAPSED_TIME, ping_info->elapsed_time);
    }
    else if (cmd_id == CMD_ID_GET_IPv4_ADDRESS)
    {
        ip_info_msg = (at_cmd_ref_host_ipv4_info_t *)msg;

        cJSON_AddStringToObject(cjson, WCM_TOKEN_METHOD, ip_info_msg->dhcp ? WCM_TOKEN_DHCP : WCM_TOKEN_STATIC);

        sprintf(tmp_str, "%d.%d.%d.%d", PRINT_IP(ip_info_msg->address));
        cJSON_AddStringToObject(cjson, STR_TOKEN_IP_ADDRESS, tmp_str);

        sprintf(tmp_str, "%d.%d.%d.%d", PRINT_IP(ip_info_msg->netmask));
        cJSON_AddStringToObject(cjson, WCM_TOKEN_NETMASK, tmp_str);

        sprintf(tmp_str, "%d.%d.%d.%d", PRINT_IP(ip_info_msg->gateway));
        cJSON_AddStringToObject(cjson, WCM_TOKEN_GATEWAY, tmp_str);

        sprintf(tmp_str, "%d.%d.%d.%d", PRINT_IP(ip_info_msg->primary));
        cJSON_AddStringToObject(cjson, WCM_TOKEN_PRIMARY_DNS, tmp_str);

        sprintf(tmp_str, "%d.%d.%d.%d", PRINT_IP(ip_info_msg->secondary));
        cJSON_AddStringToObject(cjson, WCM_TOKEN_SECONDARY_DNS, tmp_str);
    }
    else if (cmd_id == CMD_ID_WCM_NETWORK_CHANGE_NOTIFICATION)
    {
        nw_event_info_msg = (at_cmd_ref_app_network_change_t *)msg;
        cJSON_AddNumberToObject(cjson, WCM_TOKEN_NW_STATUS, nw_event_info_msg->event);
    }
    else
    {
        AT_CMD_REFAPP_LOG_MSG(("Unimplemented cmd: 0x%04lx\n", cmd_id));
        cJSON_Delete(cjson);
        cjson = NULL;
    }

    /*
     * Do we have a JSON object to output?
     */
    if (cjson != NULL)
    {
        /*
         * Create the output JSON text and copy it to the output buffer.
         */
        json_text = cJSON_PrintUnformatted(cjson);
        cJSON_Delete(cjson);

        if (json_text)
        {
            len = strlen(json_text) + 1;
            len = len > buflen ? buflen - 1 : len;
            memcpy(buffer, json_text, len);
            free(json_text);
        }
        else
        {
            AT_CMD_REFAPP_LOG_MSG(("error alloc json text\n"));
            result = CY_RSLT_AT_CMD_REF_APP_ERR;
        }
    }

    return result;
}

/*
 * Callback function which receives the scan results.
 */
static void wifi_scan_handler(cy_wcm_scan_result_t *result_ptr, void *user_data, cy_wcm_scan_status_t status)
{
    at_cmd_ref_app_scan_result_t *msg;

    msg = (at_cmd_ref_app_scan_result_t *)malloc(sizeof(at_cmd_ref_app_scan_result_t));
    if (msg == NULL)
    {
        AT_CMD_REFAPP_LOG_MSG(("wifi_scan_handler malloc failed\n"));
        return;
    }
    memset(msg, 0, sizeof(at_cmd_ref_app_scan_result_t));
    msg->base.serial = (uint32_t)user_data;
    msg->base.cmd_id = CMD_ID_HOST_WCM_SCAN_INFO;
    if (status == CY_WCM_SCAN_INCOMPLETE)
    {
        /*
         * Copy the scan result into the message structure.
         */
        msg->ssid_length = strlen((char *)result_ptr->SSID);
        memcpy(msg->ssid, result_ptr->SSID, msg->ssid_length);
        memcpy(msg->macaddr, result_ptr->BSSID, CY_WCM_MAC_ADDR_LEN);
        msg->channel = result_ptr->channel;
        msg->band = result_ptr->band;
        msg->signal_strength = result_ptr->signal_strength;
        msg->security_type = result_ptr->security;
    }
    else
    {
        /*
         * Just mark that the scan is complete.
         */
        msg->scan_complete = true;
    }
    if (at_cmd_refapp_send_message((at_cmd_msg_base_t *)msg) != CY_RSLT_SUCCESS)
    {
        AT_CMD_REFAPP_LOG_MSG(("error sending at_cmd_refapp_send_message!!!\n"));
        free(msg);
    }
    else
    {
        AT_CMD_REFAPP_LOG_MSG(("posted message cmd_id:%ld \n", msg->base.cmd_id));
    }
}

/**
 *  event callback for handling wcm connection events
 */
static void network_event_change_callback(cy_wcm_event_t event, cy_wcm_event_data_t *event_data)
{
    at_cmd_ref_app_network_change_t *msg = NULL;
    at_cmd_msg_base_t *at_cmd_msg = NULL;

    AT_CMD_REFAPP_LOG_MSG(("Received WCM event = %d\n", event));

    msg = calloc(1, sizeof(at_cmd_ref_app_network_change_t));
    if (msg == NULL)
    {
        AT_CMD_REFAPP_LOG_MSG(("alloc at_cmd_ref_app_network_change_t failed\n"));
        return;
    }

    memset(msg, 0, sizeof(at_cmd_ref_app_network_change_t));
    at_cmd_msg = &msg->base;
    at_cmd_msg->cmd_id = CMD_ID_WCM_NETWORK_CHANGE_NOTIFICATION;
    at_cmd_msg->serial = CMD_ID_WCM_NETWORK_CHANGE_NOTIFICATION;
    msg->event = event;

    if (msg->event == CY_WCM_EVENT_IP_CHANGED)
    {
        memcpy(&(msg->ip_addr), &(event_data->ip_addr), sizeof(cy_wcm_ip_address_t));
    }

    if (at_cmd_refapp_send_message((at_cmd_msg_base_t *)msg) != CY_RSLT_SUCCESS)
    {
        AT_CMD_REFAPP_LOG_MSG(("error sending at_cmd_refapp_send_message!!!\n"));
        free(msg);
    }
}
/** look up table to get security type base on name */
int at_cmd_refapp_security_table_lookup_by_name(char *cmd_name, at_cmd_security_def_t *table)
{
    int idx = -1;
    int i;
    if (table)
    {
        for (i = 0; table[i].cmd_name != NULL; i++)
        {
            if ((strncmp(table[i].cmd_name, cmd_name, strlen(table[i].cmd_name)) == 0) &&
                (strlen(table[i].cmd_name) == strlen(cmd_name)))
            {
                idx = i;
                break;
            }
        }
    }
    return idx;
}

/** look up table to get security type base on security value */
int at_cmd_refapp_security_table_lookup_by_value(uint32_t value, at_cmd_security_def_t *table)
{
    int idx = -1;
    int i;

    if (table)
    {
        for (i = 0; table[i].cmd_name != NULL; i++)
        {
            if (table[i].cmd_id == value)
            {
                idx = i;
                break;
            }
        }
    }
    return idx;
}

/**
 * Process WiFi commands
 */
at_cmd_msg_base_t *at_cmd_refapp_wcm_process_message(at_cmd_msg_base_t *msg, at_cmd_result_data_t *result_str)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint32_t cmd_id;
    char *response_text = NULL;
    cy_wcm_associated_ap_info_t ap_info;
    at_cmd_ref_app_host_ap_info_result_t *ap_msg = NULL;
    at_cmd_ref_host_ipv4_info_t *ip_info_msg = NULL;
    at_cmd_msg_base_t *at_cmd_msg = NULL;
    at_cmd_ref_app_wcm_get_ip_type_t *ip_msg = NULL;
    at_cmd_ref_ping_ip_addr_t *ping_info = NULL;

    cmd_id = msg->cmd_id;

    switch (cmd_id)
    {
    case CMD_ID_AP_CONNECT:
    {
        at_cmd_ref_app_wcm_connect_specific_t *ptr = (at_cmd_ref_app_wcm_connect_specific_t *)msg;
        cy_wcm_connect_params_t connect_param;
        cy_wcm_ip_address_t ip_addr;
        char ip_addr_str[AT_CMD_REF_APP_IP_ADDR_STR_LEN];

        /*
         * Setup WCM connect parameters.
         */
        memset(&connect_param, 0, sizeof(cy_wcm_connect_params_t));
        memcpy(connect_param.ap_credentials.SSID, ptr->ssid, ptr->ssid_length);
        connect_param.ap_credentials.security = ptr->security_type;

        if (connect_param.ap_credentials.security != CY_WCM_SECURITY_OPEN)
        {
            memcpy(connect_param.ap_credentials.password, ptr->password, strlen(ptr->password) + 1);
        }
        else if (connect_param.ap_credentials.security == CY_WCM_SECURITY_WPA2_AES_PSK)
        {
            AT_CMD_REFAPP_LOG_MSG(("auth type is set to CY_WCM_SECURITY_WPA2_AES_PSK\n"));
        }

        if (!NULL_MAC(ptr->macaddr))
        {
            memcpy(connect_param.BSSID, ptr->macaddr, CY_WCM_MAC_ADDR_LEN);
        }
        connect_param.band = ptr->band;
        /*
         * Connect to AP.
         */
        result = cy_wcm_connect_ap(&connect_param, &ip_addr);
        if (result == CY_RSLT_SUCCESS)
        {
            sprintf(ip_addr_str, "%d.%d.%d.%d", PRINT_IP(ip_addr.ip.v4));
            AT_CMD_REFAPP_LOG_MSG(("network Connection successful IP:%s \n", ip_addr_str));
        }
        else
        {
            response_text = "wcm-error";
            AT_CMD_REFAPP_LOG_MSG(("network Connection failed %lx\n", result));
            strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
            result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
        }
        break;
    }

    case CMD_ID_SCAN_START:
    {
        result = cy_wcm_start_scan(wifi_scan_handler, (void *)((at_cmd_msg_base_t *)msg)->serial, NULL);

        if (result != CY_RSLT_SUCCESS)
        {
            response_text = "wcm-error";
            AT_CMD_REFAPP_LOG_MSG(("Start Scan failed\n"));
            strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
            result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
        }
        break;
    }

    case CMD_ID_SCAN_STOP:
    {
        result = cy_wcm_stop_scan();
        if (result != CY_RSLT_SUCCESS)
        {
            response_text = "no-active-scan";
            AT_CMD_REFAPP_LOG_MSG(("Stop Scan failed %lx\n", result));
            strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
            result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
            ;
        }
        break;
    }

    case CMD_ID_AP_GET_INFO:
    {
        if (cy_wcm_is_connected_to_ap())
        {
            memset(&ap_info, 0, sizeof(ap_info));
            result = cy_wcm_get_associated_ap_info(&ap_info);

            if (result == CY_RSLT_SUCCESS)
            {
                ap_msg = (at_cmd_ref_app_host_ap_info_result_t *)calloc(1, sizeof(at_cmd_ref_app_host_ap_info_result_t));
                if (ap_msg == NULL)
                {
                    AT_CMD_REFAPP_LOG_MSG(("memory error"));
                    response_text = "memory error";
                    strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
                    result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
                    return NULL;
                }

                ap_msg->ssid_length = strlen((char *)ap_info.SSID);
                memcpy(ap_msg->ssid, ap_info.SSID, ap_msg->ssid_length);
                memcpy(ap_msg->bssid, ap_info.BSSID, CY_WCM_MAC_ADDR_LEN);

                ap_msg->channel = ap_info.channel;
                ap_msg->channel_width = ap_info.channel_width;
                ap_msg->signal_strength = ap_info.signal_strength;
                ap_msg->security_type = ap_info.security;

                ap_msg->base.serial = ((at_cmd_msg_base_t *)msg)->serial;
                ap_msg->base.cmd_id = CMD_ID_AP_GET_INFO;

                at_cmd_msg = (at_cmd_msg_base_t *)ap_msg;
            }
            else
            {
                AT_CMD_REFAPP_LOG_MSG(("Get AP info failed\n"));
                response_text = "not connected error";
                strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
                result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
            }
        }
        else
        {
            AT_CMD_REFAPP_LOG_MSG(("Not connected to AP\n"));
            response_text = "not connected error";
            strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
            result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
        }
        break;
    }

    case CMD_ID_AP_DISCONNECT:
    {
        /*
         * Disconnect from connected AP.
         */
        result = cy_wcm_disconnect_ap();
        if (result == CY_RSLT_SUCCESS)
        {
            AT_CMD_REFAPP_LOG_MSG(("network Disconnection successful\n"));
        }
        else
        {
            response_text = "wcm-error";
            AT_CMD_REFAPP_LOG_MSG(("network Disconnection failed %lx\n", result));
            strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
            result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
        }
        break;
    }

    case CMD_ID_GET_IP_ADDRESS:
    {
        if (cy_wcm_is_connected_to_ap())
        {
            at_cmd_ref_app_wcm_get_ip_type_t *ptr = (at_cmd_ref_app_wcm_get_ip_type_t *)msg;

            if (ptr->addr_type.version == CY_WCM_IP_VER_V4)
            {
                ip_msg = (at_cmd_ref_app_wcm_get_ip_type_t *)calloc(1, sizeof(at_cmd_ref_app_wcm_get_ip_type_t));
                if (ip_msg == NULL)
                {
                    AT_CMD_REFAPP_LOG_MSG(("memory error"));
                    response_text = "memory error";
                    strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
                    result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
                    return NULL;
                }

                /*
                 * Retrieve IPv4 address from WCM library.
                 */
                result = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_STA, &ip_msg->addr_type);
                if (result == CY_RSLT_SUCCESS)
                {
                    AT_CMD_REFAPP_LOG_MSG(("WCM GetIP address successful\n"));
                    ip_msg->base.serial = ptr->base.serial;
                    ip_msg->base.cmd_id = CMD_ID_GET_IP_ADDRESS;
                    at_cmd_msg = (at_cmd_msg_base_t *)ip_msg;
                }
                else
                {
                    free(ip_msg);
                    AT_CMD_REFAPP_LOG_MSG(("WCM GetIP address failed %lx\n", result));
                    response_text = "not connected error";
                    strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
                    result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
                }
            }
            else
            {
                AT_CMD_REFAPP_LOG_MSG(("IPV6 not supported %lx\n", result));
                response_text = "not connected error";
                strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
                result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
            }
        }
        else
        {
            AT_CMD_REFAPP_LOG_MSG(("Not connected to AP\n"));
            response_text = "not connected error";
            strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
            result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
        }
        break;
    }

    case CMD_ID_GET_IPv4_ADDRESS:
    {
        if (cy_wcm_is_connected_to_ap())
        {
            cy_wcm_ip_address_t ip_addr;

            ip_info_msg = (at_cmd_ref_host_ipv4_info_t *)calloc(1, sizeof(at_cmd_ref_host_ipv4_info_t));
            if (ip_info_msg == NULL)
            {
                AT_CMD_REFAPP_LOG_MSG(("memory error"));
                response_text = "memory error";
                strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
                result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
                return NULL;
            }

            /*
             * For now setting method dhcp always. Need to fix it once the static method is supported.
             */

            ip_info_msg->dhcp = true;

            /*
             * Retrieve IPv4 address from WCM library.
             */
            result = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
            if (result == CY_RSLT_SUCCESS)
            {
                AT_CMD_REFAPP_LOG_MSG(("WCM GetIP address successful\n"));
                ip_info_msg->address = ip_addr.ip.v4;
            }

            /*
             * Retrieve netmask address from WCM library.
             */
            result = cy_wcm_get_ip_netmask(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
            if (result == CY_RSLT_SUCCESS)
            {
                AT_CMD_REFAPP_LOG_MSG(("WCM Netmask address successful\n"));
                ip_info_msg->netmask = ip_addr.ip.v4;
            }

            /*
             * Retrieve gateway address from WCM library.
             */
            result = cy_wcm_get_gateway_ip_address(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
            if (result == CY_RSLT_SUCCESS)
            {
                AT_CMD_REFAPP_LOG_MSG(("WCM Get gateway address successful\n"));
                ip_info_msg->gateway = ip_addr.ip.v4;
            }

            ip_info_msg->primary = 0;
            ip_info_msg->base.serial = ((at_cmd_msg_base_t *)msg)->serial;
            ip_info_msg->base.cmd_id = CMD_ID_GET_IPv4_ADDRESS;
            at_cmd_msg = (at_cmd_msg_base_t *)ip_info_msg;
        }
        else
        {
            response_text = "not connected error";
            AT_CMD_REFAPP_LOG_MSG(("not connected error"));
            strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
            result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
        }
        break;
    }

    case CMD_ID_WCM_NETWORK_CHANGE_NOTIFICATION:
    {
        at_cmd_ref_app_network_change_t *ptr = (at_cmd_ref_app_network_change_t *)msg;
        ptr->base.cmd_id = CMD_ID_WCM_NETWORK_CHANGE_NOTIFICATION;
        ptr->base.serial = msg->serial;
        at_cmd_msg = (at_cmd_msg_base_t *)ptr;
        break;
    }

    case CMD_ID_HOST_WCM_SCAN_INFO:
    {
        at_cmd_ref_app_scan_result_t *ptr = (at_cmd_ref_app_scan_result_t *)msg;
        ptr->base.cmd_id = CMD_ID_HOST_WCM_SCAN_INFO;
        ptr->base.serial = msg->serial;
        at_cmd_msg = (at_cmd_msg_base_t *)ptr;
        break;
    }

    case CMD_ID_PING:
        if (cy_wcm_is_connected_to_ap())
        {
            cy_wcm_ip_address_t ip_addr;
            uint32_t timeout_ms = AT_CMD_REF_APP_IP_ADDR_STR_LEN;

            ping_info = (at_cmd_ref_ping_ip_addr_t *)calloc(1, sizeof(at_cmd_ref_ping_ip_addr_t));

            if (ping_info == NULL)
            {
                AT_CMD_REFAPP_LOG_MSG(("memory error"));
                response_text = "memory error";
                strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
                result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
                return NULL;
            }

            memset(ping_info, 0, sizeof(at_cmd_ref_ping_ip_addr_t));

            /* Retrieve gateway address from WCM library */
            result = cy_wcm_get_gateway_ip_address(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
            if (result == CY_RSLT_SUCCESS)
            {
                AT_CMD_REFAPP_LOG_MSG(("WCM Get gateway address successful\n"));
                result = cy_wcm_ping(CY_WCM_INTERFACE_TYPE_STA, &ip_addr, timeout_ms, &ping_info->elapsed_time);
                if (result != CY_RSLT_SUCCESS)
                {
                    AT_CMD_REFAPP_LOG_MSG(("Ping failed result:%lx\n", result));
                }
            }
            else
            {
                response_text = "ping error";
                strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
                result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
            }
        }
        else
        {
            AT_CMD_REFAPP_LOG_MSG(("Not connected to AP\n"));
            response_text = "not connected error";
            strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
            result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
        }
        at_cmd_msg = (at_cmd_msg_base_t *)ping_info;
        break;

    default:
    {
        AT_CMD_REFAPP_LOG_MSG(("invalid command id\n\r"));
        break;
    }
    }

    return at_cmd_msg;
}
/* [] END OF FILE */
