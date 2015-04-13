#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<unistd.h>
#include <string.h>
#include "acfg_config_file.h"
#include "acfg_api.h"
#include <acfg_drv_if.h>

struct acfg_radio_params radio_params[] = 
{
    {"hostapd_ctrl_interface",  OFFSET(acfg_wlan_profile_t, ctrl_hapd),
        ACFG_TYPE_STR},
    {"supplicant_ctrl_interface",  OFFSET(acfg_wlan_profile_t, ctrl_wpasupp),
        ACFG_TYPE_STR},
    {"radio_name",  OFFSET_RADIOPARAM(acfg_wlan_profile_t, 
            radio_name), ACFG_TYPE_STR},
    {"channel", OFFSET_RADIOPARAM(acfg_wlan_profile_t, chan), ACFG_TYPE_CHAR },
    {"freq", OFFSET_RADIOPARAM(acfg_wlan_profile_t, freq), ACFG_TYPE_INT},
    {"r_ampdu", OFFSET_RADIOPARAM(acfg_wlan_profile_t, ampdu), 
        ACFG_TYPE_STR},
    {"ampdu_size", OFFSET_RADIOPARAM(acfg_wlan_profile_t, ampdu_limit_bytes), 
        ACFG_TYPE_INT},
    {"ampdu_sub_frames", OFFSET_RADIOPARAM(acfg_wlan_profile_t, 
            ampdu_subframes), 
    ACFG_TYPE_CHAR},
    {"txpower", OFFSET_TXPOWER(acfg_wlan_profile_vap_params_t, val), 
        ACFG_TYPE_INT},
    {"countrycode", OFFSET_RADIOPARAM(acfg_wlan_profile_t, country_code), 
        ACFG_TYPE_INT16},
    {"radiomac", OFFSET_RADIOPARAM(acfg_wlan_profile_t, radio_mac), 
        ACFG_TYPE_STR},
    {"macreq_enabled", OFFSET_RADIOPARAM(acfg_wlan_profile_t, macreq_enabled),
        ACFG_TYPE_CHAR},
    {"aggr_burst", OFFSET_RADIOPARAM(acfg_wlan_profile_t, aggr_burst),
        ACFG_TYPE_CHAR},
    {"aggr_burst_dur", OFFSET_RADIOPARAM(acfg_wlan_profile_t, aggr_burst_dur),
        ACFG_TYPE_INT}, 
    {"vap_name", OFFSET(acfg_wlan_profile_vap_params_t, vap_name), 
        ACFG_TYPE_STR},
    {"opmode", OFFSET(acfg_wlan_profile_vap_params_t, opmode), 
        ACFG_TYPE_INT},
    {"vapid", OFFSET(acfg_wlan_profile_vap_params_t, vapid), 
        ACFG_TYPE_INT},
    {"phymode", OFFSET(acfg_wlan_profile_vap_params_t, phymode), 
        ACFG_TYPE_INT},
    {"vap_ampdu", OFFSET(acfg_wlan_profile_vap_params_t, ampdu), 
        ACFG_TYPE_STR},
    {"amsdu", OFFSET(acfg_wlan_profile_vap_params_t, amsdu),
        ACFG_TYPE_INT},
    {"max_clients", OFFSET(acfg_wlan_profile_vap_params_t, max_clients),
        ACFG_TYPE_INT},
    {"ssid", OFFSET(acfg_wlan_profile_vap_params_t, ssid), 
        ACFG_TYPE_STR},
    {"bitrate", OFFSET(acfg_wlan_profile_vap_params_t, bitrate), 
        ACFG_TYPE_INT},
    {"rate", OFFSET(acfg_wlan_profile_vap_params_t, rate), 
        ACFG_TYPE_STR},
    {"retries", OFFSET(acfg_wlan_profile_vap_params_t, retries), 
        ACFG_TYPE_INT},
    {"beaconint", OFFSET(acfg_wlan_profile_vap_params_t, beacon_interval), 
        ACFG_TYPE_INT},
    {"rtsthresh", OFFSET_RTSTHR(acfg_wlan_profile_vap_params_t, val), 
        ACFG_TYPE_INT},
    {"fragthresh", OFFSET_FRAGTHR(acfg_wlan_profile_vap_params_t, val), 
        ACFG_TYPE_INT},
    {"vapmac", OFFSET(acfg_wlan_profile_vap_params_t, vap_mac), 
        ACFG_TYPE_STR},
    {"pureg", OFFSET(acfg_wlan_profile_vap_params_t, pureg),
        ACFG_TYPE_INT},
    {"hidessid", OFFSET(acfg_wlan_profile_vap_params_t, hide_ssid),
        ACFG_TYPE_INT},
    {"doth", OFFSET(acfg_wlan_profile_vap_params_t, doth),
        ACFG_TYPE_INT},
    {"puren", OFFSET(acfg_wlan_profile_vap_params_t, puren),
        ACFG_TYPE_INT},
    {"ht2040coext", OFFSET(acfg_wlan_profile_vap_params_t, coext),
        ACFG_TYPE_CHAR},
    {"client_isolation", OFFSET(acfg_wlan_profile_vap_params_t, 
            client_isolation),
    ACFG_TYPE_INT},
    {"uapsd", OFFSET(acfg_wlan_profile_vap_params_t, uapsd),
        ACFG_TYPE_INT},
    {"shortgi", OFFSET(acfg_wlan_profile_vap_params_t, shortgi),
        ACFG_TYPE_CHAR},
    {"vlanid", OFFSET(acfg_wlan_profile_vap_params_t, vlanid),
        ACFG_TYPE_INT},
    {"authtype", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, sec_method), 
        ACFG_TYPE_CHAR},
    {"cipher", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, cipher_method), 
        ACFG_TYPE_CHAR},
    {"group_cipher", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, 
            g_cipher_method), ACFG_TYPE_CHAR},
    {"psk", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, psk), 
        ACFG_TYPE_STR},
    {"wep_key0", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wep_key0), 
        ACFG_TYPE_STR},
    {"wep_key1", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wep_key1), 
        ACFG_TYPE_STR},
    {"wep_key2", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wep_key2), 
        ACFG_TYPE_STR},
    {"wep_key3", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wep_key3), 
        ACFG_TYPE_STR},
    {"wep_key_default", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, 
            wep_key_defidx), ACFG_TYPE_CHAR},
    {"wps_pin", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wps_pin), 
        ACFG_TYPE_INT},
    {"wps_flag", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wps_flag),
        ACFG_TYPE_CHAR},
    {"wps_manufacturer",  OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wps_manufacturer),
        ACFG_TYPE_STR},
    {"wps_model_name",  OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wps_model_name),
        ACFG_TYPE_STR},
    {"wps_model_number",  OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wps_model_number),
        ACFG_TYPE_STR},
    {"wps_serial_number",  OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wps_serial_number),
        ACFG_TYPE_STR},
    {"wps_device_type",  OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wps_device_type),
        ACFG_TYPE_STR},
    {"wps_config_methods",  OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, wps_config_methods),
        ACFG_TYPE_STR},
    {"wps_upnp_iface", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, 
            wps_upnp_iface),
    ACFG_TYPE_STR},
    {"wps_friendly_name", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, 
            wps_friendly_name),
    ACFG_TYPE_STR},
    {"wps_man_url", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, 
            wps_man_url),
    ACFG_TYPE_STR},
    {"wps_model_desc", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, 
            wps_model_desc),
    ACFG_TYPE_STR},
    {"wps_upc", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, 
            wps_upc),
    ACFG_TYPE_STR},
    {"wps_pbc_in_m1", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, 
            wps_pbc_in_m1),
    ACFG_TYPE_INT},
    {"wps_device_name", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t, 
            wps_device_name),
    ACFG_TYPE_STR},
    {"wps_rf_bands", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t,
            wps_rf_bands),
    ACFG_TYPE_STR},
    {"eap_type", OFFSET_EAPPARAM(acfg_wlan_profile_vap_params_t, eap_type), 
        ACFG_TYPE_CHAR},
    {"eap_identity", OFFSET_EAPPARAM(acfg_wlan_profile_vap_params_t, 
            identity), 
    ACFG_TYPE_STR},
    {"eap_password", OFFSET_EAPPARAM(acfg_wlan_profile_vap_params_t, 
            password), 
    ACFG_TYPE_STR},
    {"ca_cert_path", OFFSET_EAPPARAM(acfg_wlan_profile_vap_params_t, ca_cert), 
        ACFG_TYPE_STR},
    {"client_cert_path", OFFSET_EAPPARAM(acfg_wlan_profile_vap_params_t, 
            client_cert), 
    ACFG_TYPE_STR},
    {"private_key_path", OFFSET_EAPPARAM(acfg_wlan_profile_vap_params_t, 
            private_key), 
    ACFG_TYPE_STR},
    {"private_key_password", OFFSET_EAPPARAM(acfg_wlan_profile_vap_params_t, 
            private_key_passwd), 
    ACFG_TYPE_STR},
    {"radius_retry_primary_interval", OFFSET_SECPARAM(acfg_wlan_profile_vap_params_t,
            radius_retry_primary_interval),
    ACFG_TYPE_INT},
    {"pri_radius_ip", OFFSET_PRI_RADIUSPARAM(acfg_wlan_profile_vap_params_t, 
            radius_ip), 
    ACFG_TYPE_STR},
    {"pri_radius_port", OFFSET_PRI_RADIUSPARAM(acfg_wlan_profile_vap_params_t, 
            radius_port), 
    ACFG_TYPE_INT},
    {"pri_radius_sharedsecret", OFFSET_PRI_RADIUSPARAM(acfg_wlan_profile_vap_params_t, 
            shared_secret), 
    ACFG_TYPE_STR},
    {"sec_1_radius_ip", OFFSET_SEC1_RADIUSPARAM(acfg_wlan_profile_vap_params_t,
            radius_ip),
    ACFG_TYPE_STR},
    {"sec_1_radius_port", OFFSET_SEC1_RADIUSPARAM(acfg_wlan_profile_vap_params_t,
            radius_port),
    ACFG_TYPE_INT},
    {"sec_1_radius_sharedsecret", OFFSET_SEC1_RADIUSPARAM(acfg_wlan_profile_vap_params_t,
            shared_secret),
    ACFG_TYPE_STR},
    {"sec_2_radius_ip", OFFSET_SEC2_RADIUSPARAM(acfg_wlan_profile_vap_params_t,
            radius_ip),
    ACFG_TYPE_STR},
    {"sec_2_radius_port", OFFSET_SEC2_RADIUSPARAM(acfg_wlan_profile_vap_params_t,
            radius_port),
    ACFG_TYPE_INT},
    {"sec_2_radius_sharedsecret", OFFSET_SEC2_RADIUSPARAM(acfg_wlan_profile_vap_params_t,
            shared_secret),
    ACFG_TYPE_STR},
    {"pri_acct_ip", OFFSET_PRI_ACCTPARAM(acfg_wlan_profile_vap_params_t,
            acct_ip),
    ACFG_TYPE_STR},
    {"pri_acct_port", OFFSET_PRI_ACCTPARAM(acfg_wlan_profile_vap_params_t,
            acct_port),
    ACFG_TYPE_INT},
    {"pri_acct_sharedsecret", OFFSET_PRI_ACCTPARAM(acfg_wlan_profile_vap_params_t,
            shared_secret),
    ACFG_TYPE_STR},
    {"sec_1_acct_ip", OFFSET_SEC1_ACCTPARAM(acfg_wlan_profile_vap_params_t,
            acct_ip),
    ACFG_TYPE_STR},
    {"sec_1_acct_port", OFFSET_SEC1_ACCTPARAM(acfg_wlan_profile_vap_params_t,
            acct_port),
    ACFG_TYPE_INT},
    {"sec_1_acct_sharedsecret", OFFSET_SEC1_ACCTPARAM(acfg_wlan_profile_vap_params_t,
            shared_secret),
    ACFG_TYPE_STR},
    {"sec_2_acct_ip", OFFSET_SEC2_ACCTPARAM(acfg_wlan_profile_vap_params_t,
            acct_ip),
    ACFG_TYPE_STR},
    {"sec_2_acct_port", OFFSET_SEC2_ACCTPARAM(acfg_wlan_profile_vap_params_t,
            acct_port),
    ACFG_TYPE_INT},
    {"sec_2_acct_sharedsecret", OFFSET_SEC2_ACCTPARAM(acfg_wlan_profile_vap_params_t,
            shared_secret),
    ACFG_TYPE_STR},
    {"hs_enabled", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hs_enabled),
    ACFG_TYPE_CHAR},
    {"iw_enabled", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            iw_enabled),
    ACFG_TYPE_CHAR},
    {"iw_network_type", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            network_type),
    ACFG_TYPE_CHAR},
    {"iw_internet", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            internet),
    ACFG_TYPE_CHAR},
    {"iw_asra", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            asra),
    ACFG_TYPE_CHAR},
    {"iw_esr", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            esr),
    ACFG_TYPE_CHAR},
    {"iw_uesa", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            uesa),
    ACFG_TYPE_CHAR},
    {"iw_venue_group", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            venue_group),
    ACFG_TYPE_CHAR},
    {"iw_venue_type", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            venue_type),
    ACFG_TYPE_CHAR},
    {"iw_hessid", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hessid),
    ACFG_TYPE_STR},
    {"iw_roaming_consortium", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            roaming_consortium),
    ACFG_TYPE_STR},
    {"iw_roaming_consortium2", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            roaming_consortium2),
    ACFG_TYPE_STR},
    {"iw_venue_name", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            venue_name),
    ACFG_TYPE_STR},
    {"manage_p2p", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            manage_p2p),
    ACFG_TYPE_CHAR},
    {"disable_dgaf", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            disable_dgaf),
    ACFG_TYPE_CHAR},
    {"hs20_venue_name", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hs20_venue_name),
    ACFG_TYPE_STR},
    {"hs20_network_auth_type", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hs20_network_auth_type),
    ACFG_TYPE_STR},
    {"hs20_ipaddr_type_availability", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hs20_ipaddr_type_availability),
    ACFG_TYPE_STR},
    {"hs20_nai_realm_list", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hs20_nai_realm_list),
    ACFG_TYPE_STR},
    {"hs20_3gpp_cellular_network", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hs20_3gpp_cellular_network),
    ACFG_TYPE_STR},
    {"hs20_domain_name_list", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hs20_domain_name_list),
    ACFG_TYPE_STR},
    {"hs20_operator_friendly_name", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hs20_operator_friendly_name),
    ACFG_TYPE_STR},
    {"hs20_wan_metrics", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hs20_wan_metrics),
    ACFG_TYPE_STR},
    {"hs20_connection_capability", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hs20_connection_capability),
    ACFG_TYPE_STR},
    {"hs20_operating_class", OFFSET_HSPARAM(acfg_wlan_profile_vap_params_t,
            hs20_operating_class),
    ACFG_TYPE_STR},
    {"acl_node", OFFSET_ACLPARAM(acfg_wlan_profile_vap_params_t, 
            acfg_acl_node_list), 
    ACFG_TYPE_ACL},
    {"acl_policy", OFFSET_ACLPARAM(acfg_wlan_profile_vap_params_t, 
            node_acl), 
    ACFG_TYPE_INT},
    {"wds_enabled", OFFSET_WDSPARAM(acfg_wlan_profile_vap_params_t, 
            enabled), 
    ACFG_TYPE_CHAR},
    {"wds_addr", OFFSET_WDSPARAM(acfg_wlan_profile_vap_params_t, 
            wds_addr), 
    ACFG_TYPE_STR},
    {"wds_flag", OFFSET_WDSPARAM(acfg_wlan_profile_vap_params_t,
            wds_flags),
    ACFG_TYPE_INT},
    {"bridge", OFFSET(acfg_wlan_profile_vap_params_t, bridge),
        ACFG_TYPE_STR},
    {"vendor_param_print", OFFSET_VENDORPARAM(acfg_wlan_profile_vap_params_t),
        ACFG_TYPE_STR},
    {"vendor_param_int", OFFSET_VENDORPARAM(acfg_wlan_profile_vap_params_t),
        ACFG_TYPE_INT},
    {"vendor_param_mac", OFFSET_VENDORPARAM(acfg_wlan_profile_vap_params_t),
        ACFG_TYPE_MACADDR},
};

/* vendors can fill this table */
const acfg_vendor_param_cmd_map_t acfg_vendor_param_cmd_mapping[] = {
    /*=====================================================
      vendor param name           command id              
      /=====================================================*/ 
    {"vendor_param_print", ACFG_VENDOR_PARAM_CMD_PRINT},
    {"vendor_param_int",   ACFG_VENDOR_PARAM_CMD_INT},
    {"vendor_param_mac",   ACFG_VENDOR_PARAM_CMD_MAC},
};

void acfg_set_profile_param(void *buf, 
        a_uint8_t *val1, 
        a_uint8_t type)
{
    char *val = (char *)val1;

    switch (type) {
        case ACFG_TYPE_INT:
            *(int *)buf = atol(val);
            break;
        case ACFG_TYPE_CHAR:
            *(char *)buf = atoi(val);
            break;
        case ACFG_TYPE_INT16:
            *(a_uint16_t *)buf = atoi(val);
            break;
        case ACFG_TYPE_MACADDR:
            acfg_mac_str_to_octet((a_uint8_t *)val, buf);	
            break;
        case ACFG_TYPE_STR:
            strcpy((char *)buf, val);
            break;
        case ACFG_TYPE_ACL:
            {
                a_uint8_t *mac_str, *pos;
                a_uint8_t i = 0;


                mac_str = pos = (a_uint8_t *) val;
                while ((pos) && i < ACFG_MAX_ACL_NODE) {
                    if ((*pos == ' ') || (*pos == '\t')) {
                        *pos = '\0';
                        pos++;
                        acfg_mac_str_to_octet(mac_str, buf);
                        buf += ACFG_MACADDR_LEN;
                        mac_str = pos;
                        i++;
                    }
                    if (*pos == '\n' || *pos == '\0') {
                        acfg_mac_str_to_octet(mac_str, buf);
                        buf += ACFG_MACADDR_LEN;
                        i++;
                        break;
                    }
                    pos++;
                }
            }
    }
}

int acfg_read_file(char *filename, acfg_wlan_profile_t *profile)
{
    FILE *fp;
    char buf[512];
    a_uint8_t *pos  = 0;
    a_uint8_t i = 0, vap_index = 0, num_node, vendor_param_index = 0; 
    a_uint8_t nai_realm_data_index = 0; 
    a_uint32_t offset = 0;
    a_uint8_t zero_mac[ACFG_MACADDR_LEN] = {0,0,0,0,0,0};
    acfg_wlan_profile_vap_params_t *vap_params;
    acfg_wlan_profile_node_params_t *node_params;

    if (filename == NULL) {
        return -1;
    }
    fp = fopen(filename, "r");
    if (fp == NULL) {
        return -1;
    }
    while (fgets(buf, sizeof (buf), fp)) {
        if(buf[0] == '#') {
            continue;
        }
        pos = (a_uint8_t *)buf;
        while (*pos != '\0') {
            if (*pos == '\n') {
                *pos = '\0';
                break;
            }
            pos++;
        }
        pos = (a_uint8_t *) strchr(buf, '=');
        if (pos == NULL) {
            continue;
        }
        *pos = '\0';
        pos++;
        if (strcmp(buf, "vap_name") == 0) {
            vap_index++;
            /* set the vapid to auto, so that MAC addr allocated automatically */
            profile->vap_params[vap_index - 1].vapid = VAP_ID_AUTO;
            vendor_param_index = 0;
            nai_realm_data_index = 0;
        }
        for (i = 0; i < (sizeof (radio_params) / 
                    sizeof (struct acfg_radio_params)); i++) {
            if (strcmp(buf, (char *)radio_params[i].name) == 0) {
                if (vap_index == 0) {
                    offset =  radio_params[i].offset;
                } else {
                    if(radio_params[i].offset == OFFSET_VENDORPARAM(acfg_wlan_profile_vap_params_t))
                    {
                        /* This is a vendor parameter "no name!", so handle differently */
                        if(vendor_param_index < ACFG_MAX_VENDOR_PARAMS)
                        {
                            int j;
                            acfg_wlan_profile_vendor_param_t *vendor_param = 
                                &profile->vap_params[vap_index - 1].vendor_param[vendor_param_index];

                            if(radio_params[i].type == ACFG_TYPE_INT)
                            {
                                vendor_param->len = sizeof(a_uint32_t);
                                vendor_param->type = ACFG_TYPE_INT;
                            }
                            else if(radio_params[i].type == ACFG_TYPE_STR)
                            {
                                vendor_param->len = strlen((char *)pos) + 1;
                                vendor_param->type = ACFG_TYPE_STR;
                            }
                            else if(radio_params[i].type == ACFG_TYPE_MACADDR)
                            {
                                vendor_param->len = ACFG_MACADDR_LEN;
                                vendor_param->type = ACFG_TYPE_MACADDR;
                            }
                            for(j = 0; j < (sizeof(acfg_vendor_param_cmd_mapping) /
                                        sizeof(acfg_vendor_param_cmd_map_t)); j++)
                            {
                                if(strcmp((char *)acfg_vendor_param_cmd_mapping[j].name, buf) == 0)
                                    vendor_param->cmd = acfg_vendor_param_cmd_mapping[j].cmd;
                            }
                            offset = (a_uint32_t)((a_uint8_t *)&vendor_param->data - (a_uint8_t *)profile);
                            profile->vap_params[vap_index-1].num_vendor_params = ++vendor_param_index;
                        }
                    }
                    else if(strcmp(buf, "hs20_nai_realm_list") == 0)
                    {
                        offset = (a_uint32_t)((a_uint8_t *)&profile->vap_params[vap_index - 1].security_params.hs_iw_param.hs20_nai_realm_list[nai_realm_data_index] - 
                                (a_uint8_t *)profile);
                        profile->vap_params[vap_index-1].num_nai_realm_data = ++nai_realm_data_index;

                    }
                    else
                    {
                        offset = OFFSET(acfg_wlan_profile_t, vap_params[0]);
                        offset += (vap_index - 1) * 
                            sizeof (acfg_wlan_profile_vap_params_t); 
                        offset += radio_params[i].offset;
                    }
                }
                acfg_set_profile_param(((a_uint8_t *)profile) + 
                        offset, 
                        pos, radio_params[i].type);	
                break;	
            }
        }
    }
    fclose(fp);
    profile->num_vaps = vap_index;
    for (i = 0; i < profile->num_vaps; i++) {
        vap_params = &profile->vap_params[i];
        node_params = &vap_params->node_params;
        for (num_node = 0; num_node < ACFG_MAX_ACL_NODE; num_node++) {
            if (!memcmp(node_params->acfg_acl_node_list[num_node],
                        zero_mac, ACFG_MACADDR_LEN))
            {

                break;
            }
        }
        node_params->num_node = num_node;
    }
    if (strlen((char *)profile->radio_params.radio_name) == 0) {
        return -1;
    }
    return 0;
}
