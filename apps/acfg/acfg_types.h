#ifndef _ACFG_TYPES_H
#define _ACFG_TYPES_H

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
#define MAC2STR_ADDR(a) &(a)[0], &(a)[1], &(a)[2], &(a)[3], &(a)[4], &(a)[5]

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

typedef void acfg_wsupp_hdl_t;
typedef char acfg_ssid_t;

typedef unsigned char mac_addr_t[ETH_ALEN];
typedef unsigned int mac_addr_int_t[ETH_ALEN];

typedef enum {
    ACFG_STATUS_OK = 0,
    ACFG_STATUS_ETIMEDOUT,
    ACFG_STATUS_FAILED,
    ACFG_STATUS_BAD_ARG,
    ACFG_STATUS_IN_USE,
    ACFG_STATUS_ARG_TOO_BIG,
} acfg_status_t;

/** negotiated GO/CLIENT role number */
enum acfg_role {
    ACFG_CLIENT,
    ACFG_GO,
};
/** types of P2P_FIND - scan type */
enum acfg_find_type {
    ACFG_FIND_TYPE_SOCIAL, /**< default find type */
    ACFG_FIND_TYPE_PROGRESSIVE,
};
/** sources of a given pin */
enum acfg_pin_type {
    ACFG_PIN_TYPE_UNKNOWN, /**< default pin type */
    ACFG_PIN_TYPE_LABEL,
    ACFG_PIN_TYPE_DISPLAY,
    ACFG_PIN_TYPE_KEYPAD,
};
/** network items that can be set */
enum acfg_network_item {
    ACFG_NW_SSID,
    ACFG_NW_PSK,
    ACFG_NW_KEY_MGMT,
    ACFG_NW_ENABLE,     /**< actually starts the network connection */
    ACFG_NW_PAIRWISE,
    ACFG_NW_GROUP,
    ACFG_NW_PROTO,
};
/** types of wps calls */
enum acfg_wps_type {
    ACFG_WPS_PIN,
    ACFG_WPS_PBC,
    ACFG_WPS_REG,
    ACFG_WPS_ER_START,
    ACFG_WPS_ER_STOP,
    ACFG_WPS_ER_PIN,
    ACFG_WPS_ER_PBC,
    ACFG_WPS_ER_LEARN,
};
/** types of p2p_prov_disc calls */
enum acfg_prov_disc_type {
    ACFG_PROV_DISC_DISPLAY,
    ACFG_PROV_DISC_KEYPAD,
    ACFG_PROV_DISC_PBC,
};
/** types of p2p_service_add calls */
enum acfg_service_type {
    ACFG_SERVICE_ADD_UPNP,
    ACFG_SERVICE_ADD_BONJOUR,
};
/** types of p2p_set calls */
enum acfg_p2pset_type {
    ACFG_P2PSET_DISC,
    ACFG_P2PSET_MANAGED,
    ACFG_P2PSET_LISTEN,
    ACFG_P2PSET_SSID_POST,
};

/** types of set calls */
enum acfg_set_type {
    ACFG_SET_UUID,
    ACFG_SET_DEVICE_NAME,
    ACFG_SET_MANUFACTURER,
    ACFG_SET_MODEL_NAME,
    ACFG_SET_MODEL_NUMBER,
    ACFG_SET_SERIAL_NUMBER,
    ACFG_SET_DEVICE_TYPE,
    ACFG_SET_OS_VERSION,
    ACFG_SET_CONFIG_METHODS,
    ACFG_SET_SEC_DEVICE_TYPE,
    ACFG_SET_P2P_GO_INTENT,
    ACFG_SET_P2P_SSID_POSTFIX,
    ACFG_SET_PERSISTENT_RECONNECT,
    ACFG_SET_COUNTRY,
};

/**
 * All async events are arbitrarily enumerated here.
 */
enum acfg_eventnumber {
    ACFG_WPA_EVENT_CONNECTED,
    ACFG_WPA_EVENT_DISCONNECTED,
    ACFG_WPA_EVENT_TERMINATING,
    ACFG_P2P_EVENT_DEVICE_FOUND,
    ACFG_P2P_EVENT_GO_NEG_REQUEST,
    ACFG_P2P_EVENT_GO_NEG_SUCCESS,
    ACFG_P2P_EVENT_GO_NEG_FAILURE,
    ACFG_P2P_EVENT_GROUP_FORMATION_SUCCESS,
    ACFG_P2P_EVENT_GROUP_FORMATION_FAILURE,
    ACFG_P2P_EVENT_GROUP_STARTED,
    ACFG_P2P_EVENT_GROUP_REMOVED,
    ACFG_P2P_EVENT_PROV_DISC_SHOW_PIN,
    ACFG_P2P_EVENT_PROV_DISC_ENTER_PIN,
    ACFG_P2P_EVENT_SERV_DISC_REQ,
    ACFG_P2P_EVENT_SERV_DISC_RESP,
    ACFG_P2P_EVENT_INVITATION_RECEIVED,
    ACFG_P2P_EVENT_INVITATION_RESULT,
    ACFG_P2P_EVENT_PROV_DISC_PBC_REQ,
    ACFG_P2P_EVENT_PROV_DISC_PBC_RESP,
    ACFG_AP_STA_CONNECTED,
    ACFG_WPS_EVENT_ENROLLEE_SEEN,
    ACFG_WPA_EVENT_SCAN_RESULTS,
};

/**
 * Structure returned on a ACFG_P2P_EVENT_DEVICE_FOUND event \n
 * decoded from the wpa_supplicant text response. \n
 * the pri_dev_type is decoded into 3 integers, see p2p spec
 * \n Sample output from wpa_supplicant: \n
 *  P2P-DEVICE-FOUND 00:03:7f:10:a5:e6 p2p_dev_addr=00:03:7f:10:a5:e6 pri_dev_type=1-0050F204-1 name='PB44_MB' config_methods=0x188 dev_capab=0x21 group_capab=0x0 \n
 *  
 */
struct S_P2P_EVENT_DEVICE_FOUND {
    unsigned int pri_dev_type1;     /**< always, from pri_dev_type */
    unsigned int pri_dev_type2;     /**< always, from pri_dev_type */
    unsigned int pri_dev_type3;     /**< always, from pri_dev_type */
    int config_methods;             /**< always, see p2p spec */
    int dev_capab;                  /**< always, see p2p spec */
    int group_capab;                /**< always, see p2p spec */
    mac_addr_t dev_addr;            /**< always, station addr returned */
    mac_addr_t p2p_dev_addr;        /**< always, p2p addr returned */
    char name[32];                  /**< always, client name */
};
/**
 * Structure returned on a ACFG_P2P_EVENT_PROV_DISC_PBC_REQ event \n
 * decoded from the wpa_supplicant text response. \n
 * the pri_dev_type is decoded into 3 integers, see p2p spec
 * \n Sample output from wpa_supplicant: \n
 *  P2P-PROV-DISC-PBC-REQ 02:03:7f:10:a5:e6 p2p_dev_addr=02:03:7f:10:a5:e6 pri_dev_type=1-0050F204-1 name='Calfee-p2p-linux' config_methods=0x188 dev_capab=0x23 group_capab=0x0 \n
 *
 * At this time the exact same data is returned as P2P_EVENT_DEVICE_FOUND
 *  
 */
struct S_P2P_EVENT_PROV_DISC_PBC_REQ {
    struct S_P2P_EVENT_DEVICE_FOUND data_struct; /**< return identical data as P2P-DEVICE-FOUND*/
};
/**
 * Structure returned on a ACFG_P2P_EVENT_INVITATION_RESULT event \n
 * decoded from the wpa_supplicant text response. \n
 * the pri_dev_type is decoded into 3 integers, see p2p spec
 * \n Sample output from wpa_supplicant: \n
 *  P2P-DEVICE-FOUND 00:03:7f:10:a5:e6 p2p_dev_addr=00:03:7f:10:a5:e6 pri_dev_type=1-0050F204-1 name='PB44_MB' config_methods=0x188 dev_capab=0x21 group_capab=0x0 \n
 *  
 */

struct S_P2P_EVENT_INVITATION_RESULT {
    int status;                     /**< always, see p2p spec */
    mac_addr_t bssid;               /**< optional bssid returned */
};

/**
 * Structure returned on a ACFG_P2P_EVENT_INVITATION_RECEIVED event \n
 * decoded from the wpa_supplicant text response.
 * \n Sample output of 3 possibilities from wpa_supplicant: \n
 *  P2P-INVITATION-RECEIVED sa=02:03:7f:10:a5:ee persistent=1 \n
 *  P2P-INVITATION-RECEIVED sa=02:03:7f:10:a5:ee go_dev_addr=12:03:7f:10:a5:ee unknown-network \n
 *  P2P-INVITATION-RECEIVED sa=02:03:7f:10:a5:ee go_dev_addr=12:03:7f:10:a5:ee bssid=12:03:7f:10:a5:ee unknown-network \n
 *  
 */
struct S_P2P_EVENT_INVITATION_RECEIVED {
    int persistent;                 /**< if false, have a go_dev_addr */
    int persistent_id;              /**< optional persistent_id returned, -1 if not persistent */
    mac_addr_t station_address;     /**< always, station addr returned */
    mac_addr_t go_dev_addr;         /**< optional GO addr returned */
    mac_addr_t bssid;               /**< optional bssid returned = 00:00:00:00:00:00 if not returned */
};

/**
 * Structure returned on a ACFG_P2P_EVENT_SERV_DISC_RESP event \n
 * decoded from the wpa_supplicant text response.
 * \n Sample output from wpa_supplicant: \n
 * P2P-SERV-DISC-RESP sa=02:f0:bc:44:87:62 1 "undecoded string data"
 *  
 */
struct S_P2P_EVENT_SERV_DISC_RESP {
    int update_indicator;           /**< always, update flag */
    mac_addr_t station_address;     /**< always, station addr returned */
    char upd_string[64];            /**< always, disc info text */
};

/**
 * Structure returned on a ACFG_P2P_EVENT_SERV_DISC_REQ event \n
 * decoded from the wpa_supplicant text response.
 * \n Sample output from wpa_supplicant: \n
 * P2P-SERV-DISC-REQ 2437 02:f0:bc:44:87:62 1 2 "undecoded string data"
 *  
 */
struct S_P2P_EVENT_SERV_DISC_REQ {
    int update_indicator;           /**< always, update_indicator */
    int dialog_token;               /**< always, dialog_token */
    int freq;                       /**< always, channel freq */
    mac_addr_t station_address;     /**< always, station addr returned */
    char upd_string[64];            /**< always, disc info text */
};

/**
 * Structure returned on a ACFG_P2P_EVENT_PROV_DISC_SHOW_PIN event \n
 * decoded from the wpa_supplicant text response. \n
 * The intent is to display an addr/pin on a local p2p device display
 * \n Sample output from wpa_supplicant: \n
 * P2P-PROV-DISC-SHOW-PIN 02:40:61:c2:f3:b7 12345670
 *  
 */
struct S_P2P_EVENT_PROV_DISC_SHOW_PIN {
    mac_addr_t station_address;     /**< always, station addr returned */
    char pin[32];                   /**< always, generated PIN */
};

/**
 * Structure returned on a ACFG_P2P_EVENT_PROV_DISC_ENTER_PIN event \n
 * decoded from the wpa_supplicant text response. \n
 * The intent is to prompt user to enter a pin on a local p2p device
 * \n Sample output from wpa_supplicant: \n
 * P2P-PROV-DISC-ENTER-PIN 02:40:61:c2:f3:b7
 *  
 */
struct S_P2P_EVENT_PROV_DISC_ENTER_PIN {
    mac_addr_t station_address;     /**< always, station addr */
};

/**
 * Structure returned on a ACFG_P2P_EVENT_PROV_DISC_PBC_RESP event \n
 * decoded from the wpa_supplicant text response. \n
 * It appears to be a response to a connect request, handled with pbc
 * \n Sample output from wpa_supplicant: \n
 * P2P-PROV-DISC-PBC-RESP 02:03:7f:10:a5:ee
 *  
 */
struct S_P2P_EVENT_PROV_DISC_PBC_RESP {
    mac_addr_t station_address;     /**< always, station addr */
};

/**
 * Structure returned on a ACFG_P2P_EVENT_GROUP_STARTED event \n
 * decoded from the wpa_supplicant text response.
 * \n Sample output from wpa_supplicant: \n
 * P2P-GROUP-STARTED wlan1 client ssid="DIRECT-qM" psk=24b4f go_dev_addr=00:00:00:00:00:00 \n
 * or: \n
 * P2P-GROUP-STARTED wlan1 GO ssid="DIRECT-4W" passphrase="I3oIObG7" go_dev_addr=00:03:7f:10:a5:ee \n
 * 
 */
struct S_P2P_EVENT_GROUP_STARTED {
    enum acfg_role my_role;         /**< always, negotiated role */
    mac_addr_t go_dev_addr;         /**< always, other addr returned */
    char iface[IFNAMSIZ];                 /**< always, interface used in group */
    char ssid[32];                  /**< always, direct group name */
    int  freq;                      /**< always, channel used by the group */
    char psk_or_passphrase[64];     /**< always, pre shared key if client or passphrase if GO */
};

/**
 * Structure returned on a ACFG_P2P_EVENT_GO_NEG_REQUEST event\n
 * decoded from the wpa_supplicant text response.\n
 * P2P-GO-NEG-REQUEST 02:03:7f:10:a5:ee\n
 *  
 */
struct S_P2P_EVENT_GO_NEG_REQUEST {
    mac_addr_t station_address;     /**< always, station addr that wants to start GO NEG */
};

/**
 * Structure returned on a ACFG_WPS_EVENT_ENROLLEE_SEEN event\n
 * This event only happens on the group interface. \n
 * decoded from the wpa_supplicant text response.\n
 * WPS-EVENT-ENROLLEE-SEEN 02:03:7f:10:a5:ee plusmore\n
 *  
 */
struct S_WPS_EVENT_ENROLLEE_SEEN {
    mac_addr_t station_address;     /**< always, station that is enrolling, addr */
    char misc_params[64];           /**< always, undecoded char data */
};

/**
 * Structure returned on a WPA_EVENT_CONNECTED event\n
 * This event will probably only be seen on the p2pdev interface. \n
 * decoded from the wpa_supplicant text response.\n
 * CTRL-EVENT-CONNECTED 02:03:7f:10:a5:ee plusmore\n
 *  
 */
struct S_WPA_EVENT_CONNECTED {
    mac_addr_t station_address;     /**< always, station that is connected */
    char misc_params[64];           /**< always, undecoded char data */
};

/**
 * Structure returned on a ACFG_AP_STA_CONNECTED event\n
 * This event only happens on the group interface. On the GO side\n
 * decoded from the wpa_supplicant text response.\n
 * AP-STA-CONNECTED 02:03:7f:10:a5:ee\n
 *  
 */
struct S_AP_STA_CONNECTED {
    mac_addr_t station_address;     /**< always, station that connected */
};

/**
 * All async events are read back in this common structure. \n
 * The actual struct used in the union is determined by the event variable. \n
 * If an event doesn't contain data, just the eventnumber is relevant \n
 */
typedef struct acfg_eventdata {
    enum acfg_eventnumber event; /**< event number returned, for caller decoding*/
    union {
        struct S_P2P_EVENT_DEVICE_FOUND         m_P2P_EVENT_DEVICE_FOUND;
        struct S_P2P_EVENT_PROV_DISC_PBC_REQ    m_P2P_EVENT_PROV_DISC_PBC_REQ;
        struct S_P2P_EVENT_INVITATION_RESULT    m_P2P_EVENT_INVITATION_RESULT;
        struct S_P2P_EVENT_INVITATION_RECEIVED  m_P2P_EVENT_INVITATION_RECEIVED;
        struct S_P2P_EVENT_SERV_DISC_RESP       m_P2P_EVENT_SERV_DISC_RESP;
        struct S_P2P_EVENT_SERV_DISC_REQ        m_P2P_EVENT_SERV_DISC_REQ;
        struct S_P2P_EVENT_PROV_DISC_SHOW_PIN   m_P2P_EVENT_PROV_DISC_SHOW_PIN;
        struct S_P2P_EVENT_PROV_DISC_ENTER_PIN  m_P2P_EVENT_PROV_DISC_ENTER_PIN;
        struct S_P2P_EVENT_PROV_DISC_PBC_RESP   m_P2P_EVENT_PROV_DISC_PBC_RESP;
        struct S_P2P_EVENT_GROUP_STARTED        m_P2P_EVENT_GROUP_STARTED;
        struct S_P2P_EVENT_GO_NEG_REQUEST       m_P2P_EVENT_GO_NEG_REQUEST;
        struct S_WPS_EVENT_ENROLLEE_SEEN        m_WPS_EVENT_ENROLLEE_SEEN;
        struct S_AP_STA_CONNECTED               m_AP_STA_CONNECTED;
        struct S_WPA_EVENT_CONNECTED            m_WPA_EVENT_CONNECTED;
    } u;
} acfg_eventdata_t;

#endif
