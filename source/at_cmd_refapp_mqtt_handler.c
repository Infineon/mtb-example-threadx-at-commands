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
 * @file at_cmd_refapp_mqtt_handler.c
 * @brief Main file for the MQTT handler.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "cy_result.h"
#include "cyabs_rtos.h"
#include "at_cmd_refapp.h"
#include "cy_wcm.h"

static cy_linked_list_t g_mqtt_server_list;
cy_linked_list_node_t *g_node_found = NULL;
bool is_mqtt_initialized = false;

/*String that describes the MQTT handle that is being created in order to uniquely identify it*/
#define MQTT_HANDLE_DESCRIPTOR "MQTThandleID"

/******************************************************
 *               Static Function Declarations
 ******************************************************/
at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_cmd(uint32_t cmd_id, uint32_t serial, uint32_t cmd_len, char *cmd);
static bool at_cmd_refapp_mqtt_find_item(cy_linked_list_node_t *node_to_compare, void *user_data);
static void at_cmd_refapp_create_mqtt_broker_info(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server, at_cmd_ref_app_mqtt_define_server_t *server_config);
static cy_rslt_t at_cmd_refapp_mqtt_connect(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server);
static cy_rslt_t at_cmd_refapp_mqtt_disconnect(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server);
static cy_rslt_t at_cmd_refapp_mqtt_publish(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server, at_cmd_ref_app_mqtt_publish_t *publish);
static cy_rslt_t at_cmd_refapp_mqtt_subscribe(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server, at_cmd_ref_app_mqtt_subscribe_t *subscribe);
static cy_rslt_t at_cmd_refapp_mqtt_unsubscribe(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server, at_cmd_ref_app_mqtt_unsubscribe_t *unsubscribe);
static void at_cmd_refapp_mqtt_event_cb(cy_mqtt_t mqtt_handle, cy_mqtt_event_t event, void *user_data);
static cy_rslt_t at_cmd_refapp_mqtt_server_config(at_cmd_ref_app_mqtt_define_server_t *server_config, cJSON *json);
at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_broker_id(char *cmd_txt, uint32_t cmd_id);
at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_unsubscribe(char *cmd_txt, uint32_t cmd_id);
at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_subscribe(char *cmd_txt, uint32_t cmd_id);
at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_publish(char *cmd_txt, uint32_t cmd_id);
at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_define_server(char *cmd_txt, uint32_t cmd_id);
static cy_linked_list_node_t *at_cmd_refapp_find_broker_id(uint32_t broker_id);
static cy_rslt_t at_cmd_refapp_cleanup_broker(at_cmd_ref_app_mqtt_broker_info_t *broker);
static void at_cmd_refapp_mqtt_set_result_string(char *response_text, at_cmd_result_data_t *result_str);

/******************************************************
 *               Function Definitions
 ******************************************************/

cy_rslt_t at_cmd_refapp_mqtt_init(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if (is_mqtt_initialized == false)
    {
        /* Initialize the MQTT library. */
        result = cy_mqtt_init();
        if (result == CY_RSLT_SUCCESS)
        {
            is_mqtt_initialized = true;
        }
    }
    /*
     * Create linked list to keep track of the mqtt server list.
     */
    cy_linked_list_init(&g_mqtt_server_list);
    return result;
}

void at_cmd_refapp_build_mqtt_json_text_to_host(uint32_t cmd_id, uint32_t serial, at_cmd_msg_base_t *cmd, at_cmd_result_data_t *result_str)
{
    at_cmd_msg_base_t *host_resp_msg = NULL;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    switch (cmd_id)
    {
    case CMD_ID_MQTT_DEFINE_BROKER:
    case CMD_ID_MQTT_GET_BROKER:
    case CMD_ID_MQTT_DELETE_BROKER:
    case CMD_ID_MQTT_CONNECT_BROKER:
    case CMD_ID_MQTT_DISCONNECT_BROKER:
    case CMD_ID_MQTT_SUBSCRIBE:
    case CMD_ID_MQTT_UNSUBSCRIBE:
    case CMD_ID_MQTT_PUBLISH:
        host_resp_msg = at_cmd_refapp_mqtt_process_message((at_cmd_msg_base_t *)cmd, result_str);
        if (host_resp_msg != NULL)
        {
            /* process host message */
            result = at_cmd_refapp_process_mqtt_host_msg(cmd_id, host_resp_msg, result_str->result_text, sizeof(result_str->result_text));
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

at_cmd_msg_base_t *at_cmd_refapp_mqtt_process_message(at_cmd_msg_base_t *msg, at_cmd_result_data_t *result_str)
{
    at_cmd_msg_base_t *at_cmd_msg = NULL;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    at_cmd_ref_app_mqtt_find_item_t item;
    at_cmd_ref_app_mqtt_brokerid_t *brokerid = NULL;
    at_cmd_ref_app_mqtt_define_server_t *mqtt_define_server = NULL;
    at_cmd_ref_app_mqtt_broker_info_t *mqtt_broker_info = NULL;
    at_cmd_ref_app_mqtt_subscribe_t *subscribe = NULL;
    at_cmd_ref_app_mqtt_unsubscribe_t *unsubscribe = NULL;
    at_cmd_ref_app_mqtt_publish_t *publish = NULL;
    char *response_text = NULL;

    memset(&item, 0, sizeof(at_cmd_ref_app_mqtt_find_item_t));

    switch (msg->cmd_id)
    {
    case CMD_ID_MQTT_GET_BROKER:
    {
        brokerid = (at_cmd_ref_app_mqtt_brokerid_t *)msg;
        cy_linked_list_node_t *nodeptr = NULL;
        nodeptr = at_cmd_refapp_find_broker_id(brokerid->brokerid);
        if (nodeptr == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found \n"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        mqtt_broker_info = (at_cmd_ref_app_mqtt_broker_info_t *)nodeptr->data;

        if (mqtt_broker_info == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found \n"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        at_cmd_msg = (at_cmd_msg_base_t *)mqtt_broker_info;
        mqtt_broker_info->base.cmd_id = msg->cmd_id;
        mqtt_broker_info->base.serial = msg->cmd_id;
        break;
    }

    case CMD_ID_MQTT_DEFINE_BROKER:
    {
        mqtt_define_server = (at_cmd_ref_app_mqtt_define_server_t *)msg;
        /*
         * Add the mqtt logical server to mqtt server list.
         */

        mqtt_broker_info = malloc(sizeof(at_cmd_ref_app_mqtt_broker_info_t));
        if (mqtt_broker_info == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory-error"));
            response_text = "memory-error";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        memset(mqtt_broker_info, 0, sizeof(at_cmd_ref_app_mqtt_broker_info_t));
        mqtt_broker_info->base.cmd_id = msg->cmd_id;
        mqtt_broker_info->base.serial = msg->cmd_id;

        at_cmd_refapp_create_mqtt_broker_info(mqtt_broker_info, mqtt_define_server);
        cy_linked_list_set_node_data(&mqtt_broker_info->node, mqtt_broker_info);
        cy_linked_list_insert_node_at_rear(&g_mqtt_server_list, &mqtt_broker_info->node);

        AT_CMD_REFAPP_LOG_MSG(("mqtt_server hostname:%s port:%d clientid:%s username:%s password:%s\n",
                               mqtt_broker_info->hostname, mqtt_broker_info->port, mqtt_broker_info->clientid, mqtt_broker_info->username, mqtt_broker_info->password));
        break;
    }

    case CMD_ID_MQTT_CONNECT_BROKER:
    {
        brokerid = (at_cmd_ref_app_mqtt_brokerid_t *)msg;
        cy_linked_list_node_t *nodeptr = NULL;
        nodeptr = at_cmd_refapp_find_broker_id(brokerid->brokerid);
        if (nodeptr == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        mqtt_broker_info = (at_cmd_ref_app_mqtt_broker_info_t *)nodeptr->data;

        if (mqtt_broker_info == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        AT_CMD_REFAPP_LOG_MSG(("mqtt hostname:%s \n", mqtt_broker_info->hostname));
        result = at_cmd_refapp_mqtt_connect(mqtt_broker_info);
        if (result != CY_RSLT_SUCCESS)
        {
            AT_CMD_REFAPP_LOG_MSG(("MQTT Connection failed \n"));
            response_text = "MQTT Connection failed";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        AT_CMD_REFAPP_LOG_MSG(("MQTT Connection successful \n"));
        break;
    }

    case CMD_ID_MQTT_DISCONNECT_BROKER:
    {
        brokerid = (at_cmd_ref_app_mqtt_brokerid_t *)msg;
        cy_linked_list_node_t *nodeptr = NULL;
        nodeptr = at_cmd_refapp_find_broker_id(brokerid->brokerid);
        if (nodeptr == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        mqtt_broker_info = (at_cmd_ref_app_mqtt_broker_info_t *)nodeptr->data;
        if (mqtt_broker_info == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        result = at_cmd_refapp_mqtt_disconnect(mqtt_broker_info);
        if (result != CY_RSLT_SUCCESS)
        {
            AT_CMD_REFAPP_LOG_MSG(("MQTT Disconnect failed \n"));
            response_text = "MQTT Disconnect failed";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        AT_CMD_REFAPP_LOG_MSG(("MQTT Disconnect successful \n"));
        mqtt_broker_info->base.cmd_id = msg->cmd_id;
        mqtt_broker_info->base.serial = msg->cmd_id;
        break;
    }

    case CMD_ID_MQTT_DELETE_BROKER:
    {
        brokerid = (at_cmd_ref_app_mqtt_brokerid_t *)msg;
        cy_linked_list_node_t *nodeptr = NULL;
        nodeptr = at_cmd_refapp_find_broker_id(brokerid->brokerid);
        if (nodeptr == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        mqtt_broker_info = (at_cmd_ref_app_mqtt_broker_info_t *)nodeptr->data;
        if (mqtt_broker_info == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        if (mqtt_broker_info == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        /*
         * disconnect server
         */
        result = at_cmd_refapp_mqtt_disconnect(mqtt_broker_info);
        if (result != CY_RSLT_SUCCESS)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt disconnect failed"));
            response_text = "mqtt disconnect failed";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
        }

        /*
         * Delete the server from the list.
         */
        cy_linked_list_remove_node(&g_mqtt_server_list, g_node_found);
        at_cmd_refapp_mqtt_init();

        /*
         * Free the memory allocated for server
         */
        at_cmd_refapp_cleanup_broker(mqtt_broker_info);
        AT_CMD_REFAPP_LOG_MSG(("MQTT Delete broker successful \n"));
        break;
    }

    case CMD_ID_MQTT_SUBSCRIBE:
    {
        subscribe = (at_cmd_ref_app_mqtt_subscribe_t *)msg;

        if (subscribe == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt subscribe info not found"));
            response_text = "mqtt subscribe info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        cy_linked_list_node_t *nodeptr = NULL;
        nodeptr = at_cmd_refapp_find_broker_id(subscribe->brokerid);
        if (nodeptr == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        mqtt_broker_info = (at_cmd_ref_app_mqtt_broker_info_t *)nodeptr->data;
        if (mqtt_broker_info == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        if (subscribe->topic == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("MQTT Subscribe failed, topic NULL \n"));
            response_text = "MQTT Subscribe failed, topic NULL";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        result = at_cmd_refapp_mqtt_subscribe(mqtt_broker_info, subscribe);
        if (result != CY_RSLT_SUCCESS)
        {
            AT_CMD_REFAPP_LOG_MSG(("MQTT Subscribe failed"));
            response_text = "MQTT Subscribe failed";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        if (subscribe->topic != NULL)
        {
            free(subscribe->topic);
        }
        break;
    }

    case CMD_ID_MQTT_UNSUBSCRIBE:
    {
        unsubscribe = (at_cmd_ref_app_mqtt_unsubscribe_t *)msg;
        
        if (unsubscribe == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("unsubscribe info not found"));
            response_text = "unsubscribe info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        cy_linked_list_node_t *nodeptr = NULL;
        nodeptr = at_cmd_refapp_find_broker_id(unsubscribe->brokerid);
        if (nodeptr == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        mqtt_broker_info = (at_cmd_ref_app_mqtt_broker_info_t *)nodeptr->data;
        if (mqtt_broker_info == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        if (unsubscribe->topic == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("MQTT UnSubscribe failed, topic NULL \n"));
            response_text = "MQTT UnSubscribe failed, topic NULL";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        result = at_cmd_refapp_mqtt_unsubscribe(mqtt_broker_info, unsubscribe);
        if (result != CY_RSLT_SUCCESS)
        {
            AT_CMD_REFAPP_LOG_MSG(("MQTT UnSubscribe failed \n"));
            response_text = "MQTT UnSubscribe failed";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        if (unsubscribe->topic != NULL)
        {
            free(unsubscribe->topic);
        }
        break;
    }

    case CMD_ID_MQTT_PUBLISH:
    {

        publish = (at_cmd_ref_app_mqtt_publish_t *)msg;

        if (publish == NULL)
        {
            response_text = "mqtt publish info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        cy_linked_list_node_t *nodeptr = NULL;

        nodeptr = at_cmd_refapp_find_broker_id(publish->brokerid);

        if (nodeptr == NULL)
        {
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        mqtt_broker_info = (at_cmd_ref_app_mqtt_broker_info_t *)nodeptr->data;

        if (mqtt_broker_info == NULL)
        {
            response_text = "mqtt broker info not found";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        if (publish->topic == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("MQTT Publish failed, topic NULL \n"));
            response_text = "MQTT Publish failed, topic NULL";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        if (publish->msg == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("MQTT Publish failed, msg NULL \n"));
            response_text = "MQTT Publish failed, msg NULL";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }

        result = at_cmd_refapp_mqtt_publish(mqtt_broker_info, publish);
        if (result != CY_RSLT_SUCCESS)
        {
            AT_CMD_REFAPP_LOG_MSG(("MQTT Publish failed \n"));
            response_text = "MQTT Publish failed";
            at_cmd_refapp_mqtt_set_result_string(response_text, result_str);
            return NULL;
        }
        if (publish->topic != NULL)
        {
            free(publish->topic);
        }
        if (publish->msg != NULL)
        {
            free(publish->msg);
        }
        break;
    }

    default:
    {
        AT_CMD_REFAPP_LOG_MSG(("at_cmd_refapp_mqtt_process_message unknown command id:%ld!! \n", msg->cmd_id));
        break;
    }

    } /* end of switch */

    return at_cmd_msg;
}

static void at_cmd_refapp_mqtt_set_result_string(char *response_text, at_cmd_result_data_t *result_str)
{
    AT_CMD_REFAPP_LOG_MSG((response_text));
    strncpy(result_str->result_text, response_text, strlen(response_text) + 1);
    result_str->result_status = AT_CMD_REF_APP_RESULT_STATUS_ERROR;
}

static cy_rslt_t at_cmd_refapp_cleanup_broker(at_cmd_ref_app_mqtt_broker_info_t *broker)
{
    if (broker->cert != NULL)
    {
        free(broker->cert);
    }
    if (broker->clientid != NULL)
    {
        free(broker->clientid);
    }
    if (broker->hostname != NULL)
    {
        free(broker->hostname);
    }
    if (broker->key != NULL)
    {
        free(broker->key);
    }
    if (broker->lastwillmessage != NULL)
    {
        free(broker->lastwillmessage);
    }
    if (broker->lastwilltopic != NULL)
    {
        free(broker->lastwilltopic);
    }
    if (broker->password != NULL)
    {
        free(broker->password);
    }
    if (broker->rootca != NULL)
    {
        free(broker->rootca);
    }
    if (broker->username != NULL)
    {
        free(broker->username);
    }
    if (broker->mqtt_buffer != NULL)
    {
        free(broker->mqtt_buffer);
    }

    free(broker);
    return CY_RSLT_SUCCESS;
}

static cy_linked_list_node_t *at_cmd_refapp_find_broker_id(uint32_t broker_id)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    at_cmd_ref_app_mqtt_find_item_t item;

    item.id = broker_id;
    item.type = AT_CMD_REF_APP_MQTT_FIND_SERVER;

    result = cy_linked_list_find_node(&g_mqtt_server_list, at_cmd_refapp_mqtt_find_item, (void *)&item, &g_node_found);
    if (result != CY_RSLT_SUCCESS)
    {
        AT_CMD_REFAPP_LOG_MSG(("at_cmd_refapp_find_broker_id failed count:%ld brokerid:%ld  input broker_id:%ld\n",
                               g_mqtt_server_list.count, item.id, broker_id));
        AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
        return NULL;
    }
    return g_node_found;
}

cy_rslt_t at_cmd_refapp_process_mqtt_host_msg(uint32_t cmd_id, at_cmd_msg_base_t *msg, char *buffer, uint32_t buflen)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cJSON *cjson = NULL;
    char *json_text = NULL;
    uint32_t len;

    /*
     * Create a JSON object to construct the output data.
     */
    cjson = cJSON_CreateObject();
    if (cjson == NULL)
    {
        return CY_RSLT_AT_CMD_REF_APP_ERR;
    }

    switch (cmd_id)
    {
    case CMD_ID_MQTT_GET_BROKER:
    {
        at_cmd_ref_app_mqtt_broker_info_t *server_info;
        server_info = (at_cmd_ref_app_mqtt_broker_info_t *)msg;

        cJSON_AddStringToObject(cjson, MQTT_TOKEN_HOSTNAME, server_info->hostname);
        cJSON_AddNumberToObject(cjson, MQTT_TOKEN_PORT, server_info->port);
        cJSON_AddBoolToObject(cjson, MQTT_TOKEN_TLS, server_info->tls);

        if (server_info->clientid)
        {
            cJSON_AddStringToObject(cjson, MQTT_TOKEN_CLIENTID, server_info->clientid);
        }

        cJSON_AddBoolToObject(cjson, MQTT_TOKEN_CLEANSESSION, server_info->cleansession);

        if (server_info->username)
        {
            cJSON_AddStringToObject(cjson, MQTT_TOKEN_USERNAME, server_info->username);
        }

        if (server_info->password)
        {
            cJSON_AddStringToObject(cjson, MQTT_TOKEN_PASSWORD, server_info->password);
        }

        if (server_info->lastwilltopic)
        {
            cJSON_AddStringToObject(cjson, MQTT_TOKEN_LASTWILLTOPIC, server_info->lastwilltopic);
        }

        cJSON_AddNumberToObject(cjson, MQTT_TOKEN_LASTWILLQOS, server_info->lastwillqos);

        if (server_info->lastwillmessage)
        {
            cJSON_AddStringToObject(cjson, MQTT_TOKEN_LASTWILLMSG, server_info->lastwillmessage);
        }

        cJSON_AddBoolToObject(cjson, MQTT_TOKEN_LASTWILLRETAIN, server_info->lastwillretain);

        cJSON_AddNumberToObject(cjson, MQTT_TOKEN_KEEPALIVE, server_info->keepalive);

        cJSON_AddNumberToObject(cjson, MQTT_TOKEN_PUBLISHQOS, server_info->publishqos);

        cJSON_AddBoolToObject(cjson, MQTT_TOKEN_PUBLISHRETAIN, server_info->publishretain);

        cJSON_AddNumberToObject(cjson, MQTT_TOKEN_PUBLISHRETRYLIMIT, server_info->publishretrylimit);

        cJSON_AddNumberToObject(cjson, MQTT_TOKEN_SUBSCRIBERQOS, server_info->subscribeqos);
        break;
    }

    case CMD_ID_MQTT_ASYNC_SUBSCRIPTION_EVENT:
    {
        at_cmd_ref_app_mqtt_publish_t *publish_msg = NULL;
        publish_msg = (at_cmd_ref_app_mqtt_publish_t *)msg;

        cJSON_AddNumberToObject(cjson, MQTT_TOKEN_BROKERID_TYPE, publish_msg->brokerid);

        if (publish_msg->topic != NULL)
        {
            AT_CMD_REFAPP_LOG_MSG((" publish topic:%s\n", publish_msg->topic));
            cJSON_AddStringToObject(cjson, MQTT_TOKEN_TOPIC, publish_msg->topic);
        }
        cJSON_AddNumberToObject(cjson, MQTT_TOKEN_QOS, publish_msg->qos);

        if (publish_msg->msg != NULL)
        {
            AT_CMD_REFAPP_LOG_MSG((" publish msg:%s\n", publish_msg->msg));
            cJSON_AddStringToObject(cjson, MQTT_TOKEN_MSG, publish_msg->msg);
        }
        break;
    }
    case CMD_ID_MQTT_ASYNC_DISCONNECT_EVENT:
    {
        at_cmd_ref_app_mqtt_disconnect_event_t *disconnect_event = NULL;
        disconnect_event = (at_cmd_ref_app_mqtt_disconnect_event_t *)msg;
        cJSON_AddNumberToObject(cjson, MQTT_TOKEN_DISCONNECT_REASON, disconnect_event->disconnect_reason);
        break;
    }

    default:
    {
        AT_CMD_REFAPP_LOG_MSG((" unknown cmd_id:%ld received\n", cmd_id));
        break;
    }
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

    AT_CMD_REFAPP_LOG_MSG(("exit func:%s \n", __func__));
    return result;
}

static bool at_cmd_refapp_mqtt_find_item(cy_linked_list_node_t *node_to_compare, void *user_data)
{
    at_cmd_ref_app_mqtt_find_item_t *find = (at_cmd_ref_app_mqtt_find_item_t *)user_data;
    uint32_t id = ((at_cmd_ref_app_mqtt_find_item_t *)user_data)->id;
    cy_linked_list_node_t *node_ptr = NULL;
    at_cmd_ref_app_mqtt_broker_info_t *mqtt_broker_info = NULL;

    if (find->type == AT_CMD_REF_APP_MQTT_FIND_SERVER)
    {
        node_ptr = (cy_linked_list_node_t *)node_to_compare;
        mqtt_broker_info = (at_cmd_ref_app_mqtt_broker_info_t *)node_ptr->data;
        if (mqtt_broker_info->serverid == id)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    return false;
}

at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_broker_id(char *cmd_txt, uint32_t cmd_id)
{
    at_cmd_ref_app_mqtt_brokerid_t *mqtt_broker_id = NULL;
    uint32_t brokerid = 0;
    cJSON *json;

    json = cJSON_Parse(cmd_txt);
    if (!json)
    {
        AT_CMD_REFAPP_LOG_MSG(("error parsing the CMD_ID_GET_BROKER \n"));
        return NULL;
    }
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_BROKERID_TYPE))
    {
        brokerid = cJSON_GetObjectItem(json, MQTT_TOKEN_BROKERID_TYPE)->valueint;
    }
    else
    {
        AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
        return NULL;
    }
    mqtt_broker_id = calloc(1, sizeof(at_cmd_ref_app_mqtt_broker_info_t));
    if (mqtt_broker_id == NULL)
    {
        AT_CMD_REFAPP_LOG_MSG(("memory error"));
        return NULL;
    }
    memset(mqtt_broker_id, 0, sizeof(at_cmd_ref_app_mqtt_brokerid_t));
    mqtt_broker_id->base.cmd_id = cmd_id;
    mqtt_broker_id->base.serial = cmd_id;
    mqtt_broker_id->brokerid = brokerid;

    cJSON_Delete(json);

    return (at_cmd_msg_base_t *)mqtt_broker_id;
}

at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_subscribe(char *cmd_txt, uint32_t cmd_id)
{
    at_cmd_ref_app_mqtt_subscribe_t *subscribe = NULL;
    uint32_t brokerid = 0;
    cJSON *json;
    uint32_t topiclen = 0;

    json = cJSON_Parse(cmd_txt);
    if (!json)
    {
        AT_CMD_REFAPP_LOG_MSG(("error parsing the CMD_ID_GET_BROKER \n"));
        return NULL;
    }
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_BROKERID_TYPE))
    {
        brokerid = cJSON_GetObjectItem(json, MQTT_TOKEN_BROKERID_TYPE)->valueint;
    }
    else
    {
        AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
        return NULL;
    }
    subscribe = calloc(1, sizeof(at_cmd_ref_app_mqtt_broker_info_t));
    if (subscribe == NULL)
    {
        AT_CMD_REFAPP_LOG_MSG(("memory error"));
        return NULL;
    }
    memset(subscribe, 0, sizeof(at_cmd_ref_app_mqtt_subscribe_t));
    subscribe->base.cmd_id = cmd_id;
    subscribe->base.serial = cmd_id;
    subscribe->brokerid = brokerid;

    if (cJSON_HasObjectItem(json, MQTT_TOKEN_TOPIC))
    {
        topiclen = strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_TOPIC)->valuestring) + 1;
        subscribe->topic = malloc(topiclen);
        if (subscribe->topic == NULL)
        {
            free(subscribe);
            AT_CMD_REFAPP_LOG_MSG(("memory error"));
            return NULL;
        }
        strcpy(subscribe->topic, cJSON_GetObjectItem(json, MQTT_TOKEN_TOPIC)->valuestring);
    }
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_QOS))
    {
        subscribe->qos = cJSON_GetObjectItem(json, MQTT_TOKEN_QOS)->valueint;
    }

    cJSON_Delete(json);
    return (at_cmd_msg_base_t *)subscribe;
}

at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_define_server(char *cmd_txt, uint32_t cmd_id)
{
    at_cmd_ref_app_mqtt_define_server_t *server_config = NULL;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint32_t count = 0;
    cJSON *json;

    json = cJSON_Parse(cmd_txt);
    if (!json)
    {
        AT_CMD_REFAPP_LOG_MSG(("error parsing the CMD_ID_DEFINE_BROKER \n"));
        return NULL;
    }

    if ((!cJSON_HasObjectItem(json, MQTT_TOKEN_BROKERID_TYPE)) || (!cJSON_HasObjectItem(json, MQTT_TOKEN_HOSTNAME)))
    {
        AT_CMD_REFAPP_LOG_MSG(("BrokerID or hostname  parameter not set\n"));
        return NULL;
    }

    if (cJSON_HasObjectItem(json, MQTT_TOKEN_HOSTNAME))
    {
        count += (strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_HOSTNAME)->valuestring) + 1);
    }
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_ROOTCA))
    {
        count += (strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_ROOTCA)->valuestring) + 1);
    }

    if (cJSON_HasObjectItem(json, MQTT_TOKEN_CLIENTCERT))
    {
        count += (strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_CLIENTCERT)->valuestring) + 1);
    }

    if (cJSON_HasObjectItem(json, MQTT_TOKEN_CLIENTKEY))
    {
        count += (strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_CLIENTKEY)->valuestring) + 1);
    }

    if (cJSON_HasObjectItem(json, MQTT_TOKEN_CLIENTID))
    {
        count += (strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_CLIENTID)->valuestring) + 1);
    }

    if (cJSON_HasObjectItem(json, MQTT_TOKEN_USERNAME))
    {
        count += (strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_USERNAME)->valuestring) + 1);
    }

    if (cJSON_HasObjectItem(json, MQTT_TOKEN_PASSWORD))
    {
        count += (strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_PASSWORD)->valuestring) + 1);
    }

    if (cJSON_HasObjectItem(json, MQTT_TOKEN_LASTWILLTOPIC))
    {
        count += (strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_PASSWORD)->valuestring) + 1);
    }
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_LASTWILLMSG))
    {
        count += (strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_LASTWILLMSG)->valuestring) + 1);
    }

    server_config = calloc(1, sizeof(at_cmd_ref_app_mqtt_define_server_t) + count);
    if (server_config == NULL)
    {
        AT_CMD_REFAPP_LOG_MSG(("memory error"));
        cJSON_Delete(json);
        return NULL;
    }

    server_config->data_length = count;
    server_config->base.cmd_id = CMD_ID_MQTT_DEFINE_BROKER;
    server_config->base.serial = CMD_ID_MQTT_DEFINE_BROKER;

    result = at_cmd_refapp_mqtt_server_config(server_config, json);
    if (result != CY_RSLT_SUCCESS)
    {
        free(server_config);
        AT_CMD_REFAPP_LOG_MSG(("error MQTT define server setup\n"));
        return NULL;
    }

    if ((server_config->username != NULL) && (server_config->clientid != NULL) && (server_config->password != NULL))
    {
        AT_CMD_REFAPP_LOG_MSG(("clientid %s username %s password %s\n",
                               server_config->clientid, server_config->username, server_config->password));
    }
    return (at_cmd_msg_base_t *)server_config;
}

at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_unsubscribe(char *cmd_txt, uint32_t cmd_id)
{
    at_cmd_ref_app_mqtt_unsubscribe_t *unsubscribe = NULL;
    uint32_t brokerid = 0;
    cJSON *json;
    uint32_t topiclen = 0;

    json = cJSON_Parse(cmd_txt);
    if (!json)
    {
        AT_CMD_REFAPP_LOG_MSG(("error parsing the CMD_ID_GET_BROKER \n"));
        return NULL;
    }
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_BROKERID_TYPE))
    {
        brokerid = cJSON_GetObjectItem(json, MQTT_TOKEN_BROKERID_TYPE)->valueint;
    }
    else
    {
        AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
        return NULL;
    }
    unsubscribe = calloc(1, sizeof(at_cmd_ref_app_mqtt_unsubscribe_t));
    if (unsubscribe == NULL)
    {
        AT_CMD_REFAPP_LOG_MSG(("memory error"));
        return NULL;
    }
    memset(unsubscribe, 0, sizeof(at_cmd_ref_app_mqtt_unsubscribe_t));
    unsubscribe->base.cmd_id = cmd_id;
    unsubscribe->base.serial = cmd_id;
    unsubscribe->brokerid = brokerid;
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_TOPIC))
    {
        topiclen = strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_TOPIC)->valuestring) + 1;
        unsubscribe->topic = malloc(topiclen);
        if (unsubscribe->topic == NULL)
        {
            free(unsubscribe);
            AT_CMD_REFAPP_LOG_MSG(("memory error"));
            return NULL;
        }
        strcpy(unsubscribe->topic, cJSON_GetObjectItem(json, MQTT_TOKEN_TOPIC)->valuestring);
    }
    cJSON_Delete(json);

    return (at_cmd_msg_base_t *)unsubscribe;
}

at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_publish(char *cmd_txt, uint32_t cmd_id)
{
    at_cmd_ref_app_mqtt_publish_t *publish = NULL;
    uint32_t brokerid = 0;
    cJSON *json;
    uint32_t publish_msglen = 0, publish_topiclen = 0;

    json = cJSON_Parse(cmd_txt);
    if (!json)
    {
        AT_CMD_REFAPP_LOG_MSG(("error parsing the CMD_ID_GET_BROKER \n"));
        return NULL;
    }
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_BROKERID_TYPE))
    {
        brokerid = cJSON_GetObjectItem(json, MQTT_TOKEN_BROKERID_TYPE)->valueint;
    }
    else
    {
        AT_CMD_REFAPP_LOG_MSG(("mqtt broker info not found"));
        return NULL;
    }
    publish = calloc(1, sizeof(at_cmd_ref_app_mqtt_publish_t));
    if (publish == NULL)
    {
        AT_CMD_REFAPP_LOG_MSG(("memory error"));
        return NULL;
    }
    memset(publish, 0, sizeof(at_cmd_ref_app_mqtt_publish_t));
    publish->base.cmd_id = cmd_id;
    publish->base.serial = cmd_id;
    publish->brokerid = brokerid;
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_TOPIC))
    {
        publish_topiclen = strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_TOPIC)->valuestring) + 1;
        publish->topic = malloc(publish_topiclen);
        if (publish->topic == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory-error"));
            free(publish);
            return NULL;
        }
        strcpy(publish->topic, cJSON_GetObjectItem(json, MQTT_TOKEN_TOPIC)->valuestring);
    }
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_QOS))
    {
        publish->qos = cJSON_GetObjectItem(json, MQTT_TOKEN_QOS)->valueint;
    }
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_MSG))
    {
        publish_msglen = strlen(cJSON_GetObjectItem(json, MQTT_TOKEN_MSG)->valuestring) + 1;
        publish->msg = malloc(publish_msglen);
        if (publish->msg == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory-error"));
            free(publish);
            return NULL;
        }
        strcpy(publish->msg, cJSON_GetObjectItem(json, MQTT_TOKEN_MSG)->valuestring);
    }

    cJSON_Delete(json);
    return (at_cmd_msg_base_t *)publish;
}

at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_cmd(uint32_t cmd_id, uint32_t serial, uint32_t cmd_len, char *cmd)
{
    at_cmd_msg_base_t *msg = NULL;
    char *cmd_txt = cmd;

    switch (cmd_id)
    {
    case CMD_ID_MQTT_GET_BROKER:
    case CMD_ID_MQTT_CONNECT_BROKER:
    case CMD_ID_MQTT_DISCONNECT_BROKER:
    case CMD_ID_MQTT_DELETE_BROKER:
    {
        msg = at_cmd_refapp_parse_mqtt_broker_id(cmd_txt, cmd_id);
        break;
    }
    case CMD_ID_MQTT_DEFINE_BROKER:
    {
        msg = at_cmd_refapp_parse_mqtt_define_server(cmd_txt, cmd_id);
        break;
    }
    case CMD_ID_MQTT_SUBSCRIBE:
    {
        msg = at_cmd_refapp_parse_mqtt_subscribe(cmd_txt, cmd_id);
        break;
    }

    case CMD_ID_MQTT_PUBLISH:
    {
        msg = at_cmd_refapp_parse_mqtt_publish(cmd_txt, cmd_id);
        break;
    }

    case CMD_ID_MQTT_UNSUBSCRIBE:
    {
        msg = at_cmd_refapp_parse_mqtt_unsubscribe(cmd_txt, cmd_id);
        break;
    }
    default:
    {
        AT_CMD_REFAPP_LOG_MSG(("unknown cmd_id:%ld\n", cmd_id));
        break;
    }
    }
    return msg;
}

static cy_rslt_t at_cmd_refapp_mqtt_connect(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_awsport_ssl_credentials_t credentials;
    cy_awsport_ssl_credentials_t *security = NULL;
    cy_mqtt_connect_info_t connect_info;
    cy_mqtt_broker_info_t broker_info;
    cy_mqtt_publish_info_t will_info;

    if (!mqtt_server->connected)
    {
        memset(&credentials, 0x00, sizeof(cy_awsport_ssl_credentials_t));
        memset(&connect_info, 0x00, sizeof(cy_mqtt_connect_info_t));
        memset(&broker_info, 0x00, sizeof(cy_mqtt_broker_info_t));
        memset(&will_info, 0x00, sizeof(cy_mqtt_publish_info_t));

        if (mqtt_server->tls)
        {
            /*
             * Set credential information.
             */
            credentials.client_cert = mqtt_server->cert;
            if (credentials.client_cert)
            {
                credentials.client_cert_size = strlen(credentials.client_cert) + 1;
            }

            credentials.private_key = mqtt_server->key;
            if (credentials.private_key)
            {
                credentials.private_key_size = strlen(credentials.private_key) + 1;
            }

            credentials.root_ca = mqtt_server->rootca;
            if (credentials.root_ca)
            {
                credentials.root_ca_size = strlen(credentials.root_ca) + 1;
            }

            credentials.sni_host_name = mqtt_server->hostname;
            if (credentials.sni_host_name)
            {
                credentials.sni_host_name_size = strlen(mqtt_server->hostname) + 1;
            }

            security = &credentials;
        }

        /*
         * Set hostname and port.
         */
        broker_info.hostname = mqtt_server->hostname;
        broker_info.hostname_len = strlen(mqtt_server->hostname);
        broker_info.port = mqtt_server->port;

        AT_CMD_REFAPP_LOG_MSG(("mqtt_server->hostname:%p\n", mqtt_server->hostname));
        AT_CMD_REFAPP_LOG_MSG(("broker hostname:%s len:%d port:%d\n", broker_info.hostname, broker_info.hostname_len, broker_info.port));
        /*
         * Create network buffer used by MQTT library for send and receive.
         */
        mqtt_server->mqtt_buffer = malloc(AT_CMD_REF_APP_MQTT_BUFFER_SIZE);
        if (mqtt_server->mqtt_buffer == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory allocation failed for mqtt network buffer \r\n"));
            return CY_RSLT_AT_CMD_REF_APP_ERR;
        }

        /*
         * Create MQTT instance.
         */
        result = cy_mqtt_create(mqtt_server->mqtt_buffer, AT_CMD_REF_APP_MQTT_BUFFER_SIZE, security, &broker_info, MQTT_HANDLE_DESCRIPTOR, &mqtt_server->mqtt_handle);
        if (result != CY_RSLT_SUCCESS)
        {
            AT_CMD_REFAPP_LOG_MSG(("cy_mqtt_create failed %lx\n", result));

            free(mqtt_server->mqtt_buffer);
            mqtt_server->mqtt_buffer = NULL;
            return CY_RSLT_AT_CMD_REF_APP_ERR;
        }

        /* Register a MQTT event callback */
        result = cy_mqtt_register_event_callback(mqtt_server->mqtt_handle, (cy_mqtt_callback_t)at_cmd_refapp_mqtt_event_cb, NULL);
        if (CY_RSLT_SUCCESS == result)
        {
            printf("\nMQTT library initialization successful.\n");
        }

        AT_CMD_REFAPP_LOG_MSG(("cy_mqtt_create success %lx mqtt handle %p\n", result, mqtt_server->mqtt_handle));
        /*
         * Set connection info.
         */
        connect_info.client_id = mqtt_server->clientid;
        connect_info.client_id_len = strlen(connect_info.client_id);
        connect_info.keep_alive_sec = mqtt_server->keepalive;

        /*
         * Set last will info if present.
         */
        if (mqtt_server->lastwilltopic)
        {
            will_info.topic = mqtt_server->lastwilltopic;
            will_info.topic_len = strlen(mqtt_server->lastwilltopic);
            will_info.payload = mqtt_server->lastwillmessage;
            will_info.payload_len = strlen(will_info.payload);
            will_info.qos = mqtt_server->lastwillqos;
            will_info.retain = mqtt_server->lastwillretain;
            connect_info.will_info = &will_info;
        }

        /*
         * Set clean session flag.
         */
        connect_info.clean_session = mqtt_server->cleansession;

        /*
         * Set MQTT username if present
         */
        if (mqtt_server->username)
        {
            connect_info.username = mqtt_server->username;
            connect_info.username_len = strlen(mqtt_server->username);
        }

        /*
         * Set MQTT password if present
         */
        if (mqtt_server->password)
        {
            connect_info.password = mqtt_server->password;
            connect_info.password_len = strlen(mqtt_server->password);
        }

        AT_CMD_REFAPP_LOG_MSG(("calling cy_mqtt_connect ..\n"));
        /*
         * Connect to MQTT broker.
         */
        result = cy_mqtt_connect(mqtt_server->mqtt_handle, &connect_info);
        if (result != CY_RSLT_SUCCESS)
        {
            AT_CMD_REFAPP_LOG_MSG(("cy_mqtt_connect failed %lx\n", result));

            cy_mqtt_delete(mqtt_server->mqtt_handle);
            mqtt_server->mqtt_handle = NULL;

            free(mqtt_server->mqtt_buffer);
            mqtt_server->mqtt_buffer = NULL;

            return CY_RSLT_AT_CMD_REF_APP_ERR;
        }

        AT_CMD_REFAPP_LOG_MSG(("cy_mqtt_connect returned success!!\n"));
        mqtt_server->connected = true;
    }
    else
    {
        AT_CMD_REFAPP_LOG_MSG(("MQTT server already connected %d\n", mqtt_server->connected));
    }
    return CY_RSLT_SUCCESS;
}

static cy_rslt_t at_cmd_refapp_mqtt_disconnect(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if (mqtt_server->connected)
    {
        cy_mqtt_disconnect(mqtt_server->mqtt_handle);
        cy_mqtt_delete(mqtt_server->mqtt_handle);

        mqtt_server->mqtt_handle = NULL;

        free(mqtt_server->mqtt_buffer);
        mqtt_server->mqtt_buffer = NULL;

        mqtt_server->connected = false;
    }

    return result;
}

static cy_rslt_t at_cmd_refapp_mqtt_publish(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server, at_cmd_ref_app_mqtt_publish_t *publish)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_mqtt_publish_info_t pub_info;

    /*
     * Publish message on topic.
     */
    memset(&pub_info, 0x00, sizeof(cy_mqtt_publish_info_t));

    pub_info.qos = publish->qos;
    pub_info.topic = publish->topic;
    pub_info.topic_len = strlen(pub_info.topic);
    pub_info.payload = publish->msg;
    pub_info.payload_len = strlen(pub_info.payload);

    result = cy_mqtt_publish(mqtt_server->mqtt_handle, &pub_info);
    if (result != CY_RSLT_SUCCESS)
    {
        AT_CMD_REFAPP_LOG_MSG(("cy_mqtt_publish failed %lx\n", result));
        return CY_RSLT_AT_CMD_REF_APP_ERR;
    }

    AT_CMD_REFAPP_LOG_MSG(("MQTT publish successful \n"));

    return result;
}

static cy_rslt_t at_cmd_refapp_mqtt_subscribe(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server, at_cmd_ref_app_mqtt_subscribe_t *subscribe)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_mqtt_subscribe_info_t sub_info[1];

    /*
     * subscribe on topic.
     */
    memset(&sub_info, 0x00, sizeof(cy_mqtt_subscribe_info_t));

    sub_info[0].topic = subscribe->topic;
    sub_info[0].topic_len = strlen(subscribe->topic);

    AT_CMD_REFAPP_LOG_MSG(("Subscribing with QOS : %d, Topic : %s \n", sub_info[0].qos, sub_info[0].topic));

    result = cy_mqtt_subscribe(mqtt_server->mqtt_handle, &sub_info[0], 1);
    if (result != CY_RSLT_SUCCESS)
    {
        AT_CMD_REFAPP_LOG_MSG(("cy_mqtt_subscribe failed %lx\n", result));
        return CY_RSLT_AT_CMD_REF_APP_ERR;
    }

    AT_CMD_REFAPP_LOG_MSG(("MQTT subscribe successful \n"));
    return result;
}

static cy_rslt_t at_cmd_refapp_mqtt_unsubscribe(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server, at_cmd_ref_app_mqtt_unsubscribe_t *unsubscribe)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_mqtt_unsubscribe_info_t unsub_info[1];

    memset(&unsub_info, 0x00, sizeof(cy_mqtt_unsubscribe_info_t));

    unsub_info[0].topic = unsubscribe->topic;
    unsub_info[0].topic_len = strlen(unsubscribe->topic);

    AT_CMD_REFAPP_LOG_MSG(("UnSubscribing with QOS : %d, Topic : %s \n", unsub_info[0].qos, unsub_info[0].topic));

    result = cy_mqtt_unsubscribe(mqtt_server->mqtt_handle, &unsub_info[0], 1);
    if (result != CY_RSLT_SUCCESS)
    {
        AT_CMD_REFAPP_LOG_MSG(("cy_mqtt_unsubscribe failed %lx\n", result));
        return CY_RSLT_AT_CMD_REF_APP_ERR;
    }

    AT_CMD_REFAPP_LOG_MSG(("MQTT unsubscribe successful \n"));
    return result;
}

static void at_cmd_refapp_create_mqtt_broker_info(at_cmd_ref_app_mqtt_broker_info_t *mqtt_server, at_cmd_ref_app_mqtt_define_server_t *server_config)
{
    mqtt_server->serverid = server_config->brokerid;
    mqtt_server->connected = false;

    if (server_config->publishqos == -1)
    {
        mqtt_server->publishqos = AT_CMD_REF_APP_MQTT_PUBLISH_QOS;
    }
    else
    {
        mqtt_server->publishqos = server_config->publishqos;
    }

    if (server_config->subscribeqos == -1)
    {
        mqtt_server->subscribeqos = AT_CMD_REF_APP_MQTT_SUBSCRIBE_QOS;
    }
    else
    {
        mqtt_server->subscribeqos = server_config->subscribeqos;
    }

    mqtt_server->keepalive = server_config->keepalive;

    /*
     * hostname.
     */
    mqtt_server->hostname = malloc(strlen(server_config->hostname) + 1);
    if (mqtt_server->hostname == NULL)
    {
        AT_CMD_REFAPP_LOG_MSG(("memory error\n"));
        return;
    }
    memset(mqtt_server->hostname, 0, (strlen(server_config->hostname)+1));
    strcpy(mqtt_server->hostname, server_config->hostname);

    AT_CMD_REFAPP_LOG_MSG(("mqtt_server->hostname:%p\n", mqtt_server->hostname));
    AT_CMD_REFAPP_LOG_MSG(("func:%s mqtt_server->hostname :%s\n", __func__, mqtt_server->hostname));

    /*
     * Port number.
     */
    if (server_config->port != 0)
    {
        mqtt_server->port = server_config->port;
    }
    else
    {
        if (server_config->tls == true)
        {
            /*
             * Default MQTT TLS port.
             */
            mqtt_server->port = AT_CMD_REF_APP_MQTT_TLS_PORT;
        }
        else
        {
            mqtt_server->port = AT_CMD_REF_APP_MQTT_PORT;
        }
    }

    /*
     * TLS.
     */
    mqtt_server->tls = server_config->tls;

    /*
     * TLS certificates.
     */
    if (server_config->rootca)
    {
        mqtt_server->rootca = malloc(strlen(server_config->rootca) + 1);
        if (mqtt_server->rootca == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory error\n"));
            return;
        }
        memset(mqtt_server->rootca, 0, (strlen(server_config->rootca) + 1));
        strcpy(mqtt_server->rootca, server_config->rootca);
    }
    if (server_config->cert)
    {
        mqtt_server->cert = malloc(strlen(server_config->cert) + 1);
        if (mqtt_server->cert == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory error\n"));
            return;
        }
        memset(mqtt_server->cert, 0, (strlen(server_config->cert) + 1));
        strcpy(mqtt_server->cert, server_config->cert);
    }
    if (server_config->key)
    {
        mqtt_server->key = malloc(strlen(server_config->key) + 1);
        if (mqtt_server->key == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory error\n"));
            return;
        }
        memset(mqtt_server->key, 0, (strlen(server_config->key) + 1));
        strcpy(mqtt_server->key, server_config->key);
    }

    /*
     * Cliend ID.
     */
    if (server_config->clientid)
    {
        mqtt_server->clientid = malloc(strlen(server_config->clientid) + 1);
        if (mqtt_server->clientid == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory error\n"));
            return;
        }
        memset(mqtt_server->clientid, 0, (strlen(server_config->clientid) + 1));
        strcpy(mqtt_server->clientid, server_config->clientid);
    }
    else
    {
        mqtt_server->clientid = AT_CMD_REF_APP_MQTT_CLIENT_ID;
    }

    /*
     * Clean session]
     */
    mqtt_server->cleansession = server_config->cleansession;

    /*
     * Username.
     */

    if (server_config->username)
    {
        mqtt_server->username = malloc(strlen(server_config->username) + 1);
        if (mqtt_server->username == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory error\n"));
            return;
        }
        memset(mqtt_server->username, 0, (strlen(server_config->username) + 1));
        strcpy(mqtt_server->username, server_config->username);
    }

    /*
     * Password.
     */
    if (server_config->password)
    {
        mqtt_server->password = malloc(strlen(server_config->password) + 1);
        if (mqtt_server->password == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory error\n"));
            return;
        }
        memset(mqtt_server->password, 0, (strlen(server_config->password) + 1));
        strcpy(mqtt_server->password, server_config->password);
    }

    /*
     * lastwilltopic.
     */
    if (server_config->lastwilltopic)
    {
        mqtt_server->lastwilltopic = malloc(strlen(server_config->lastwilltopic) + 1);
        if (mqtt_server->lastwilltopic == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory error\n"));
            return;
        }
        memset(mqtt_server->lastwilltopic, 0, (strlen(server_config->lastwilltopic) + 1));
        strcpy(mqtt_server->lastwilltopic, server_config->lastwilltopic);
    }

    if (server_config->lastwillmessage)
    {
        mqtt_server->lastwillmessage = malloc(strlen(server_config->lastwillmessage) + 1);
        if (mqtt_server->lastwillmessage == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("memory error\n"));
            return;
        }
        memset(mqtt_server->lastwillmessage, 0, (strlen(server_config->lastwillmessage) + 1));
        strcpy(mqtt_server->lastwillmessage, server_config->lastwillmessage);
    }

    /*
     * Retry limit.
     */
    if (server_config->publishretrylimit == -1)
    {
        mqtt_server->publishretrylimit = AT_CMD_REF_APP_MQTT_PUBLISH_RETRY_LIMIT;
    }
    AT_CMD_REFAPP_LOG_MSG(("at_cmd_refapp_create_mqtt_broker_info hostname:%s\n", mqtt_server->hostname));
    return;
}

static void at_cmd_refapp_mqtt_event_cb(cy_mqtt_t mqtt_handle, cy_mqtt_event_t event, void *user_data)
{
    at_cmd_ref_app_mqtt_disconnect_event_t *msg = NULL;
    at_cmd_ref_app_mqtt_publish_t *publish = NULL;
    uint32_t topiclen = 0;
    uint32_t payloadlen = 0;

    AT_CMD_REFAPP_LOG_MSG(("Received  event=%d from MQTT callback\n", event.type));

    if (event.type == CY_MQTT_EVENT_TYPE_DISCONNECT)
    {
        /*
         * Tell the main loop about the MQTT change event.
         */

        msg = calloc(1, sizeof(at_cmd_ref_app_mqtt_disconnect_event_t));
        if (msg == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("error alloc failed \n"));
            return;
        }

        msg->base.cmd_id = CMD_ID_MQTT_ASYNC_DISCONNECT_EVENT;
        msg->base.serial = CMD_ID_MQTT_ASYNC_DISCONNECT_EVENT;
        msg->disconnect_reason = event.data.reason;
        if (at_cmd_refapp_send_message((at_cmd_msg_base_t *)msg) != CY_RSLT_SUCCESS)
        {
            AT_CMD_REFAPP_LOG_MSG(("error sending at_cmd_refapp_send_message!!!\n"));
            free(msg);
        }
    }
    else if (event.type == CY_MQTT_EVENT_TYPE_SUBSCRIPTION_MESSAGE_RECEIVE)
    {
        /*
         * Tell the main loop about the MQTT change event.
         */

        publish = calloc(1, sizeof(at_cmd_ref_app_mqtt_publish_t));
        if (publish == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("error alloc failed\n"));
            return;
        }
        publish->base.cmd_id = CMD_ID_MQTT_ASYNC_SUBSCRIPTION_EVENT;
        publish->base.serial = CMD_ID_MQTT_ASYNC_SUBSCRIPTION_EVENT;

        publish->brokerid = event.data.pub_msg.packet_id;
        publish->qos = event.data.pub_msg.received_message.qos;
        topiclen = strlen(event.data.pub_msg.received_message.topic) + 1;
        publish->topic = malloc(topiclen);
        if (publish->topic == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("error alloc failed\n"));
            free(publish);
            return;
        }
        memset(publish->topic, 0, topiclen);
        memcpy(publish->topic, event.data.pub_msg.received_message.topic, event.data.pub_msg.received_message.topic_len);
        AT_CMD_REFAPP_LOG_MSG(("publish->topic:%s\n", publish->topic));

        payloadlen = strlen(event.data.pub_msg.received_message.payload) + 1;
        publish->msg = malloc(payloadlen);
        if (publish->msg == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("error alloc failed\n"));
            free(publish->topic);
            free(publish);
            return;
        }
        memset(publish->msg, 0, payloadlen);
        memcpy(publish->msg, event.data.pub_msg.received_message.payload, event.data.pub_msg.received_message.payload_len);
        AT_CMD_REFAPP_LOG_MSG(("publish->msg:%s\n", publish->msg));

        if (at_cmd_refapp_send_message((at_cmd_msg_base_t *)publish) != CY_RSLT_SUCCESS)
        {
            AT_CMD_REFAPP_LOG_MSG(("error sending at_cmd_refapp_send_message!!!\n"));
            free(publish->topic);
            free(publish->msg);
            free(publish);
        }
    }
    return;
}

static cy_rslt_t at_cmd_refapp_mqtt_server_config(at_cmd_ref_app_mqtt_define_server_t *server_config, cJSON *json)
{
    char *host;
    char *ptr = &server_config->data[0];

    server_config->brokerid = cJSON_GetObjectItem(json, MQTT_TOKEN_BROKERID_TYPE)->valueint;

    host = cJSON_GetObjectItem(json, MQTT_TOKEN_HOSTNAME)->valuestring;

    /*
     * Hostname
     */
    server_config->hostname = ptr;
    strcpy(ptr, host);
    ptr += strlen(host) + 1;

    /*
     * Port.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_PORT))
    {
        server_config->port = cJSON_GetObjectItem(json, MQTT_TOKEN_PORT)->valueint;
    }

    /*
     * TLS.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_TLS))
    {
        char *rootca, *cert, *key;
        server_config->tls = cJSON_IsTrue(cJSON_GetObjectItem(json, MQTT_TOKEN_TLS));

        if (server_config->tls)
        {
            if (cJSON_HasObjectItem(json, MQTT_TOKEN_ROOTCA))
            {
                rootca = cJSON_GetObjectItem(json, MQTT_TOKEN_ROOTCA)->valuestring;
                server_config->rootca = ptr;
                strcpy(ptr, rootca);
                ptr += strlen(rootca) + 1;
            }

            if (cJSON_HasObjectItem(json, MQTT_TOKEN_CLIENTCERT))
            {
                cert = cJSON_GetObjectItem(json, MQTT_TOKEN_CLIENTCERT)->valuestring;
                server_config->cert = ptr;
                strcpy(ptr, cert);
                ptr += strlen(cert) + 1;
            }

            if (cJSON_HasObjectItem(json, MQTT_TOKEN_CLIENTKEY))
            {
                key = cJSON_GetObjectItem(json, MQTT_TOKEN_CLIENTKEY)->valuestring;
                server_config->key = ptr;
                strcpy(ptr, key);
                ptr += strlen(key) + 1;
            }
        }
    }
    /*
     * Client ID.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_CLIENTID))
    {
        char *clientid;
        clientid = cJSON_GetObjectItem(json, MQTT_TOKEN_CLIENTID)->valuestring;

        server_config->clientid = ptr;
        strcpy(ptr, clientid);
        ptr += strlen(clientid) + 1;
    }

    /*
     * Clean session.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_CLEANSESSION))
    {
        server_config->cleansession = cJSON_IsTrue(cJSON_GetObjectItem(json, MQTT_TOKEN_CLEANSESSION));
    }
    else
    {
        /*
         * Set default to clean session(non-persistent).
         */
        server_config->cleansession = 1;
    }

    /*
     * username.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_USERNAME))
    {
        char *username;
        username = cJSON_GetObjectItem(json, MQTT_TOKEN_USERNAME)->valuestring;

        server_config->username = ptr;
        strcpy(ptr, username);
        ptr += strlen(username) + 1;
    }

    /*
     * password.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_PASSWORD))
    {
        char *password;
        password = cJSON_GetObjectItem(json, MQTT_TOKEN_PASSWORD)->valuestring;

        server_config->password = ptr;
        strcpy(ptr, password);
        ptr += strlen(password) + 1;
    }

    /*
     * lastwilltopic.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_LASTWILLTOPIC))
    {
        char *lastwilltopic;
        lastwilltopic = cJSON_GetObjectItem(json, MQTT_TOKEN_LASTWILLTOPIC)->valuestring;

        server_config->lastwilltopic = ptr;
        strcpy(ptr, lastwilltopic);
        ptr += strlen(lastwilltopic) + 1;
    }

    /*
     * lastwillqos.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_LASTWILLQOS))
    {
        server_config->lastwillqos = cJSON_GetObjectItem(json, MQTT_TOKEN_LASTWILLQOS)->valueint;
    }

    /*
     * lastwillmessage.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_LASTWILLMSG))
    {
        char *lastwilltmsg;
        lastwilltmsg = cJSON_GetObjectItem(json, MQTT_TOKEN_LASTWILLMSG)->valuestring;

        server_config->lastwillmessage = ptr;
        strcpy(ptr, lastwilltmsg);
        ptr += strlen(lastwilltmsg) + 1;
    }

    /*
     * lastwillretain.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_LASTWILLRETAIN))
    {
        server_config->lastwillretain = cJSON_IsTrue(cJSON_GetObjectItem(json, MQTT_TOKEN_LASTWILLRETAIN));
    }

    /*
     * keepalive.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_KEEPALIVE))
    {
        server_config->keepalive = cJSON_GetObjectItem(json, MQTT_TOKEN_KEEPALIVE)->valueint;
    }

    /*
     * Publish QoS
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_PUBLISHQOS))
    {
        server_config->publishqos = cJSON_GetObjectItem(json, MQTT_TOKEN_PUBLISHQOS)->valueint;
    }
    else
    {
        server_config->publishqos = -1;
    }

    /*
     * Publish retain.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_PUBLISHRETAIN))
    {
        server_config->publishretain = cJSON_IsTrue(cJSON_GetObjectItem(json, MQTT_TOKEN_PUBLISHRETAIN));
    }

    /*
     * Publish retry limit.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_PUBLISHRETRYLIMIT))
    {
        server_config->publishretrylimit = cJSON_GetObjectItem(json, MQTT_TOKEN_PUBLISHRETRYLIMIT)->valueint;
    }
    else
    {
        server_config->publishretrylimit = -1;
    }

    /*
     * Subscribe QoS.
     */
    if (cJSON_HasObjectItem(json, MQTT_TOKEN_SUBSCRIBERQOS))
    {
        server_config->subscribeqos = cJSON_GetObjectItem(json, MQTT_TOKEN_SUBSCRIBERQOS)->valueint;
    }
    else
    {
        server_config->subscribeqos = -1;
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t at_cmd_refapp_mqtt_event_callback(uint32_t cmd_id, at_cmd_msg_base_t *mqtt_async_event, at_cmd_result_data_t *result_str)
{
    at_cmd_ref_app_mqtt_publish_t *publish_msg = (at_cmd_ref_app_mqtt_publish_t *)mqtt_async_event;
    at_cmd_ref_app_mqtt_disconnect_event_t *disconn_msg = (at_cmd_ref_app_mqtt_disconnect_event_t *)mqtt_async_event;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    at_cmd_ref_app_mqtt_broker_info_t *mqtt_broker_info = NULL;

    if ((publish_msg == NULL) || (disconn_msg == NULL))
    {
        AT_CMD_REFAPP_LOG_MSG(("func:%s publish_msg or disconn_msg is NULL!!\n", __func__));
        return CY_RSLT_AT_CMD_REF_APP_ERR;
    }
    switch (cmd_id)
    {
    case CMD_ID_MQTT_ASYNC_DISCONNECT_EVENT:
    {
        cy_linked_list_node_t *nodeptr = NULL;
        nodeptr = (cy_linked_list_node_t *)g_mqtt_server_list.rear;
        if (nodeptr == NULL)
        {
            AT_CMD_REFAPP_LOG_MSG(("func:%s disconn_msg unable to get broker info!!\n", __func__));
            return CY_RSLT_AT_CMD_REF_APP_ERR;
        }
        mqtt_broker_info = (at_cmd_ref_app_mqtt_broker_info_t *)nodeptr->data;
        if (mqtt_broker_info != NULL)
        {
            /* Clear the status flag bit to indicate MQTT disconnection. */
            at_cmd_refapp_mqtt_disconnect(mqtt_broker_info);
        }
        /* MQTT connection with the MQTT broker is broken as the client
         * is unable to communicate with the broker. Set the appropriate
         * command to be sent to the MQTT task.
         */
        AT_CMD_REFAPP_LOG_MSG(("\nUnexpectedly disconnected from MQTT broker reason :%d!\n", disconn_msg->disconnect_reason));
        at_cmd_refapp_process_mqtt_host_msg(cmd_id, (at_cmd_msg_base_t *)disconn_msg, result_str->result_text, sizeof(result_str->result_text));
        break;
    }

    case CMD_ID_MQTT_ASYNC_SUBSCRIPTION_EVENT:
    {
        if (publish_msg != NULL && publish_msg->msg != NULL && publish_msg->topic != NULL)
        {
            at_cmd_refapp_process_mqtt_host_msg(cmd_id, (at_cmd_msg_base_t *)publish_msg, result_str->result_text, sizeof(result_str->result_text));

            AT_CMD_REFAPP_LOG_MSG(("  Subscriber: Incoming MQTT message received:\n"
                                   "    Publish topic name length:%d\n"
                                   "    Publish topic name: %s\n"
                                   "    Publish QoS: %d\n"
                                   "    Publish payload length: %d\n\n"
                                   "    Publish payload: %s\n\n",
                                   (int)strlen(publish_msg->topic), publish_msg->topic,
                                   (int)publish_msg->qos,
                                   (int)strlen(publish_msg->msg), publish_msg->msg));

            if (publish_msg->msg != NULL)
            {
                free(publish_msg->msg);
                publish_msg->msg = NULL; // Set to NULL after freeing the memory
            }
            if (publish_msg->topic != NULL)
            {
                free(publish_msg->topic);
                publish_msg->topic = NULL; // Set to NULL after freeing the memory
            }
        }

        break;
    }
    default:
    {
        /* Unknown MQTT event */
        AT_CMD_REFAPP_LOG_MSG(("\nUnknown Event received from MQTT callback!\n"));
        break;
    }
    }
    return result;
}
/* [] END OF FILE */
