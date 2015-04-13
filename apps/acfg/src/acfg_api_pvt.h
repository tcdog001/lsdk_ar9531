/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __ACFG_API_PVT_H
#define __ACFG_API_PVT_H

#include <a_base_types.h>
#include <acfg_api_types.h>
#include <acfg_api.h>
#include <acfg_drv_if.h>

#define ACFG_CONF_FILE "/etc/acfg_common.conf"
#define ACFG_APP_CTRL_IFACE "/tmp/acfg-app"

typedef struct {
    a_uint8_t new_vap_idx[ACFG_MAX_VAPS];
    a_uint8_t cur_vap_idx[ACFG_MAX_VAPS];
    a_uint8_t num_vaps;
} acfg_wlan_profile_vap_list_t;

typedef struct vap_list {
    char iface[16][ACFG_MAX_IFNAME];
    int num_iface;
} acfg_vap_list_t;


/** 
 * @brief Send Request Data in a OS specific way
 * 
 * @param hdl
 * @param req (Request Structure)
 * 
 * @return 
 */
a_status_t   
acfg_os_send_req(a_uint8_t  *ifname, acfg_os_req_t  *req);

a_status_t 
acfg_os_check_str(a_uint8_t *src, a_uint32_t maxlen);

a_uint32_t
acfg_os_strcpy(a_uint8_t  *dst, a_uint8_t *src, a_uint32_t  maxlen);

a_uint32_t
acfg_os_cmp_str(a_uint8_t *str1, a_uint8_t *str2, a_uint32_t maxlen) ;

a_status_t 
acfg_log(a_uint8_t *msg);

a_uint8_t
acfg_mhz2ieee(a_uint32_t);

a_status_t
acfg_hostapd_modify_bss(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params,
        a_int8_t *sec);

a_status_t
acfg_hostapd_delete_bss(acfg_wlan_profile_vap_params_t *vap_params);

a_status_t
acfg_hostapd_add_bss(acfg_wlan_profile_vap_params_t *vap_params, a_int8_t *sec);

a_status_t 
acfg_get_iface_list(acfg_vap_list_t *list, int *count);

int
acfg_get_ctrl_iface_path(char *filename, char *hapd_ctrl_iface_dir,
        char *wpa_supp_ctrl_iface_dir);
int acfg_ctrl_req(a_uint8_t *ifname, char *cmd, size_t cmd_len, a_char_t *replybuf, 
        a_uint32_t *reply_len, acfg_opmode_t opmode);
void
acfg_update_wps_config_file(a_uint8_t *ifname, char *prefix, char *data, int len);

void
acfg_update_wps_dev_config_file(acfg_wlan_profile_vap_params_t *vap_params, int force_update);

void acfg_set_wps_default_config(acfg_wlan_profile_vap_params_t *vap_params);

void acfg_set_hs_iw_vap_param(acfg_wlan_profile_vap_params_t *vap_params);

a_status_t 
acfg_wlan_iface_up(a_uint8_t  *ifname);

a_status_t
acfg_wlan_iface_down(a_uint8_t *ifname);

void        
acfg_rem_wps_config_file(a_uint8_t *ifname);

void acfg_reset_errstr(void);

void _acfg_log_errstr(const char *fmt, ...);
void _acfg_print(const char *fmt, ...);

//Prints all the debug messages
#define ACFG_DEBUG 0
//Prints only the error messages
#define ACFG_DEBUG_ERROR 0

#if ACFG_DEBUG
#undef ACFG_DEBUG_ERROR
#define ACFG_DEBUG_ERROR 1
#define acfg_print(fmt, ...) _acfg_print(fmt, ##__VA_ARGS__)
#else
#define acfg_print(fmt, ...) 
#endif

#if ACFG_DEBUG_ERROR
#define acfg_log_errstr(fmt, ...) _acfg_print(fmt, ##__VA_ARGS__)
#else
#define acfg_log_errstr(fmt, ...) _acfg_log_errstr(fmt, ##__VA_ARGS__)
#endif

#endif
