#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <string.h>
#include <sys/errno.h>
#include <stdio.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/stat.h>
#include <acfg_api_pvt.h>
#include <linux/un.h>
#include <acfg_security.h>
#include <acfg_wsupp_types.h>
#include <acfg_misc.h>

#define MAX_SIZE    300
#define SUPP_FIELD  3

/* Extern declarations */
extern int wsupp_status_init;
extern int acfg_ctrl_iface_present (a_uint8_t *ifname, acfg_opmode_t opmode);
extern a_status_t acfg_get_br_name(a_uint8_t *ifname, char *brname);

void acfg_parse_hapd_param(a_char_t *buf, a_int32_t len, 
        acfg_hapd_param_t *hapd_param)
{
    a_int32_t offset = 0;

    while (offset < len) {
        if (!strncmp(buf + offset, "wps_state=", strlen("wps_state="))) {
            offset += strlen("wps_state=");
            if (!strncmp(buf + offset, "disabled", strlen("disabled"))) {
                hapd_param->wps_state = WPS_FLAG_DISABLED;
                offset += strlen("disabled");
            } else if (!strncmp(buf + offset, "not configured", 
                        strlen("not configured")))
            {
                hapd_param->wps_state = WPS_FLAG_UNCONFIGURED;
                offset += strlen("not configured");
            } else if (!strncmp(buf + offset, "configured", 
                        strlen("configured")))
            {
                hapd_param->wps_state = WPS_FLAG_CONFIGURED;
                offset += strlen("configured");
            }
        } else {
            while ((*(buf + offset) != '\n') && (offset < len )) {
                offset++;   
            }
            offset++;
        }
    }		
}

a_status_t
acfg_get_hapd_params(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_hapd_param_t *hapd_param)
{
    a_char_t cmd[255], replybuf[1024];
    a_uint32_t len = sizeof (replybuf);

    memset(replybuf, '\0', sizeof (replybuf));
    sprintf(cmd, "%s", WPA_HAPD_GET_CONFIG_CMD_PREFIX);

    if(acfg_ctrl_req(vap_params->vap_name, cmd, strlen(cmd),
            replybuf, &len, ACFG_OPMODE_HOSTAP) < 0){
        return A_STATUS_FAILED;
    }
    acfg_parse_hapd_param(replybuf, len, hapd_param);

    return A_STATUS_OK;
}

void 
acfg_get_security_state(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t * cur_vap_params,
        a_uint32_t *state)
{
    acfg_wlan_profile_security_params_t sec_params, cur_sec_params;
    a_uint8_t set_sec = 0, cur_set_sec = 0; 
    acfg_hapd_param_t hapd_param;

    memset((char *)&hapd_param, 0, sizeof (acfg_hapd_param_t));

    sec_params = vap_params->security_params;
    cur_sec_params = cur_vap_params->security_params;

    if (ACFG_IS_SEC_ENABLED(sec_params.sec_method)) {
        set_sec = 1;
    }
    if (ACFG_IS_SEC_ENABLED(cur_sec_params.sec_method)) {
        cur_set_sec = 1;
    }
    if (set_sec && cur_set_sec) {
        if (ACFG_SEC_CMP(vap_params, cur_vap_params)) {
            *state = ACFG_SEC_MODIFY_SECURITY_PARAMS;
        } else {
            *state = ACFG_SEC_UNCHANGED;
        }
        if (ACFG_WPS_CMP(vap_params, cur_vap_params))
        {
            *state = ACFG_SEC_RESET_SECURITY;
        }	
    } else if (set_sec && !cur_set_sec) {
        *state = ACFG_SEC_SET_SECURITY;
        if (acfg_ctrl_iface_present(vap_params->vap_name,
                    ACFG_OPMODE_HOSTAP) == 1) 
        {
            *state = ACFG_SEC_RESET_SECURITY;
        }
    } else if (!set_sec && cur_set_sec) {
        *state = ACFG_SEC_DISABLE_SECURITY;
        if (vap_params->security_params.wps_flag) {
            *state = ACFG_SEC_RESET_SECURITY;
        }
    } else if (!set_sec && !cur_set_sec) {
        if (sec_params.sec_method != cur_sec_params.sec_method) {
            *state = ACFG_SEC_DISABLE_SECURITY_CHANGED;
        }
        if (ACFG_IS_VALID_WPS(vap_params->security_params)) {
            if (!ACFG_IS_VALID_WPS(cur_vap_params->security_params)) 
            {
                *state = ACFG_SEC_SET_SECURITY;
            } else if (ACFG_IS_VALID_WPS(cur_vap_params->security_params))
            {
                int res = 0;
                /*It is open authentication with WPS enabled. Check for 
                  any modification*/
                res = acfg_get_open_wep_state(vap_params, cur_vap_params);
                if (res == 1) {
                    *state = ACFG_SEC_RESET_SECURITY;
                } else if (res == 2) {
                    *state = ACFG_SEC_MODIFY_SECURITY_PARAMS;
                } else {	
                    *state = ACFG_SEC_UNCHANGED;
                }
            }
        } else {
            if (ACFG_IS_VALID_WPS(cur_vap_params->security_params)) {
                *state = ACFG_SEC_DISABLE_SECURITY;
            }
        }
    }
}

a_status_t 
acfg_set_auth_open(acfg_wlan_profile_vap_params_t *vap_params,
        a_uint32_t state)
{
    int flag = 0;
    a_status_t status = A_STATUS_OK;

    if ((state  == ACFG_SEC_SET_SECURITY) ||
            (state == ACFG_SEC_DISABLE_SECURITY) || 
            (state == ACFG_SEC_DISABLE_SECURITY_CHANGED) ||
            (state == ACFG_SEC_RESET_SECURITY)) 
    {

        flag |= ACFG_ENCODE_DISABLED;
        flag |= ACFG_ENCODE_OPEN;
        status = acfg_set_enc(vap_params->vap_name, flag, "off");
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set enc\n");
            return A_STATUS_FAILED;
        }
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_AUTHMODE, 1);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed Set vap param\n");
            return A_STATUS_FAILED;
        }
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_DROPUNENCRYPTED, 0);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed Set vap param\n");
            return A_STATUS_FAILED;
        }
    }
    return status;
}

void 
acfg_get_cipher_str(acfg_wlan_profile_cipher_meth_e cipher_method, 
        a_char_t *cipher)
{
    if(cipher_method & ACFG_WLAN_PROFILE_CIPHER_METH_TKIP)
    {
        strcat(cipher, " TKIP");
    }
    if(cipher_method & ACFG_WLAN_PROFILE_CIPHER_METH_AES)
    {
        strcat(cipher, " CCMP");
    }
}

a_status_t
acfg_wpa_supp_fill_config_file(FILE *configfile_fp)
{
    char buf[1024], tempbuf[50];

    /*Add ctrl interface*/
    sprintf(tempbuf, "%s", CTRL_IFACE_STRING);
    sprintf(buf, "%s", tempbuf);
    strcat (buf, "=");
    strcat(buf, ctrl_wpasupp);
    strcat(buf, "\n");

    strcat(buf, "ap_scan=1\n");
    strcat(buf, "eapol_version=1\n");
    if(0 > fprintf(configfile_fp, "%s",buf))
        return A_STATUS_FAILED;

    return A_STATUS_OK;
}

a_status_t
acfg_wpa_supp_create_config (acfg_wlan_profile_vap_params_t *vap_params,
        a_char_t *conffile)
{
    FILE *configfile_fp;
    a_status_t status = A_STATUS_OK;

    strcpy(conffile, WPA_SUPP_CONFFILE_PREFIX);
    strcat(conffile, ".");
    strcat(conffile, vap_params->radio_name);
    strcat(conffile, ".");
    strcat(conffile, (a_char_t *)vap_params->vap_name);

    configfile_fp = fopen(conffile, "w+");
    if (configfile_fp == NULL) {
        acfg_log_errstr("config file open failed: %s\n", strerror(errno));
        return A_STATUS_FAILED;
    }
    status = acfg_wpa_supp_fill_config_file(configfile_fp);
    fclose(configfile_fp);

    return status;
}

a_status_t
acfg_wpa_supp_add_network(acfg_wlan_profile_vap_params_t *vap_params)
{
    a_char_t cmd[255], reply[255];
    a_uint32_t len = sizeof (reply);

    sprintf(cmd, "%s", WPA_ADD_NETWORK_CMD_PREFIX);
    if (acfg_ctrl_req(vap_params->vap_name, cmd,  strlen(cmd),
                reply, &len, ACFG_OPMODE_STA) < 0){ 
        acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                cmd, 
                vap_params->vap_name);
        return A_STATUS_FAILED;
    }

    return A_STATUS_OK;
}

a_status_t 
acfg_set_roaming(acfg_wlan_profile_vap_params_t *vap_params,
        int value)
{
    return acfg_set_vap_param(vap_params->vap_name, 
            ACFG_PARAM_ROAMING, 
            value);
}

a_status_t 
acfg_set_autoassoc(acfg_wlan_profile_vap_params_t *vap_params,
        int value)
{
    return acfg_set_vap_param(vap_params->vap_name, 
            ACFG_PARAM_AUTO_ASSOC, 
            value);
}

a_status_t acfg_security_open_or_wep(acfg_wlan_profile_vap_params_t *vap_params)
{
    a_status_t status = A_STATUS_FAILED;

    status = acfg_set_roaming(vap_params, 1);
    if(status == A_STATUS_OK){
        status = acfg_set_autoassoc(vap_params, 0);
    }
    if(status == A_STATUS_OK){
        status = acfg_wlan_iface_up(vap_params->vap_name);
    }
    if(status == A_STATUS_OK){
        status = acfg_wlan_iface_down(vap_params->vap_name);
    }

    return status;
}

a_status_t
acfg_wpa_supp_disable_network(acfg_wlan_profile_vap_params_t *vap_params)
{
    if(A_STATUS_OK != acfg_wpa_supp_delete(vap_params)){
        return A_STATUS_FAILED;
    }
    if(A_STATUS_OK !=acfg_security_open_or_wep(vap_params)){
        return A_STATUS_FAILED;
    }

    return A_STATUS_OK;
}

a_status_t
acfg_wpa_supp_enable_network(acfg_wlan_profile_vap_params_t *vap_params)
{
    a_char_t cmd[512];
    a_int8_t id = 0;
    a_char_t reply[255];
    a_uint32_t len = sizeof (reply);

    sprintf(cmd, "%s %d", WPA_ENABLE_NETWORK_CMD_PREFIX, id);
    if((acfg_ctrl_req(vap_params->vap_name, cmd, strlen(cmd),
                    reply, &len, ACFG_OPMODE_STA) < 0) || 
            strncmp (reply, "OK", strlen("OK"))){
        acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                cmd, 
                vap_params->vap_name);
        return A_STATUS_FAILED;
    }

    return A_STATUS_OK;
}

a_status_t
acfg_wpa_supp_set_network(acfg_wlan_profile_vap_params_t *vap_params,
        a_uint32_t flags, a_uint32_t *enable_network)
{
    a_uint8_t disable_supplicant = 0;
    a_char_t param[255], cmd[1024];
    a_int8_t id = 0;
    a_int32_t psk_len = 0;
    a_char_t reply[255];
    a_uint32_t len = sizeof (reply);
    a_char_t cipher[128];
    a_int32_t i, index = 0;
    a_char_t cmd_list[32][255];

    *enable_network = 1;
    if (flags & WPA_SUPP_MODIFY_SSID) {
        sprintf(cmd, "%s %d %s \"%s\"", WPA_SET_NETWORK_CMD_PREFIX, id,
                "ssid", vap_params->ssid);
        if((acfg_ctrl_req(vap_params->vap_name, cmd, strlen(cmd),
                        reply, &len, ACFG_OPMODE_STA) < 0) || 
                strncmp (reply, "OK", strlen("OK"))) {
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    cmd, 
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }
    }
    if (flags & WPA_SUPP_MODIFY_BSSID) {
        sprintf(cmd, "%s %d %s %s", WPA_SET_NETWORK_CMD_PREFIX, id,
                "bssid", vap_params->wds_params.wds_addr);
        if((acfg_ctrl_req(vap_params->vap_name, cmd, strlen(cmd),
                        reply, &len, ACFG_OPMODE_STA) < 0) ||
                strncmp (reply, "OK", strlen("OK"))) {
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    cmd, 
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }
    }
    if (flags & WPA_SUPP_MODIFY_SEC_METHOD) {
        if (vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_WPA) {
            sprintf(param, "%s", WPA_SUPP_PROTO_WPA);
        } else if (vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_WPA2) {
            sprintf(param, "%s", WPA_SUPP_PROTO_WPA2);
        } else if (vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2) {
            sprintf(param, "%s %s", WPA_SUPP_PROTO_WPA, WPA_SUPP_PROTO_WPA2);
        } else if (vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_WPS) {
        } else {
            disable_supplicant = 1;
        }
        if (vap_params->security_params.wps_flag) {
            disable_supplicant = 0;
        }
        if (disable_supplicant) {
            *enable_network = 0;
            return A_STATUS_OK;
        }
        if ((vap_params->security_params.sec_method ==
                    ACFG_WLAN_PROFILE_SEC_METH_WPA) ||
                (vap_params->security_params.sec_method ==
                 ACFG_WLAN_PROFILE_SEC_METH_WPA2) ||
                (vap_params->security_params.sec_method ==
                 ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2))
        {
            sprintf(cmd, "%s %d %s %s", WPA_SET_NETWORK_CMD_PREFIX, id,
                    "proto", param);
            if((acfg_ctrl_req(vap_params->vap_name, cmd, strlen(cmd),
                            reply, &len, ACFG_OPMODE_STA) < 0) ||
                    strncmp (reply, "OK", strlen("OK"))) {
                acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                        cmd, 
                        vap_params->vap_name);
                return A_STATUS_FAILED;
            }
        }
        if ((vap_params->security_params.sec_method ==
                    ACFG_WLAN_PROFILE_SEC_METH_WPA) ||
                (vap_params->security_params.sec_method ==
                 ACFG_WLAN_PROFILE_SEC_METH_WPA2) ||
                (vap_params->security_params.sec_method ==
                 ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2))
        {

            psk_len = strlen(vap_params->security_params.psk);
            sprintf(param, "%s", vap_params->security_params.psk);
            if (psk_len <= WPA_PSK_ASCII_LEN_MAX &&
                    psk_len >= WPA_PSK_ASCII_LEN_MIN)
            {
                sprintf(cmd, "%s %d %s \"%s\"", WPA_SET_NETWORK_CMD_PREFIX,
                        id, "psk", param);
            } else if (strlen(param) == WPA_PSK_HEX_LEN) {
                sprintf(cmd, "%s %d %s %s", WPA_SET_NETWORK_CMD_PREFIX, id,
                        "psk", param);
            } else {
                acfg_log_errstr("Invalid PSK\n");
            }
            if((acfg_ctrl_req(vap_params->vap_name, cmd,
                            strlen(cmd), reply, &len, ACFG_OPMODE_STA) < 0) ||
                    strncmp (reply, "OK", strlen("OK"))) {
                acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                        cmd, 
                        vap_params->vap_name);
                return A_STATUS_FAILED;
            }
            sprintf(param, "%s", WPA_SUPP_KEYMGMT_WPA_PSK);
            sprintf(cmd, "%s %d %s %s", WPA_SET_NETWORK_CMD_PREFIX, id,
                    "key_mgmt", param);
            if((acfg_ctrl_req(vap_params->vap_name, cmd,
                            strlen(cmd), reply, &len, ACFG_OPMODE_STA) < 0) ||
                    strncmp (reply, "OK", strlen("OK"))) {
                acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                        cmd, 
                        vap_params->vap_name);
                return A_STATUS_FAILED;
            }
        } else if ((vap_params->security_params.sec_method == 
                    ACFG_WLAN_PROFILE_SEC_METH_OPEN) &&
                (vap_params->security_params.cipher_method ==  
                 ACFG_WLAN_PROFILE_CIPHER_METH_NONE))
        {
            sprintf(cmd_list[index], "%s %d %s %s", 
                    WPA_SET_NETWORK_CMD_PREFIX, id, 
                    "key_mgmt", "NONE");
            index++;	
        } else if (ACFG_IS_SEC_WEP(vap_params->security_params))
        {
            sprintf(cmd_list[index], "%s %d %s %s", 
                    WPA_SET_NETWORK_CMD_PREFIX, id, 
                    "key_mgmt", "NONE");
            index++;	
            sprintf(cmd_list[index], "%s %d %s %s", 
                    WPA_SET_NETWORK_CMD_PREFIX, id, 
                    "wep_key0", vap_params->security_params.wep_key0);
            index++;	
            sprintf(cmd_list[index], "%s %d %s %s", 
                    WPA_SET_NETWORK_CMD_PREFIX, id, 
                    "wep_key1", vap_params->security_params.wep_key1);
            index++;	
            sprintf(cmd_list[index], "%s %d %s %s", 
                    WPA_SET_NETWORK_CMD_PREFIX, id, 
                    "wep_key2", vap_params->security_params.wep_key2);
            index++;	
            sprintf(cmd_list[index], "%s %d %s %s", 
                    WPA_SET_NETWORK_CMD_PREFIX, id, 
                    "wep_key3", vap_params->security_params.wep_key3);
            index++;	
            sprintf(cmd_list[index], "%s %d %s %d", 
                    WPA_SET_NETWORK_CMD_PREFIX, id, 
                    "wep_tx_keyidx",
                    vap_params->security_params.wep_key_defidx);
            index++;	
        }
    }
    if (flags & WPA_SUPP_MODIFY_CIPHER) {
        //Set pairwise ciphers
        memset (cipher, '\0', sizeof (cipher));
        acfg_get_cipher_str(vap_params->security_params.cipher_method, 
                cipher);
        sprintf(cmd, "%s %d %s %s", WPA_SET_NETWORK_CMD_PREFIX, id,
                "pairwise", cipher);
        if((acfg_ctrl_req(vap_params->vap_name, cmd,
                        strlen(cmd), reply, &len, ACFG_OPMODE_STA) < 0) ||
                strncmp (reply, "OK", strlen("OK"))) {
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    cmd, 
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }

        //Set group ciphers
        memset (cipher, '\0', sizeof (cipher));
        acfg_get_cipher_str(vap_params->security_params.g_cipher_method, 
                cipher);
        sprintf(cmd, "%s %d %s %s", WPA_SET_NETWORK_CMD_PREFIX, id,
                "group", cipher);
        if((acfg_ctrl_req(vap_params->vap_name, cmd,
                        strlen(cmd), reply, &len, ACFG_OPMODE_STA) < 0) ||
                strncmp (reply, "OK", strlen("OK"))) {
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    cmd, 
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }
    }
    //Set WPS related parameters
    {
        sprintf(cmd_list[index], "%s %s %d", WPA_SET_CMD_PREFIX, 
                "update_config", 0);
        index++;
        if(vap_params->security_params.wps_device_name[0] != 0)
        {
            sprintf(cmd_list[index], "%s %s %s", WPA_SET_CMD_PREFIX, "device_name",
                    vap_params->security_params.wps_device_name);
            index++;
        }
        if(vap_params->security_params.wps_model_name[0] != 0)
        {
            sprintf(cmd_list[index], "%s %s %s", WPA_SET_CMD_PREFIX, "model_name",
                    vap_params->security_params.wps_model_name);
            index++;
        }
        if(vap_params->security_params.wps_manufacturer[0] != 0)
        {
            sprintf(cmd_list[index], "%s %s %s", WPA_SET_CMD_PREFIX, "manufacturer",
                    vap_params->security_params.wps_manufacturer);
            index++;
        }
        if(vap_params->security_params.wps_model_number[0] != 0)
        {
            sprintf(cmd_list[index], "%s %s %s", WPA_SET_CMD_PREFIX, "model_number",
                    vap_params->security_params.wps_model_number);
            index++;
        }
        if(vap_params->security_params.wps_serial_number[0] != 0)
        {
            sprintf(cmd_list[index], "%s %s %s", WPA_SET_CMD_PREFIX, "serial_number",
                    vap_params->security_params.wps_serial_number);
            index++;
        }
        if(vap_params->security_params.wps_device_type[0] != 0)
        {
            sprintf(cmd_list[index], "%s %s %s", WPA_SET_CMD_PREFIX, "device_type",
                    vap_params->security_params.wps_device_type);
            index++;
        }
        sprintf(cmd_list[index], "%s %s %s", WPA_SET_CMD_PREFIX, "os_version",
                "01020300");
        index++;
        if(vap_params->security_params.wps_config_methods[0] != 0)
        {
            sprintf(cmd_list[index], "%s %s %s", WPA_SET_CMD_PREFIX, "config_methods",
                    vap_params->security_params.wps_config_methods);
            index++;
        }
    }
    for (i = 0; i < index; i++) {
        if((acfg_ctrl_req(vap_params->vap_name, cmd_list[i],
                        strlen(cmd_list[i]), reply, &len, ACFG_OPMODE_STA) < 0) ||
                strncmp (reply, "OK", strlen("OK"))) {
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    cmd_list[i], 
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }
    }
    return A_STATUS_OK;
}

a_status_t
acfg_wpa_supp_interface_remove(a_uint8_t *ifname )
{
    a_char_t cmd[255];
    a_char_t reply[255];
    a_uint32_t len = sizeof (reply);

    sprintf(cmd, "%s %s",  WPA_INTERFACE_REMOVE_CMD_PREFIX, ifname);
    if((acfg_ctrl_req((a_uint8_t*)ACFG_GLOBAL_CTRL_IFACE, cmd, strlen(cmd), 
                    reply,
                    &len, ACFG_OPMODE_STA) < 0) ||
            strncmp (reply, "OK", strlen("OK"))){
        acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                cmd, 
                ifname);
        return A_STATUS_FAILED;
    }

    return A_STATUS_OK;
}


a_status_t
acfg_wpa_supp_add_interface(acfg_wlan_profile_vap_params_t *vap_params,
        a_int8_t force_enable, a_int8_t *sec)
{
    a_char_t  driver_name[32], cmd[255];
    a_uint32_t modify_flags = 0, enable_network = 0;
    a_char_t conffile[255];
    a_char_t reply[255];
    a_uint32_t len = sizeof (reply);
    a_char_t ctrl_iface[25];
    a_status_t status = A_STATUS_OK;

    status = acfg_wpa_supp_create_config(vap_params, conffile);
    if(status != A_STATUS_OK){
        goto fail;
    }

    strcpy(driver_name, WPA_DRIVER_ATHR);
    sprintf(ctrl_iface, "%s", ctrl_wpasupp);
    if (vap_params->bridge[0] != 0) {
        sprintf(cmd, "%s %s\t%s\t%s\t%s\t%d\t%s",  WPA_INTERFACE_ADD_CMD_PREFIX,
                vap_params->vap_name, conffile, driver_name, ctrl_iface, 0,
                vap_params->bridge);
    } else {
        sprintf(cmd, "%s %s\t%s\t%s",  WPA_INTERFACE_ADD_CMD_PREFIX,
                vap_params->vap_name, conffile, driver_name);
    }

    if((acfg_ctrl_req((a_uint8_t *)ACFG_GLOBAL_CTRL_IFACE, cmd, strlen(cmd),
                reply, &len, ACFG_OPMODE_STA) < 0) ||
            strncmp (reply, "OK", strlen("OK"))) {
        acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                cmd,
                vap_params->vap_name);
        goto fail;
    }

    status = acfg_wpa_supp_add_network(vap_params);
    if(status != A_STATUS_OK){
        goto fail;
    }
    modify_flags = (WPA_SUPP_MODIFY_SSID |
            WPA_SUPP_MODIFY_SEC_METHOD);

    if (ACFG_IS_SEC_ENABLED(vap_params->security_params.sec_method)) {
        modify_flags |= WPA_SUPP_MODIFY_CIPHER;
    }

    if (vap_params->wds_params.enabled == 1 &&
            vap_params->wds_params.wds_addr[0] != 0)
    {
        modify_flags |= WPA_SUPP_MODIFY_BSSID;
    }

    if(acfg_wpa_supp_set_network(vap_params, modify_flags,
                &enable_network) != A_STATUS_OK){
        goto fail;
    }

    if (enable_network || force_enable) {
        status = acfg_wpa_supp_enable_network(vap_params);
        if(status != A_STATUS_OK){
            goto fail;
        }
        *sec = 1;
    } else {
        status = acfg_wpa_supp_disable_network(vap_params);
        if(status != A_STATUS_OK){
            goto fail;
        }
        *sec = 0;
    }
    return status;
fail:
    return A_STATUS_FAILED;
}

a_status_t
acfg_wpa_supp_delete(acfg_wlan_profile_vap_params_t *vap_params)
{
    a_char_t cmd[512];
    a_int8_t id = 0;
    a_char_t reply[255];
    a_uint32_t len = sizeof (reply);

    if (acfg_ctrl_iface_present(vap_params->vap_name,
                ACFG_OPMODE_STA) == -1) {
        return A_STATUS_OK;
    }

    memset(cmd, '\0', sizeof (cmd));
    sprintf(cmd, "%s %s %s",  WPA_SET_CMD_PREFIX, "ap_scan", "0");
    if((acfg_ctrl_req(vap_params->vap_name, cmd,
                strlen(cmd), reply, &len, ACFG_OPMODE_STA) < 0) ||
            strncmp (reply, "OK", strlen("OK"))){
        acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                cmd,
                vap_params->vap_name);
        return A_STATUS_FAILED;
    }

    memset(cmd, '\0', sizeof (cmd));
    sprintf(cmd, "%s %d", WPA_DISABLE_NETWORK_CMD_PREFIX, id);
    if((acfg_ctrl_req(vap_params->vap_name, cmd,
                strlen(cmd), reply, &len, ACFG_OPMODE_STA) < 0) ||
            strncmp (reply, "OK", strlen("OK"))){
        acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                cmd,
                vap_params->vap_name);
        return A_STATUS_FAILED;
    }

    sprintf(cmd, "%s %d", WPA_REMOVE_NETWORK_CMD_PREFIX, id);
    if((acfg_ctrl_req(vap_params->vap_name, cmd,
                strlen(cmd), reply, &len, ACFG_OPMODE_STA) < 0) ||
            strncmp (reply, "OK", strlen("OK"))){
        acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                cmd,
                vap_params->vap_name);
        return A_STATUS_FAILED;
    }

    if(A_STATUS_OK != acfg_wpa_supp_interface_remove(vap_params->vap_name))
        return A_STATUS_FAILED;

    return A_STATUS_OK;
}

a_status_t
acfg_wpa_supp_modify(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params,
        a_int8_t *sec)
{
    a_int8_t cur_wpa = 0;
    a_uint32_t state = ACFG_SEC_UNCHANGED;

    if(ACFG_IS_WPS_WEP_ENABLED(vap_params->security_params)) {
        fprintf(stderr, "configuring wep: Disabling wps\n");
        if (acfg_ctrl_iface_present(vap_params->vap_name,
                    ACFG_OPMODE_STA) == 1)
        {
            if(A_STATUS_OK != acfg_wpa_supp_disable_network(vap_params))
                return A_STATUS_FAILED;
            if(A_STATUS_OK != acfg_set_auth_open(vap_params, 
                        ACFG_SEC_DISABLE_SECURITY))
                return A_STATUS_FAILED;
            acfg_rem_wps_config_file(vap_params->vap_name);
        }
        *sec = 0;
        return A_STATUS_OK;	
    }
    acfg_get_security_state (vap_params, cur_vap_params, &state);		
    if (ACFG_IS_SEC_ENABLED(vap_params->security_params.sec_method) &&
            ACFG_IS_SEC_ENABLED(cur_vap_params->security_params.sec_method) &&
            strcmp(vap_params->bridge, cur_vap_params->bridge))
    {
        state = ACFG_SEC_SET_SECURITY;
    }
    if (strcmp(vap_params->bridge, cur_vap_params->bridge) &&
            ACFG_IS_VALID_WPS(vap_params->security_params))
    {
        state = ACFG_SEC_RESET_SECURITY;
    }

    if (state == ACFG_SEC_UNCHANGED) {
        if (ACFG_IS_SEC_ENABLED(vap_params->security_params.sec_method) ||
                vap_params->security_params.wps_flag) {
            *sec = 1;
        } else {
            *sec = 0;
            }
        return A_STATUS_OK;
        }
    if(A_STATUS_OK != acfg_set_auth_open(vap_params, state)){
        return A_STATUS_FAILED;
    }
    if (acfg_ctrl_iface_present(vap_params->vap_name,
                ACFG_OPMODE_STA) == 1)
    {
        cur_wpa = 1;
    } else {
        cur_wpa = 0;
    }
    if ((cur_wpa == 0) &&
            !((vap_params->security_params.sec_method ==
                    ACFG_WLAN_PROFILE_SEC_METH_WPA) ||
                (vap_params->security_params.sec_method ==
                 ACFG_WLAN_PROFILE_SEC_METH_WPA2) ||
                (vap_params->security_params.sec_method ==
                 ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2) ||
                vap_params->security_params.wps_flag))
    {
        *sec = 0;
        return A_STATUS_OK;
    }
    if(A_STATUS_OK != acfg_wpa_supp_disable_network(vap_params)){
        return A_STATUS_FAILED;
    }
    if (state == ACFG_SEC_DISABLE_SECURITY) {
        *sec = 0;
        return A_STATUS_OK;
    }

    if(A_STATUS_OK != acfg_wpa_supp_add_interface(vap_params, 0, sec)){
        return A_STATUS_FAILED;
    }
    if (vap_params->security_params.wps_flag) {
        acfg_wps_cred_t wps_cred;
        if (acfg_get_wps_config(vap_params->vap_name, &wps_cred) >= 0) {
            //Security param is modified. So remove the wps configuration
            acfg_rem_wps_config_file(vap_params->vap_name);
        }
    }

    return A_STATUS_OK;
}

a_status_t
acfg_set_hapd_config_params(acfg_wlan_profile_vap_params_t *vap_params)
{
    int index = 0, i;
    a_char_t replybuf[255];
    a_uint32_t len = sizeof (replybuf);
    a_char_t acfg_hapd_param_list[ACFG_MAX_HAPD_CONFIG_PARAM][1024];
    a_char_t cmd[255];
    a_char_t cipher[128];

    index = 0;
    sprintf(acfg_hapd_param_list[index], "SET ssid %s", vap_params->ssid);
    index++;

    if (vap_params->bridge[0]) {
        sprintf(acfg_hapd_param_list[index], "SET bridge %s", vap_params->bridge);
        index++;
    }
    if (ACFG_IS_SEC_WEP(vap_params->security_params))
    {
        sprintf(acfg_hapd_param_list[index], "SET wep_key0 %s", 
                vap_params->security_params.wep_key0);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET wep_key1 %s", 
                vap_params->security_params.wep_key1);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET wep_key2 %s", 
                vap_params->security_params.wep_key2);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET wep_key3 %s", 
                vap_params->security_params.wep_key3);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET wep_default_key %d", 
                vap_params->security_params.wep_key_defidx);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET auth_algs 1");
        index++;
    }
    if ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN) &&
            (vap_params->security_params.cipher_method == 
             ACFG_WLAN_PROFILE_CIPHER_METH_NONE))
    {
        sprintf(cmd, "CLEAR_WEP");
        if((acfg_ctrl_req (vap_params->vap_name,
                cmd, 
                strlen(cmd),
                replybuf, &len, 
                        ACFG_OPMODE_HOSTAP) < 0) ||
                strncmp (replybuf, "OK", strlen("OK"))){
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    cmd, 
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }
        sprintf(acfg_hapd_param_list[index], "SET auth_algs 1");
        index++;
    }

    if ((vap_params->security_params.sec2_radius_param.radius_ip[0] != 0) &&
            (vap_params->security_params.sec2_radius_param.radius_port != 0))
    {
        sprintf(acfg_hapd_param_list[index], "SET auth_server_addr %s",
                vap_params->security_params.sec2_radius_param.radius_ip);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET auth_server_port %d",
                vap_params->security_params.sec2_radius_param.radius_port);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET auth_server_shared_secret %s",
                vap_params->security_params.sec2_radius_param.shared_secret);
        index++;
    }
    if ((vap_params->security_params.sec1_radius_param.radius_ip[0] != 0) &&
            (vap_params->security_params.sec1_radius_param.radius_port != 0))
    {
        sprintf(acfg_hapd_param_list[index], "SET auth_server_addr %s",
                vap_params->security_params.sec1_radius_param.radius_ip);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET auth_server_port %d",
                vap_params->security_params.sec1_radius_param.radius_port);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET auth_server_shared_secret %s",
                vap_params->security_params.sec1_radius_param.shared_secret);
        index++;
    }
    if ((vap_params->security_params.pri_radius_param.radius_ip[0] != 0) &&
            (vap_params->security_params.pri_radius_param.radius_port != 0))
    {
        sprintf(acfg_hapd_param_list[index], "SET auth_server_addr %s",
                vap_params->security_params.pri_radius_param.radius_ip);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET auth_server_port %d",
                vap_params->security_params.pri_radius_param.radius_port);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET auth_server_shared_secret %s",
                vap_params->security_params.pri_radius_param.shared_secret);
        index++;
    } 
    if (vap_params->security_params.radius_retry_primary_interval != 0)
    {
        sprintf(acfg_hapd_param_list[index], "SET radius_retry_primary_interval %d",
                vap_params->security_params.radius_retry_primary_interval);
        index++;
    }
    if ((vap_params->security_params.sec_method !=
                ACFG_WLAN_PROFILE_SEC_METH_WPA_EAP) &&
            (vap_params->security_params.sec_method !=
             ACFG_WLAN_PROFILE_SEC_METH_WPA2_EAP))
    {
        sprintf(acfg_hapd_param_list[index], "SET eap_server 1");
        index++;
    }

    if ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_WPA) || 
            (vap_params->security_params.sec_method ==
             ACFG_WLAN_PROFILE_SEC_METH_WPA2) ||
            (vap_params->security_params.sec_method ==
             ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2))
    {
        if(strlen (vap_params->security_params.psk) <= (ACFG_MAX_PSK_LEN - 2)) {
            sprintf(acfg_hapd_param_list[index], "SET wpa_passphrase %s",
                    vap_params->security_params.psk);
            index++;
        } else if (strlen(vap_params->security_params.psk) == 
                ACFG_MAX_PSK_LEN - 1) 
        {
            sprintf(acfg_hapd_param_list[index], "SET wpa_psk %s",
                    vap_params->security_params.psk);
            index++;
        }
        sprintf(acfg_hapd_param_list[index], "SET wpa_key_mgmt %s", "WPA-PSK");
        index++;
    } else if ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_WPA_EAP) ||
            (vap_params->security_params.sec_method ==
             ACFG_WLAN_PROFILE_SEC_METH_WPA2_EAP))
    {
        sprintf(acfg_hapd_param_list[index], "SET wpa_key_mgmt %s", "WPA-EAP");
        index++;

        sprintf(acfg_hapd_param_list[index], "SET ieee8021x 1");
        index++;
    }

    memset (cipher, '\0', sizeof (cipher));
    acfg_get_cipher_str(vap_params->security_params.cipher_method, 
            cipher);
    if ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_WPA) ||	
            (vap_params->security_params.sec_method ==
             ACFG_WLAN_PROFILE_SEC_METH_WPA_EAP))
    {
        sprintf(acfg_hapd_param_list[index], "SET wpa %d", 1);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET wpa_pairwise %s",
                cipher);
        index++;

    } else if ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_WPA2) ||
            (vap_params->security_params.sec_method ==
             ACFG_WLAN_PROFILE_SEC_METH_WPA2_EAP))
    {
        sprintf(acfg_hapd_param_list[index], "SET wpa %d", 2);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET wpa_pairwise %s",
                cipher);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET rsn_pairwise %s",
                cipher);
        index++;
    } else if (vap_params->security_params.sec_method ==
            ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2)
    {
        sprintf(acfg_hapd_param_list[index], "SET wpa %d", 3);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET wpa_pairwise %s",
                cipher);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET rsn_pairwise %s",
                cipher);
        index++;
    } else {
        sprintf(acfg_hapd_param_list[index], "SET wpa %d", 0);
        index++;
    }
    if (vap_params->security_params.wps_flag) {
        sprintf(acfg_hapd_param_list[index], "SET wps_state %d",
                vap_params->security_params.wps_flag);
        index++;
        if(vap_params->security_params.wps_config_methods[0] != 0)
        {
            sprintf(acfg_hapd_param_list[index], "SET config_methods %s",
                    vap_params->security_params.wps_config_methods);
            index++;
        }
        if(vap_params->security_params.wps_device_type[0] != 0)
        {
            sprintf(acfg_hapd_param_list[index], "SET device_type %s",
                    vap_params->security_params.wps_device_type);
            index++;
        }
        if(vap_params->security_params.wps_manufacturer[0] != 0)
        {
            sprintf(acfg_hapd_param_list[index], "SET manufacturer %s",
                    vap_params->security_params.wps_manufacturer);
            index++;
        }
        if(vap_params->security_params.wps_model_name[0] != 0)
        {
            sprintf(acfg_hapd_param_list[index], "SET model_name %s",
                    vap_params->security_params.wps_model_name);
            index++;
        }
        if(vap_params->security_params.wps_model_number[0] != 0)
        {
            sprintf(acfg_hapd_param_list[index], "SET model_number %s",
                    vap_params->security_params.wps_model_number);
            index++;
        }
        if(vap_params->security_params.wps_serial_number[0] != 0)
        {
            sprintf(acfg_hapd_param_list[index], "SET serial_number %s",
                    vap_params->security_params.wps_serial_number);
            index++;
        }
        if(vap_params->security_params.wps_device_name[0] != 0)
        {
            sprintf(acfg_hapd_param_list[index], "SET device_name %s", 
                    vap_params->security_params.wps_device_name);
            index++;
        }
        if (vap_params->security_params.wps_upnp_iface[0] != 0) {
            sprintf(acfg_hapd_param_list[index], "SET upnp_iface %s", 
                    vap_params->security_params.wps_upnp_iface);
            index++;
        }
        if (vap_params->security_params.wps_friendly_name[0] != 0) {
            sprintf(acfg_hapd_param_list[index], "SET friendly_name %s", 
                    vap_params->security_params.wps_friendly_name);
            index++;
        }
        if (vap_params->security_params.wps_man_url[0] != 0) {
            sprintf(acfg_hapd_param_list[index], "SET manufacturer_url %s", 
                    vap_params->security_params.wps_man_url);
            index++;
        }
        if (vap_params->security_params.wps_model_desc[0] != 0) {
            sprintf(acfg_hapd_param_list[index], "SET model_description %s", 
                    vap_params->security_params.wps_model_desc);
            index++;
        }
        if (vap_params->security_params.wps_upc[0] != 0) {
            sprintf(acfg_hapd_param_list[index], "SET upc %s", 
                    vap_params->security_params.wps_upc);
            index++;
        }
        if (vap_params->security_params.wps_pbc_in_m1) {
            sprintf(acfg_hapd_param_list[index], "SET pbc_in_m1 %d", 
                    vap_params->security_params.wps_pbc_in_m1);
            index++;
        }

        if (vap_params->security_params.wps_rf_bands[0] != 0) {
            sprintf(acfg_hapd_param_list[index], "SET wps_rf_bands %s",
                    vap_params->security_params.wps_rf_bands);
            index++;
        }

    } else {
        sprintf(acfg_hapd_param_list[index], "SET wps_state %d",
                vap_params->security_params.wps_flag);
        index++;
    }
    if ((vap_params->security_params.sec2_acct_server_param.acct_ip[0] != 0) &&
            (vap_params->security_params.sec2_acct_server_param.acct_port != 0))
    {
        sprintf(acfg_hapd_param_list[index], "SET acct_server_addr %s",
                vap_params->security_params.sec2_acct_server_param.acct_ip);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET acct_server_port %d",
                vap_params->security_params.sec2_acct_server_param.acct_port);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET acct_server_shared_secret %s",
                vap_params->security_params.sec2_acct_server_param.shared_secret);
        index++;
    }
    if ((vap_params->security_params.sec1_acct_server_param.acct_ip[0] != 0) &&
            (vap_params->security_params.sec1_acct_server_param.acct_port != 0))
    {
        sprintf(acfg_hapd_param_list[index], "SET acct_server_addr %s",
                vap_params->security_params.sec1_acct_server_param.acct_ip);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET acct_server_port %d",
                vap_params->security_params.sec1_acct_server_param.acct_port);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET acct_server_shared_secret %s",
                vap_params->security_params.sec1_acct_server_param.shared_secret);
        index++;
    }
    if ((vap_params->security_params.pri_acct_server_param.acct_ip[0] != 0) &&
            (vap_params->security_params.pri_acct_server_param.acct_port != 0))
    {
        sprintf(acfg_hapd_param_list[index], "SET acct_server_addr %s",
                vap_params->security_params.pri_acct_server_param.acct_ip);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET acct_server_port %d",
                vap_params->security_params.pri_acct_server_param.acct_port);
        index++;

        sprintf(acfg_hapd_param_list[index], "SET acct_server_shared_secret %s",
                vap_params->security_params.pri_acct_server_param.shared_secret);
        index++;
    }
    if ((vap_params->security_params.hs_iw_param.hs_enabled == 1) &&
            (vap_params->security_params.hs_iw_param.iw_enabled == 1))
    {
        sprintf(acfg_hapd_param_list[index], "SET hs20 %d",
                vap_params->security_params.hs_iw_param.hs_enabled);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET interworking %d",
                vap_params->security_params.hs_iw_param.iw_enabled);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET access_network_type %d",
                vap_params->security_params.hs_iw_param.network_type);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET internet %d",
                vap_params->security_params.hs_iw_param.internet);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET asra %d",
                vap_params->security_params.hs_iw_param.asra);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET esr %d",
                vap_params->security_params.hs_iw_param.esr);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET uesa %d",
                vap_params->security_params.hs_iw_param.uesa);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET venue_group %d",
                vap_params->security_params.hs_iw_param.venue_group);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET venue_type %d",
                vap_params->security_params.hs_iw_param.venue_type);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET hessid %s",
                vap_params->security_params.hs_iw_param.hessid);
        index++;
        if(vap_params->security_params.hs_iw_param.roaming_consortium[0] != '\0')
        {
            sprintf(acfg_hapd_param_list[index], "SET roaming_consortium %s",
                    vap_params->security_params.hs_iw_param.roaming_consortium);
            index++;
        }
        if(vap_params->security_params.hs_iw_param.roaming_consortium2[0] != '\0')
        {
            sprintf(acfg_hapd_param_list[index], "SET roaming_consortium %s",
                    vap_params->security_params.hs_iw_param.roaming_consortium2);
            index++;
        }
        sprintf(acfg_hapd_param_list[index], "SET venue_name %s",
                vap_params->security_params.hs_iw_param.venue_name);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET manage_p2p %d", 
                vap_params->security_params.hs_iw_param.manage_p2p);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET disable_dgaf %d", 
                vap_params->security_params.hs_iw_param.disable_dgaf);
        index++;
        if(vap_params->security_params.hs_iw_param.hs20_venue_name[0] != '\0')
        {
            sprintf(acfg_hapd_param_list[index], "SET hs20_venue_name %s", 
                    vap_params->security_params.hs_iw_param.hs20_venue_name);
            index++;
        }
        if(vap_params->security_params.hs_iw_param.hs20_network_auth_type[0] != '\0')
        {
            sprintf(acfg_hapd_param_list[index], "SET hs20_network_auth_type %s",
                    vap_params->security_params.hs_iw_param.hs20_network_auth_type);
            index++;
        }
        if(vap_params->security_params.hs_iw_param.hs20_ipaddr_type_availability[0] != '\0')
        {
            sprintf(acfg_hapd_param_list[index], "SET hs20_ipaddr_type_availability %s",
                    vap_params->security_params.hs_iw_param.hs20_ipaddr_type_availability);
            index++;
        }
        if(vap_params->security_params.hs_iw_param.hs20_nai_realm_list[0][0] != '\0')
        {
            a_uint8_t nai_realam_index;

            for(nai_realam_index = 0; nai_realam_index < vap_params->num_nai_realm_data; nai_realam_index++)
            {
                if(vap_params->security_params.hs_iw_param.hs20_nai_realm_list[nai_realam_index][0] != '\0')
                {
                    sprintf(acfg_hapd_param_list[index], "SET hs20_nai_realm_list %s",
                            vap_params->security_params.hs_iw_param.hs20_nai_realm_list[nai_realam_index]);
                    index++;
                }
            }
        }
        if(vap_params->security_params.hs_iw_param.hs20_3gpp_cellular_network[0] != '\0')
        {
            sprintf(acfg_hapd_param_list[index], "SET hs20_3gpp_cellular_network %s",
                    vap_params->security_params.hs_iw_param.hs20_3gpp_cellular_network);
            index++;
        }
        if(vap_params->security_params.hs_iw_param.hs20_domain_name_list[0] != '\0')
        {
            sprintf(acfg_hapd_param_list[index], "SET hs20_domain_name_list %s",
                    vap_params->security_params.hs_iw_param.hs20_domain_name_list);
            index++;
        }
        if(vap_params->security_params.hs_iw_param.hs20_operator_friendly_name[0] != '\0')
        {
            sprintf(acfg_hapd_param_list[index], "SET hs20_operator_friendly_name %s",
                    vap_params->security_params.hs_iw_param.hs20_operator_friendly_name);
            index++;
        }
        if(vap_params->security_params.hs_iw_param.hs20_wan_metrics[0] != '\0')
        {
            sprintf(acfg_hapd_param_list[index], "SET hs20_wan_metrics %s",
                    vap_params->security_params.hs_iw_param.hs20_wan_metrics);
            index++;
        }
        if(vap_params->security_params.hs_iw_param.hs20_connection_capability[0] != '\0')
        {
            sprintf(acfg_hapd_param_list[index], "SET hs20_connection_capability %s",
                    vap_params->security_params.hs_iw_param.hs20_connection_capability);
            index++;
        }
        if(vap_params->security_params.hs_iw_param.hs20_operating_class[0] != '\0')
        {
            sprintf(acfg_hapd_param_list[index], "SET hs20_operating_class %s",
                    vap_params->security_params.hs_iw_param.hs20_operating_class);
            index++;
        }
    }
    if (index >= ACFG_MAX_HAPD_CONFIG_PARAM) {
        acfg_log_errstr("%s: hostapd config array overflow\n", __func__);
        return A_STATUS_FAILED;
    }
    for (i = 0; i < index ; i++) {	
        if((acfg_ctrl_req (vap_params->vap_name,
                acfg_hapd_param_list[i], 
                strlen(acfg_hapd_param_list[i]),
                replybuf, &len, 
                        ACFG_OPMODE_HOSTAP) < 0) ||
                strncmp (replybuf, "OK", strlen("OK"))){
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    acfg_hapd_param_list[i],
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }
    }
    return A_STATUS_OK;
}

a_status_t
acfg_hostapd_add_bss(acfg_wlan_profile_vap_params_t *vap_params, a_int8_t *sec)
{
    char buffer[4096];
    int add_bss = 0;
    char replybuf[255];
    a_uint32_t len = sizeof (replybuf);

    if(ACFG_IS_WPS_WEP_ENABLED(vap_params->security_params)) {
        *sec = 0;
        return A_STATUS_OK;
    }
    if (ACFG_IS_SEC_ENABLED(vap_params->security_params.sec_method) ||
            vap_params->security_params.wps_flag)
    {
        add_bss = 1;
    }

    if (add_bss) {
        sprintf(buffer, "ADD %s %s", vap_params->vap_name, ctrl_hapd);
        if((acfg_ctrl_req ((a_uint8_t *)ACFG_HAPD_GLOBAL_CTRL_IFACE,
                buffer, strlen(buffer),
                replybuf, &len,
                    ACFG_OPMODE_HOSTAP) < 0) ||
                strncmp (replybuf, "OK", strlen("OK"))){
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    buffer,
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }
        if(A_STATUS_OK != acfg_set_hapd_config_params(vap_params)){
            acfg_log_errstr("%s: Failed to configure security for %s\n", __func__,
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }
        sprintf(buffer, "ENABLE");
        if((acfg_ctrl_req (vap_params->vap_name,
                buffer, strlen(buffer),
                replybuf, &len,
                    ACFG_OPMODE_HOSTAP) < 0) ||
                strncmp (replybuf, "OK", strlen("OK"))){
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    buffer,
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }
        *sec = 1;
    }
    return A_STATUS_OK;
}

a_int8_t 
acfg_check_reset(acfg_wlan_profile_vap_params_t *vap_params, 
        acfg_wlan_profile_vap_params_t *cur_vap_params)
{
    a_int8_t ret = 0;

    if (strcmp(vap_params->security_params.wps_upnp_iface, 
                vap_params->security_params.wps_upnp_iface))
    {
        ret = 1;
    }
    if (strcmp(vap_params->bridge, cur_vap_params->bridge)) {
        ret = 1;
    }
    if (ACFG_SEC_CMP_RADIUS(vap_params->security_params,
                cur_vap_params->security_params))
    {
        ret = 1;
    }
    return ret;
}


a_status_t
acfg_hostapd_modify_bss(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params,
        a_int8_t *sec)
{
    char buffer[4096];
    int index = 0, i;
    char replybuf[255];
    a_uint32_t len = sizeof (replybuf);
    a_char_t acfg_hapd_param_list[32][512];
    a_uint32_t state = ACFG_SEC_UNCHANGED;
    a_status_t status = A_STATUS_FAILED;

    if(ACFG_IS_WPS_WEP_ENABLED(vap_params->security_params)) {
        fprintf(stderr, "configuring wep: Disabling wps\n");
        if (acfg_ctrl_iface_present(vap_params->vap_name,
                    ACFG_OPMODE_HOSTAP) == 1)
        {
            if (A_STATUS_OK != acfg_hostapd_delete_bss(vap_params)) {
                acfg_log_errstr("hostapd delete error\n");
                return A_STATUS_FAILED;
            }
            status = acfg_set_auth_open(vap_params, ACFG_SEC_DISABLE_SECURITY);
            if(status != A_STATUS_OK){
                return A_STATUS_FAILED;
            }
            acfg_rem_wps_config_file(vap_params->vap_name);
        }
        *sec = 0;
        return A_STATUS_OK;	
    }
    acfg_get_security_state (vap_params, cur_vap_params, &state);	
    if (ACFG_IS_SEC_ENABLED(vap_params->security_params.sec_method) &&
            ACFG_IS_SEC_ENABLED(cur_vap_params->security_params.sec_method) &&
            strcmp(vap_params->bridge, cur_vap_params->bridge) &&
            (acfg_ctrl_iface_present(vap_params->vap_name,
                                     ACFG_OPMODE_HOSTAP) == 1))
    {
        if (A_STATUS_OK != acfg_hostapd_delete_bss(vap_params)) {
            acfg_log_errstr("Hostapd delete error\n");
            return A_STATUS_FAILED;
        }
        if(A_STATUS_OK != acfg_set_auth_open(vap_params, state)){
            return A_STATUS_FAILED;
        }
        state = ACFG_SEC_SET_SECURITY;
    }
    if (strcmp(vap_params->bridge, cur_vap_params->bridge) &&
            ACFG_IS_VALID_WPS(vap_params->security_params))
    {
        state = ACFG_SEC_RESET_SECURITY;
    }

    if (state == ACFG_SEC_UNCHANGED) {
        if (ACFG_IS_SEC_ENABLED(vap_params->security_params.sec_method)) {
            *sec = 1;
            return A_STATUS_OK;
        } else {
            if (vap_params->security_params.wps_flag) {
                *sec = 1;
                return A_STATUS_OK;
            }
            *sec = 0;
            return A_STATUS_OK;
            }
        }
    if(A_STATUS_OK != acfg_set_auth_open(vap_params, state)){
        return A_STATUS_FAILED;
    }
    if (state == ACFG_SEC_MODIFY_SECURITY_PARAMS) {
        if (acfg_check_reset(vap_params, cur_vap_params)) {
            state = ACFG_SEC_RESET_SECURITY;
        }
        if (vap_params->security_params.wps_flag) {
            acfg_wps_cred_t wps_cred;

            state = ACFG_SEC_RESET_SECURITY;
            if(acfg_get_wps_config(vap_params->vap_name, &wps_cred) >= 0){
            if ( wps_cred.wps_state == WPS_FLAG_CONFIGURED) {	
                //Security param is modified. So remove the wps configuration
                acfg_rem_wps_config_file(vap_params->vap_name);
            }
        }
    }
    }

    if ((state == ACFG_SEC_SET_SECURITY) ||
            (state == ACFG_SEC_MODIFY_SECURITY_PARAMS) ||
            (state == ACFG_SEC_RESET_SECURITY))
    {
        if (state == ACFG_SEC_RESET_SECURITY) {
            if(A_STATUS_OK != acfg_hostapd_delete_bss(vap_params)) {
                return A_STATUS_FAILED;
                acfg_log_errstr("Hostapd delete error\n");
            }
            state = ACFG_SEC_SET_SECURITY;	
        }
        if (state == ACFG_SEC_SET_SECURITY) {
            sprintf(buffer, "ADD %s %s", vap_params->vap_name, ctrl_hapd);
            if((acfg_ctrl_req ((a_uint8_t *)ACFG_HAPD_GLOBAL_CTRL_IFACE,
                    buffer, strlen(buffer),
                    replybuf, &len,
                            ACFG_OPMODE_HOSTAP) < 0) ||
                    strncmp (replybuf, "OK", strlen("OK"))){
                return A_STATUS_FAILED;
            }

            sprintf(acfg_hapd_param_list[index], "ENABLE");
            index++;
        }
        if(A_STATUS_OK != acfg_set_hapd_config_params(vap_params)){
            return A_STATUS_FAILED;
        }
        if (state == ACFG_SEC_MODIFY_SECURITY_PARAMS) {
            sprintf(acfg_hapd_param_list[index], "RELOAD");
            index++;
        }
        *sec = 1;
    }

    if (state == ACFG_SEC_DISABLE_SECURITY) {
        if(A_STATUS_OK != acfg_hostapd_delete_bss(vap_params)) {
            acfg_log_errstr("Hostapd delete error\n");
            return A_STATUS_FAILED;
        }
        if(A_STATUS_OK != acfg_set_auth_open(vap_params, state)){
            return A_STATUS_FAILED;
        }
        *sec = 0;
        return A_STATUS_OK;
    }
    for (i = 0; i < index; i++) {
        if((acfg_ctrl_req (vap_params->vap_name,
                        acfg_hapd_param_list[i],
                        strlen(acfg_hapd_param_list[i]),
                        replybuf, &len,
                        ACFG_OPMODE_HOSTAP) < 0) ||
                strncmp (replybuf, "OK", strlen("OK"))){
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    acfg_hapd_param_list[i],
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }
    }
    return A_STATUS_OK;
}

a_status_t
acfg_hostapd_delete_bss(acfg_wlan_profile_vap_params_t *vap_params)
{
    char buffer[4096];
    char replybuf[255];
    a_uint32_t reply_len = sizeof(replybuf);

    sprintf(buffer, "REMOVE %s", vap_params->vap_name);
    if((acfg_ctrl_req ((a_uint8_t *)ACFG_HAPD_GLOBAL_CTRL_IFACE,
            buffer, strlen(buffer),
                    replybuf, &reply_len,
                    ACFG_OPMODE_HOSTAP) < 0) ||
            strncmp (replybuf, "OK", strlen("OK"))){
        return A_STATUS_FAILED;
    }

    return A_STATUS_OK;
}

a_status_t
acfg_set_security(acfg_wlan_profile_vap_params_t *vap_params, 
        acfg_wlan_profile_vap_params_t *cur_vap_params,
        a_uint8_t action,
        a_int8_t *sec)
{
    a_status_t status = A_STATUS_OK;

    if (vap_params->opmode == ACFG_OPMODE_STA) {
        if (action == PROFILE_CREATE) {
            if(ACFG_IS_WPS_WEP_ENABLED(vap_params->security_params)) {
                return status;
            }
            status = acfg_wpa_supp_add_interface(vap_params, 0, sec);
            if(status != A_STATUS_OK){
                acfg_log_errstr("%s: Failed to ADD %s with security\n", __func__, 
                        vap_params->vap_name);
                return A_STATUS_FAILED;
            }
            if (*sec == 0) {
                status = acfg_set_auth_open(vap_params, ACFG_SEC_DISABLE_SECURITY);
            }
        } else if (action  == PROFILE_MODIFY) {
            status = acfg_wpa_supp_modify(vap_params, cur_vap_params, sec);
            if(status != A_STATUS_OK){
                acfg_log_errstr("%s: Failed to MODIFY %s with security\n", __func__, 
                        vap_params->vap_name);
            }
        } else if (action == PROFILE_DELETE) {
            status = acfg_wpa_supp_delete(vap_params);
            if(status != A_STATUS_OK){
                acfg_log_errstr("%s: Failed to DEL %s with security\n", __func__, 
                        vap_params->vap_name);
            }
        }
    }

    if (vap_params->opmode == ACFG_OPMODE_HOSTAP) {
        if (action == PROFILE_CREATE) {
            status = acfg_hostapd_add_bss(vap_params, sec);
            if(status != A_STATUS_OK){
                acfg_log_errstr("%s: Failed to ADD bss %s with security\n", __func__, 
                        vap_params->vap_name);
                return A_STATUS_FAILED;
            }
            if(*sec == 0){
                status = acfg_set_auth_open(vap_params, ACFG_SEC_DISABLE_SECURITY);
            }
        } else if (action  == PROFILE_MODIFY) {
            status = acfg_hostapd_modify_bss(vap_params, cur_vap_params, sec);
            if(status != A_STATUS_OK){
                acfg_log_errstr("%s: Failed to MODIFY bss %s with security\n", __func__,
                        vap_params->vap_name);
            }
        } else if (action == PROFILE_DELETE) {
            if (ACFG_IS_SEC_ENABLED(vap_params->security_params.sec_method)) {
                status = acfg_hostapd_delete_bss(vap_params);
                if(status != A_STATUS_OK){
                    acfg_log_errstr("%s: Failed to DEL bss %s with security\n", __func__,
                            vap_params->vap_name);
                }
            }
        }
    }
    return status;
}

int
acfg_get_supplicant_param(acfg_wlan_profile_vap_params_t *vap_params,
        enum wpa_supp_param_type param)
{
    a_char_t cmd[255], replybuf[255];
    a_uint32_t len = sizeof (replybuf);
    a_uint32_t id = 0;

    if (acfg_ctrl_iface_present(vap_params->vap_name,
                ACFG_OPMODE_STA) == -1)
    {
        return 0;
    }

    switch (param) {
        case ACFG_WPA_PROTO:
            sprintf(cmd, "%s %d %s", WPA_GET_NETWORK_CMD_PREFIX, id, "proto");

            if(acfg_ctrl_req(vap_params->vap_name, cmd, strlen(cmd),
                        replybuf, &len, ACFG_OPMODE_STA) < 0){
                acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                        cmd,
                        vap_params->vap_name);
                return -1;
            }

            if (strncmp(replybuf, WPA_SUPP_PROTO_WPA, 3) == 0) {
                return ACFG_WLAN_PROFILE_SEC_METH_WPA;
            } else if (strncmp(replybuf, WPA_SUPP_PROTO_WPA2, 4) == 0) {
                return ACFG_WLAN_PROFILE_SEC_METH_WPA2;
            }
            break;

        case ACFG_WPA_KEY_MGMT:
            break;

        case ACFG_WPA_UCAST_CIPHER:
            sprintf(cmd, "%s %d %s", WPA_GET_NETWORK_CMD_PREFIX,
                    id, "pairwise");

            if(acfg_ctrl_req(vap_params->vap_name, cmd, strlen(cmd),
                        replybuf, &len, ACFG_OPMODE_STA) < 0){
                acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                        cmd,
                        vap_params->vap_name);
                return -1;
            }
            if (strncmp(replybuf, WPA_SUPP_PAIRWISE_CIPHER_TKIP, 4) == 0) {
                return ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
            } else if (strncmp(replybuf, WPA_SUPP_PAIRWISE_CIPHER_CCMP, 4)
                    == 0) {
                return ACFG_WLAN_PROFILE_CIPHER_METH_AES;
            }
            break;

        case ACFG_WPA_MCAST_CIPHER:
            break;

        default:
            return -1;
    }
    return 0;
}

void
acfg_parse_hapd_get_params(acfg_wlan_profile_vap_params_t *vap_params, a_char_t *buf,
        a_int32_t len)
{
    a_int32_t offset = 0;
    a_int8_t wpa = 0;

    while (offset < len) {
        if (!strncmp(buf + offset, "wps_state=", strlen("wps_state="))) {

            offset += strlen("wps_state=");

            if (!strncmp(buf + offset, "disabled", strlen("disabled"))) {
                offset += strlen("disabled");
            } else if (!strncmp(buf + offset, "not configured",
                        strlen("not configured"))) {
                offset += strlen("not configured");
            } else if (!strncmp(buf + offset, "configured",
                        strlen("configured"))) {
                vap_params->security_params.sec_method =
                    ACFG_WLAN_PROFILE_SEC_METH_WPS;
                offset += strlen("configured");
            }
            offset++;
        } else if (!strncmp(buf + offset, "key_mgmt=", strlen("key_mgmt="))) {
            offset += strlen("key_mgmt=");
            if (!strncmp (buf + offset, "WPA-PSK", strlen("WPA-PSK"))) {
                wpa = 1;
                offset += strlen("WPA-PSK");
            }
            offset++;
        } else if (!strncmp (buf + offset, "group_cipher=",
                    strlen("group_cipher=")))
        {
            offset += strlen("group_cipher=");
            if (!strncmp(buf + offset, "CCMP", strlen ("CCMP"))) {
                offset += strlen ("CCMP");
            } else if (!strncmp(buf + offset, "TKIP", strlen ("TKIP"))) {
                offset += strlen ("TKIP");
            }
            offset++;

        } else if (!strncmp(buf + offset, "rsn_pairwise_cipher=",
                    strlen("rsn_pairwise_cipher=")))
        {
            offset += strlen("rsn_pairwise_cipher=");
            if (wpa == 1) {
                vap_params->security_params.sec_method =
                    ACFG_WLAN_PROFILE_SEC_METH_WPA2;
            }

            if (!strncmp(buf + offset, "CCMP ", strlen("CCMP "))) {
                vap_params->security_params.cipher_method =
                    ACFG_WLAN_PROFILE_CIPHER_METH_AES;

                offset += strlen("CCMP ");

                if (!strncmp(buf + offset, "TKIP ", strlen("TKIP "))) {
                    offset += strlen("TKIP ");
                }

                offset++;
            } else if (!strncmp(buf + offset, "TKIP ", strlen("TKIP "))) {
                vap_params->security_params.cipher_method =
                    ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;

                offset += strlen("TKIP ");

                if (!strncmp(buf + offset, "CCMP ", strlen("CCMP "))) {
                    offset += strlen("CCMP ");
                }

                offset++;
            }
        } else if (!strncmp(buf + offset, "wpa_pairwise_cipher=",
                    strlen("wpa_pairwise_cipher=")))
        {
            vap_params->security_params.sec_method =
                ACFG_WLAN_PROFILE_SEC_METH_WPA;
            offset += strlen("wpa_pairwise_cipher=");

            if (!strncmp(buf + offset, "CCMP ", strlen("CCMP "))) {
                vap_params->security_params.cipher_method =
                    ACFG_WLAN_PROFILE_CIPHER_METH_AES;

                offset += strlen("CCMP ");

                if (!strncmp(buf + offset, "TKIP ", strlen("TKIP "))) {
                    offset += strlen("TKIP ");
                }

                offset++;
            } else if (!strncmp(buf + offset, "TKIP ", strlen("TKIP "))) {
                vap_params->security_params.cipher_method =
                    ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;

                offset += strlen("TKIP ");

                if (!strncmp(buf + offset, "CCMP ", strlen("CCMP "))) {
                    offset += strlen("CCMP ");
                }

                offset++;
            }
        } else {
            while ((*(buf + offset) != '\n') && (offset < len )) {
                offset++;
            }
            offset++;
        }
    }
}

a_status_t
acfg_get_hapd_param(acfg_wlan_profile_vap_params_t *vap_params)
{
    a_char_t cmd[255], replybuf[1024];
    a_uint32_t len = sizeof (replybuf);

    memset(replybuf, '\0', sizeof (replybuf));
    sprintf(cmd, "%s", WPA_HAPD_GET_CONFIG_CMD_PREFIX);

    if(acfg_ctrl_req(vap_params->vap_name, cmd, strlen(cmd),
                replybuf, &len, ACFG_OPMODE_HOSTAP) < 0){
        acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                cmd,
                vap_params->vap_name);
        return A_STATUS_FAILED;
    }

    acfg_parse_hapd_get_params(vap_params, replybuf, len);

    return A_STATUS_OK;
}

int
acfg_get_wpa_params(acfg_wlan_profile_vap_params_t *vap_params)
{
    a_uint32_t wpa_sec_method, ucast_cipher;

    if (vap_params->opmode == ACFG_OPMODE_STA) {
        if (acfg_ctrl_iface_present(vap_params->vap_name,
                    ACFG_OPMODE_STA) == -1)
        {
            return 0;
        }

        wpa_sec_method = acfg_get_supplicant_param(vap_params, ACFG_WPA_PROTO);
        if ((wpa_sec_method == ACFG_WLAN_PROFILE_SEC_METH_WPA) ||
                (wpa_sec_method == ACFG_WLAN_PROFILE_SEC_METH_WPA2))
        {
            vap_params->security_params.sec_method = wpa_sec_method;
            ucast_cipher = acfg_get_supplicant_param(vap_params,
                    ACFG_WPA_UCAST_CIPHER);
            vap_params->security_params.cipher_method = ucast_cipher;
        }
    } else if (vap_params->opmode == ACFG_OPMODE_HOSTAP) {
        if(A_STATUS_OK != acfg_get_hapd_param(vap_params)){
            return A_STATUS_FAILED;
        }
    }
    return 0;
}

a_status_t
acfg_set_wps(a_uint8_t *ifname, enum acfg_wsupp_set_type type,
             a_int8_t *str)
{
    a_char_t cmd[255], replybuf[255];
    a_uint32_t len;

    switch (type) {
        case ACFG_WSUPP_SET_UUID:
            sprintf(cmd, "%s %s %s", WPA_SET_CMD_PREFIX, "uuid", str);
            break;
        case ACFG_WSUPP_SET_DEVICE_NAME:
            sprintf(cmd, "%s %s %s", WPA_SET_CMD_PREFIX, "device_name", str);
            break;
        case ACFG_WSUPP_SET_MANUFACTURER:
            sprintf(cmd, "%s %s %s", WPA_SET_CMD_PREFIX, "manufacturer", str);
            break;
        case ACFG_WSUPP_SET_MODEL_NAME:
            sprintf(cmd, "%s %s %s", WPA_SET_CMD_PREFIX, "model_name", str);
            break;
        case ACFG_WSUPP_SET_MODEL_NUMBER:
            sprintf(cmd, "%s %s %s", WPA_SET_CMD_PREFIX, "model_number", str);
            break;
        case ACFG_WSUPP_SET_SERIAL_NUMBER:
            sprintf(cmd, "%s %s %s", WPA_SET_CMD_PREFIX, "serial_number", str);
            break;
        case ACFG_WSUPP_SET_DEVICE_TYPE:
            sprintf(cmd, "%s %s %s", WPA_SET_CMD_PREFIX, "device_type", str);
            break;
        case ACFG_WSUPP_SET_OS_VERSION:
            sprintf(cmd, "%s %s %s", WPA_SET_CMD_PREFIX, "os_version", str);
            break;
        case ACFG_WSUPP_SET_CONFIG_METHODS:
            sprintf(cmd, "%s %s %s", WPA_SET_CMD_PREFIX,
                    "config_methods", str);
            break;
        default:
            return A_STATUS_FAILED;
    }
    len = sizeof(replybuf);
    if((acfg_ctrl_req(ifname, cmd, strlen(cmd), 
                    replybuf, &len, ACFG_OPMODE_STA) < 0) ||
            strncmp (replybuf, "OK", strlen("OK"))){
        acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                cmd,
                ifname);
        return A_STATUS_FAILED;
    }
    return A_STATUS_OK;
}

static int 
hex2num(a_int8_t c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}


int
hwaddr_aton(a_char_t  *txt, a_uint8_t *addr)
{
    int i;

    for (i = 0; i < 6; i++) {
        int a, b;
        if (*txt == '\0') {
            return -1;
        }
        a = hex2num(*txt++);
        if (a < 0)
            return -1;
        if (*txt == '\0') {
            if (i < 6) {
                return -1;
            }
        }
        b = hex2num(*txt++);
        if (b < 0)
            return -1;
        *addr++ = (a << 4) | b;
        if (i < 5 && *txt != ':')
            return -1;
        txt++;
    }
    return 0;
}


a_status_t
acfg_set_wps_mode(a_char_t *radio_name, a_uint8_t *ifname,
        enum acfg_wsupp_wps_type wps,
        a_char_t *params1, a_int8_t *params2)
{
    a_char_t cmd[255];
    a_char_t bssid[24];
    a_uint8_t macaddr[6];
    acfg_opmode_t opmode;
    a_char_t reply[255];
    a_uint32_t len = sizeof (reply);
    a_int8_t sec;

    if (acfg_get_opmode(ifname, &opmode) != A_STATUS_OK) {
        acfg_log_errstr("%s: Opmode get failed for %s\n", __func__, ifname);
        return A_STATUS_FAILED;
    }

    if (opmode == ACFG_OPMODE_STA) {
        /* Check if the network is set in wpa_supplicant */
        if (acfg_ctrl_iface_present (ifname, opmode) == -1) {
            acfg_wlan_profile_vap_params_t vap_params;

            strcpy((a_char_t *)vap_params.vap_name, (a_char_t *)ifname);
            strcpy(vap_params.radio_name, radio_name);
            if(A_STATUS_OK != acfg_wpa_supp_add_interface(&vap_params, 1, &sec))
            {
                return A_STATUS_FAILED;
            }
        }
    }

    if (hwaddr_aton(params1, macaddr)) {
        strcpy (bssid, "any");
    } else {
        strcpy (bssid, params1);
    }

    switch (wps) {
        case ACFG_WSUPP_WPS_PBC:
            if (opmode == ACFG_OPMODE_STA) {
                sprintf(cmd, "%s %s", WPA_WPS_PBC_CMD_PREFIX, bssid);
                if((acfg_ctrl_req(ifname, cmd, strlen(cmd), reply,
                                &len, ACFG_OPMODE_STA) < 0) ||
                        strncmp (reply, "OK", strlen("OK"))){
                    acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                            cmd,
                            ifname);
                    return A_STATUS_FAILED;
                }
            } else if (opmode == ACFG_OPMODE_HOSTAP) {
                sprintf(cmd, "%s", WPA_WPS_PBC_CMD_PREFIX);
                if((acfg_ctrl_req(ifname, cmd, strlen(cmd), reply, 
                        &len, ACFG_OPMODE_HOSTAP) < 0) ||
                        strncmp (reply, "OK", strlen("OK"))){
                    acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                            cmd,
                            ifname);
                    return A_STATUS_FAILED;
                }
            }
            break;

        case ACFG_WSUPP_WPS_PIN:
            sprintf(cmd, "%s %s", WPA_WPS_CHECK_PIN_CMD_PREFIX, params2);
            len = sizeof (reply);

            if (opmode == ACFG_OPMODE_STA) {
                if(acfg_ctrl_req(ifname, cmd, strlen(cmd), reply,
                        &len, ACFG_OPMODE_STA) < 0){
                    acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                            cmd,
                            ifname);
                    return A_STATUS_FAILED;
                }
            } else if (opmode == ACFG_OPMODE_HOSTAP) {
                if(acfg_ctrl_req(ifname, cmd, strlen(cmd), reply,
                        &len, ACFG_OPMODE_HOSTAP) < 0){
                    acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                            cmd,
                            ifname);
                    return A_STATUS_FAILED;
                }
            }

            if (!strncmp(reply, "FAIL-CHECKSUM", strlen("FAIL-CHECKSUM")) ||
                    !strncmp(reply, "FAIL", strlen("FAIL"))) {
                acfg_log_errstr("%s: Invalid pin\n", __func__);
                return A_STATUS_FAILED;
            }

            memset(reply, '\0', sizeof (reply));
            len = sizeof (reply);

            if (opmode == ACFG_OPMODE_STA) {

                sprintf(cmd, "%s %s %s", WPA_WPS_PIN_CMD_PREFIX,
                        bssid, params2);
                if(acfg_ctrl_req(ifname, cmd, strlen(cmd), reply, &len,
                        ACFG_OPMODE_STA) < 0){
                    acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                            cmd,
                            ifname);
                    return A_STATUS_FAILED;
                }

            } else if (opmode == ACFG_OPMODE_HOSTAP) {
                sprintf(cmd, "%s %s %s %d", WPA_WPS_AP_PIN_CMD_PREFIX,  "set",
                        params2, WPS_TIMEOUT);
                if(acfg_ctrl_req(ifname, cmd, strlen(cmd), reply, &len,
                        ACFG_OPMODE_HOSTAP) < 0){
                    acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                            cmd,
                            ifname);
                    return A_STATUS_FAILED;
                }
            }
            break;

        default:
            return A_STATUS_EINVAL;
    }
    return A_STATUS_OK;
}

a_status_t acfg_wps_cancel(a_uint8_t *ifname)
{
    a_char_t cmd[255];
    a_uint32_t len = 0;
    acfg_opmode_t opmode;

    if (acfg_get_opmode(ifname, &opmode) != A_STATUS_OK) {
        acfg_log_errstr("%s: Opmode get failed\n", __func__);
        return A_STATUS_FAILED;
    }

    sprintf(cmd, "%s", WPA_WPS_CANCEL_CMD_PREFIX);
    if (opmode == ACFG_OPMODE_STA) {
        if(acfg_ctrl_req(ifname, cmd, strlen(cmd), 
                    NULL, &len, ACFG_OPMODE_STA) < 0){
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    cmd,
                    ifname);
            return A_STATUS_FAILED;
        }
    } else if (opmode == ACFG_OPMODE_HOSTAP) {
        if(acfg_ctrl_req(ifname, cmd, strlen(cmd), 
                    NULL, &len, ACFG_OPMODE_HOSTAP) < 0){
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    cmd,
                    ifname);
            return A_STATUS_FAILED;
        }
    }
    return A_STATUS_OK;
}

a_status_t acfg_set_hapd_wps_params(acfg_wlan_profile_vap_params_t *vap_params)
{
    a_char_t acfg_hapd_param_list[ACFG_MAX_HAPD_CONFIG_PARAM][512];
    int index = 0, i;
    a_char_t replybuf[255];
    a_uint32_t len = sizeof (replybuf);

    sprintf(acfg_hapd_param_list[index], "SET ssid %s", vap_params->ssid);
    index++;
    if (ACFG_IS_SEC_WEP(vap_params->security_params))
    {
        sprintf(acfg_hapd_param_list[index], "SET wep_key0 %s", 
                vap_params->security_params.wep_key0);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET wep_key1 %s", 
                vap_params->security_params.wep_key1);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET wep_key2 %s", 
                vap_params->security_params.wep_key2);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET wep_key3 %s", 
                vap_params->security_params.wep_key3);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET wep_default_key %d", 
                vap_params->security_params.wep_key_defidx);
        index++;
        sprintf(acfg_hapd_param_list[index], "SET auth_algs 0");
        index++;
    }
    if ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN) &&
            (vap_params->security_params.cipher_method ==
             ACFG_WLAN_PROFILE_CIPHER_METH_NONE))
    {
        sprintf(acfg_hapd_param_list[index], "SET auth_algs 0");
        index++;
    }
    sprintf(acfg_hapd_param_list[index], "SET wps_state %d",
            vap_params->security_params.wps_flag);
    index++;
    if (vap_params->security_params.wps_flag) {
        sprintf(acfg_hapd_param_list[index], "SET upnp_iface %s", "br0");
        index++;
        sprintf(acfg_hapd_param_list[index], "SET config_methods %s", 
                "\"label virtual_display virtual_push_button keypad\"");
        index++;
        sprintf(acfg_hapd_param_list[index], "SET device_typee %s", 
                "6-0050F204-1");
        index++;

    }

    for (i = 0; i < index ; i++) {
        if((acfg_ctrl_req (vap_params->vap_name,
                        acfg_hapd_param_list[i], 
                        strlen(acfg_hapd_param_list[i]),
                        replybuf, &len, 
                        ACFG_OPMODE_HOSTAP) < 0) ||
                strncmp (replybuf, "OK", strlen("OK"))){
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__, 
                    acfg_hapd_param_list[i],
                    vap_params->vap_name);
            return A_STATUS_FAILED;
        }
    }
    return A_STATUS_OK;
}

int 
acfg_get_open_wep_state(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params)
{
    if (ACFG_VAP_CMP_SSID(vap_params, cur_vap_params)) {
        return 2;		
    }
    return 0;
}

a_status_t
acfg_config_security(acfg_wlan_profile_vap_params_t *vap_params)
{
    acfg_opmode_t opmode;

    if (acfg_get_opmode(vap_params->vap_name, &opmode) != A_STATUS_OK) {
        acfg_log_errstr("%s: Opmode get failed for %s\n", __func__, 
                vap_params->vap_name);
        return A_STATUS_FAILED;
    }
    acfg_get_br_name(vap_params->vap_name, vap_params->bridge);

    if (acfg_ctrl_iface_present(vap_params->vap_name,
                opmode) == 1) 
    {
        a_char_t buffer[4096];
        a_char_t replybuf[255];
        a_uint32_t len = sizeof (replybuf);
        a_char_t acfg_hapd_param_list[32][512];
        int index = 0, i;

        if (opmode == ACFG_OPMODE_HOSTAP) {
            if(A_STATUS_OK != acfg_set_auth_open(vap_params, 
                        ACFG_SEC_DISABLE_SECURITY)){
                return A_STATUS_FAILED;
            }
            if(A_STATUS_OK != acfg_hostapd_delete_bss(vap_params)) {
                acfg_log_errstr("%s: Failed to DEL bss %s\n", __func__, 
                        vap_params->vap_name);
                return A_STATUS_FAILED;
            }
            sprintf(buffer, "ADD %s %s", vap_params->vap_name, ctrl_hapd);
            if((acfg_ctrl_req ((a_uint8_t *)ACFG_HAPD_GLOBAL_CTRL_IFACE,
                    buffer, strlen(buffer),
                    replybuf, &len,
                    ACFG_OPMODE_HOSTAP) < 0) ||
                    strncmp(replybuf, "OK", strlen("OK"))){
                acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                        buffer,
                        vap_params->vap_name);
                return A_STATUS_FAILED;
            }

            sprintf(acfg_hapd_param_list[index], "ENABLE");
            index++;

            if(A_STATUS_OK != acfg_set_hapd_config_params(vap_params)){
                acfg_log_errstr("%s: Failed to configure security for %s\n", __func__, 
                        vap_params->vap_name);
                return A_STATUS_FAILED;
            }
            for (i = 0; i < index; i++) {
                if((acfg_ctrl_req (vap_params->vap_name,
                                acfg_hapd_param_list[i],
                                strlen(acfg_hapd_param_list[i]),
                                replybuf, &len,
                                ACFG_OPMODE_HOSTAP) < 0) ||
                        strncmp(replybuf, "OK", strlen("OK"))){
                    acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                            acfg_hapd_param_list[i],
                            vap_params->vap_name);
                    return A_STATUS_FAILED;
                }
            }
        }
        wsupp_status_init = 1;
    }
    return A_STATUS_OK;
}

void
acfg_parse_wpa_supplicant(char value[][MAX_SIZE],
        acfg_wlan_profile_vap_params_t *vap_params)
{
    char *cpr = value[0];
    char *sec = value[1];
    char *gr_cpr = value[2];
    char *key_mgmt = value[3];

    if (!(strcmp(key_mgmt, "WPS"))) {
        vap_params->security_params.wps_flag = 1;
    } else {
        vap_params->security_params.wps_flag = 0;
    }

    if (!(strcmp(cpr, "TKIP")) && !(strcmp(sec, "WPA")))  {
        vap_params->security_params.sec_method = 
            ACFG_WLAN_PROFILE_SEC_METH_WPA;
        vap_params->security_params.cipher_method = 
            ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
    }
    if (!(strcmp(cpr, "CCMP")) &&
            !(strcmp(sec, "RSN")))  {
        vap_params->security_params.sec_method = 
            ACFG_WLAN_PROFILE_SEC_METH_WPA2;
        vap_params->security_params.cipher_method = 
            ACFG_WLAN_PROFILE_CIPHER_METH_AES;
    }
    if (!(strcmp(cpr, "CCMP TKIP")) && !(strcmp(sec, "WPA RSN")))  {
        vap_params->security_params.sec_method = 
            ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2;
        vap_params->security_params.cipher_method = 
            ACFG_WLAN_PROFILE_CIPHER_METH_TKIP | 
            ACFG_WLAN_PROFILE_CIPHER_METH_AES;
    }
    if (!(strcmp(cpr, "CCMP TKIP")) && !(strcmp(sec, "RSN")))  {
        vap_params->security_params.sec_method = 
            ACFG_WLAN_PROFILE_SEC_METH_WPA2;
        vap_params->security_params.cipher_method = 
            ACFG_WLAN_PROFILE_CIPHER_METH_TKIP | 
            ACFG_WLAN_PROFILE_CIPHER_METH_AES;
    }

    if (!(strcmp(gr_cpr, "TKIP"))) {
        vap_params->security_params.g_cipher_method = 
            ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
    }
    if (!(strcmp(gr_cpr, "CCMP"))) {
        vap_params->security_params.g_cipher_method = 
            ACFG_WLAN_PROFILE_CIPHER_METH_AES;
    }
    if (!(strcmp(gr_cpr, "CCMP TKIP"))) {
        vap_params->security_params.g_cipher_method = 
            ACFG_WLAN_PROFILE_CIPHER_METH_TKIP	| 
            ACFG_WLAN_PROFILE_CIPHER_METH_AES;
    }

}

a_status_t
acfg_wpa_supplicant_get(acfg_wlan_profile_vap_params_t *vap_params)
{
    struct sockaddr_un local;
    struct sockaddr_un remote;
    int s;
    char buf[MAX_SIZE], str[MAX_SIZE];
    char rcv_buf[SUPP_FIELD][MAX_SIZE];
    int len_local = sizeof(local);
    a_uint32_t len_remote = sizeof(remote);
    int bind_status = 0, send_status = 0, recv_status = 0;

    memset(rcv_buf, 0,sizeof(rcv_buf));
    memset(buf, 0, sizeof(buf));
    memset(&local, 0, sizeof(local));

    s = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (s < 0) {
        acfg_log_errstr("%s: socket failed: %s\n", __func__, strerror(errno));
        return A_STATUS_FAILED;
    }
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, "/tmp/testwpa");
    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    sprintf(buf, "%s%s%s","GET_NETWORK ",vap_params->vap_name, " pairwise");
    sprintf(str, "%s%s", "/var/run/wpa_supplicant/", vap_params->vap_name);
    memcpy(remote.sun_path, str, strlen(str) + 1);
    unlink("/tmp/testwpa");

    bind_status = bind(s, (struct sockaddr *)&local, (socklen_t)len_local);
    if (bind_status < 0) {
        acfg_log_errstr("%s: bind Failed: %s\n", __func__, strerror(errno));
        close(s);
        return A_STATUS_FAILED;
    }

    send_status = sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)&remote,
            len_remote);
    memset(buf, 0,sizeof(buf));
    if (send_status < 0) {
        acfg_log_errstr("%s: send failed: %s\n", __func__, strerror(errno));
        close(s);
        return A_STATUS_FAILED;
    }
    len_remote = sizeof(remote);
    recv_status = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&remote,
            &len_remote);
    if (recv_status < 0) {
        acfg_log_errstr("%s: receive failed: %s\n", __func__, strerror(errno));
        close(s);
        return A_STATUS_FAILED;
    }
    strcpy(rcv_buf[0], buf);
    memset(buf, 0, sizeof(buf));

    sprintf(buf, "%s%s%s","GET_NETWORK ",vap_params->vap_name, " proto");
    send_status = sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)&remote,
            len_remote);
    memset(buf, 0, sizeof(buf));
    if (send_status < 0) {
        acfg_log_errstr("%s: send failed: %s\n", __func__, strerror(errno));
        close(s);
        return A_STATUS_FAILED;
    }
    len_remote = sizeof(remote);
    recv_status = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&remote,
            &len_remote);
    if (recv_status < 0) {
        acfg_log_errstr("%s: receive failed: %s\n", __func__, strerror(errno));
        close(s);
        return A_STATUS_FAILED;
    }
    strcpy(rcv_buf[1], buf);
    memset(buf, 0, sizeof(buf));

    sprintf(buf, "%s%s%s","GET_NETWORK ",vap_params->vap_name, " group");
    send_status = sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)&remote,
            len_remote);
    memset(buf, 0, sizeof(buf));
    if (send_status < 0) {
        acfg_log_errstr("%s: send failed: %s\n", __func__, strerror(errno));
        close(s);
        return A_STATUS_FAILED;
    }
    len_remote = sizeof(remote);
    recv_status = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&remote,
            &len_remote);
    if (recv_status < 0) {
        acfg_log_errstr("%s: receive failed: %s\n", __func__, strerror(errno));
        close(s);
        return A_STATUS_FAILED;
    }
    strcpy(rcv_buf[2], buf);
    acfg_parse_wpa_supplicant(rcv_buf, vap_params);
    close(s);

    return A_STATUS_OK;
}

void
acfg_parse_hostapd(char *value, acfg_wlan_profile_vap_params_t *vap_params)
{
    int last;
    char *start, *end, *buf;
    buf = strdup(value);
    if (buf == NULL) {
        return;
    }
    start = buf;
    while (*start != '\0') {
        while (*start == ' ' || *start == '\t') {
            start++;
        }
        if (*start == '\0') {
            break;
        }
        end = start;
        while (*end != ' ' && *end != '\t' && *end != '\0' && *end != '\n') {
            end++;
        }
        last = *end == '\0';
        *end = '\0';
        if (strcmp (start, "wps_state=configured") == 0) {
            vap_params->security_params.wps_flag = 1;
        } else {
            vap_params->security_params.wps_flag = 0;
        }

        if (strcmp (start, "key_mgmt=WPA-PSK") == 0) {
            if (strstr (value, "wpa_pairwise_cipher=TKIP"))  {
                vap_params->security_params.sec_method = 
                    ACFG_WLAN_PROFILE_SEC_METH_WPA;
                vap_params->security_params.cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
                vap_params->security_params.g_cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
            } else if((strstr(value, "wpa_pairwise_cipher=CCMP TKIP") &&
                        strstr(value, "rsn_pairwise_cipher=CCMP TKIP")) != 0) {
                vap_params->security_params.sec_method = 
                    ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2;
                vap_params->security_params.g_cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
                vap_params->security_params.cipher_method = 
                    (ACFG_WLAN_PROFILE_CIPHER_METH_TKIP | 
                     ACFG_WLAN_PROFILE_CIPHER_METH_AES);
            } else if (strstr(value, "rsn_pairwise_cipher=CCMP TKIP")) {
                vap_params->security_params.sec_method = 
                    ACFG_WLAN_PROFILE_SEC_METH_WPA2;
                vap_params->security_params.g_cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
                vap_params->security_params.cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_TKIP | 
                    ACFG_WLAN_PROFILE_CIPHER_METH_AES;
            } else if (strstr(value, "rsn_pairwise_cipher=CCMP")) {
                vap_params->security_params.sec_method = 
                    ACFG_WLAN_PROFILE_SEC_METH_WPA2;
                vap_params->security_params.g_cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_AES;
                vap_params->security_params.cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_AES ;
            }
        }
        if (strcmp(start, "key_mgmt=WPA-EAP") == 0) {
            if (strstr(value, "wpa_pairwise_cipher=TKIP") != NULL)  {
                vap_params->security_params.sec_method = 
                    ACFG_WLAN_PROFILE_SEC_METH_WPA_EAP;
                vap_params->security_params.cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
                vap_params->security_params.g_cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
            } else if (strstr(value, "rsn_pairwise_cipher=CCMP TKIP")) {
                vap_params->security_params.sec_method = 
                    ACFG_WLAN_PROFILE_SEC_METH_WPA2_EAP;
                vap_params->security_params.cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_TKIP | 
                    ACFG_WLAN_PROFILE_CIPHER_METH_AES;
                vap_params->security_params.g_cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
            } else if (strstr(value, "rsn_pairwise_cipher=CCMP")) {
                vap_params->security_params.sec_method = 
                    ACFG_WLAN_PROFILE_SEC_METH_WPA2_EAP;
                vap_params->security_params.cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_AES;
                vap_params->security_params.g_cipher_method = 
                    ACFG_WLAN_PROFILE_CIPHER_METH_AES; 
            }
        }
        if (last) {
            break;
        }
        start = end + 1;
    }
}

a_status_t
acfg_hostapd_get(acfg_wlan_profile_vap_params_t *vap_params)
{
    struct sockaddr_un local;
    struct sockaddr_un remote;
    int s;
    char buf[MAX_SIZE], str[MAX_SIZE];
    int len_local = sizeof(local);
    a_uint32_t len_remote = sizeof(remote);
    memset(buf, 0, sizeof(buf));
    memset(&local, 0, sizeof(local));
    memset(&remote, 0, sizeof(remote));

    int bind_status = 0, send_status = 0, recv_status = 0;
    s = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (s < 0) {
        acfg_log_errstr("%s: socket failed: %s\n", __func__, strerror(errno));
        return A_STATUS_FAILED;
    }
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, "/tmp/testapp");
    remote.sun_family = AF_UNIX;
    strcpy(buf, "GET_CONFIG");
    sprintf(str, "%s%s","/var/run/hostapd/",vap_params->vap_name);
    if (!strcmp(buf, "GET_CONFIG")) {
        memcpy(remote.sun_path, str, strlen(str) + 1);
    }
    unlink("/tmp/testapp");

    bind_status = bind(s, (struct sockaddr *)&local, (socklen_t)len_local);
    if (bind_status < 0) {
        acfg_log_errstr("%s: bind Failed: %s\n", __func__, strerror(errno));
        close(s);
        return A_STATUS_FAILED;
    }

    send_status = sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)&remote,
            len_remote);
    if (send_status < 0) {
        acfg_log_errstr("%s: send failed: %s\n", __func__, strerror(errno));
        close(s);
        return A_STATUS_FAILED;
    }
    len_remote = sizeof(remote);
    recv_status = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&remote,
            &len_remote);
    if (recv_status < 0) {
        acfg_log_errstr("%s: receive failed: %s\n", __func__, strerror(errno));
        close(s);
        return A_STATUS_FAILED;
    }

    acfg_parse_hostapd(buf, vap_params);
    close(s);

    return A_STATUS_OK;
}
