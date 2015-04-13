/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * acfg_api - OS and chip independent p2p wrapper for wpa_supplicant
 */


#include "includes.h"
#include "common.h"
#include "common/wpa_ctrl.h"

#include "acfg_wsupp.h"

//enum { MSG_MSGDUMP, MSG_DEBUG, MSG_INFO, MSG_WARNING, MSG_ERROR };

static int dbg_lvl = MSG_DEBUG;
#if 1
#define dbgprintf(lvl, fmt, args...) do { \
    if (lvl >= dbg_lvl) { \
        printf("%s(%d): " fmt "\n", __func__, __LINE__, ## args); \
    } \
} while (0)
#else
#define dbgprintf(args...) do { } while (0)
#endif
#define MAX(a, b) (((a) > (b)) ? a : b)
#define MIN(a, b) (((a) < (b)) ? a : b)

#define DEFAULT_IFNAME "/var/run/wpa_supplicant/wlan1"
#define DEFAULT_GLOBAL "/var/run/wpa_supplicant-global"

struct acfg_wsupp {
    struct wpa_ctrl *ctrl;
    struct wpa_ctrl *global;
    struct wpa_ctrl *events;
    acfg_eventdata_t res;
};

static void macint_to_macbyte(mac_addr_int_t in_mac, mac_addr_t out_mac)
{
    int i;

    for (i = 0; i < ETH_ALEN; i++)
        out_mac[i] = (unsigned char) in_mac[i];
}

acfg_wsupp_hdl_t *acfg_wsupp_init(const char *ifname)
{
    struct acfg_wsupp *aptr;
    char *realname = (void *) ifname;

    if (!realname) realname = DEFAULT_IFNAME;

    dbgprintf(MSG_INFO, "open %s", realname);

    aptr = os_zalloc(sizeof(*aptr));
    if (!aptr)
        return NULL;

    aptr->ctrl = wpa_ctrl_open(realname);

    aptr->global = wpa_ctrl_open(DEFAULT_GLOBAL);

    if (!aptr->ctrl && !aptr->global) {
        os_free(aptr);          
        dbgprintf(MSG_ERROR, "Both connects failed!");
        return NULL;
    }

    dbgprintf(MSG_INFO, "2 %p", aptr);
    return (void *) aptr;
}

void acfg_wsupp_uninit(acfg_wsupp_hdl_t *mctx)
{
    struct acfg_wsupp *aptr = (void *) mctx;

    dbgprintf(MSG_INFO, "1 %p", aptr);

    wpa_ctrl_close(aptr->ctrl);

    if (aptr->events)
        wpa_ctrl_close(aptr->events);

    if (aptr->global)
        wpa_ctrl_close(aptr->global);

    dbgprintf(MSG_INFO, "2 %p", aptr);
    os_free(aptr);
}
/* if reply is not null, return response on the command */
static acfg_status_t acfg_global_command(acfg_wsupp_hdl_t *mctx, char *cmd)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    char buf[2048];
    size_t len = sizeof(buf);
    int ret;

    if (aptr->global == NULL) {
        dbgprintf(MSG_INFO, 
                "Not connected to wpa_supplicant with global");
        return -1;
    }
    dbgprintf(MSG_INFO, "executing %s", cmd);

    ret = wpa_ctrl_request(aptr->global, cmd, os_strlen(cmd), buf, &len, 
            NULL);
    if (ret == -2) {
        dbgprintf(MSG_INFO, "'%s' command timed out.", cmd);
        return ACFG_STATUS_ETIMEDOUT;
    } else if (ret < 0) {
        dbgprintf(MSG_INFO, "'%s' command failed.", cmd);
        return ACFG_STATUS_FAILED;
    }
    buf[len] = '\0';
    dbgprintf(MSG_INFO, "%s", buf);

    return ACFG_STATUS_OK;
}
/* 
 * if reply is not null, return response on the command 
 */
static acfg_status_t acfg_ctrl_command(acfg_wsupp_hdl_t *mctx, char *cmd,
        char *reply, size_t *reply_len)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    int ret;
    char buf[10];
    size_t len = sizeof(buf);
    size_t *mlen = &len;
    char *mrep = buf;

    if (reply && reply_len && *reply_len) {
        mrep = reply;
        mlen = reply_len; 
    }

    if (aptr->ctrl == NULL) {
        dbgprintf(MSG_INFO, 
                "Not connected to wpa_supplicant - command dropped.");
        return -1;
    }
    dbgprintf(MSG_INFO, "executing (%d bytes) %s",
            (int) os_strlen(cmd), cmd);

    ret = wpa_ctrl_request(aptr->ctrl, cmd, os_strlen(cmd), mrep, mlen,
            NULL);
    if (ret == -2) {
        dbgprintf(MSG_INFO, "'%s' command timed out.", cmd);
        return ACFG_STATUS_ETIMEDOUT;
    } else if (ret < 0) {
        dbgprintf(MSG_INFO, "'%s' command failed.", cmd);
        return ACFG_STATUS_FAILED;
    }

    mrep[*mlen] = '\0';
    dbgprintf(MSG_INFO, "%s", mrep);

    if (*mlen && (!strncmp(mrep, "FAIL",4)))
        return ACFG_STATUS_FAILED;


    return ACFG_STATUS_OK;
}

static acfg_status_t acfg_stringize_cmd_reply(acfg_wsupp_hdl_t *mctx, 
        char *reply, size_t *reply_len, char *fmt, ...)
{
    va_list ap;
    char cmd[128];
    size_t res;

    va_start(ap, fmt);
    res = vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    if (res < 0 || res >= sizeof(cmd))
        return ACFG_STATUS_BAD_ARG;

    cmd[sizeof(cmd) - 1] = '\0';
    return acfg_ctrl_command(mctx, cmd, reply, reply_len);
}

static acfg_status_t acfg_stringize_cmd(acfg_wsupp_hdl_t *mctx, char *fmt, ...)
{
    va_list ap;
    char cmd[128];
    size_t res;

    va_start(ap, fmt);
    res = vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    if (res < 0 || res >= sizeof(cmd))
        return ACFG_STATUS_BAD_ARG;

    cmd[sizeof(cmd) - 1] = '\0';
    return acfg_ctrl_command(mctx, cmd, NULL, 0);
}

static acfg_status_t acfg_stringize_cmd_opt_arg(acfg_wsupp_hdl_t *mctx, 
        const char *command, const char *optarg, char *reply, size_t *reply_len)
{
    if (!optarg) {
        /* supplicant is picky about the string length if no arg */
        return acfg_stringize_cmd_reply(mctx, reply, reply_len,
                "%s", command);
    }
    return acfg_stringize_cmd_reply(mctx, reply, reply_len,
            "%s %s", command, optarg);
}
static const char * f_find_type(enum acfg_find_type item)
{
    switch (item) {
        case ACFG_FIND_TYPE_SOCIAL: return "social";
        case ACFG_FIND_TYPE_PROGRESSIVE: return "progressive";
        default:
                                         return "??";
    }
}
/*
 * @brief Request a p2p_find - with optional timeout.
 *
 * @return  status of operation
 */
acfg_status_t acfg_p2p_find(acfg_wsupp_hdl_t *mctx, int timeout,
        enum acfg_find_type type)
{
    return acfg_stringize_cmd(mctx, "P2P_FIND %d type=%s", 
            timeout, f_find_type(type));
}

/*
 * @brief Request a p2p_listen - with optional timeout.
 *
 * @return  status of operation
 */
acfg_status_t acfg_p2p_listen(acfg_wsupp_hdl_t *mctx, int timeout)
{
    return acfg_stringize_cmd(mctx, "P2P_LISTEN %d", timeout);
}

/*
 * @brief acfg_p2p_ext_listen [\<period> \<interval>]
 * 
 * Configure Extended Listen Timing. \n If the parameters are zero, this
 * feature is disabled. If the parameters are included, Listen State will
 * be entered every interval msec for at least period msec. Both values
 * have acceptable range of 1-65535 (with interval obviously having to be
 * larger than or equal to duration). \n If the P2P module is not idle at the
 * time the Extended Listen Timing timeout occurs, the Listen State
 * operation will be skipped.
 * 
 * The configured values will also be advertised to other P2P Devices. The
 * received values are available in the p2p_peer command output:
 * 
 * ext_listen_period=100 ext_listen_interval=5000
 */
acfg_status_t acfg_p2p_ext_listen(acfg_wsupp_hdl_t *mctx,
        int ext_listen_period, int ext_listen_interval)
{
    return acfg_stringize_cmd(mctx, "P2P_EXT_LISTEN %d %d",
            ext_listen_period, ext_listen_interval);
}

static const char * f_pintype(enum acfg_pin_type item)
{
    switch (item) {
        case ACFG_PIN_TYPE_LABEL:   return "label";
        case ACFG_PIN_TYPE_DISPLAY: return "display";
        case ACFG_PIN_TYPE_KEYPAD:  return "keypad";
        default:
                                    return " ";
    }
}
/*
 * @brief Invite a peer to join a group (e.g., group=p2p1) or to reinvoke a
 * persistent group (e.g., persistent=4). If the peer device is the GO of
 * the persisten group, the peer parameter is not needed. Otherwise it is
 * used to specify which device to invite. go_dev_addr parameter can be
 * used to override the GO device address for Invitation Request should it
 * be not known for some reason (this should not be needed in most cases).
 * 
 * ex: p2p_invite [persistent=<network id>|group=<group ifname>] [peer=address] [go_dev_addr=address] 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_invite(acfg_wsupp_hdl_t *mctx, int persistent,
        const char *group, 
        mac_addr_t peer_mac_addr, mac_addr_t go_dev_addr)
{
    char cmd[128], peer_addr[80], go_addr[80];
    int res = 0;

    cmd[0] = '\0';
    peer_addr[0] = '\0';
    go_addr[0] = '\0';

    if (persistent >= 0) {
        res = os_snprintf(cmd, sizeof(cmd), "persistent=%d", 
                persistent);
    } else if (group) {
        res = os_snprintf(cmd, sizeof(cmd), "group=%s", group);
    }
    if (res < 0) 
        return ACFG_STATUS_ARG_TOO_BIG;

    if (peer_mac_addr) {
        res = os_snprintf(peer_addr, sizeof(peer_addr), "peer=" MACSTR, 
                MAC2STR(peer_mac_addr));
    }
    if (go_dev_addr) {
        res = os_snprintf(go_addr, sizeof(go_addr), 
                "go_dev_addr=" MACSTR, MAC2STR(go_dev_addr));
    }
    return acfg_stringize_cmd(mctx, "P2P_INVITE %s %s %s", 
            cmd, peer_addr, go_addr );

}


/*
 * @brief Request a p2p_connect - connect to an existing p2p network
 *
 * @return  status of operation
 */
acfg_status_t acfg_p2p_connect(acfg_wsupp_hdl_t *mctx, mac_addr_t mac_addr,
        char *PIN, enum acfg_pin_type pintype, int persistent, int join,
        int go_intent, int freq)
{
    char *mypin_or_pbc = "pbc";
    char go_str[32];
    char freq_str[32];

    if (PIN)
        mypin_or_pbc = PIN;

    go_str[0] = '\0';
    freq_str[0] = '\0';

    if (freq) {
        os_snprintf(freq_str, sizeof(freq_str), "freq=%d", freq);
    }

    if (go_intent >= 0) {
        if (go_intent > 15) {
            dbgprintf(MSG_ERROR, "Too much go_intent!\n");
            return ACFG_STATUS_BAD_ARG;
        }
        os_snprintf(go_str, sizeof(go_str), "go_intent=%d", go_intent);
    }
    return acfg_stringize_cmd(mctx, 
            "P2P_CONNECT " MACSTR " %s %s %s %s %s %s",
            MAC2STR(mac_addr), mypin_or_pbc, f_pintype(pintype),
            (persistent) ? "persistent" : " ",
            (join) ? "join" : " ",
            (go_intent >=0) ? go_str : " ",
            (freq) ? freq_str : " ");
}

/*
 * @brief Request a p2p_group_add - create a group with a name
 *
 * @return  status of operation
 */
acfg_status_t acfg_p2p_group_add(acfg_wsupp_hdl_t *mctx, int freq, 
        int persistent, int persist_id)
{
    char p[32];
    char f[32];
    if (freq || persistent || persist_id >=0) {
        if (persistent && persist_id >=0) {
            dbgprintf(MSG_ERROR, "Too much persistence!\n");
            return ACFG_STATUS_BAD_ARG;
        }
        p[0] = '\0';
        f[0] = '\0';
        if (freq) {
            os_snprintf(f, sizeof(f), "freq=%d", freq);
        }
        if (persist_id >= 0) {
            os_snprintf(p, sizeof(p), "persistent=%d", persist_id);
        }

        return acfg_stringize_cmd(mctx, "P2P_GROUP_ADD %s %s %s",
                (persistent) ? "persistent" : " ",
                (persist_id >=0) ? p : " ",
                (freq) ? f : " ");
    }
    return acfg_stringize_cmd(mctx, "P2P_GROUP_ADD");
}

/*
 * @brief Request a p2p_group_remove - remove a group with a iface name
 *
 * @return  status of operation
 */
acfg_status_t acfg_p2p_group_remove(acfg_wsupp_hdl_t *mctx, const char *name)
{
    return acfg_stringize_cmd(mctx, "P2P_GROUP_REMOVE %s", name);
}

static const char * f_acfg_prov_disc_type(enum acfg_prov_disc_type item)
{
    switch (item) {
        case ACFG_PROV_DISC_DISPLAY:    return "display";
        case ACFG_PROV_DISC_KEYPAD:     return "keypad";
        case ACFG_PROV_DISC_PBC:        return "pbc";
        default:
                                        return "??";
    }
}

/*
 * @brief p2p_prov_disc \<peer device address> \<display|keypad|pbc> Send
 * P2P provision discovery request to the specified peer. The parameters
 * for this command are the P2P device address of the peer and the desired
 * configuration method. 
 * For example, "p2p_prov_disc 02:01:02:03:04:05 display" 
 * would request the peer to display a PIN for us and
 * "p2p_prov_disc 02:01:02:03:04:05 keypad" 
 * would request the peer to enter
 * a PIN that we display..  
 */
acfg_status_t acfg_p2p_prov_disc(acfg_wsupp_hdl_t *mctx, mac_addr_t mac_addr,
        enum acfg_prov_disc_type type)
{
    return acfg_stringize_cmd(mctx, "P2P_PROV_DISC " MACSTR " %s", 
            MAC2STR(mac_addr), 
            f_acfg_prov_disc_type(type));

}
/*
 * @brief Schedule a P2P service discovery request. The parameters for this
 * command are the device address of the peer device (or 00:00:00:00:00:00
 * for wildcard query that is sent to every discovered P2P peer that
 * supports service discovery) and P2P Service Query TLV(s) as hexdump. For
 * example, "p2p_serv_disc_req 00:00:00:00:00:00 02000001" schedules a
 * request for listing all supported service discovery protocols and
 * requests this to be sent to all discovered peers. The pending requests
 * are sent during device discovery (see acfg_p2p_find).  This command
 * returns an identifier for the pending query (e.g., "1f77628") that can
 * be used to cancel the request. Directed requests will be automatically
 * removed when the specified peer has replied to it.
 * 
 */
acfg_status_t acfg_p2p_serv_disc_req(acfg_wsupp_hdl_t *mctx,
        mac_addr_t mac_addr,
        char *blob, int *query_handle)
{
    char buf[128];
    size_t buflen = sizeof(buf);
    int res = acfg_stringize_cmd_reply(mctx, buf, &buflen, 
            "P2P_SERV_DISC_REQ " MACSTR " %s",
            MAC2STR(mac_addr), blob);
    *query_handle = atoi(buf);
    return res;
}
/*
 * @brief acfg_p2p_serv_disc_cancel_req \<query identifier>  
 * Cancel a pending P2P service discovery request. This command takes a
 * single parameter: identifier for the pending query (the value returned
 * by p2p_serv_disc_req, e.g., 
 * "p2p_serv_disc_cancel_req 1f77628".
 * 
 */
acfg_status_t acfg_p2p_serv_disc_cancel_req(acfg_wsupp_hdl_t *mctx, 
        int query_handle)
{
    return acfg_stringize_cmd(mctx, "P2P_SERV_DISC_CANCEL_REQ %d", 
            query_handle);
}

/*
 * @brief acfg_p2p_serv_disc_resp Reply to a service discovery query. This
 * command takes following parameters: frequency in MHz, destination
 * address, dialog token, response TLV(s). The first three parameters are
 * copied from the request event. For example, "p2p_serv_disc_resp 2437
 * 02:40:61:c2:f3:b7 1 0300000101".
 * 
 */
acfg_status_t acfg_p2p_serv_disc_resp(acfg_wsupp_hdl_t *mctx, int freqMHz,
        mac_addr_t dest_addr, int dialog_token, char *blob)
{ 
    return acfg_stringize_cmd(mctx, "P2P_SERV_DISC_RESP %d " MACSTR "%d %s", 
            freqMHz, MAC2STR(dest_addr),
            dialog_token, blob);
}

/*
 * @brief acfg_p2p_service_update Indicate that local services have changed.
 * This is used to increment the P2P service indicator value so that peers
 * know when previously cached information may have changed.

 * @param [in] mctx opaque handle to the module
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_service_update(acfg_wsupp_hdl_t *mctx)
{
    return acfg_stringize_cmd(mctx, "P2P_SERVICE_UPDATE");
}

static const char * f_service_type(enum acfg_service_type item)
{
    switch (item) {
        case ACFG_SERVICE_ADD_UPNP:     return "upnp";
        case ACFG_SERVICE_ADD_BONJOUR:  return "bonjour";
        default:
                                        return " ";
    }
}
/*
 * @brief acfg_p2p_service_add \n
 * 
 * Add a local UPnP or Bonjour service for internal SD query processing.
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_service_add(acfg_wsupp_hdl_t *mctx,
        enum acfg_service_type type, char *blob)
{
    return acfg_stringize_cmd(mctx, "P2P_SERVICE_ADD %s %s", 
            f_service_type(type), blob);
}
/*
 * @brief acfg_p2p_service_del \n
 * 
 * Remove a local Bonjour/UpNp service from internal SD query processing
 * 
 */
acfg_status_t acfg_p2p_service_del(acfg_wsupp_hdl_t *mctx, 
        enum acfg_service_type type, char *blob)
{
    return acfg_stringize_cmd(mctx, "P2P_SERVICE_DEL %s %s", 
            f_service_type(type), blob);
}
/*
 * @brief acfg_p2p_service_flush \n
 * 
 * Remove all local services from internal SD query processing. 
 *
 */
acfg_status_t acfg_p2p_service_flush(acfg_wsupp_hdl_t *mctx)
{
    return acfg_stringize_cmd(mctx, "P2P_SERVICE_FLUSH");
}

/*
 * @brief acfg_p2p_get_passphrase  Get the passphrase for a group (only
 * available when acting as a GO).  
 */
acfg_status_t acfg_p2p_get_passphrase(acfg_wsupp_hdl_t *mctx, char *reply, 
        size_t *reply_len)
{
    return acfg_stringize_cmd_reply(mctx, reply, reply_len, 
            "P2P_GET_PASSPHRASE");
}
/*
 * @brief p2p_presence_req [\<duration> \<interval>] [\<duration> \<interval>] \n
 * 
 * Send a P2P Presence Request to the GO (this is only available when
 * acting as a P2P client).  \n
 * 
 * If no duration/interval pairs are given, (set values to zero), the
 * request indicates that this client has no special needs for GO presence. \n
 * 
 * The first parameter pair gives the preferred duration and interval
 * values in microseconds. \n 
 * 
 * If the second pair is included, that indicates
 * which value would be acceptable. \n 
 * 0 for a duration/interval pair means it is not requested.
 * 
 */
acfg_status_t acfg_p2p_presence_req(acfg_wsupp_hdl_t *mctx, 
        uint32_t preferred_duration, uint32_t preferred_interval,
        uint32_t acceptable_duration, uint32_t acceptable_interval)
{
    return acfg_stringize_cmd(mctx, "P2P_PRESENCE_REQ %d %d %d %d",
            preferred_duration, preferred_interval,
            acceptable_duration, acceptable_interval);
}

/*
 * @brief Add a new network to an interface - it must later be configured
 * and enabled before use.
 *
 * @return status of operation
 * @return also the new network_id
 */
acfg_status_t acfg_wsupp_nw_create(acfg_wsupp_hdl_t *mctx,
        char *ifname,
        int *network_id)
{
    acfg_status_t retv;
    char buf[16];
    size_t buflen = sizeof(buf);
    retv = acfg_stringize_cmd_reply(mctx, buf, &buflen, "ADD_NETWORK");
    if (retv == ACFG_STATUS_OK) {
        *network_id = atoi(buf);
    }
    return retv;
}
/*
 * @brief disconnects, and removes a network
 * 
 * @return status of operation
 */
acfg_status_t acfg_wsupp_nw_delete(acfg_wsupp_hdl_t *mctx, 
        char *ifname,
        int network_id)
{
    return acfg_stringize_cmd(mctx, "REMOVE_NETWORK %d", network_id);

}

static const char * f_nw_name(enum acfg_network_item item)
{
    switch (item) {
        case ACFG_NW_SSID: return "ssid";
        case ACFG_NW_PSK: return "psk";
        case ACFG_NW_KEY_MGMT: return "key_mgmt";
        case ACFG_NW_PAIRWISE: return "pairwise";
        case ACFG_NW_GROUP: return "group";
        case ACFG_NW_PROTO: return "proto";
        default:
                            return "??";
    }
}
/**
 * @brief sets an item number to the passed string.
 * once a network is enabled, some attributes may no longer be set.
 * 
 * @return status of operation
 */
acfg_status_t acfg_wsupp_nw_set(acfg_wsupp_hdl_t *mctx,
        int network_id,
        enum acfg_network_item item,
        char *in_string)
{
    if (item == ACFG_NW_ENABLE) {
        return acfg_stringize_cmd(mctx, "ENABLE_NETWORK %d", network_id);
    } else {
        return acfg_stringize_cmd(mctx, "SET_NETWORK %d %s %s", 
                network_id, f_nw_name(item), in_string);
    }
}

/*
 * @brief Stop ongoing P2P device discovery or other operation (connect,
 * listen mode). Like a soft reset.
 * 
 * @return status of operation
 */
acfg_status_t acfg_p2p_stop_find(acfg_wsupp_hdl_t *mctx)
{
    return acfg_stringize_cmd(mctx, "P2P_STOP_FIND");
}

/*
 * @brief brief Flush P2P peer table and state. Like a hard reset.
 * 
 * @return status of operation
 */
acfg_status_t acfg_p2p_flush(acfg_wsupp_hdl_t *mctx)
{
    return acfg_stringize_cmd(mctx, "P2P_FLUSH");
}

/**
 * @brief Reject connection attempt from a peer (specified with a device
 * address). This is a mechanism to reject a pending GO Negotiation with a
 * peer and request to automatically block any further connection or
 * discovery of the peer.
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_reject(acfg_wsupp_hdl_t *mctx, mac_addr_t peer_mac_addr)
{
    return acfg_stringize_cmd(mctx, "P2P_REJECT " MACSTR, 
            MAC2STR(peer_mac_addr));
}
/**
 * @brief Fetch information about a known P2P peer.
 */
acfg_status_t acfg_p2p_peer(acfg_wsupp_hdl_t *mctx, mac_addr_t peer_mac_addr, 
        char *reply, size_t *reply_len)
{
    return acfg_stringize_cmd_reply(mctx, reply, reply_len, 
            "P2P_PEER " MACSTR, MAC2STR(peer_mac_addr));
}
/*
 * @brief List P2P Device Addresses of all the P2P peers we know. The
 * optional "discovered" parameter filters out the peers that we have not
 * fully discovered, i.e., which we have only seen in a received Probe
 * Request frame.  
 * 
 * @return status of operation
 */
acfg_status_t acfg_p2p_peers(acfg_wsupp_hdl_t *mctx, int discovered, int first,
        mac_addr_t peer_mac_addr)
{
    acfg_status_t res;
    mac_addr_int_t station_address;
    char reply[128];
    size_t reply_len = sizeof(reply);
    int offs;

    if (first) {
        res = acfg_stringize_cmd_reply(mctx, reply, &reply_len, 
                "P2P_PEER FIRST");
    } else {
        res = acfg_stringize_cmd_reply(mctx, reply, &reply_len, 
                "P2P_PEER NEXT-" MACSTR, MAC2STR(peer_mac_addr));
    }

    offs = sscanf(reply, MACSTR, MAC2STR_ADDR(station_address));

    os_memset(peer_mac_addr, 0, sizeof(peer_mac_addr));

    if (!discovered || 
            os_strstr(reply + offs + 1, "[PROBE_REQ_ONLY]") == NULL) {
        macint_to_macbyte(station_address, peer_mac_addr);
    } 
    dbgprintf(MSG_INFO, "first=%d disc=%d " MACSTR, first, discovered, 
            MAC2STR(peer_mac_addr));
    return res;
}
/*
 * @brief Lists the configured networks, including stored information for
 * persistent groups. The identifier in this list is used with
 * p2p_group_add and p2p_invite to indicate which persistent group is to be
 * reinvoked.
 * 
 * @return  status of operation
 */
acfg_status_t acfg_list_networks(acfg_wsupp_hdl_t *mctx, 
        char *reply, size_t *reply_len)
{
    return acfg_stringize_cmd_reply(mctx, reply, reply_len, "LIST_NETWORKS");

}

/*
 * @brief Request a wifi scan
 */
acfg_status_t acfg_scan(acfg_wsupp_hdl_t *mctx)
{
    return acfg_stringize_cmd(mctx, "SCAN");
}
static int decode_int(char *pos, char *str, char *fmt)
{
    int temp = 0;
    const char *npos = os_strstr(pos, str) + os_strlen(str);
    sscanf(npos, fmt, &temp);
    return temp;
}
/*
 * @brief Request One complete scan entry, by entry number 1..n
 */
acfg_status_t acfg_bss(acfg_wsupp_hdl_t *mctx, int entry_id, mac_addr_t bssid,
        int *freq, int *beacon_int, int *capabilities, int *qual, int *noise, 
        int *level, char *misc, size_t *misc_len)
{
    acfg_status_t res;
    const char *npos;
    mac_addr_int_t station_address;

    res = acfg_stringize_cmd_reply(mctx, misc, misc_len,"BSS %d", entry_id);

    if (res == ACFG_STATUS_OK && misc && *misc_len) {
        /* decode some of the string */
        npos = os_strstr(misc, "bssid=") + sizeof("bssid=") - 1;
        sscanf(npos, MACSTR, MAC2STR_ADDR(station_address));
        macint_to_macbyte(station_address, bssid);
        *freq = decode_int(misc, "freq=", "%d");
        *beacon_int = decode_int(misc, "beacon_int=", "%d");
        *capabilities = decode_int(misc, "capabilities=", "%x");
        *qual = decode_int(misc, "qual=", "%d");
        *noise = decode_int(misc, "noise=", "%d");
        *level = decode_int(misc, "level=", "%d");
    }
    if (res == ACFG_STATUS_OK && *misc_len == 0)
        res = ACFG_STATUS_ARG_TOO_BIG;
    return res;
}
static const char * f_set_p2p_name(enum acfg_p2pset_type item)
{
    switch (item) {
        case ACFG_P2PSET_DISC:             return "discoverability";
        case ACFG_P2PSET_MANAGED:          return "managed";
        case ACFG_P2PSET_LISTEN:           return "listen_channel";
        case ACFG_P2PSET_SSID_POST:        return "ssid_postfix";
        default:
                                           return "??";
    }
}
/*
 * @brief set some p2p parameter at run time.
 */
acfg_status_t acfg_p2p_set(acfg_wsupp_hdl_t *mctx, enum acfg_p2pset_type type,
        int val, char *str)
{
    if (type == ACFG_P2PSET_SSID_POST) {
        return acfg_stringize_cmd(mctx, "P2P_SET %s %s", 
                f_set_p2p_name(type), str);
    }
    return acfg_stringize_cmd(mctx, "P2P_SET %s %d", f_set_p2p_name(type), val);
}
static const char * f_set_name(enum acfg_set_type item)
{
    switch (item) {
        case ACFG_SET_UUID:                     return "uuid";
        case ACFG_SET_DEVICE_NAME:              return "device_name";
        case ACFG_SET_MANUFACTURER:             return "manufacturer";
        case ACFG_SET_MODEL_NAME:               return "model_name";
        case ACFG_SET_MODEL_NUMBER:             return "model_number";
        case ACFG_SET_SERIAL_NUMBER:            return "serial_number";
        case ACFG_SET_DEVICE_TYPE:              return "device_type";
        case ACFG_SET_OS_VERSION:               return "os_version";
        case ACFG_SET_CONFIG_METHODS:           return "config_methods";
        case ACFG_SET_SEC_DEVICE_TYPE:          return "sec_device_type";
        case ACFG_SET_P2P_GO_INTENT:            return "p2p_go_intent";
        case ACFG_SET_P2P_SSID_POSTFIX:         return "p2p_ssid_postfix";
        case ACFG_SET_PERSISTENT_RECONNECT:     return "persistent_reconnect";
        case ACFG_SET_COUNTRY:                  return "country";
        default:
                                                return "??";
    }
}

/*
 * @brief set some supplicant parameter at run time.
 */
acfg_status_t acfg_set(acfg_wsupp_hdl_t *mctx, enum acfg_set_type type,
        int val, char *str)
{
    if (type != ACFG_SET_PERSISTENT_RECONNECT &&
            type != ACFG_SET_P2P_GO_INTENT) {
        return acfg_stringize_cmd(mctx, "SET %s %s", 
                f_set_name(type), str);
    }
    return acfg_stringize_cmd(mctx, "SET %s %d", f_set_name(type), val);
}

/*
 * @brief open_event_socket - open an async event socket
 *
 * @return  status of operation
 */
acfg_status_t acfg_open_event_socket(acfg_wsupp_hdl_t *mctx, char *name)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    char *realname = name;

    if (!realname)
        realname = DEFAULT_IFNAME;


    dbgprintf(MSG_INFO, "1 %p %s", aptr, realname);

    if (aptr->events)
        return ACFG_STATUS_IN_USE;

    aptr->events = wpa_ctrl_open(realname);

    if (!aptr->events)
        return ACFG_STATUS_FAILED;

    if (wpa_ctrl_attach(aptr->events) < 0) {
        dbgprintf(MSG_ERROR, "Failed to attach %p %s", aptr, realname);
        acfg_close_event_socket(mctx);
        return ACFG_STATUS_FAILED;
    }

    return ACFG_STATUS_OK;
}
/*
 * @brief Add a WLAN n/w interface for configuration
 * This only works if there is a global, control interface working.
 * 
 * @return status of operation
 */
acfg_status_t acfg_wsupp_if_add(acfg_wsupp_hdl_t *mctx, 
        void *if_uctx, 
        char *ifname_plus)
{
    char cmd[128];
    size_t res;

    dbgprintf(MSG_DEBUG, "%s", ifname_plus);
    res = snprintf(cmd, sizeof(cmd), "INTERFACE_ADD %s", ifname_plus);

    if (res < 0 || res >= sizeof(cmd))
        return ACFG_STATUS_BAD_ARG;

    cmd[sizeof(cmd) - 1] = '\0';
    return acfg_global_command(mctx, cmd);
}
/*
 * @brief remove a WLAN n/w interface for configuration
 * This only works if there is a global, control interface working.
 * 
 * @return status of operation
 */
acfg_status_t acfg_wsupp_if_remove(acfg_wsupp_hdl_t *mctx, 
        char *ifname)
{
    char cmd[128];
    size_t res;
    dbgprintf(MSG_DEBUG, "%s", ifname);

    res = snprintf(cmd, sizeof(cmd), "INTERFACE_REMOVE %s", ifname);

    dbgprintf(MSG_DEBUG, "%s", ifname);
    if (res < 0 || res >= sizeof(cmd))
        return ACFG_STATUS_BAD_ARG;

    cmd[sizeof(cmd) - 1] = '\0';
    dbgprintf(MSG_DEBUG, "%s", ifname);
    return acfg_global_command(mctx, cmd);
}
/*
 * @brief close_event_socket - close an async event socket
 *
 */
void acfg_close_event_socket(acfg_wsupp_hdl_t *mctx)
{
    struct acfg_wsupp *aptr = (void *) mctx;

    if (aptr->events)
        wpa_ctrl_close(aptr->events);

    aptr->events = NULL;
}
/*
 * @brief For socket implementations, get the socket fd which
 *        can be used in the caller's poll/select loop. Once the
 *        fd shows data is available, the user calls
 *        acfg_wsupp_event_read_socket to read the event.
 *
 * @return fd handle or 0 if acfg_open_event_socket failed.
 */
int acfg_wsupp_event_get_socket_fd(acfg_wsupp_hdl_t *mctx)
{
    struct acfg_wsupp *aptr = (void *) mctx;

    if (aptr->events)
        return wpa_ctrl_get_fd(aptr->events);
    return 0;

}
static const char * f_wps_name(enum acfg_wps_type item)
{
    switch (item) {
        case ACFG_WPS_PIN:      return "WPS_PIN";
        case ACFG_WPS_PBC:      return "WPS_PBC";
        case ACFG_WPS_REG:      return "WPS_REG";
        case ACFG_WPS_ER_START: return "WPS_ER_START";
        case ACFG_WPS_ER_STOP:  return "WPS_ER_STOP";
        case ACFG_WPS_ER_PIN:   return "WPS_ER_PIN";
        case ACFG_WPS_ER_PBC:   return "WPS_ER_PBC";
        case ACFG_WPS_ER_LEARN: return "WPS_ER_LEARN";
        default:
                                return "??";
    }
}
/*
 * @brief Request a wps call to handle various security requests
 * 
 * @return  status of operation
 */
acfg_status_t acfg_wps_req(acfg_wsupp_hdl_t *mctx, enum acfg_wps_type wps,
        const char *params,
        char *reply, size_t *reply_len)
{
    return acfg_stringize_cmd_opt_arg(mctx, f_wps_name(wps), params,
            reply, reply_len);
}

static int str_match(const char *a, const char *b)
{
    return os_strncmp(a, b, os_strlen(b)) == 0;
}

static acfg_eventdata_t *handle_WPA_EVENT_CONNECTED(acfg_wsupp_hdl_t *mctx,
        const char *pos,
        enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_WPA_EVENT_CONNECTED *mptr = &aptr->res.u.m_WPA_EVENT_CONNECTED;
    mac_addr_int_t station_address;

    sscanf(pos, WPA_EVENT_CONNECTED MACSTR " %s",
            MAC2STR_ADDR(station_address), mptr->misc_params);

    macint_to_macbyte(station_address, mptr->station_address);

    dbgprintf(MSG_DEBUG, MACSTR " %s",
            MAC2STR(mptr->station_address), mptr->misc_params);
    return &aptr->res;
}

static acfg_eventdata_t *handle_WPS_EVENT_ENROLLEE_SEEN(acfg_wsupp_hdl_t *mctx,
        const char *pos,
        enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_WPS_EVENT_ENROLLEE_SEEN *mptr = 
        &aptr->res.u.m_WPS_EVENT_ENROLLEE_SEEN;
    mac_addr_int_t station_address;

    sscanf(pos, WPS_EVENT_ENROLLEE_SEEN MACSTR " %s",
            MAC2STR_ADDR(station_address), mptr->misc_params);

    macint_to_macbyte(station_address, mptr->station_address);

    dbgprintf(MSG_DEBUG, MACSTR " %s",
            MAC2STR(mptr->station_address), mptr->misc_params);
    return &aptr->res;
}

static acfg_eventdata_t *found_and_pbc_req(struct acfg_wsupp *aptr,
        struct S_P2P_EVENT_DEVICE_FOUND *mptr,
        const char *npos)
{
    mac_addr_int_t devmac;
    mac_addr_int_t p2pmac;
    size_t len;
    const char *tpos;
    const char *epos;

    sscanf(npos, MACSTR " p2p_dev_addr=" MACSTR " pri_dev_type=%u-%08X-%u",
            MAC2STR_ADDR(devmac), MAC2STR_ADDR(p2pmac),
            &mptr->pri_dev_type1,
            &mptr->pri_dev_type2,
            &mptr->pri_dev_type3);

    /* names can contain spaces, extract 'quoted name' */
    epos = tpos = os_strstr(npos, "name='");
    if (tpos) {
        tpos += sizeof("name='") - 1;
        epos = os_strstr(npos, "config_methods=");
        while (epos[0] !=  '\'' && --epos > tpos);
    }

    len = MIN(epos - tpos, sizeof(mptr->name));
    os_strncpy(mptr->name, tpos, len);
    mptr->name[len] = '\0';

    tpos = os_strstr(npos, "config_methods=");
    sscanf(tpos, "config_methods=%x dev_capab=%x group_capab=%x",
            &mptr->config_methods,
            &mptr->dev_capab,
            &mptr->group_capab);

    macint_to_macbyte(devmac, mptr->dev_addr);
    macint_to_macbyte(p2pmac, mptr->p2p_dev_addr);

    dbgprintf(MSG_DEBUG, MACSTR " p2p_dev_addr=" MACSTR \
            " pri_dev_type=%u-%08X-%u" \
            " name=%s config_methods=0x%x dev_capab=0x%x group_capab=0x%x",
            MAC2STR(mptr->dev_addr),
            MAC2STR(mptr->p2p_dev_addr),
            mptr->pri_dev_type1,
            mptr->pri_dev_type2,
            mptr->pri_dev_type3,
            mptr->name,
            mptr->config_methods,
            mptr->dev_capab,
            mptr->group_capab);

    return &aptr->res;
}
/*
 * decode: P2P-DEVICE-FOUND 00:03:7f:10:a5:e6 p2p_dev_addr=00:03:7f:10:a5:e6 pri_dev_type=1-0050F204-1 name='PB44_MB' config_methods=0x188 dev_capab=0x21 group_capab=0x0
 * P2P-DEVICE-FOUND 02:b5:64:63:30:63
 * p2p_dev_addr=02:b5:64:63:30:63 pri_dev_type=1-0050f204-1
 * name='Wireless Client' config_methods=0x84 dev_capab=0x21
 * group_capab=0x0
 */
static acfg_eventdata_t *handle_P2P_EVENT_DEVICE_FOUND(acfg_wsupp_hdl_t *mctx,
        const char *pos,
        enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_P2P_EVENT_DEVICE_FOUND *mptr =
        &aptr->res.u.m_P2P_EVENT_DEVICE_FOUND;
    const char *npos = pos + sizeof(P2P_EVENT_DEVICE_FOUND) - 1;

    dbgprintf(MSG_MSGDUMP, "%p %p\n%s", pos, npos, npos);
    return found_and_pbc_req(aptr, mptr, npos);

}

/* P2P-PROV-DISC-SHOW-PIN 02:40:61:c2:f3:b7 12345670 */

static acfg_eventdata_t *handle_P2P_EVENT_PROV_DISC_SHOW_PIN(acfg_wsupp_hdl_t *mctx,
 const char *pos,
 enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_P2P_EVENT_PROV_DISC_SHOW_PIN *mptr =
        &aptr->res.u.m_P2P_EVENT_PROV_DISC_SHOW_PIN;
    mac_addr_int_t station_address;

    sscanf(pos, P2P_EVENT_PROV_DISC_SHOW_PIN MACSTR " %s",
            MAC2STR_ADDR(station_address), mptr->pin);

    macint_to_macbyte(station_address, mptr->station_address);

    dbgprintf(MSG_DEBUG, MACSTR " %s",
            MAC2STR(mptr->station_address), mptr->pin);
    return &aptr->res;
}
/* P2P-PROV-DISC-ENTER-PIN 02:40:61:c2:f3:b7 */
static acfg_eventdata_t *handle_P2P_EVENT_PROV_DISC_ENTER_PIN(acfg_wsupp_hdl_t *mctx,
 const char *pos,
 enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_P2P_EVENT_PROV_DISC_ENTER_PIN *mptr =
        &aptr->res.u.m_P2P_EVENT_PROV_DISC_ENTER_PIN;
    mac_addr_int_t station_address;

    sscanf(pos, P2P_EVENT_PROV_DISC_ENTER_PIN MACSTR,
            MAC2STR_ADDR(station_address) );

    macint_to_macbyte(station_address, mptr->station_address);

    dbgprintf(MSG_DEBUG, MACSTR , MAC2STR(mptr->station_address));
    return &aptr->res;
}
/* P2P_EVENT_SERV_DISC_REQ
 *      wpa_msg_ctrl(wpa_s, MSG_INFO, P2P_EVENT_SERV_DISC_REQ "%d "
 MACSTR " %u %u %s",
 freq, MAC2STR(sa), dialog_token, update_indic,
 buf);
 */
static acfg_eventdata_t *handle_P2P_EVENT_SERV_DISC_REQ(acfg_wsupp_hdl_t *mctx,
 const char *pos,
 enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_P2P_EVENT_SERV_DISC_REQ *mptr =
        &aptr->res.u.m_P2P_EVENT_SERV_DISC_REQ;
    mac_addr_int_t station_address;

    sscanf(pos, P2P_EVENT_SERV_DISC_REQ " %d " MACSTR " %u %u %s",
            &mptr->freq,
            MAC2STR_ADDR(station_address),
            &mptr->dialog_token, &mptr->update_indicator,
            mptr->upd_string);

    macint_to_macbyte(station_address, mptr->station_address);

    dbgprintf(MSG_DEBUG, "%d " MACSTR " %u %u %s" ,
            mptr->freq, MAC2STR(mptr->station_address), mptr->dialog_token,
            mptr->update_indicator, mptr->upd_string);
    return &aptr->res;
}
/*P2P_EVENT_SERV_DISC_RESP
 *		wpa_msg_ctrl(wpa_s, MSG_INFO, P2P_EVENT_SERV_DISC_RESP MACSTR
 " %u %s",
 MAC2STR(sa), update_indic, buf);

 */
static acfg_eventdata_t *handle_P2P_EVENT_SERV_DISC_RESP(acfg_wsupp_hdl_t *mctx,
 const char *pos,
 enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_P2P_EVENT_SERV_DISC_RESP *mptr =
        &aptr->res.u.m_P2P_EVENT_SERV_DISC_RESP;
    mac_addr_int_t station_address;

    sscanf(pos, P2P_EVENT_SERV_DISC_RESP MACSTR " %u %s",
            MAC2STR_ADDR(station_address), &mptr->update_indicator,
            mptr->upd_string);

    macint_to_macbyte(station_address, mptr->station_address);

    dbgprintf(MSG_DEBUG, MACSTR " %u %s",
            MAC2STR(mptr->station_address), mptr->update_indicator,
            mptr->upd_string);
    return &aptr->res;
}

/*  looks like three formats:
    P2P-INVITATION-RECEIVED sa=02:03:7f:10:a5:ee persistent=1
    P2P-INVITATION-RECEIVED sa=02:03:7f:10:a5:ee go_dev_addr=12:03:7f:10:a5:ee unknown-network
    P2P-INVITATION-RECEIVED sa=02:03:7f:10:a5:ee go_dev_addr=12:03:7f:10:a5:ee bssid=12:03:7f:10:a5:ee unknown-network
 */
static acfg_eventdata_t *handle_P2P_EVENT_INVITATION_RECEIVED(acfg_wsupp_hdl_t *mctx,
 const char *pos,
 enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_P2P_EVENT_INVITATION_RECEIVED *mptr =
        &aptr->res.u.m_P2P_EVENT_INVITATION_RECEIVED;
    mac_addr_int_t station_address;
    const char *npos = os_strstr(pos, "sa=") + sizeof("sa=") - 1;

    sscanf(npos, MACSTR, MAC2STR_ADDR(station_address));
    macint_to_macbyte(station_address, mptr->station_address);

    npos = os_strstr(pos, "persistent=");

    if (npos) {
        sscanf(npos, "persistent=%d", &mptr->persistent_id);
        mptr->persistent = TRUE;
        dbgprintf(MSG_DEBUG, MACSTR " %u",
                MAC2STR(mptr->station_address), mptr->persistent_id);
    } else {
        npos = os_strstr(pos, "go_dev_addr=");
        sscanf(npos, "go_dev_addr=" MACSTR, MAC2STR_ADDR(station_address));
        macint_to_macbyte(station_address, mptr->go_dev_addr);
        mptr->persistent = FALSE;
        npos = os_strstr(pos, "bssid=");
        if (npos) {
            sscanf(npos, "bssid=" MACSTR, MAC2STR_ADDR(station_address));
        } else {
            memset(station_address, 0, sizeof(station_address));
        }
        macint_to_macbyte(station_address, mptr->bssid);
        dbgprintf(MSG_DEBUG, MACSTR " go_dev_addr=" MACSTR " bssid=" 
                MACSTR " %u",
                MAC2STR(mptr->station_address), 
                MAC2STR(mptr->go_dev_addr),
                MAC2STR(mptr->bssid),
                mptr->persistent_id);
    }
    return &aptr->res;
}
/* 	if (bssid) {
        wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_INVITATION_RESULT
        "status=%d " MACSTR,
        status, MAC2STR(bssid));
        } else {
        wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_INVITATION_RESULT
        "status=%d ", status);
        }

 */
static acfg_eventdata_t *handle_P2P_EVENT_INVITATION_RESULT(
        acfg_wsupp_hdl_t *mctx,
        const char *pos,
        enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_P2P_EVENT_INVITATION_RESULT *mptr =
        &aptr->res.u.m_P2P_EVENT_INVITATION_RESULT;
    mac_addr_int_t bssid;
    const char *npos = pos + sizeof(P2P_EVENT_INVITATION_RESULT) - 1;

    os_memset(bssid, 0, sizeof(bssid));
    sscanf(npos, "status=%d " MACSTR, &mptr->status, MAC2STR_ADDR(bssid));
    macint_to_macbyte(bssid, mptr->bssid);

    dbgprintf(MSG_DEBUG, "status=%d " MACSTR, mptr->status,
            MAC2STR(mptr->bssid));
    return &aptr->res;
}

/* P2P-GROUP-STARTED wlan1 client ssid="DIRECT-qM" psk=24b4f go_dev_addr=00:00:00:00:00:00
 * or:
 * P2P-GROUP-STARTED wlan1 GO ssid="DIRECT-4W" passphrase="I3oIObG7" go_dev_addr=00:03:7f:10:a5:ee
 */
static acfg_eventdata_t *handle_P2P_EVENT_GROUP_STARTED(
        acfg_wsupp_hdl_t *mctx,
        const char *pos,
        enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_P2P_EVENT_GROUP_STARTED *mptr =
        &aptr->res.u.m_P2P_EVENT_GROUP_STARTED;
    mac_addr_int_t station_address;
    char role_name[128];
    const char *npos;

    sscanf(pos, P2P_EVENT_GROUP_STARTED "%s %s ssid=%s freq=%d",
            mptr->iface, role_name, mptr->ssid, &mptr->freq);

    npos = os_strstr(pos, "GO");

    if (npos) {
        npos = os_strstr(pos, "passphrase=");
        mptr->my_role = ACFG_GO;
        if (npos) {
            sscanf(npos, "passphrase=%s go_dev_addr=" MACSTR,
                    mptr->psk_or_passphrase, MAC2STR_ADDR(station_address));
        }
    } else {
        npos = os_strstr(pos, "psk=");
        mptr->my_role = ACFG_CLIENT;
        if (npos) {
            sscanf(npos, "psk=%s go_dev_addr=" MACSTR,
                    mptr->psk_or_passphrase, MAC2STR_ADDR(station_address));
        }
    }
    macint_to_macbyte(station_address, mptr->go_dev_addr);

    dbgprintf(MSG_DEBUG, "%s %s=%s ssid=%s freq=%d psk_or_passphrase=%s go_dev_addr="
            MACSTR, mptr->iface, role_name,
            (mptr->my_role == ACFG_GO) ? "go" : "client",
            mptr->ssid, mptr->freq, mptr->psk_or_passphrase, MAC2STR(mptr->go_dev_addr));

    return &aptr->res;
}
/*
 *  P2P-GO-NEG-REQUEST 02:03:7f:10:a5:ee
 */
static acfg_eventdata_t *handle_P2P_EVENT_GO_NEG_REQUEST(
        acfg_wsupp_hdl_t *mctx,
        const char *pos,
        enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_P2P_EVENT_GO_NEG_REQUEST *mptr =
        &aptr->res.u.m_P2P_EVENT_GO_NEG_REQUEST;
    mac_addr_int_t bssid;

    sscanf(pos, P2P_EVENT_GO_NEG_REQUEST MACSTR, MAC2STR_ADDR(bssid));
    macint_to_macbyte(bssid, mptr->station_address);

    dbgprintf(MSG_DEBUG, MACSTR, MAC2STR(mptr->station_address));
    return &aptr->res;
}
static acfg_eventdata_t *handle_AP_STA_CONNECTED(
        acfg_wsupp_hdl_t *mctx,
        const char *pos,
        enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_AP_STA_CONNECTED *mptr =
        &aptr->res.u.m_AP_STA_CONNECTED;
    mac_addr_int_t bssid;

    sscanf(pos, AP_STA_CONNECTED MACSTR, MAC2STR_ADDR(bssid));
    macint_to_macbyte(bssid, mptr->station_address);

    dbgprintf(MSG_DEBUG, MACSTR, MAC2STR(mptr->station_address));
    return &aptr->res;
}
/*
 * P2P-PROV-DISC-PBC-REQ 02:03:7f:10:a5:e6 p2p_dev_addr=02:03:7f:10:a5:e6 pri_dev_type=1-0050F204-1 name='Calfee-p2p-linux' config_methods=0x188 dev_capab=0x23 group_capab=0x0
 */
static acfg_eventdata_t *handle_P2P_EVENT_PROV_DISC_PBC_REQ(
        acfg_wsupp_hdl_t *mctx,
        const char *pos,
        enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_P2P_EVENT_DEVICE_FOUND *mptr =
        &aptr->res.u.m_P2P_EVENT_PROV_DISC_PBC_REQ.data_struct;
    const char *npos = pos + sizeof(P2P_EVENT_PROV_DISC_PBC_REQ) - 1;

    dbgprintf(MSG_MSGDUMP, "%p %p\n%s", pos, npos, npos);
    return found_and_pbc_req(aptr, mptr, npos);

}

/*
 * P2P-PROV-DISC-PBC-RESP 02:03:7f:10:a5:ee
 */
static acfg_eventdata_t *handle_P2P_EVENT_PROV_DISC_PBC_RESP(
        acfg_wsupp_hdl_t *mctx,
        const char *pos,
        enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    struct S_P2P_EVENT_PROV_DISC_PBC_RESP *mptr =
        &aptr->res.u.m_P2P_EVENT_PROV_DISC_PBC_RESP;
    mac_addr_int_t bssid;

    sscanf(pos, P2P_EVENT_PROV_DISC_PBC_RESP MACSTR, MAC2STR_ADDR(bssid));
    macint_to_macbyte(bssid, mptr->station_address);

    dbgprintf(MSG_DEBUG, MACSTR, MAC2STR(mptr->station_address));
    return &aptr->res;
}

/*
 * default response - No string provided in response.
 */
static acfg_eventdata_t *handle_default(acfg_wsupp_hdl_t *mctx,
        const char *pos,
        enum acfg_eventnumber event)
{
    struct acfg_wsupp *aptr = (void *) mctx;

    dbgprintf(MSG_DEBUG, "event=%d ", event);

    return &aptr->res;
}

#define ifhandle(a, b) if (str_match((a), (b))) { \
    do { aptr->res.event = ACFG_ ## b; \
        return handle_ ## b(mctx, a, ACFG_ ## b ); } while (0)
#define ifhandle_default(a, b) if (str_match((a), (b))) { \
    do { aptr->res.event = ACFG_ ## b; \
        return handle_default(mctx, a, ACFG_ ## b ); } while (0)
/*
 * convert return info from a string to a binary struct
 */
static acfg_eventdata_t *acfg_process_event(acfg_wsupp_hdl_t *mctx, char *msg)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    const char *pos;

    dbgprintf(MSG_MSGDUMP, "Entry %p %s", mctx, msg);

    pos = msg;
    if (*pos == '<') {
        /* skip priority */
        pos = os_strchr(pos, '>');
        if (pos)
            pos++;
        else
            pos = msg;
    }
    /*
     *  match the string, save the event number, vector to a handler routine
     */
    ifhandle(pos, WPA_EVENT_CONNECTED);
    } else ifhandle_default(pos, WPA_EVENT_DISCONNECTED);
    } else ifhandle_default(pos, WPA_EVENT_TERMINATING);
    } else ifhandle(pos, P2P_EVENT_DEVICE_FOUND);
    } else ifhandle(pos, P2P_EVENT_GO_NEG_REQUEST);
    } else ifhandle_default(pos, P2P_EVENT_GO_NEG_SUCCESS);
    } else ifhandle_default(pos, P2P_EVENT_GO_NEG_FAILURE);
    } else ifhandle_default(pos, P2P_EVENT_GROUP_FORMATION_SUCCESS);
    } else ifhandle_default(pos, P2P_EVENT_GROUP_FORMATION_FAILURE);
    } else ifhandle_default(pos, P2P_EVENT_GROUP_REMOVED);
    } else ifhandle(pos, P2P_EVENT_PROV_DISC_SHOW_PIN);
    } else ifhandle(pos, P2P_EVENT_PROV_DISC_ENTER_PIN);
    } else ifhandle(pos, P2P_EVENT_SERV_DISC_REQ);
    } else ifhandle(pos, P2P_EVENT_SERV_DISC_RESP);
    } else ifhandle(pos, P2P_EVENT_PROV_DISC_PBC_REQ);
    } else ifhandle(pos, P2P_EVENT_PROV_DISC_PBC_RESP);
    } else ifhandle(pos, P2P_EVENT_INVITATION_RECEIVED);
    } else ifhandle(pos, P2P_EVENT_INVITATION_RESULT);
    } else ifhandle(pos, P2P_EVENT_GROUP_STARTED);
    } else ifhandle(pos, AP_STA_CONNECTED);
    } else ifhandle(pos, WPS_EVENT_ENROLLEE_SEEN);
    } else ifhandle_default(pos, WPA_EVENT_SCAN_RESULTS);
    }
/*
 * I don't know what the event is, but it is not an error
 */
    return &aptr->res;
}
/*
 * @brief Reads pending events from a socket. The caller should
 *        have made sure the socket has data available to read.
 *        This function calls one of the functions in the event
 *        table.
 * @return  structure ptr with results of operation, 
 *           NULL if nothing to read or on failure
 */
acfg_eventdata_t *acfg_wsupp_event_read_socket(acfg_wsupp_hdl_t *mctx,
        int socket_fd)
{
    struct acfg_wsupp *aptr = (void *) mctx;
    char buf[256];
    size_t len = sizeof(buf) - 1;
    size_t res;

    if (!aptr->events)
        goto errorexit;

    if (wpa_ctrl_pending(aptr->events)) {
        if (wpa_ctrl_recv(aptr->events, buf, &len) == 0) {
            if (res < 0) {
                goto errorexit;
            }
            buf[len] = '\0';
            return acfg_process_event(mctx, buf);
        }
    } else {
        return NULL;
    }
errorexit:
    dbgprintf(MSG_ERROR, "Could not read pending message.\n");
    return NULL;
}

