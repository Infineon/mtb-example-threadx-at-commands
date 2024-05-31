/***************************************************************************//**
* \file at_command_refapp.h
*
* \brief
* AT command ref app header file
*
********************************************************************************
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

#include "at_command_parser.h"
#include "cyabs_rtos.h"
#include "cy_wcm.h"
#include "cJSON.h"
#include "cy_linked_list.h"
#include "cy_mqtt_api.h"

/******************************************************
 *                    Defines
 ******************************************************/
#define CY_RSLT_AT_CMD_REF_APP_ERR 0x1
#define AT_CMD_REF_APP_OFFLOAD_NO_WAIT  0

#define AT_CMD_REF_APP_BUFFER_SIZE   4096
#define AT_CMD_REF_APP_PREFIX_CHARS  3

#define AT_CMD_REF_APP_WAITFOREVER ( (uint32_t)0xffffffffUL )

#define NULL_MAC(a)  ( ( ( ( (unsigned char *)a )[0] ) == 0 ) && \
                       ( ( ( (unsigned char *)a )[1] ) == 0 ) && \
                       ( ( ( (unsigned char *)a )[2] ) == 0 ) && \
                       ( ( ( (unsigned char *)a )[3] ) == 0 ) && \
                       ( ( ( (unsigned char *)a )[4] ) == 0 ) && \
                       ( ( ( (unsigned char *)a )[5] ) == 0 ) )
#define AT_CMD_REF_APP_NUM_CMD_QUEUE_MSGS              (10)

/*
 * Command IDs.
 */
#define CMD_ID_AP_CONNECT                      (1)
#define CMD_ID_AP_DISCONNECT                   (2)
#define CMD_ID_AP_GET_INFO                     (3)
#define CMD_ID_GET_IP_ADDRESS                  (4)
#define CMD_ID_GET_IPv4_ADDRESS                (5)
#define CMD_ID_PING                            (6)
#define CMD_ID_SCAN_START                      (7)
#define CMD_ID_SCAN_STOP                       (8)
#define CMD_ID_WCM_NETWORK_CHANGE_NOTIFICATION (9)
#define CMD_ID_HOST_WCM_SCAN_INFO              (10)

#define CMD_ID_MQTT_DEFINE_BROKER              (11)
#define CMD_ID_MQTT_GET_BROKER                 (12)
#define CMD_ID_MQTT_DELETE_BROKER              (13)
#define CMD_ID_MQTT_CONNECT_BROKER             (14)
#define CMD_ID_MQTT_DISCONNECT_BROKER          (15)
#define CMD_ID_MQTT_SUBSCRIBE                  (16)
#define CMD_ID_MQTT_UNSUBSCRIBE                (17)
#define CMD_ID_MQTT_PUBLISH                    (18)
#define CMD_ID_MQTT_ASYNC_DISCONNECT_EVENT     (19)
#define CMD_ID_MQTT_ASYNC_SUBSCRIPTION_EVENT   (20)

#define CMD_ID_INVALID                  (255)

#define STR_TOKEN_DATA_FORMAT           "data_format"
#define STR_TOKEN_DATA_VALUE            "data_value"
#define STR_TOKEN_VALUE_TYPE            "valuetype"
#define STR_TOKEN_VALUE                 "value"
#define STR_TOKEN_VALUE_TYPE_INTEGER    "integer"
#define STR_TOKEN_VALUE_TYPE_FLOAT      "float"
#define STR_TOKEN_VALUE_TYPE_BOOLEAN    "boolean"
#define STR_TOKEN_VALUE_TYPE_STRING     "string"
#define STR_TOKEN_VALUE_TYPE_JSON       "json"

#define STR_TOKEN_MODULEID              "moduleid"
#define STR_TOKEN_ACTIONID              "actionid"
#define STR_TOKEN_TRIGGERID             "triggerid"

#define STR_TOKEN_ENABLE                "enable"
#define STR_TOKEN_ENABLED               "enabled"

#define STR_TOKEN_ADDR_TYPE             "addr-type"
#define STR_TOKEN_IP_ADDRESS            "ip-address"
#define STR_TOKEN_ELAPSED_TIME          "time"
#define STR_TOKEN_TYPE                  "type"

#define WCM_TOKEN_SSID_LENGTH             "ssid-length"
#define WCM_TOKEN_SSID                    "ssid"
#define WCM_TOKEN_SECURITY_TYPE           "security-type"
#define WCM_TOKEN_PASSWORD                "password"
#define WCM_TOKEN_STATUS                  "status"
#define WCM_TOKEN_COMPLETE                "complete"
#define WCM_TOKEN_INCOMPLETE              "incomplete"
#define WCM_TOKEN_UNKNOWN                 "unknown"
#define WCM_TOKEN_MACADDR                 "macaddr"
#define WCM_TOKEN_CHANNEL                 "channel"
#define WCM_TOKEN_CHANNEL_WIDTH           "channel-width"
#define WCM_TOKEN_BAND                    "band"
#define WCM_TOKEN_SIGNAL_STRENGTH         "signal-strength"
#define WCM_TOKEN_BSSID                   "bssid"
#define WCM_TOKEN_METHOD                  "method"
#define WCM_TOKEN_DHCP                    "dhcp"
#define WCM_TOKEN_STATIC                  "static"
#define WCM_TOKEN_NETMASK                 "netmask"
#define WCM_TOKEN_GATEWAY                 "gateway"
#define WCM_TOKEN_PRIMARY_DNS             "primary-dns"
#define WCM_TOKEN_SECONDARY_DNS           "secondary-dns"
#define WCM_TOKEN_NW_STATUS               "link-status"

#define MQTT_TOKEN_BROKERID_TYPE          "brokerid"
#define MQTT_TOKEN_HOSTNAME               "host"
#define MQTT_TOKEN_PORT                   "port"
#define MQTT_TOKEN_TLS                    "tls"
#define MQTT_TOKEN_ROOTCA                 "rootca"
#define MQTT_TOKEN_CLIENTCERT             "clientcert"
#define MQTT_TOKEN_CLIENTKEY              "clientkey"
#define MQTT_TOKEN_CLIENTID               "clientid"
#define MQTT_TOKEN_CLEANSESSION           "cleansession"
#define MQTT_TOKEN_USERNAME               "username"
#define MQTT_TOKEN_PASSWORD               "password"
#define MQTT_TOKEN_LASTWILLTOPIC          "lastwilltopic"
#define MQTT_TOKEN_LASTWILLQOS            "lastwillqos"
#define MQTT_TOKEN_LASTWILLMSG            "lastwillmsg"
#define MQTT_TOKEN_LASTWILLRETAIN         "lastwillretain"
#define MQTT_TOKEN_KEEPALIVE              "keepalive"
#define MQTT_TOKEN_PUBLISHQOS             "publishqos"
#define MQTT_TOKEN_PUBLISHRETAIN          "publishretain"
#define MQTT_TOKEN_PUBLISHRETRYLIMIT      "publishretrylimit"
#define MQTT_TOKEN_SUBSCRIBERQOS          "subscribeqos"
#define MQTT_TOKEN_TOPIC                  "topic"
#define MQTT_TOKEN_QOS                    "qos"
#define MQTT_TOKEN_MSG                    "message"
#define MQTT_TOKEN_DISCONNECT_REASON      "disconnectreason"

/*
 * IP Addresses are stored in big endian format.
 */
#define PRINT_IP(addr)    (int)((addr >> 0) & 0xFF), (int)((addr >> 8) & 0xFF), (int)((addr >> 16) & 0xFF), (int)((addr >> 24) & 0xFF)

#define AT_CMD_REFAPP_LOG_ENABLE

#ifdef AT_CMD_REFAPP_LOG_ENABLE
#define AT_CMD_REFAPP_LOG_MSG(args) { printf args;}
#else
#define AT_CMD_REFAPP_LOG_MSG(args)
#endif

#define AT_CMD_REF_APP_IP_ADDR_STR_LEN   ( 20)
#define AT_CMD_REF_APP_MQTT_BUFFER_SIZE  (5*1024)

/******************************************************
 *                    Constants
 ******************************************************/
/*
 * Default MQTT port. Used when host does not supply the port number.
 */
#define AT_CMD_REF_APP_MQTT_PORT     1883

/*
 * Default MQTT TLS port. Used when host does not supply the port number.
 */
#define AT_CMD_REF_APP_MQTT_TLS_PORT 8883

/*
 * Default MQTT Publish QOS. Used when host does not supply the publish QOS.
 */
#define AT_CMD_REF_APP_MQTT_PUBLISH_QOS 1

/*
 * Default MQTT Subscribe QOS. Used when host does not supply the subscribe QOS.
 */
#define AT_CMD_REF_APP_MQTT_SUBSCRIBE_QOS 1

/*
 * Default MQTT publish retry count. Used when host does not supply the publish retry count value.
 */
#define AT_CMD_REF_APP_MQTT_PUBLISH_RETRY_LIMIT 3

/*
 * Default Client ID
 */
#define  AT_CMD_REF_APP_MQTT_CLIENT_ID "h1cp-test-client"

/******************************************************
 *                   Enumerations
 ******************************************************/
/*
 * Used for finding various items for example servers in the respective list.
 */
typedef enum
{
    AT_CMD_REF_APP_MQTT_FIND_SERVER
} at_cmd_ref_app_mqtt_find_type_t;

/******************************************************
 *                 Type Definitions
 ******************************************************/
typedef uint32_t ipv4_addr_t;       /**< IPv4 address in network byte order. */


/******************************************************
 *                    Enumerations
 ******************************************************/
typedef enum
{
    AT_CMD_REF_APP_RESULT_STATUS_SUCCESS = 0,             /**< Success status */
    AT_CMD_REF_APP_RESULT_STATUS_ERROR                    /**< Error   status */
} at_cmd_ref_app_result_status_t;


/******************************************************
 *                    Structures
 ******************************************************/
/**
 * security table entry.
 */
typedef struct
{
    char                        *cmd_name;          /**< String command name                */
    uint32_t                    cmd_id;             /**< Command identifier for the command */
} at_cmd_security_def_t;

/**
 * network change notification structure
 */
typedef struct
{
    at_cmd_msg_base_t       base;       /**< AT command message header  structure */
    cy_wcm_event_t          event;      /**< WCM event */
    cy_wcm_ip_address_t     ip_addr;    /**< Contains the IP address for the CY_WCM_EVENT_IP_CHANGED event. */
} at_cmd_ref_app_network_change_t;


/**
 * wifi connect structure
 */
typedef struct
{
    at_cmd_msg_base_t       base;                                /**< AT command message header  structure */
    size_t                  ssid_length;                         /**< SSID length */
    char                    ssid[CY_WCM_MAX_SSID_LEN + 1];       /**< SSID of the access point */
    cy_wcm_security_t       security_type;                       /**< WiFi security type */
    char                    password[CY_WCM_MAX_PASSPHRASE_LEN]; /**< Length of WIFI password */
    uint8_t                 macaddr[CY_WCM_MAC_ADDR_LEN];        /**< MAC Address of the access point */
    cy_wcm_wifi_band_t      band;                                /**< WiFi band of access point */
}  at_cmd_ref_app_wcm_connect_specific_t;

/**
 * Get IP Address structure
 */
typedef struct
{
    at_cmd_msg_base_t              base;           /**< AT command message header  structure */
    cy_wcm_ip_address_t            addr_type;      /**< IP Address for IPV4 and IPV6 */
}  at_cmd_ref_app_wcm_get_ip_type_t;


/**
 * Get AP info result structure
 */
typedef struct
{
    at_cmd_msg_base_t       base;                          /**< AT command message header  structure */
    size_t                  ssid_length;                   /**< SSID length */
    char                    ssid[CY_WCM_MAX_SSID_LEN + 1]; /**< SSID of the access point */
    cy_wcm_security_t       security_type;                 /**< WiFi security type */
    uint8_t                 bssid[CY_WCM_MAC_ADDR_LEN];    /**< MAC Address of the access point */
    uint16_t                channel_width;                 /**< channel width   */
    int16_t                 signal_strength;               /**< signal strength */
    uint8_t                 channel;                       /**< channel number  */
} at_cmd_ref_app_host_ap_info_result_t;

/**
 * Get IPv4 Address, Netmask, Gateway and DNS Addresses
 */
typedef struct
{
    at_cmd_msg_base_t       base;       /**< AT command message header  structure */
    bool                    dhcp;       /**< true if DHCP is enabled else false   */
    ipv4_addr_t             address;    /**< IP Address of the device             */
    ipv4_addr_t             netmask;    /**< netmask address                      */
    ipv4_addr_t             gateway;    /**< gateway address                      */
    ipv4_addr_t             primary;    /**< primary DNS address                  */
    ipv4_addr_t             secondary;  /**< secondary DNS address                */
} at_cmd_ref_host_ipv4_info_t ;

/**
 * ICMP ping structure
 */
typedef struct
{
    at_cmd_msg_base_t     base;          /**< AT command message header  structure */
    uint32_t              elapsed_time;  /**< elapsed time */
} at_cmd_ref_ping_ip_addr_t;

/**
 * Enable/Disable network change notification Message
 */
typedef struct
{
    at_cmd_msg_base_t       base;        /**< AT command message header  structure          */
    bool                    enable;      /**< Enable/Disable notification of network change */
}  at_cmd_ref_app_wcm_nw_change_notification_t;

/**
 * Get WiFi scan result structure
 */
typedef struct
{
    at_cmd_msg_base_t       base;                          /**< AT command message header  structure */
    size_t                  ssid_length;                   /**< SSID length                          */
    char                    ssid[CY_WCM_MAX_SSID_LEN + 1]; /**< SSID of the access point             */
    cy_wcm_security_t       security_type;                 /**< WiFi security type                   */
    uint8_t                 macaddr[CY_WCM_MAC_ADDR_LEN];  /**< MAC Address of the access point      */
    int16_t                 signal_strength;               /**< signal strength                      */
    uint8_t                 channel;                       /**< channel number                       */
    cy_wcm_wifi_band_t      band;                          /**< WiFi band of access point            */
    bool                    scan_complete;                 /**< WiFi scan complete/incomplete        */
}  at_cmd_ref_app_scan_result_t;

/**
 * Json Text result structure
 */
typedef struct
{
    char                              result_text[AT_CMD_REF_APP_BUFFER_SIZE]; /**< Result buffer */
    at_cmd_ref_app_result_status_t    result_status;                           /**< Result status */
} at_cmd_result_data_t;

/* MQTT Structures */

/**
 *  MQTT broker id
 */
typedef struct
{
    at_cmd_msg_base_t base;        /**< AT command message header  structure */
    uint32_t brokerid;             /**< MQTT broker id */
} at_cmd_ref_app_mqtt_brokerid_t;

/**
 * MQTT Define broker
 */
typedef struct
{
    at_cmd_msg_base_t base;        /**< AT command message header  structure               */
    cy_linked_list_node_t node;    /**< Linked list node to keep track of MQTT connections */
    bool connected;                /**< True if MQTT is connected to broker else false     */
    uint32_t serverid;             /**< broker id                                          */
    char *hostname;                /**< host name of the MQTT broker                       */
    uint16_t port;                 /**< port number of the MQTT broker                     */
    bool tls;                      /**< TLS connection (if true) else false                */
    char *rootca;                  /**< Root CA certificate of the MQTT broker             */
    char *cert;                    /**< client certificate of the MQTT client              */
    char *key;                     /**< client private key of the MQTT client              */
    char *clientid;                /**< client id string                                   */
    bool cleansession;             /**< clean session (true/false)                         */
    char *username;                /**< user name                )                         */
    char *password;                /**< password                                           */
    char *lastwilltopic;           /**< last will topic                                    */
    uint32_t lastwillqos;          /**< last will qos                                      */
    char *lastwillmessage;         /**< last will message                                  */
    bool lastwillretain;           /**< last will retain                                   */
    uint32_t keepalive;            /**< keep alive                                         */
    uint32_t publishqos;           /**< publish qos                                        */
    bool publishretain;            /**< publish retain                                     */
    uint32_t publishretrylimit;    /**< publish retry limit                                */
    uint32_t subscribeqos;         /**< subscribe qos                                      */
    uint16_t data_length;          /**< data length of the server strings                  */
    cy_mqtt_t mqtt_handle;         /**< MQTT handle                                        */
    uint8_t  *mqtt_buffer;         /**< MQTT buffer                                        */
} at_cmd_ref_app_mqtt_broker_info_t;

/**
 *  MQTT Define server message
 */
typedef struct
{
    at_cmd_msg_base_t base;       /**< AT command message header  structure               */
    uint32_t brokerid;            /**< broker id                                          */
    char *hostname;               /**< host name of the MQTT broker                       */
    uint16_t port;                /**< port number of the MQTT broker                     */
    bool tls;                     /**< TLS connection (if true) else false                */
    char *rootca;                 /**< Root CA certificate of the MQTT broker             */
    char *cert;                   /**< client certificate of the MQTT client              */
    char *key;                    /**< client private key of the MQTT client              */
    char *clientid;               /**< client id string                                   */
    bool cleansession;            /**< clean session (true/false)                         */
    char *username;               /**< user name                )                         */
    char *password;               /**< password                                           */
    char *lastwilltopic;          /**< last will topic                                    */
    uint32_t lastwillqos;         /**< last will qos                                      */
    char *lastwillmessage;        /**< last will message                                  */
    bool lastwillretain;          /**< last will retain                                   */
    uint32_t keepalive;           /**< keep alive                                         */
    uint32_t publishqos;          /**< publish qos                                        */
    bool     publishretain;       /**< publish retain                                     */
    uint32_t publishretrylimit;   /**< publish retry limit                                */
    uint32_t subscribeqos;        /**< subscribe qos                                      */
    uint16_t data_length;         /**< data length of the server strings                  */
    char data[0];                 /**< The pointer to the server strings                  */
} at_cmd_ref_app_mqtt_define_server_t;

/*
 * Used for finding various items like actions, triggers and servers in the respective list.
 */
typedef struct
{
    uint32_t id;                           /** Broker id         */
    at_cmd_ref_app_mqtt_find_type_t type;  /** Item type to find */
} at_cmd_ref_app_mqtt_find_item_t;

typedef struct
{
    at_cmd_msg_base_t base;       /**< AT command message header  structure               */
    uint32_t brokerid;            /**< broker id                                          */
    char *topic;                  /**< publish topic                                      */
    uint32_t qos;                 /**< publish qos                                        */
    char *msg;                    /**< publish message                                    */
} at_cmd_ref_app_mqtt_publish_t;


typedef struct
{
    at_cmd_msg_base_t base;       /**< AT command message header  structure               */
    uint32_t brokerid;            /**< broker id                                          */
    char *topic;                  /**< subscribe topic                                    */
    uint32_t qos;                 /**< subscribe qos                                      */
} at_cmd_ref_app_mqtt_subscribe_t;

typedef struct
{
    at_cmd_msg_base_t base;      /**< AT command message header  structure               */
    uint32_t brokerid;           /**< broker id                                          */
    char *topic;                  /**< un subscribe topic                                */
} at_cmd_ref_app_mqtt_unsubscribe_t;

typedef struct
{
    at_cmd_msg_base_t base;                     /**< AT command message header  structure */
    cy_mqtt_disconn_type_t disconnect_reason;   /**< MQTT async disconnect reason         */
} at_cmd_ref_app_mqtt_disconnect_event_t;

/******************************************************
 *                    Function Declarations
 ******************************************************/
/** This function checks if the protocol data is ready at the USB Receive FIFO
 *
 * @param   opaque                     : Pointer to the opaque data
 * @return  bool                       : True  ( Data is ready)
 *                                     : False ( Data is not ready )
 *
 *******************************************************************************/
bool at_cmd_refapp_transport_is_data_ready(void *opaque);

/** This function parses the WCM command in Json Text format to corresponding data structure.
 *
 * @param   cmd_id                     : The command id of the WCM command
 * @param   serial                     : The serial number of the WCM command
 * @param   cmd_len                    : The command length of the  WCM command
 * @param   cmd                        : The pointer to the command buffer containing Json Text
 * @return  at_cmd_msg_base_t          : The pointer to the at_cmd_msg_base_t
 *                                     : NULL ( error case)
 *
 *******************************************************************************/
at_cmd_msg_base_t *at_cmd_refapp_parse_wcm_cmd( uint32_t cmd_id, uint32_t serial, uint32_t cmd_len, char *cmd);

/** This function initializes the WCM and registers callback with WCM for event notification
 *
 * @param   msgq                       : The pointer to message queue
 * @return  cy_rslt_t                  : CY_RSLT_SUCCESS
 *                                     : CY_RSLT_TYPE_ERROR
 *
 *******************************************************************************/
cy_rslt_t at_cmd_refapp_wcm_init(cy_queue_t *msgq);


/** This function looks up the wifi security table given a name to return index to the table.
 *
 * @param   cmd_name                   : The pointer to wifi security name
 * @param   table                      : The pointer to the structure of the security table
 * @return  int                        : The index to the table if the wifi security name matches the name in table
 *                                     : -1 ( no match found)
 *
 *******************************************************************************/
int at_cmd_refapp_security_table_lookup_by_name(char *cmd_name, at_cmd_security_def_t *table);

/** This function looks up the wifi security table given a value to return index to the table.
 *
 * @param   value                      : The value wifi security
 * @param   table                      : The pointer to the structure of the security table
 * @return  int                        : The index to the table if the wifi security name matches the name in table
 *                                     : -1 ( no match found)
 *
 *******************************************************************************/
int at_cmd_refapp_security_table_lookup_by_value(uint32_t value, at_cmd_security_def_t *table);

/** This function sends the message to AT command reference app
 *
 * @param   msg                        : The pointer to the message data
 * @return  cy_rslt_t                  : CY_RSLT_SUCCESS
 *                                     : CY_RSLT_TYPE_ERROR
 *
 *******************************************************************************/
cy_rslt_t at_cmd_refapp_send_message(at_cmd_msg_base_t *msg);

/** This function creates a Json Text from the structure and stores into buffer
 *
 * @param   cmd_id                     : The command id of the command
 * @param   msg                        : The pointer to the message structure
 * @param   buffer                     : The buffer to which Json text needs to be copied
 * @param   buflen                     : The length of the Json text buffer
 * @return  cy_rslt_t                  : CY_RSLT_SUCCESS
 *                                     : CY_RSLT_TYPE_ERROR
 *
 *******************************************************************************/
cy_rslt_t at_cmd_refapp_process_wcm_host_msg(uint32_t cmd_id, at_cmd_msg_base_t *msg, char *buffer, uint32_t buflen);

/** This function processes the message to call respective WCM API(s) based on the command id in the message.
 *
 * @param   msg                        : The pointer to the message structure
 * @param   result_str                 : The pointer to the result structure
 * @return  at_cmd_msg_base_t          : The pointer to the WCM structure based on the WCM command id
 *                                     : NULL ( error case where the WCM API fails  )
 *
 *******************************************************************************/
at_cmd_msg_base_t *at_cmd_refapp_wcm_process_message(at_cmd_msg_base_t *msg, at_cmd_result_data_t *result_str);

/** This function builds the WCM Json text to be sent to the host after processing based on WCM command id
 *
 * @param   cmd_id                     : The command id of the WCM command
 * @param   serial                     : The serial number of the WCM command
 * @param   cmd                        : The pointer to the message structure of WCM
 * @return   result_str                 : The pointer to the result structure in Json format to be sent to host
 *
 *******************************************************************************/
void at_cmd_refapp_build_wcm_json_text_to_host(uint32_t cmd_id, uint32_t serial, at_cmd_msg_base_t *cmd, at_cmd_result_data_t *result_str);

/** This function initializes MQTT
 *
 * @return   result_str                 : The pointer to the result structure in Json format to be sent to host
 *
 *******************************************************************************/
cy_rslt_t at_cmd_refapp_mqtt_init (void);

/** This function parses Json text of the MQTT command
 *
 * @param   cmd_id                     : The command id of the MQTT command
 * @param   serial                     : The serial number of the WCM command
 * @param   cmd_len                    : The command length
 * @param   cmd                        : The pointer to the command in Json Text format
 * @return  at_cmd_msg_base_t          : The pointer to the MQTT structure based on the MQTT command id
 *
 *******************************************************************************/
at_cmd_msg_base_t *at_cmd_refapp_parse_mqtt_cmd( uint32_t cmd_id, uint32_t serial, uint32_t cmd_len, char *cmd);

/** This function builds the MQTT Json text to be sent to the host after processing based on WCM command id
 *
 * @param   cmd_id                     : The command id of the WCM command
 * @param   serial                     : The serial number of the WCM command
 * @param   cmd                        : The pointer to the message structure of WCM
 * @return   result_str                : The pointer to the result structure in Json format to be sent to host
 *
 *******************************************************************************/
void at_cmd_refapp_build_mqtt_json_text_to_host(uint32_t cmd_id, uint32_t serial, at_cmd_msg_base_t *cmd, at_cmd_result_data_t *result_str);

/** This function processes the message to call respective MQTT API(s) based on the command id in the message.
 *
 * @param   msg                        : The pointer to the message structure
 * @param   result_str                 : The pointer to the result structure
 * @return  at_cmd_msg_base_t          : The pointer to the MQTT structure based on the MQTT command id
 *                                     : NULL ( error case where the MQTT API fails  )
 *
 *******************************************************************************/
at_cmd_msg_base_t* at_cmd_refapp_mqtt_process_message(at_cmd_msg_base_t *msg, at_cmd_result_data_t *result_str);

/** This function creates a Json Text from the structure and stores into buffer
 *
 * @param   cmd_id                     : The command id of the command
 * @param   msg                        : The pointer to the message structure
 * @param   buffer                     : The buffer to which Json text needs to be copied
 * @param   buflen                     : The length of the Json text buffer
 * @return  cy_rslt_t                  : CY_RSLT_SUCCESS
 *                                     : CY_RSLT_TYPE_ERROR
 *
 *******************************************************************************/
cy_rslt_t at_cmd_refapp_process_mqtt_host_msg(uint32_t cmd_id, at_cmd_msg_base_t *msg, char *buffer, uint32_t buflen);

/** This function process MQTT event callback
 *
 * @param   cmd_id                     : The command id of the command
 * @param   event                      : The Pointer to MQTT event
 * @param   result_str                 : The pointer to the result structure in Json format to be sent to host
 * @return  cy_rslt_t                  : CY_RSLT_SUCCESS
 *                                     : CY_RSLT_TYPE_ERROR
 *
 *******************************************************************************/
cy_rslt_t at_cmd_refapp_mqtt_event_callback( uint32_t cmd_id, at_cmd_msg_base_t *mqtt_async_event, at_cmd_result_data_t *result_str );
