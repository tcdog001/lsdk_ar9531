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
#include <errno.h>
#include <net/if_arp.h>
#include <math.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <acfg_api_pvt.h>
#include <acfg_security.h>
#include <acfg_misc.h>
#include <acfg_api_event.h>
#include <linux/un.h>

#include <appbr_if.h>
#include <acfg_wireless.h>

//#define LINUX_PVT_WIOCTL  (SIOCDEVPRIVATE + 1)
#define SIOCWANDEV  0x894A
#define LINUX_PVT_WIOCTL  (SIOCWANDEV)

extern appbr_status_t appbr_if_send_cmd_remote(a_uint32_t app_id, void *buf, 
        a_uint32_t size);

extern appbr_status_t appbr_if_wait_for_response(void *buf, a_uint32_t size, 
        a_uint32_t timeout);

extern a_status_t acfg_wpa_supplicant_get(acfg_wlan_profile_vap_params_t *vap_params);

extern a_status_t acfg_hostapd_get(acfg_wlan_profile_vap_params_t *vap_params);

extern void acfg_send_interface_event(char *event, int len);

a_status_t
acfg_get_err_status(void)
{
    switch (errno)  {
        case ENOENT:        return A_STATUS_ENOENT;
        case ENOMEM:        return A_STATUS_ENOMEM;
        case EINVAL:        return A_STATUS_EINVAL;
        case EINPROGRESS:   return A_STATUS_EINPROGRESS;
        case EBUSY:         return A_STATUS_EBUSY;
        case E2BIG:         return A_STATUS_E2BIG;
        case ENXIO:         return A_STATUS_ENXIO;
        case EFAULT:        return A_STATUS_EFAULT;
        case EIO:           return A_STATUS_EIO;
        case EEXIST:        return A_STATUS_EEXIST;
        case ENETDOWN:      return A_STATUS_ENETDOWN;
        case EADDRNOTAVAIL: return A_STATUS_EADDRNOTAVAIL;
        case ENETRESET:     return A_STATUS_ENETRESET;
        case EOPNOTSUPP:    return A_STATUS_ENOTSUPP;
        default:            return A_STATUS_FAILED;
    }
}

a_status_t 
acfg_os_send_req(a_uint8_t *ifname, acfg_os_req_t  *req)
{
    struct ifreq ifr;
    int s;
    a_status_t   status = A_STATUS_OK;
    
    memset(&ifr, 0, sizeof(struct ifreq));

    strncpy(ifr.ifr_name, (const char *)ifname, 
            strlen((const char *)ifname));

    ifr.ifr_data = (__caddr_t)req;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s < 0) {
        status = A_STATUS_EBUSY;
        goto fail;
    }
    if (ioctl (s, LINUX_PVT_WIOCTL, &ifr) < 0) 
    {
        status = acfg_get_err_status();
        acfg_log_errstr("%s: IOCTL failed (cmd=%d status=%d)\n", __func__, 
                req->cmd, 
                status);
    }

    close(s);

fail:
    return status;
}

/**
 * @brief  Initialize interface for device-less configurations
 *
 * @return
 */
a_status_t acfg_dl_init()
{

    a_status_t ret_status = A_STATUS_FAILED;

    ret_status = appbr_if_open_dl_conn(APPBR_ACFG);
    if(ret_status != A_STATUS_OK)
        goto out;

    ret_status = appbr_if_open_ul_conn(APPBR_ACFG);
    if(ret_status != A_STATUS_OK)
        goto out;

out:
    return  ret_status;
}


/** 
 * @brief Send remote command through Appbr interface
 * 
 * @param cmd_id     command identifier    
 * @param cmd_buf    points to command buffer
 * @param size       size of command
 * @param resp_buf   points to response buffer
 * @param node_id    0: send to kernel & uspace, 1: send to uspace only  
 * 
 * @return 
 */
static appbr_status_t 
acfg_dl_os_send_req(a_uint8_t cmd_id, void  *cmd_buf, a_uint16_t  size, 
        void  *resp_buf, a_uint8_t   node_id, a_uint8_t timeout)
{
    appbr_status_t resp_code = APPBR_STAT_OK;
    struct acfg_dl_cfg_hdr *cmd_hdr = (struct acfg_dl_cfg_hdr *) cmd_buf;

    /**< fill command characteristics */
    cmd_hdr->data[0] |= ACFG_CMD_REQ;

    cmd_hdr->data[0] |= node_id;


    if (resp_buf)
    {
        /**  timeout is required for commands with response */
        if (timeout == 0)
            return APPBR_STAT_ENORESPACK;
        else
            cmd_hdr->data[0] |= ACFG_RESP_REQD;
    }		

    cmd_hdr->data[1] = cmd_id;

    *((a_uint16_t *)&cmd_hdr->data[2]) = size;

    /**  nd command to remote end via appbr */
    resp_code = appbr_if_send_cmd_remote (APPBR_ACFG, cmd_buf, size);

    if (resp_code != APPBR_STAT_OK)
        return resp_code;

    if (resp_buf)
    {
        /* block for acknowledgement/ response */
        resp_code = appbr_if_wait_for_response(resp_buf,size, timeout);
    }

    return resp_code;
}


/** 
 * @brief Create a VLAN Group
 * 
 * @param vlanid        -   vlan id for the group
 * 
 * @return 
 */
a_status_t
acfg_vlgrp_create(a_uint8_t *vlan_id)
{
    a_status_t      status = A_STATUS_FAILED;

    acfg_vlangrp_info_t     vlgrp_info;

    /* acfg_os_strcpy(&vlgrp_info.vlan_id[0], vlan_id, ACFG_MAX_VIDNAME); */
    strncpy(vlgrp_info.vlan_id, (char *) vlan_id, ACFG_MAX_VIDNAME);

    status =    acfg_dl_os_send_req(ACFG_DL_VLGRP_CREATE, &vlgrp_info, 
            sizeof(struct acfg_vlangrp_info),
            NULL, ACFG_DL_NODE_REMOTE, 0);
    return status;
}

a_status_t
acfg_vlgrp_delete(a_uint8_t *vlan_id)
{
    a_status_t      status = A_STATUS_FAILED;

    acfg_vlangrp_info_t     vlgrp_info;

    /* acfg_os_strcpy(&vlgrp_info.vlan_id[0], vlan_id, ACFG_MAX_VIDNAME); */
    strncpy(vlgrp_info.vlan_id, (char *) vlan_id, ACFG_MAX_VIDNAME);

    status =    acfg_dl_os_send_req(ACFG_DL_VLGRP_DELETE, &vlgrp_info, 
            sizeof(struct acfg_vlangrp_info),
            NULL, ACFG_DL_NODE_REMOTE, 0);
    return status;
}
/** 
 * @brief  Add VAP to a VLAN group
 * 
 * @param vlanid
 * 
 * @return 
 */
a_status_t
acfg_vlgrp_addvap(a_uint8_t *vlan_id, a_uint8_t *vap_name)
{
    a_status_t      status = A_STATUS_FAILED;

    acfg_vlangrp_info_t     vlgrp_info;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    /* acfg_os_strcpy((a_uint8_t *) vlgrp_info.vlan_id, vlan_id, ACFG_MAX_VIDNAME); */
    /* acfg_os_strcpy((a_uint8_t *) vlgrp_info.if_name, vap_name, ACFG_MAX_IFNAME); */

    strncpy(vlgrp_info.vlan_id, (char *) vlan_id, ACFG_MAX_VIDNAME);
    strncpy(vlgrp_info.if_name, (char *) vap_name, ACFG_MAX_IFNAME);


    status =    acfg_dl_os_send_req(ACFG_DL_VLGRP_ADDVAP, &vlgrp_info, 
            sizeof(struct acfg_vlangrp_info),
            NULL, ACFG_DL_NODE_REMOTE, 0);
    return status;

}

/** 
 * @brief Remove a VAP from a VLAN group
 * 
 * @param vlanid
 * @param vap_name
 * 
 * @return 
 */
a_status_t
acfg_vlgrp_delvap(a_uint8_t *vlan_id, a_uint8_t *vap_name)
{
    a_status_t      status = A_STATUS_FAILED;

    acfg_vlangrp_info_t     vlgrp_info;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    /* acfg_os_strcpy(&vlgrp_info.if_name[0], vap_name, ACFG_MAX_IFNAME); */
    /* acfg_os_strcpy(&vlgrp_info.vlan_id[0], vlan_id, ACFG_MAX_VIDNAME); */

    strncpy(vlgrp_info.vlan_id, (char *) vlan_id, ACFG_MAX_VIDNAME);
    strncpy(vlgrp_info.if_name, (char *) vap_name, ACFG_MAX_IFNAME);

    status =    acfg_dl_os_send_req(ACFG_DL_VLGRP_DELVAP, &vlgrp_info, 
            sizeof(struct acfg_vlangrp_info),
            NULL, ACFG_DL_NODE_REMOTE, 0);
    return status;
}

/** 
 * @brief Check whether string crossed max limit
 * 
 * @param src
 * @param maxlen
 * 
 * @return 
 */
a_status_t 
acfg_os_check_str(a_uint8_t *src, a_uint32_t maxlen)
{
    return(strnlen((const char *)src, maxlen) >= maxlen);
}

/** 
 * @brief Compare two strings
 * 
 * @param str1
 * @param str2
 * @param maxlen
 * 
 * @return 0 if strings are same. 
 *         Non zero otherwise.
 */
a_uint32_t
acfg_os_cmp_str(a_uint8_t *str1, a_uint8_t *str2, a_uint32_t maxlen)
{
    return(strncmp((const char *)str1, (const char *)str2, maxlen));
}


/** 
 * @brief Copy the dst string into the src
 * 
 * @param src (the source string)
 * @param dst (destination string)
 * @param maxlen (the maximum length of dest buf)
 *
 * @note It's assumed that the destination string is
 *       zero'ed
 */
a_uint32_t
acfg_os_strcpy(a_uint8_t  *dst, a_uint8_t *src, a_uint32_t  maxlen)
{
    a_uint32_t  len;

    len = acfg_min(strnlen((const char *)src, maxlen), maxlen); 

    strncpy((char *)dst, (const char *)src, len);

    return len;
}


/*
 *  Public API's
 */


/** 
 * @brief Create VAP
 * 
 * @param wifi_name
 * @param vap_name
 * @param mode
 * 
 * @return 
 */
a_status_t   
acfg_create_vap(a_uint8_t             *wifi_name, 
        a_uint8_t             *vap_name, 
        acfg_opmode_t          mode, 
        a_int32_t               vapid,
        acfg_vapinfo_flags_t   flags)
{
    a_status_t      status = A_STATUS_FAILED;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_CREATE_VAP}; 
    acfg_vapinfo_t    *ptr;

    ptr     = &req.data.vap_info;

    if (acfg_os_check_str(wifi_name, ACFG_MAX_IFNAME))
        return A_STATUS_EINVAL;

    acfg_os_strcpy(ptr->icp_name, vap_name, ACFG_MAX_IFNAME);

    ptr->icp_opmode    = mode;
    ptr->icp_flags     = flags;
    ptr->icp_vapid     = vapid;

    status = acfg_os_send_req(wifi_name, &req);

    return status;
}


/** 
 * @brief Delete VAP
 * 
 * @param wifi_name
 * @param vap_name
 * 
 * @return 
 */
a_status_t   
acfg_delete_vap(a_uint8_t *wifi_name, 
        a_uint8_t *vap_name)
{
    a_status_t      status = A_STATUS_FAILED;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_DELETE_VAP}; 
    acfg_vapinfo_t    *ptr;

    if (acfg_os_check_str(wifi_name, ACFG_MAX_IFNAME) 
            || acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_EINVAL;

    ptr     = &req.data.vap_info;

    acfg_os_strcpy(ptr->icp_name, vap_name, ACFG_MAX_IFNAME);

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}



/** 
 * @brief Is the VAP local or remote
 * 
 * @param vap_name
 * 
 * @return 
 */
a_status_t   
acfg_is_offload_vap(a_uint8_t *vap_name)
{
    a_status_t      status = A_STATUS_FAILED;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_IS_OFFLOAD_VAP}; 
    //acfg_vapinfo_t    *ptr;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_EINVAL;

    //ptr     = &req.data.vap_info;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}



/** 
 * @brief Set the SSID
 * 
 * @param vap_name
 * @param ssid
 * 
 * @return 
 */
a_status_t
acfg_set_ssid(a_uint8_t     *vap_name, acfg_ssid_t  *ssid)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_SSID}; 
    acfg_ssid_t        *ptr;

    ptr     = &req.data.ssid;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    ptr->len = acfg_os_strcpy((a_uint8_t *)ptr->name, (a_uint8_t *)ssid->name, ACFG_MAX_SSID_LEN);

    status = acfg_os_send_req(vap_name, &req);
    return status;    
}

/** 
 * @brief Get the SSID
 * 
 * @param vap_name
 * @param ssid
 */
a_status_t
acfg_get_ssid(a_uint8_t  *vap_name, acfg_ssid_t  *ssid)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_SSID};
    acfg_ssid_t        *ptr;

    ptr = &req.data.ssid;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    status = acfg_os_send_req(vap_name, &req);

    if (status == A_STATUS_OK)
        ssid->len = acfg_os_strcpy((a_uint8_t *)ssid->name, (a_uint8_t *)ptr->name, ACFG_MAX_SSID_LEN);

    return status;    
}

/** 
 * @brief Set the testmode
 * 
 * @param vap_name
 * @param testmode
 * 
 * @return 
 */
a_status_t
acfg_set_testmode(a_uint8_t     *vap_name, acfg_testmode_t  *testmode)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_TESTMODE}; 
    acfg_testmode_t        *ptr;

    ptr     = &req.data.testmode;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    memcpy(ptr, testmode, sizeof(acfg_testmode_t));

    status = acfg_os_send_req(vap_name, &req);

    return status;    
}

/** 
 * @brief Get the testmode
 * 
 * @param vap_name
 * @param ssid
 */
a_status_t
acfg_get_testmode(a_uint8_t  *vap_name, acfg_testmode_t  *testmode)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_TESTMODE};
    acfg_testmode_t        *ptr;

    ptr = &req.data.testmode;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    memcpy(ptr, testmode, sizeof(acfg_testmode_t));

    status = acfg_os_send_req(vap_name, &req);

    if (status == A_STATUS_OK)
        memcpy(testmode, ptr, sizeof(acfg_testmode_t));

    return status;    
}


/** 
 * @brief Get the RSSI
 * 
 * @param vap_name
 * @param rssi
 */
a_status_t
acfg_get_rssi(a_uint8_t  *vap_name, acfg_rssi_t  *rssi)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_RSSI};
    acfg_rssi_t        *ptr;

    ptr = &req.data.rssi;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    status = acfg_os_send_req(vap_name, &req);

    if (status == A_STATUS_OK)
        memcpy(rssi, ptr, sizeof(acfg_rssi_t));

    return status;    
}

/** 
 * @brief Get the CUSTDATA
 * 
 * @param vap_name
 * @param custdata
 */
a_status_t
acfg_get_custdata(a_uint8_t  *vap_name, acfg_custdata_t  *custdata)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_CUSTDATA};
    acfg_custdata_t        *ptr;

    ptr = &req.data.custdata;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    status = acfg_os_send_req(vap_name, &req);

    if (status == A_STATUS_OK)
        memcpy(custdata, ptr, sizeof(acfg_custdata_t));

    return status;    
}

/** 
 * @brief Set the channel numbers
 * 
 * @param wifi_name (Radio interface)
 * @param chan_num (IEEE Channel number)
 * 
 * @return 
 */
a_status_t
acfg_set_channel(a_uint8_t  *wifi_name, a_uint8_t  chan_num)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_CHANNEL};
    acfg_chan_t        *ptr;

    ptr = &req.data.chan;

    if (acfg_os_check_str(wifi_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    *ptr = chan_num;

    status = acfg_os_send_req(wifi_name, &req);

    return status;    

}


/** 
 * @brief Get the channel number
 * 
 * @param wifi_name (Radio interface)
 * @param chan_num
 * 
 * @return 
 */
a_status_t
acfg_get_channel(a_uint8_t *wifi_name, a_uint8_t *chan_num)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_CHANNEL};

    if (acfg_os_check_str(wifi_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    status = acfg_os_send_req(wifi_name, &req);

    if(status == A_STATUS_OK)
        *chan_num = req.data.chan;

    return status; 
}


/** 
 * @brief Set the opmode
 * 
 * @param vap_name (VAP interface)
 * @param opmode
 * 
 * @return 
 */
a_status_t
acfg_set_opmode(a_uint8_t *vap_name, acfg_opmode_t opmode)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_OPMODE};

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    req.data.opmode = opmode ;

    status = acfg_os_send_req(vap_name, &req);

    return status;    
}

acfg_opmode_t
acfg_convert_opmode(a_uint32_t opmode)
{
    switch(opmode) {
        case IW_MODE_ADHOC:
            return ACFG_OPMODE_IBSS;
            break;
        case IW_MODE_INFRA:
            return ACFG_OPMODE_STA;
            break;
        case IW_MODE_MASTER:
            return ACFG_OPMODE_HOSTAP;
            break;
        case IW_MODE_REPEAT:
            return ACFG_OPMODE_WDS;
            break;
        case IW_MODE_MONITOR:
            return ACFG_OPMODE_MONITOR;
            break;
        default:
            return -1;
            break;
    }
}

/** 
 * @brief Get the opmode
 * 
 * @param vap_name (VAP interface)
 * @param opmode
 * 
 * @return 
 */
a_status_t
acfg_get_opmode(a_uint8_t *vap_name, acfg_opmode_t *opmode)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_OPMODE};

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    status = acfg_os_send_req(vap_name, &req);

    if(status == A_STATUS_OK)
    {
        *opmode = acfg_convert_opmode(req.data.opmode);
        if(*opmode == (acfg_opmode_t)-1) {
            acfg_log_errstr("%s: Failed to convert opmode (vap=%s, opmode=%d)\n", 
                    __func__,
                    vap_name, 
                    req.data.opmode);
            status = A_STATUS_FAILED;
        }
    }

    return status ;
}


/** 
 * @brief Convert uint our internal representation of 
 *        frequencies.
 * @param in
 * @param out
 */
void
int2freq(a_uint32_t in, acfg_freq_t *out)
{
    out->e = 0;
    while(in > 1e9)
    {
        in /= 10;
        out->e++;
    }
    out->m = in;
}


/** 
 * @brief Set the frequency
 * 
 * @param wifi_name
 * @param freq - Frequency in MHz
 * 
 * @return 
 */
a_status_t
acfg_set_freq(a_uint8_t *wifi_name, a_uint32_t freq)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_FREQUENCY};

    req.data.freq.m = freq ;
    req.data.freq.e = 6 ;     
    /* int2freq(freq , &req.data.freq) ; */

    status = acfg_os_send_req(wifi_name, &req);

    return status ;
}



/** 
 * @brief Convert our internal representation 
 *        of frequency to int
 * 
 * @param in
 * 
 * @return 
 */
a_uint32_t
freq2int(const acfg_freq_t *in)
{
    a_uint32_t i;
    a_uint32_t res = in->m;

    for(i = 0; i < in->e; i++)
        res *= 10;

    return(res);
}


/** 
 * @brief Get the frequency
 * 
 * @param wifi_name
 * @param freq - Frequency returned in MHz
 * 
 * @return 
 */
a_status_t
acfg_get_freq(a_uint8_t *wifi_name, a_uint32_t *freq)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_FREQUENCY};

    status = acfg_os_send_req(wifi_name, &req);

    /* *freq = freq2int(&req.data.freq) ; */

    while(req.data.freq.e != 6) {
        if( req.data.freq.e  > 6) {
            req.data.freq.m  *= 10;
            req.data.freq.e  -= 1;
        } else {
            req.data.freq.m  /= 10;
            req.data.freq.e  += 1;
        }
    }

    *freq = req.data.freq.m ;

    return status ;
}

/**
 * @brief Set RTS threshold
 *
 * @param vap_name
 * @param rts value
 * @param rts flags
 * @return
 */
a_status_t
acfg_set_rts(a_uint8_t *vap_name, acfg_rts_t *rts)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_RTS};

    req.data.rts = *rts;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}

/** 
 * @brief Get RTS threshold
 * 
 * @param vap_name
 * @param rts
 * 
 * @return 
 */
a_status_t
acfg_get_rts(a_uint8_t *vap_name, acfg_rts_t *rts)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_RTS};

    status = acfg_os_send_req(vap_name, &req);

    *rts = req.data.rts ;

    return status ; 
}

/**
 * @brief Set frag threshold
 *
 * @param vap_name
 * @param frag
 *
 * @return
 */
a_status_t
acfg_set_frag(a_uint8_t *vap_name, acfg_frag_t *frag)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_FRAG};

    req.data.frag = *frag;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}

/** 
 * @brief Get Fragmentation threshold 
 * 
 * @param vap_name
 * @param frag
 * 
 * @return 
 */
a_status_t
acfg_get_frag(a_uint8_t *vap_name, acfg_frag_t *frag)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_FRAG};

    status = acfg_os_send_req(vap_name, &req);

    *frag = req.data.frag ;

    return status ; 
}


/**
 * @brief Set txpower
 *
 * @param vap_name
 * @param txpower
 * @param flags
 * @return
 */
a_status_t
acfg_set_txpow(a_uint8_t *vap_name, acfg_txpow_t *txpow)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_TXPOW};

    req.data.txpow = *txpow;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}

/** 
 * @brief Get default Tx Power in dBm
 * 
 * @param wifi_name
 * @param iwparam
 * 
 * @return 
 */
a_status_t
acfg_get_txpow(a_uint8_t *wifi_name, acfg_txpow_t *txpow)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_TXPOW};

    status = acfg_os_send_req(wifi_name, &req);

    *txpow = req.data.txpow ;

    return status ;
}


/** 
 * @brief Get Access Point Mac Address
 * 
 * @param vap_name
 * @param iwparam
 * 
 * @return 
 */
a_status_t
acfg_get_ap(a_uint8_t *vap_name, acfg_macaddr_t *mac)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_AP};

    status = acfg_os_send_req(vap_name, &req);

    acfg_os_strcpy(mac->addr, req.data.macaddr.addr , ACFG_MACADDR_LEN) ;

    return status ;
}


/** 
 * @brief Set the encode
 * 
 * @param wifi_name
 * @param enc - encode string
 * 
 * @return 
 */
a_status_t
acfg_set_enc(a_uint8_t *wifi_name, acfg_encode_flags_t flag, a_char_t *enc)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_ENCODE};
    acfg_encode_t      *ptr;
    const char *p;
    int dlen;
    unsigned char out[ACFG_ENCODING_TOKEN_MAX];
    unsigned char key[ACFG_ENCODING_TOKEN_MAX];
    int keylen = 0;

    ptr = &req.data.encode;

    p = (const char *)enc;
    dlen = -1;

    if(!(flag & ACFG_ENCODE_DISABLED) && (enc != NULL)) {
        while(*p != '\0') {
            int temph;
            int templ;
            int count;
            if(dlen <= 0) {
                if(dlen == 0)
                    p++;
                dlen = strcspn(p, "-:;.,");
            }
            count = sscanf(p, "%1X%1X", &temph, &templ);
            if(count < 1)
                return -1;
            if(dlen % 2)
                count = 1;
            if(count == 2)
                templ |= temph << 4;
            else
                templ = temph;
            out[keylen++] = (unsigned char) (templ & 0xFF);

            if(keylen >= ACFG_ENCODING_TOKEN_MAX )
                break;

            p += count;
            dlen -= count;
        }

        memcpy(key, out, keylen);
        ptr->buff = key;
        ptr->len = keylen;
    }
    else {
        ptr->buff = NULL;
        ptr->len = 0;
    }
    ptr->flags = flag;

    if(ptr->buff == NULL)
        ptr->flags |= ACFG_ENCODE_NOKEY;

    status = acfg_os_send_req(wifi_name, &req);

    return status;
}


/** 
 * @brief Set Vap param
 * 
 * @param vap_name
 * @param param
 * @param val
 * 
 * @return 
 */
a_status_t
acfg_set_vap_param(a_uint8_t *vap_name, \
        acfg_param_vap_t param, a_uint32_t val)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_VAP_PARAM};

    req.data.param_req.param = param ;
    req.data.param_req.val = val ;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}


/** 
 * @brief Get Vap param
 * 
 * @param vap_name
 * @param param
 * @param val
 * 
 * @return 
 */
a_status_t
acfg_get_vap_param(a_uint8_t *vap_name, \
        acfg_param_vap_t param, a_uint32_t *val)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_VAP_PARAM};

    req.data.param_req.param = param ;

    status = acfg_os_send_req(vap_name, &req);

    *val = req.data.param_req.val ;

    return status ;
}


/**
 * @brief set Vap vendor param
 *
 * @param vap_name
 * @param param
 * @param data
 * @param len
 *
 * @return
 */
a_status_t
acfg_set_vap_vendor_param(a_uint8_t *vap_name, \
        acfg_vendor_param_vap_t param, a_uint8_t *data, 
        a_uint32_t len, a_uint32_t type, acfg_vendor_param_init_flag_t reinit)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_VAP_VENDOR_PARAM};

    req.data.vendor_param_req.param = param;
    req.data.vendor_param_req.type = type;

    if(len <= sizeof(acfg_vendor_param_data_t))
        memcpy(&req.data.vendor_param_req.data, data, len);
    else
    {
        acfg_log_errstr("Vendor param size greater than max allowed by ACFG!\n");
        return status;
    }

    status = acfg_os_send_req(vap_name, &req);

    if(reinit == RESTART_SECURITY && status == A_STATUS_OK)
    {
        acfg_opmode_t opmode;
        a_char_t cmd[15], replybuf[255];
        a_uint32_t len;

        status = acfg_get_opmode(vap_name, &opmode);
        if(status != A_STATUS_OK){
            return status;
        }
        acfg_get_ctrl_iface_path(ACFG_CONF_FILE, ctrl_hapd,
                ctrl_wpasupp);
        if(opmode == ACFG_OPMODE_HOSTAP)
            strcpy((char *)cmd, "RELOAD");
        else 
            strcpy((char *)cmd, "RECONNECT");
        /* reload the security */
        if((acfg_ctrl_req (vap_name,
                        cmd,
                        strlen(cmd),
                        replybuf, &len,
                        opmode) < 0) ||
                strncmp(replybuf, "OK", strlen("OK"))){
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                    cmd,
                    vap_name);
            return A_STATUS_FAILED;
        }
    }

    return status ;
}


/**
 * @brief Get Vap vendor param
 *
 * @param vap_name
 * @param param
 * @param val
 * @param type
 *
 * @return
 */
a_status_t
acfg_get_vap_vendor_param(a_uint8_t *vap_name, \
        acfg_param_vap_t param, a_uint8_t *data, a_uint32_t *type)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_VAP_VENDOR_PARAM};

    req.data.vendor_param_req.param = param ;

    status = acfg_os_send_req(vap_name, &req);

    if(status == A_STATUS_OK){
        memcpy(data, &req.data.vendor_param_req.data, sizeof(acfg_vendor_param_data_t));
        *type = req.data.vendor_param_req.type;
    }

    return status ;
}


/** 
 * @brief Set Radio param
 * 
 * @param radio_name
 * @param param
 * @param val
 * 
 * @return 
 */
a_status_t
acfg_set_radio_param(a_uint8_t *radio_name, \
        acfg_param_radio_t param, a_uint32_t val)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_RADIO_PARAM};

    req.data.param_req.param = param ;
    req.data.param_req.val = val ;

    status = acfg_os_send_req(radio_name, &req);

    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: failed (param=0x%x status=%d)\n", __func__, param, status);
    }
    return status ;
}


/** 
 * @brief Get Radio param
 * 
 * @param radio_name
 * @param param
 * @param val
 * 
 * @return 
 */
a_status_t
acfg_get_radio_param(a_uint8_t *radio_name, \
        acfg_param_radio_t param, a_uint32_t *val)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_RADIO_PARAM};

    req.data.param_req.param = param ;

    status = acfg_os_send_req(radio_name, &req);

    *val = req.data.param_req.val ;

    return status ;
}


/**
 * @Set bit rate
 *
 * @param vap_name
 * @param rate val
 * @param rate fixed
 * @return
 */
a_status_t
acfg_set_rate(a_uint8_t *vap_name, acfg_rate_t *rate)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_RATE};

    req.data.rate = *rate;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}

/** 
 * @brief Get default bit rate
 * 
 * @param vap_name
 * @param rate
 * 
 * @return 
 */
a_status_t
acfg_get_rate(a_uint8_t *vap_name, a_uint32_t *rate)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_RATE};

    status = acfg_os_send_req(vap_name, &req);

    *rate = req.data.bitrate ;

    return status ;
}


/** 
 * @brief Set Scan Request
 * 
 * @param 
 * @param 
 * 
 * @return 
 */
a_status_t
acfg_set_scan(a_uint8_t *vap_name)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_SCAN}; 

    status = acfg_os_send_req(vap_name, &req);

    return status;    
}

/** 
 * @brief Get Scan Results
 * 
 * @param 
 * @param 
 * 
 * @return 
 */
a_status_t
acfg_get_scanresults(a_uint8_t     *vap_name)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_SCANRESULTS}; 

    status = acfg_os_send_req(vap_name, &req);

    return status;    
}


/** 
 * @brief Get wireless statistics
 * 
 * @param vap_name
 * @param ssid
 * 
 * @return 
 */
a_status_t
acfg_get_stats(a_uint8_t  *vap_name, acfg_stats_t *stats)
{
    a_status_t status = A_STATUS_FAILED;
    acfg_os_req_t req = {.cmd = ACFG_REQ_GET_STATS};
    acfg_stats_t        *ptr;

    ptr = &req.data.stats ;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    status = acfg_os_send_req(vap_name, &req);

    *stats = *ptr ;

    return status;    
}

/** 
 * @brief Set the phymode
 * 
 * @param vap_name
 * @param mode
 * 
 * @return 
 */
a_status_t
acfg_set_phymode(a_uint8_t *vap_name, acfg_phymode_t mode)
{
    a_status_t status = A_STATUS_FAILED;
    acfg_os_req_t req = {.cmd = ACFG_REQ_SET_PHYMODE};
    acfg_phymode_t *ptr;

    ptr = &req.data.phymode ;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    *ptr = mode ;
    status = acfg_os_send_req(vap_name, &req);

    return status;
}

/**
 * @brief Get the phymode
 *
 * @param vap_name
 * @param mode
 *
 * @return
 */
a_status_t
acfg_get_phymode(a_uint8_t *vap_name, acfg_phymode_t *mode)
{
    a_status_t status = A_STATUS_FAILED;
    acfg_os_req_t req = {.cmd = ACFG_REQ_GET_PHYMODE};

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    status = acfg_os_send_req(vap_name, &req);

    if (status == A_STATUS_OK) {
        *mode = req.data.phymode;
    }

    return status;
}

/** 
 * @brief 
 * 
 * @param vap_name
 * @param sinfo
 * 
 * @return 
 */
a_status_t
acfg_assoc_sta_info(a_uint8_t *vap_name, acfg_sta_info_req_t *sinfo)
{
    a_status_t status = A_STATUS_FAILED;
    acfg_os_req_t req = {.cmd = ACFG_REQ_GET_ASSOC_STA_INFO};
    acfg_sta_info_req_t *ptr ;

    ptr = &req.data.sta_info ;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    ptr->len = sinfo->len ;
    ptr->info = sinfo->info ;

    status = acfg_os_send_req(vap_name, &req);

    sinfo->len = ptr->len ;

    return status;
}


/** 
 * @brief Set Reg
 *  
 * @param radio_name
 * @param offset
 * @param value
 * 
 * @return 
 */
a_status_t
acfg_set_reg(a_uint8_t *radio_name, a_uint32_t offset, a_uint32_t value)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_REG};

    req.data.param_req.param = offset;
    req.data.param_req.val = value;

    status = acfg_os_send_req(radio_name, &req);

    return status;
}


/** 
 * @brief Get Reg
 *  
 * @param radio_name
 * @param offset
 * @param value
 * 
 * @return 
 */
a_status_t
acfg_get_reg(a_uint8_t *radio_name, a_uint32_t offset, a_uint32_t *value)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_REG};

    req.data.param_req.param = offset;

    status = acfg_os_send_req(radio_name, &req);

    *value = req.data.param_req.val;

    return status;
}

/**
 * @brief acl addmac
 *
 * @param vap name
 * @param mac addr
 *
 *
 * @return
 */
a_status_t
acfg_acl_addmac(a_uint8_t *vap_name, a_uint8_t *addr)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_ACL_ADDMAC};
    acfg_macaddr_t *mac;
    struct sockaddr sa;

    //acfg_str_to_ether(addr, &sa);
    memcpy(sa.sa_data, addr, ACFG_MACADDR_LEN);

    mac = &req.data.macaddr;

    memcpy(mac->addr, sa.sa_data, ACFG_MACADDR_LEN);

    status = acfg_os_send_req(vap_name, &req);

    return status;
}



/**
 * @brief acl getmac
 *
 * @param vap_name
 *
 *
 *
 * @return
 */
a_status_t
acfg_acl_getmac(a_uint8_t *vap_name, acfg_macacl_t *maclist)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_MAC_ADDR};
    acfg_macacl_t *list;
    a_uint32_t i = 0;

    list = &req.data.maclist;

    status = acfg_os_send_req(vap_name, &req);
    if(status == A_STATUS_OK){
        for (i = 0; i < list->num; i++) {
            memcpy(maclist->macaddr[i], list->macaddr[i], ACFG_MACADDR_LEN);
        }
        maclist->num = list->num;
    }

#if 0
    memcpy(maclist, macacllist,
            (sizeof(macacllist->num) + macacllist->num * ACFG_MACADDR_LEN) );
#endif

    return status;
}

/**
 * @brief acl delmac
 *
 * @param vap_name
 * @param macaddr
 * @
 *
 * @return
 */
a_status_t
acfg_acl_delmac(a_uint8_t *vap_name, a_uint8_t *addr)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_ACL_DELMAC};
    acfg_macaddr_t *mac;
    struct sockaddr sa;

    memcpy(sa.sa_data, addr, ACFG_MACADDR_LEN);
    mac = &req.data.macaddr;
    memcpy(mac->addr, sa.sa_data, ACFG_MACADDR_LEN);

    status = acfg_os_send_req(vap_name, &req);

    return status;

}

a_status_t
acfg_set_ap(a_uint8_t *vap_name, a_uint8_t *addr)
{
    a_status_t       status = A_STATUS_FAILED;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_AP};
    acfg_macaddr_t *mac;
    struct sockaddr sa;

    memcpy(sa.sa_data, addr, ACFG_MACADDR_LEN);

    mac = &req.data.macaddr;

    memcpy(mac->addr, sa.sa_data, ACFG_MACADDR_LEN);

    status = acfg_os_send_req(vap_name, &req);

    return status;
}


a_status_t
acfg_wlan_iface_present(a_char_t *ifname)
{
    struct ifreq ifr;
    int s;
    a_status_t   status = A_STATUS_OK;

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, (char *)ifname, ACFG_MAX_IFNAME);

    ifr.ifr_data = (__caddr_t)NULL;

    s = socket(AF_INET, SOCK_DGRAM, 0);

    if(s < 0) {
        status = A_STATUS_EBUSY;
        acfg_log_errstr("Unable to open the socket\n");
        goto fail;
    }

    if (ioctl (s, SIOCGIFFLAGS, &ifr) < 0) {
        acfg_log_errstr("Interface %s Not Present\n", ifname);
        status = acfg_get_err_status();
        //acfg_log_errstr("%s: IOCTL failed (status=%d)\n", __func__, status);
    }

    close(s);

fail:
    return status;
}

static int
acfg_str_to_ether(char *bufp, struct sockaddr *sap)
{
#define ETH_ALEN    6
    unsigned char *ptr;
    int i, j;
    unsigned char val;
    unsigned char c;

    ptr = (unsigned char *) sap->sa_data;

    i = 0;

    do {
        j = val = 0;

        /* We might get a semicolon here - not required. */
        if (i && (*bufp == ':')) {
            bufp++;
        }

        do {
            c = *bufp;
            if (((unsigned char)(c - '0')) <= 9) {
                c -= '0';
            } else if (((unsigned char)((c|0x20) - 'a')) <= 5) {
                c = (c|0x20) - ('a'-10);
            } else if (j && (c == ':' || c == 0)) {
                break;
            } else {
                return -1;
            }
            ++bufp;
            val <<= 4;
            val += c;
        } while (++j < 2);
        *ptr++ = val;
    } while (++i < ETH_ALEN);
    return (int) (*bufp);   /* Error if we don't end at end of string. */
#undef ETH_ALEN
}

void
acfg_mac_str_to_octet(a_uint8_t *mac_str, uint8_t *mac)
{
    char val[3], *str;
    int i = 0;

    strncpy(val, strtok((char *)mac_str, ":"), 2);
    val[2] = '\0';
    mac[i] = (uint8_t)strtol(val, NULL, 16);
    i++;
    while (((str = strtok(0, ":")) != NULL) && (i < ACFG_MACADDR_LEN)) {
        strncpy(val, str, 2);
        val[2] = '\0';
        mac[i] = (uint8_t)strtol(val, NULL, 16);
        i++;
    }
}


a_status_t
acfg_set_ifmac (char *ifname, char *buf, int arphdr)
{
    struct sockaddr sa;
    struct ifreq ifr;
    a_status_t   status = A_STATUS_OK;
    int s;
    int i = 0;
    
    memset(&ifr, 0, sizeof(struct ifreq));

    if(!ifname) {
        return A_STATUS_FAILED;
    }


    if (acfg_str_to_ether(buf, &sa) == 0) {
        sa.sa_family = arphdr;

        strncpy(ifr.ifr_name, ifname, ACFG_MAX_IFNAME);
        memcpy(&ifr.ifr_hwaddr, &sa, sizeof(struct sockaddr));

        s = socket(AF_INET, SOCK_DGRAM, 0);

        if(s < 0) {
            status = A_STATUS_EBUSY;
            goto fail;
        }

        if ((i = ioctl (s, SIOCSIFHWADDR, &ifr)) < 0) {
            status = acfg_get_err_status();
            acfg_log_errstr("%s: IOCTL failed (status=%d)\n", __func__, status);
        }
        close(s);
    }

fail:
    return status;

}

a_status_t
acfg_get_ifmac (char *ifname, char *buf)
{
    struct ifreq ifr;
    a_status_t   status = A_STATUS_OK;
    int s;
    int i = 0;
    a_uint8_t *ptr;

    memset(&ifr, 0, sizeof(struct ifreq));

    if(!ifname) {
        return A_STATUS_FAILED;
    }

    strncpy(ifr.ifr_name, ifname, ACFG_MAX_IFNAME);

    s = socket(AF_INET, SOCK_DGRAM, 0);

    if(s < 0) {
        status = A_STATUS_EBUSY;
        goto fail;
    }

    if ((i = ioctl (s, SIOCGIFHWADDR, &ifr)) < 0) {
        status = acfg_get_err_status();
        acfg_log_errstr("%s: IOCTL failed (status=%d)\n", __func__, status);
        goto fail;
    }

    ptr = (a_uint8_t *) ifr.ifr_hwaddr.sa_data;
    snprintf(buf, ACFG_MACSTR_LEN, "%02X:%02X:%02X:%02X:%02X:%02X",
            (ptr[0] & 0377), (ptr[1] & 0377), (ptr[2] & 0377),
            (ptr[3] & 0377), (ptr[4] & 0377), (ptr[5] & 0377));

    close(s);

fail:
    return status;
}

a_status_t
acfg_wlan_profile_get(acfg_wlan_profile_t *profile)
{
    int i;
    a_status_t status = A_STATUS_OK;

    status = acfg_wlan_iface_present((a_char_t *)profile->radio_params.radio_name);
    if(status != A_STATUS_OK) {
        return A_STATUS_EINVAL;
    }

    status = acfg_get_current_profile(profile);
    if(status != A_STATUS_OK){
        acfg_log_errstr("%s: Failed to get driver profile for one or more vaps\n", 
                __func__);
        return status;
    }

    for (i = 0; i < profile->num_vaps; i++) {
        if (profile->vap_params[i].opmode == ACFG_OPMODE_STA) {
            status = acfg_wpa_supplicant_get(&(profile->vap_params[i]));
            if(status != A_STATUS_OK)
            {
                acfg_log_errstr("%s: Failed to get security profile for %s\n", 
                        __func__, 
                        profile->vap_params[i].vap_name);
                return status;
            }
        }
        if (profile->vap_params[i].opmode == ACFG_OPMODE_HOSTAP) {
            status = acfg_hostapd_get(&(profile->vap_params[i]));
            if(status != A_STATUS_OK)
            {
                acfg_log_errstr("%s: Failed to get security profile for %s\n",
                        __func__, 
                        profile->vap_params[i].vap_name);
                return status;
            }
        }
    }

    return A_STATUS_OK;
}

a_status_t 
acfg_hostapd_getconfig(a_uint8_t *vap_name, a_char_t *reply_buf)
{
    a_status_t       status = A_STATUS_OK;
    char buffer[4096];
    a_uint32_t len = 0;

    strcpy(buffer, "GET_CONFIG");

    len = sizeof (reply_buf);
    if(acfg_ctrl_req (vap_name, buffer, strlen(buffer),
                reply_buf, &len, ACFG_OPMODE_HOSTAP) < 0){
        status = A_STATUS_FAILED;
    }

    return status;
}

a_status_t
acfg_wlan_vap_profile_get (acfg_wlan_profile_vap_params_t *vap_params)
{   
    (void)vap_params;
    a_status_t status = A_STATUS_OK;

    return status; 
}   


a_status_t
acfg_wlan_iface_up(a_uint8_t  *ifname)
{
    struct ifreq ifr;
    int s;
    a_status_t   status = A_STATUS_OK;

    memset(&ifr, 0, sizeof(struct ifreq));

    if(!ifname)
        return A_STATUS_FAILED;

    strncpy(ifr.ifr_name, (char *)ifname, ACFG_MAX_IFNAME);


    ifr.ifr_data = (__caddr_t)NULL;
    ifr.ifr_flags = (IFF_UP | IFF_RUNNING);

    s = socket(AF_INET, SOCK_DGRAM, 0);

    if(s < 0) {
        status = A_STATUS_EBUSY;
        goto fail;
    }

    if (ioctl (s, SIOCSIFFLAGS, &ifr) < 0) {
        status = acfg_get_err_status();
        acfg_log_errstr("%s: IOCTL failed (status=%d)\n", __func__, status);
    }

    close(s);

fail:
    return status;
}

a_status_t
acfg_wlan_iface_down(a_uint8_t *ifname)
{
    struct ifreq ifr;
    int s;
    a_status_t   status = A_STATUS_OK;

    memset(&ifr, 0, sizeof(struct ifreq));

    if(!ifname)
        return -A_STATUS_FAILED;

    strncpy(ifr.ifr_name, (char *)ifname,
            ACFG_MAX_IFNAME);

    ifr.ifr_data = (__caddr_t)NULL;
    ifr.ifr_flags = 0;

    s = socket(AF_INET, SOCK_DGRAM, 0);

    if(s < 0) {
        status = A_STATUS_EBUSY;
        goto fail;
    }

    if (ioctl (s, SIOCSIFFLAGS, &ifr) < 0) {
        status = acfg_get_err_status();
        acfg_log_errstr("%s: IOCTL failed (status=%d)\n", __func__, status);
    }

    close(s);

fail:
    return status;
}

a_status_t 
acfg_set_acl_policy(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params)
{
    acfg_wlan_profile_node_params_t node_params, cur_node_params;
    a_status_t status = A_STATUS_OK;

    node_params = vap_params->node_params;
    if (cur_vap_params != NULL) {
        cur_node_params = cur_vap_params->node_params;	
        if (node_params.node_acl != cur_node_params.node_acl) {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_MACCMD,
                    node_params.node_acl);
            if (status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }
        }
    } else {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_MACCMD,
                node_params.node_acl);
    }	
    return status;	
}

a_status_t 
acfg_set_node_list(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params)
{
    acfg_wlan_profile_node_params_t node_params, cur_node_params;
    a_uint8_t *mac;
    a_uint8_t new_index, cur_index, found ;
    a_status_t status = A_STATUS_OK;

    node_params = vap_params->node_params;
    if (cur_vap_params != NULL) {
        cur_node_params = cur_vap_params->node_params;		
        for (new_index = 0; new_index < node_params.num_node; new_index++) {
            mac = node_params.acfg_acl_node_list[new_index];
            found = 0;
            for (cur_index = 0; cur_index < cur_node_params.num_node;
                    cur_index++) 
            {
                if (memcmp(mac, 
                            cur_node_params.acfg_acl_node_list[cur_index],
                            ACFG_MACADDR_LEN) == 0)
                {
                    found = 1;
                    break;
                }

            }
            if (found == 0) {
                status = acfg_acl_addmac((a_uint8_t *)vap_params->vap_name, 
                        mac);
                if(status != A_STATUS_OK) {
                    return A_STATUS_FAILED;
                }
            }
        }
        for (cur_index = 0; cur_index < cur_node_params.num_node; 
                cur_index++) 
        {
            mac = cur_node_params.acfg_acl_node_list[cur_index];
            found = 0;
            for (new_index = 0; new_index < node_params.num_node;
                    new_index++) 
            {
                if (memcmp(mac, 
                            node_params.acfg_acl_node_list[new_index],
                            ACFG_MACADDR_LEN) == 0)
                {
                    found = 1;
                    break;
                }

            }
            if (found == 0) {
                status = acfg_acl_delmac((a_uint8_t *)cur_vap_params->vap_name,
                        mac);
                if(status != A_STATUS_OK) {
                    return A_STATUS_FAILED;
                }
            }
        }
    } else {
        for (new_index = 0; new_index < node_params.num_node; new_index++) {
            mac = node_params.acfg_acl_node_list[new_index];
            status = acfg_acl_addmac((a_uint8_t *)vap_params->vap_name, 
                    mac);
            if(status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }
        }
    }
    return status;
}

void
acfg_rem_wps_config_file(a_uint8_t *ifname)
{
    char filename[32];
    FILE *fp;

    sprintf(filename, "/etc/%s_%s.conf", ACFG_WPS_CONFIG_PREFIX, (char *)ifname);
    fp = fopen(filename, "r");
    if (fp != NULL) {
        unlink(filename);
        fclose(fp);
    }
}

a_status_t 
acfg_set_wps_vap_params( acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wps_cred_t *wps_cred)
{
    acfg_opmode_t opmode;
    a_status_t status = A_STATUS_OK;

    status = acfg_get_opmode(vap_params->vap_name, &opmode);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: Opmode fetch fail for %s\n", __func__, 
                vap_params->vap_name);
        return A_STATUS_FAILED;
    }
    strcpy(vap_params->ssid, wps_cred->ssid);
    if ( wps_cred->wpa == 1) {
        vap_params->security_params.sec_method = 
            ACFG_WLAN_PROFILE_SEC_METH_WPA;
    } else if ((wps_cred->wpa == 2)) {
        vap_params->security_params.sec_method =
            ACFG_WLAN_PROFILE_SEC_METH_WPA2;
    } else if ((wps_cred->wpa == 3)) {
        vap_params->security_params.sec_method =
            ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2;
    } else if (wps_cred->wpa == 0) {
        if (wps_cred->auth_alg == 1) {
            vap_params->security_params.sec_method =
                ACFG_WLAN_PROFILE_SEC_METH_OPEN;
        } else if (wps_cred->auth_alg == 2) {
            vap_params->security_params.sec_method =
                ACFG_WLAN_PROFILE_SEC_METH_SHARED;
        }
        if (strlen(wps_cred->wep_key)) {
            if (wps_cred->wep_key_idx == 0) {
                strcpy(vap_params->security_params.wep_key0,
                        wps_cred->wep_key);	
            } else if (wps_cred->wep_key_idx == 1) {
                strcpy(vap_params->security_params.wep_key1,
                        wps_cred->wep_key);	
            } else if (wps_cred->wep_key_idx == 2) {
                strcpy(vap_params->security_params.wep_key2,
                        wps_cred->wep_key);	
            } else if (wps_cred->wep_key_idx == 3) {
                strcpy(vap_params->security_params.wep_key3,
                        wps_cred->wep_key);	
            }
            vap_params->security_params.wep_key_defidx = wps_cred->wep_key_idx;
            vap_params->security_params.cipher_method = 
                ACFG_WLAN_PROFILE_CIPHER_METH_WEP;
        } else {
            vap_params->security_params.sec_method =
                ACFG_WLAN_PROFILE_SEC_METH_OPEN;
        }
    }

    if (wps_cred->key_mgmt == 2) {
        strcpy(vap_params->security_params.psk, wps_cred->key);
    }
    if (wps_cred->enc_type) {
        vap_params->security_params.cipher_method = wps_cred->enc_type;
    }
    /*Overide Cipher*/
    if ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
            (vap_params->security_params.sec_method ==
             ACFG_WLAN_PROFILE_SEC_METH_SHARED))
    {
        if (strlen(wps_cred->wep_key)) {
            vap_params->security_params.cipher_method = 
                ACFG_WLAN_PROFILE_CIPHER_METH_WEP;
        } else {
            vap_params->security_params.cipher_method = 
                ACFG_WLAN_PROFILE_CIPHER_METH_NONE;
        }
    }

    if (opmode == ACFG_OPMODE_HOSTAP) {
        vap_params->security_params.wps_flag = WPS_FLAG_CONFIGURED;
    }

    return status;
}

a_status_t
acfg_wps_config(a_uint8_t *ifname, char *ssid,
        char *auth, char *encr, char *key)
{
    char cmd[255];
    char buf[255];
    a_char_t replybuf[255];
    a_uint32_t len = sizeof(replybuf), i;
    acfg_opmode_t opmode;
    a_status_t status = A_STATUS_OK;
    char ssid_hex[2 * 32 + 1];
    char key_hex[2 * 64 + 1];


    status = acfg_get_opmode(ifname,
            &opmode);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: Opmode fetch fail for %s\n", __func__, ifname);
        return status;
    }
    acfg_get_ctrl_iface_path(ACFG_CONF_FILE, ctrl_hapd,
            ctrl_wpasupp);
    sprintf(cmd, "WPS_CONFIG");
    if (strcmp(ssid, "0")) {
        ssid_hex[0] = '\0';
        for (i = 0; i < 32; i++) {
            if (ssid[i] == '\0') {
                break;
            }
            snprintf(&ssid_hex[i * 2], 3, "%02x", ssid[i]);
        }
        strcat(cmd, " ");
        strcat(cmd, ssid_hex);
    }
    if (strcmp(auth, "0")) {
        sprintf(buf, " %s", auth);
        strcat(cmd, buf);
    }
    if (strcmp(encr, "0")) {
        sprintf(buf, " %s", encr);
        strcat(cmd, buf);
    }
    if (strcmp(key, "0")) {
        key_hex[0] = '\0';
        for (i = 0; i < 64; i++) {
            if (key[i] == '\0') {
                break;
            }
            snprintf(&key_hex[i * 2], 3, "%02x",
                    key[i]);
        }
        strcat(cmd, " ");
        strcat(cmd, key_hex);
    }

    if((acfg_ctrl_req(ifname, cmd, strlen(cmd),
                    replybuf, &len, opmode) < 0) ||
            strncmp(replybuf, "OK", strlen("OK"))){
        return A_STATUS_FAILED;
    }
    return status;	
}

int acfg_get_legacy_rate(int rate)
{
    unsigned int i = 0;
    int legacy_rate_idx[][2] = {
        {1, 0x1b},
        {2, 0x1a},
        {5, 0x19},
        {6, 0xb},
        {9, 0xf},
        {11, 0x18},
        {12, 0xa},
        {18, 0xe},
        {24, 0x9},
        {36, 0xd},
        {48, 0x8},
        {54, 0xc},
    };
    for (i = 0; i < (sizeof(legacy_rate_idx)/sizeof(legacy_rate_idx[0])); i++)
    {
        if (legacy_rate_idx[i][0] == rate) {
            return legacy_rate_idx[i][1];
        }
    }
    return 0;
}

int acfg_get_mcs_rate(int val)
{
    unsigned int i = 0;
    int mcs_rate_idx[][2] = {
        {0, 0x80},
        {1, 0x81},
        {2, 0x82},
        {3, 0x83},
        {4, 0x84},
        {5, 0x85},
        {6, 0x86},
        {7, 0x87},
        {8, 0x88},
        {9, 0x89},
        {10, 0x8a},
        {11, 0x8b},
        {12, 0x8c},
        {13, 0x8d},
        {14, 0x8e},
        {15, 0x8f},
        {16, 0x90},
        {17, 0x91},
        {18, 0x92},
        {19, 0x93},
        {20, 0x94},
        {21, 0x95},
        {22, 0x96},
        {23, 0x97},
    };

    if (val >= (int)(sizeof(mcs_rate_idx)/sizeof(mcs_rate_idx[0]))) {
        return 0;
    }
    for (i = 0; i < sizeof(mcs_rate_idx)/sizeof(mcs_rate_idx[0]); i++)
    {
        if (mcs_rate_idx[i][0] == val) {
            return mcs_rate_idx[i][1];
        }
    }
    return 0;
}

void
acfg_parse_rate(a_uint8_t *rate_str, int *val)
{
    char *pos = NULL, *start;
    char buf[16];
    int rate = 0;
    int ratecode, i;

    start = (char *)rate_str;
    pos = strchr((char *)rate_str, 'M');
    if (pos) {	
        strncpy(buf, start, pos - start);
        rate = atoi(buf);
        ratecode = acfg_get_legacy_rate(rate);
    } else {
        strcpy(buf, start);
        rate = atoi(buf);
        rate = rate - 1;
        if (rate < 0) {
            *val = 0;
            return;
        }
        ratecode = acfg_get_mcs_rate(rate);
    }
    *val = 0;
    for (i = 0; i < 4; i++) {	
        *val |= ratecode << (i * 8);
    }
}

a_status_t 
acfg_wlan_vap_profile_vlan_add(acfg_wlan_profile_vap_params_t *vap_params)
{
    a_char_t str[60];
    a_char_t vlan_bridge[ACFG_MAX_IFNAME];
    a_status_t status = A_STATUS_OK;

    status = acfg_wlan_iface_present("eth0");
    if (status == A_STATUS_OK) {
        sprintf(str, "brctl delif br0 eth0");
        system(str);
    }

    status = acfg_wlan_iface_present("eth1");
    if (status == A_STATUS_OK) {
        sprintf(str, "brctl delif br0 eth1");
        system(str);
    }

    status = acfg_wlan_iface_present((a_char_t *)vap_params->vap_name);
    if (status == A_STATUS_OK) {
        sprintf(str, "brctl delif br0 %s", vap_params->vap_name);
        system(str);
    }

    sprintf(vlan_bridge, "br%d", vap_params->vlanid);
    status = acfg_wlan_iface_present(vlan_bridge);
    if (status != A_STATUS_OK) {
        sprintf(str, "brctl addbr %s", vlan_bridge);
        system(str);
    }

    sprintf(str, "brctl delif br%d %s", vap_params->vlanid, vap_params->vap_name);
    system(str);

    sprintf(str, "vconfig add %s %d", vap_params->vap_name,
            vap_params->vlanid);
    system(str);

    sprintf(str, "vconfig add eth0 %d", vap_params->vlanid);
    system(str);

    sprintf(str, "vconfig add eth1 %d", vap_params->vlanid);
    system(str);

    sprintf(str, "brctl addif %s %s.%d", vlan_bridge,
            vap_params->vap_name, vap_params->vlanid);
    system(str);
    sprintf(str, "brctl addif %s eth0.%d", vlan_bridge,
            vap_params->vlanid);
    system(str);
    sprintf(str, "brctl addif %s eth1.%d", vlan_bridge,
            vap_params->vlanid);
    system(str);

    sprintf(str, "%s.%d", vap_params->vap_name, vap_params->vlanid);
    status = acfg_wlan_iface_up((a_uint8_t *)str);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("Failed to bring vap UP\n");
        return status;
    }
    sprintf(str, "eth0.%d", vap_params->vlanid);
    status = acfg_wlan_iface_up((a_uint8_t *)str);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("Failed to bring %s UP\n", str);
        return status;
    }
    sprintf(str, "eth1.%d", vap_params->vlanid);
    status = acfg_wlan_iface_up((a_uint8_t *)str);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("Failed to bring %s UP\n", str);
        return status;
    }
    return status;
}

void acfg_wlan_vap_profile_vlan_remove(acfg_wlan_profile_vap_params_t *cur_vap_params)
{
    a_char_t str[60];
    a_char_t vlan_bridge[ACFG_MAX_IFNAME];
    a_status_t status = A_STATUS_OK;

    sprintf(vlan_bridge, "br%d", cur_vap_params->vlanid);

    sprintf(str, "%s.%d", cur_vap_params->vap_name, cur_vap_params->vlanid);
    status = acfg_wlan_iface_present(str);
    if (status == A_STATUS_OK) {
        acfg_wlan_iface_down((a_uint8_t *)str);
        sprintf(str, "brctl delif %s %s", vlan_bridge, str);
        system(str);
    }

    sprintf(str, "vconfig rem %s.%d", cur_vap_params->vap_name,
            cur_vap_params->vlanid);
    system(str);
}

a_status_t
acfg_wlan_vap_create(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_radio_params_t radio_params)
{
    a_status_t status = A_STATUS_OK;

    status = acfg_wlan_iface_present((a_char_t *)vap_params->vap_name);
    if(status == A_STATUS_OK) {
        acfg_log_errstr("Interface Already present\n");
        return A_STATUS_EINVAL;
    }
    if ((vap_params->opmode == ACFG_OPMODE_STA) &&
            (vap_params->wds_params.wds_flags != ACFG_FLAG_VAP_IND))
    {
        if((vap_params->vapid == VAP_ID_AUTO) || (radio_params.macreq_enabled != 1))
            status = acfg_create_vap(radio_params.radio_name,
                    vap_params->vap_name,
                    vap_params->opmode,
                    vap_params->vapid,
                    IEEE80211_CLONE_BSSID | IEEE80211_CLONE_NOBEACONS);
        else
            status = acfg_create_vap(radio_params.radio_name,
                    vap_params->vap_name,
                    vap_params->opmode,
                    vap_params->vapid,
                    IEEE80211_CLONE_NOBEACONS);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to Create Vap %s\n", vap_params->vap_name);
            return A_STATUS_FAILED;
        }
    }
    else
    {
        if((vap_params->vapid == VAP_ID_AUTO) || (radio_params.macreq_enabled != 1))
            status = acfg_create_vap(radio_params.radio_name,
                    vap_params->vap_name,
                    vap_params->opmode,
                    vap_params->vapid,
                    IEEE80211_CLONE_BSSID);
        else
            status = acfg_create_vap(radio_params.radio_name,
                    vap_params->vap_name,
                    vap_params->opmode,
                    vap_params->vapid,
                    0);
        if(status != A_STATUS_OK) {
            acfg_log_errstr("Failed to Create Vap %s\n", vap_params->vap_name);
            return A_STATUS_FAILED;
        }
    }
    return status;
}

a_status_t 
acfg_wlan_vap_profile_modify(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params,
        acfg_wlan_profile_radio_params_t radio_params)
{
    acfg_ssid_t ssid;
    acfg_rate_t rate;
    a_int8_t sec = 0;
    int if_down = 0, setssid = 0, enablewep = 0, set_open = 0, 
        set_wep = 0, wps_state = 0;
    a_status_t status = A_STATUS_OK;
    a_uint8_t mac[ACFG_MACADDR_LEN];
    a_char_t str[60];
    int rate_val = 0, retries = 0, i;

    if(vap_params->opmode != cur_vap_params->opmode) {
        acfg_log_errstr("Operating Mode cannot be modified\n");
        return A_STATUS_FAILED;
    }

    if ((vap_params->vlanid) && (vap_params->vlanid != ACFG_WLAN_PROFILE_VLAN_INVALID)) {
        sprintf((char *)vap_params->bridge, "br%d", vap_params->vlanid);
     }
    if ((cur_vap_params->vlanid) && (vap_params->vlanid != ACFG_WLAN_PROFILE_VLAN_INVALID)) {
        sprintf((char *)cur_vap_params->bridge, "br%d", cur_vap_params->vlanid);
    }

    if (!ACFG_STR_MATCH(vap_params->ssid, cur_vap_params->ssid)) {
        memcpy(ssid.name, vap_params->ssid, ACFG_MAX_SSID_LEN);
        if(strlen((char *)ssid.name) > 0) {
            if (!if_down) {
                status = acfg_wlan_iface_down(vap_params->vap_name);
                if_down = 1;
                setssid = 1;
                if(status != A_STATUS_OK) {
                    return A_STATUS_FAILED;
                }	
            }
            status = acfg_set_ssid(vap_params->vap_name, &ssid);
            if(status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }
        }
    }

    if (!ACFG_STR_MATCH(vap_params->bridge, cur_vap_params->bridge)) {
        if (vap_params->bridge[0] == 0) {
            status = acfg_wlan_iface_present(cur_vap_params->bridge);

            if (status == A_STATUS_OK) {
                sprintf(str, "brctl delif %s %s", cur_vap_params->bridge,
                        vap_params->vap_name);
                system(str);
            }
        } else if (!cur_vap_params->bridge[0] && vap_params->bridge[0]) {
            status = acfg_wlan_iface_present(vap_params->bridge);

            if (status != A_STATUS_OK) {
                sprintf(str, "brctl addbr %s", vap_params->bridge);
                system(str);
            }

            status = acfg_wlan_iface_up((a_uint8_t *)vap_params->bridge);

            sprintf(str, "brctl addif %s %s", vap_params->bridge,
                    vap_params->vap_name);
            system(str);
            sprintf(str, "brctl setfd %s 0", vap_params->bridge);
            system(str);
        } else if (cur_vap_params->bridge[0] && vap_params->bridge[0]) {
            status = acfg_wlan_iface_present(cur_vap_params->bridge);
            if (status == A_STATUS_OK) {
                sprintf(str, "brctl delif %s %s", cur_vap_params->bridge,
                        vap_params->vap_name);
                system(str);
            }
            status = acfg_wlan_iface_present(vap_params->bridge);

            if (status != A_STATUS_OK) {
                sprintf(str, "brctl addbr %s", vap_params->bridge);
                system(str);
            }
            sprintf(str, "brctl addif %s %s", vap_params->bridge,
                    vap_params->vap_name);
            status = acfg_wlan_iface_up((a_uint8_t *)vap_params->bridge);
            system(str);
            sprintf(str, "brctl setfd %s 0", vap_params->bridge);
            system(str);
        }
    }
    if (vap_params->wds_params.enabled != cur_vap_params->wds_params.enabled) {
        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name);
            if_down = 1;
            setssid = 1;
            if(status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }	
        }
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_WDS,
                vap_params->wds_params.enabled);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to enbale wds\n");
            return A_STATUS_FAILED;
        }
    }

    if (vap_params->wds_params.wds_flags !=
            cur_vap_params->wds_params.wds_flags)
    {
        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name);
            if_down = 1;
            setssid = 1;
            if(status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }	
        }

        if ((vap_params->wds_params.wds_flags & ACFG_FLAG_VAP_IND) ==
                ACFG_FLAG_VAP_IND)
        {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_VAP_IND, 1);
            if (status != A_STATUS_OK) {
                acfg_log_errstr("Failed to set wds repeater independent flag\n");
                return A_STATUS_FAILED;
            }
        }
        else
        {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_VAP_IND, 0);
            if (status != A_STATUS_OK) {
                acfg_log_errstr("Failed to set wds repeater independent flag\n");
                return A_STATUS_FAILED;
            }
        }

        if (vap_params->opmode == ACFG_OPMODE_STA) {
            if ((vap_params->wds_params.wds_flags & ACFG_FLAG_EXTAP) ==
                    ACFG_FLAG_EXTAP)
            {
                status = acfg_set_vap_param(vap_params->vap_name,
                        ACFG_PARAM_EXTAP, 1);
                if (status != A_STATUS_OK) {
                    acfg_log_errstr("Failed to set wds extension flag\n");
                    return A_STATUS_FAILED;
                }
            }
            else
            {
                status = acfg_set_vap_param(vap_params->vap_name,
                        ACFG_PARAM_EXTAP, 0);
                if (status != A_STATUS_OK) {
                    acfg_log_errstr("Failed to set wds extension flag\n");
                    return A_STATUS_FAILED;
                }
            }
        }
    }

    if (vap_params->phymode != cur_vap_params->phymode) {
        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name);
            if_down = 1;
            setssid = 1;
            if(status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }	
        }
        status = acfg_set_phymode(vap_params->vap_name,
                vap_params->phymode);
        if(status != A_STATUS_OK) {
            return A_STATUS_FAILED;
        }
        status = acfg_set_channel(vap_params->vap_name,
                radio_params.chan);
        if(status != A_STATUS_OK) {
            return A_STATUS_FAILED;
        }
    }
    if(vap_params->opmode == ACFG_OPMODE_HOSTAP) {
        if (vap_params->beacon_interval != cur_vap_params->beacon_interval)
        {
            if (!if_down) {
                status = acfg_wlan_iface_down(vap_params->vap_name);
                if_down = 1;
                setssid = 1;
                if(status != A_STATUS_OK) {
                    return A_STATUS_FAILED;
                }	
            }
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_BEACON_INTERVAL,
                    vap_params->beacon_interval);
            if(status != A_STATUS_OK) {
                acfg_log_errstr("Failed to set beacon interval\n");
                return A_STATUS_FAILED;
            }
        }
    }
    if(vap_params->bitrate != cur_vap_params->bitrate) {
        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name);
            if_down = 1;
            setssid = 1;
            if(status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }	
        }
        rate.value = vap_params->bitrate;
        rate.fixed = !!rate.value;
        status = acfg_set_rate(vap_params->vap_name, &rate);
        if(status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set rate\n");
            return A_STATUS_FAILED;
        }
    }
    if (!ACFG_STR_MATCH(vap_params->rate, cur_vap_params->rate)) {	
        acfg_parse_rate(vap_params->rate, &rate_val);
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_11N_RATE,
                rate_val);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set rate\n");
        }
    }
    if (vap_params->retries != cur_vap_params->retries) {
        for (i = 0; i < 4; i++) {
            retries |= vap_params->retries << (i * 8);
        }
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_11N_RETRIES,
                retries);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set retries\n");
        }	
    }
    if(vap_params->frag_thresh.val !=
            cur_vap_params->frag_thresh.val) 
    {
        vap_params->frag_thresh.flags = 0;

        if((vap_params->frag_thresh.val >= ACFG_FRAG_MAX) ||
                (vap_params->frag_thresh.val <= 0)) 
        {
            vap_params->frag_thresh.flags |= ACFG_FRAG_DISABLED;
        } else {
            vap_params->frag_thresh.flags |= ACFG_FRAG_FIXED;
        }
        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name);
            if_down = 1;
            setssid = 1;
            if(status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }	
        }
        status = acfg_set_frag(vap_params->vap_name,
                &vap_params->frag_thresh);
        if(status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set fragmentation Threshold\n");
        }
    }
    if(vap_params->rts_thresh.val !=
            cur_vap_params->rts_thresh.val) 
    {
        vap_params->rts_thresh.flags = 0;

        if((vap_params->rts_thresh.val == ACFG_RTS_MAX) ||
                (vap_params->rts_thresh.val == 0)) 
        {
            vap_params->rts_thresh.flags |= ACFG_RTS_DISABLED;
        } else if (vap_params->rts_thresh.val) {
            vap_params->rts_thresh.flags |= ACFG_RTS_FIXED;
        }
        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name);
            if_down = 1;
            setssid = 1;
            if(status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }	
        }
        status = acfg_set_rts(vap_params->vap_name,
                &vap_params->rts_thresh);
        if(status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set rts threshold\n");
        }
    }
    status = acfg_set_node_list(vap_params, cur_vap_params);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: Failed to set node list (vap=%s status=%d)!\n",
                __func__, vap_params->vap_name, status);
        return A_STATUS_FAILED;
    }
    status = acfg_set_acl_policy(vap_params, cur_vap_params);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: Failed to set ACL policy (vap=%s status=%d)!\n",
                __func__, vap_params->vap_name, status);
        return A_STATUS_FAILED;
    }

    if ((vap_params->opmode == ACFG_OPMODE_HOSTAP) &&
            (vap_params->pureg != cur_vap_params->pureg))
    {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_PUREG,
                vap_params->pureg);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set pureg\n");
            return A_STATUS_FAILED;
        }
    }
    if ((vap_params->opmode == ACFG_OPMODE_HOSTAP) &&
            (vap_params->puren != cur_vap_params->puren))
    {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_PUREN,
                vap_params->puren);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set puren\n");
            return A_STATUS_FAILED;
        }
    }
    if ((vap_params->opmode == ACFG_OPMODE_HOSTAP) && 
            (vap_params->hide_ssid != 
             cur_vap_params->hide_ssid))
    {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_HIDE_SSID,
                vap_params->hide_ssid);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set hide ssid param\n");
            return A_STATUS_FAILED;
        }

    }
    if (vap_params->doth != cur_vap_params->doth) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_DOTH,
                vap_params->doth);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set hide doth param\n");
            return A_STATUS_FAILED;
        }
    }
    if (vap_params->coext != cur_vap_params->coext) {
        status = acfg_set_vap_param(vap_params->vap_name,
                        ACFG_PARAM_COEXT_DISABLE,
                        !vap_params->coext);
        if (status != A_STATUS_OK) {
                acfg_log_errstr("Failed to set coext param\n");
                return A_STATUS_FAILED;
        }	
    }
    if (vap_params->client_isolation != cur_vap_params->client_isolation) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_APBRIDGE,
                !vap_params->client_isolation);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set ap bridge param\n");
            return A_STATUS_FAILED;
        }
    }
    if (vap_params->ampdu != cur_vap_params->ampdu) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_AMPDU,
                vap_params->ampdu);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set ampdu param\n");
            return A_STATUS_FAILED;
        }
    }
    if (vap_params->uapsd != cur_vap_params->uapsd) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_UAPSD,
                vap_params->uapsd);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set uapsd\n");
            return A_STATUS_FAILED;
        }
    }
    if (vap_params->shortgi != cur_vap_params->shortgi) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_VAP_SHORT_GI,
                vap_params->shortgi);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set shortgi\n");
            return A_STATUS_FAILED;
        }
    }
    if (vap_params->amsdu != cur_vap_params->amsdu) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_AMSDU,
                vap_params->amsdu);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set amsdu\n");
            return A_STATUS_FAILED;
        }
    }
    if (vap_params->max_clients != cur_vap_params->max_clients) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_MAXSTA,
                vap_params->max_clients);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set max_clients\n");
            return A_STATUS_FAILED;
        }
    }

    if((vap_params->security_params.hs_iw_param.hs_enabled == 1) &&
            (vap_params->security_params.hs_iw_param.iw_enabled == 1))
    {
        acfg_set_hs_iw_vap_param(vap_params);
    }

    //Set security parameters
    if (vap_params->security_params.wps_flag == 0) {	
        acfg_rem_wps_config_file(vap_params->vap_name);
    } else if (ACFG_SEC_CMP(vap_params, cur_vap_params)) {
        acfg_rem_wps_config_file(vap_params->vap_name);
    } else {
        acfg_wps_cred_t wps_cred;
        memset(&wps_cred, 0x00, sizeof(wps_cred));
        /* Check & Set default WPS dev params */
        acfg_set_wps_default_config(vap_params);
        /* Update/create the WPS config file*/
        acfg_update_wps_dev_config_file(vap_params, 0);

        wps_state = acfg_get_wps_config(vap_params->vap_name, &wps_cred);
        if (wps_state == 1) {
            status = acfg_set_wps_vap_params(vap_params, &wps_cred);
            if (status != A_STATUS_OK) {
                acfg_log_errstr("%s: Failed to set WPS VAP params (vap=%s status=%d)!\n",
                        __func__, vap_params->vap_name, status);
                return A_STATUS_FAILED;
            }
        }
    }

    if(vap_params->num_vendor_params != 0)
    {
        int i, j, configure;
        for(i = 0; i < vap_params->num_vendor_params; i++)
        {
            configure = 1;

            for(j = 0; j < cur_vap_params->num_vendor_params; j++)
            {
                if(vap_params->vendor_param[i].cmd == 
                        cur_vap_params->vendor_param[j].cmd)
                {
                    int len = 0;

                    if(vap_params->vendor_param[i].len == cur_vap_params->vendor_param[j].len)
                    {
                        /* Length is equal, check data */
                        len = vap_params->vendor_param[i].len;
                        if(0 == memcmp((void *)&vap_params->vendor_param[i].data, 
                                    (void *)&cur_vap_params->vendor_param[j].data, 
                                    len))
                        {
                            /* Data is same, No need to configure again */
                            configure = 0;
                        }
                        else
                        {
                            /* Data is different, Need to configure again */
                            configure = 1;
                        }
                    }
                }                
            }
            if(configure == 1)
            {
                status = acfg_set_vap_vendor_param(vap_params->vap_name,
                        vap_params->vendor_param[i].cmd,
                        (a_uint8_t *)&vap_params->vendor_param[i].data,
                        vap_params->vendor_param[i].len,
                        vap_params->vendor_param[i].type,
                        0);
                if (status != A_STATUS_OK)
                {
                    acfg_log_errstr("Failed to set vendor param: status %d\n", status);
                    return A_STATUS_FAILED;
                }
            }
        }
    }
    if(A_STATUS_OK != acfg_set_security(vap_params, cur_vap_params, 
                PROFILE_MODIFY, &sec)){
        acfg_log_errstr("%s: Failed to set %s security params\n", __func__,
                vap_params->vap_name);
        return A_STATUS_FAILED;
    }
    if (vap_params->security_params.sec_method !=
            cur_vap_params->security_params.sec_method && (sec != 1))
    {
        if (vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_SHARED)
        {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_AUTHMODE, 2);
            if (status != A_STATUS_OK) {
                acfg_log_errstr("Failed Set vap param\n");
                return A_STATUS_FAILED;
            }
            enablewep = 1;
        } else if (vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_AUTO)
        {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_AUTHMODE, 4);
            if (status != A_STATUS_OK) {
                acfg_log_errstr("Failed Set vap param\n");
                return A_STATUS_FAILED;
            }
            enablewep = 1;
        } else if (vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN)
        {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_AUTHMODE, 1);
            if (status != A_STATUS_OK) {
                acfg_log_errstr("Failed Set vap param\n");
                return A_STATUS_FAILED;
            }
            enablewep = 1;
        } else if (vap_params->security_params.sec_method >= 
                ACFG_WLAN_PROFILE_SEC_METH_INVALID)
        {
            acfg_log_errstr("Invalid Security Method \n\r");
            return A_STATUS_FAILED;
        }
    }
    set_wep = ((vap_params->security_params.cipher_method ==
                ACFG_WLAN_PROFILE_CIPHER_METH_WEP) &&
            ((vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_SHARED) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_AUTO)));
    if (set_wep) 
    {
        int flag = 0;
        if (vap_params->security_params.cipher_method != 
                cur_vap_params->security_params.cipher_method) 
        {
            enablewep = 1;
            setssid = 1;
        }

        if (!ACFG_STR_MATCH(vap_params->security_params.wep_key0,
                    cur_vap_params->security_params.wep_key0) ||
                enablewep) 
        {
            if (vap_params->security_params.wep_key0[0] != '\0') {
                flag = 1;
                status = acfg_set_enc(vap_params->vap_name, flag,
                                        vap_params->security_params.wep_key0);
                if (status != A_STATUS_OK) {
                    acfg_log_errstr("Failed to set enc\n");
                    return A_STATUS_FAILED;
                }
            }
            setssid = 1;
        }
        if (!ACFG_STR_MATCH(vap_params->security_params.wep_key1,
                    cur_vap_params->security_params.wep_key1) ||
                enablewep) 
        {
            if(vap_params->security_params.wep_key1[0] != '\0') {
                flag = 2;
                status = acfg_set_enc(vap_params->vap_name, flag,
                                        vap_params->security_params.wep_key1);
                if (status != A_STATUS_OK) {
                    acfg_log_errstr("Failed to set enc\n");
                    return A_STATUS_FAILED;
                }
            }
            setssid = 1;
        }
        if (!ACFG_STR_MATCH(vap_params->security_params.wep_key2,
                    cur_vap_params->security_params.wep_key2) ||
                enablewep) 
        {
            if(vap_params->security_params.wep_key2[0] != '\0') {
                flag = 3;
                status = acfg_set_enc(vap_params->vap_name, flag,
                                        vap_params->security_params.wep_key2);
                if (status != A_STATUS_OK) {
                    acfg_log_errstr("Failed to set enc\n");
                    return A_STATUS_FAILED;
                }
            }
            setssid = 1;
        }
        if (!ACFG_STR_MATCH(vap_params->security_params.wep_key3,
                    cur_vap_params->security_params.wep_key3) ||
                enablewep) 
        {
            if(vap_params->security_params.wep_key3[0] != '\0') {
                flag = 4;
                status = acfg_set_enc(vap_params->vap_name, flag,
                                        vap_params->security_params.wep_key3);
                if (status != A_STATUS_OK) {
                    acfg_log_errstr("Failed to set enc\n");
                    return A_STATUS_FAILED;
                }
            }
            setssid = 1;
        }
        //Set default key idx
        if ((vap_params->security_params.wep_key_defidx != 0)) {
            if ((vap_params->security_params.wep_key_defidx != 
                        cur_vap_params->security_params.wep_key_defidx) ||
                    enablewep) 
            {
                flag = vap_params->security_params.wep_key_defidx;
                status = acfg_set_enc(vap_params->vap_name, flag, 0);
                if (status != A_STATUS_OK) {
                    acfg_log_errstr("Failed to set enc\n");
                    return A_STATUS_FAILED;
                }
                setssid = 1;
            }
        }
    }
    if((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN) && (sec != 1))
    {
        if (vap_params->security_params.sec_method !=
                cur_vap_params->security_params.sec_method)
        {
            if (vap_params->security_params.cipher_method ==
                    ACFG_WLAN_PROFILE_CIPHER_METH_NONE)
            {
                set_open = 1;
            }
        }
        if (vap_params->security_params.cipher_method !=
                cur_vap_params->security_params.cipher_method)
        {
            if (vap_params->security_params.cipher_method ==
                    ACFG_WLAN_PROFILE_CIPHER_METH_NONE)
            {
                set_open = 1;
            }
        }
        if (set_open) {
            status = acfg_set_auth_open(vap_params, ACFG_SEC_DISABLE_SECURITY);
            if (status != A_STATUS_OK) {
                acfg_log_errstr("%s: Failed to set auth to open (vap=%s status=%d)!\n",
                        __func__, vap_params->vap_name, status);
                return A_STATUS_FAILED;
            }
        }
        if (vap_params->security_params.sec_method != 
                cur_vap_params->security_params.sec_method) 
        {
            setssid = 1;
        }

    }
    if ((vap_params->opmode == ACFG_OPMODE_STA) &&
            ((vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_SHARED) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_AUTO)) &&
            (vap_params->wds_params.enabled == 1))
    {
        if ((!ACFG_STR_MATCH(vap_params->wds_params.wds_addr,
                        cur_vap_params->wds_params.wds_addr)) && \
                (vap_params->wds_params.wds_addr[0] != 0))
        {		
            acfg_mac_str_to_octet(vap_params->wds_params.wds_addr, mac);

            status = acfg_set_ap(vap_params->vap_name, mac);

            if (status != A_STATUS_OK) {
                acfg_log_errstr("Failed to set ROOTAP MAC\n");
                return A_STATUS_FAILED;
            }
        }
    }

    status = acfg_wlan_iface_up(vap_params->vap_name);
    if(status != A_STATUS_OK) {
        acfg_log_errstr("Failed to bring vap UP\n");
        return A_STATUS_FAILED;
    }
    if((vap_params->txpow.val != cur_vap_params->txpow.val) &&
            (vap_params->opmode == ACFG_OPMODE_HOSTAP)) {
        vap_params->txpow.flags = 0;
        vap_params->txpow.flags |= ACFG_TXPOW_FIXED;
        status = acfg_set_txpow(vap_params->vap_name,
                &vap_params->txpow);
        if(status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set txpower\n");
            return A_STATUS_FAILED;
        }
    }

    if (vap_params->vlanid != cur_vap_params->vlanid) {
        if ((cur_vap_params->vlanid == 0) && (vap_params->vlanid != 0)) {
            status = acfg_wlan_vap_profile_vlan_add(vap_params);
            if(status != A_STATUS_OK){
                acfg_log_errstr("Failed to add %s to vlan\n", vap_params->vap_name);
                return A_STATUS_FAILED;
            }
        } else if ((cur_vap_params->vlanid != 0) && (vap_params->vlanid == 0)) {
            acfg_wlan_vap_profile_vlan_remove(cur_vap_params);
        } else {
            acfg_wlan_vap_profile_vlan_remove(cur_vap_params);
            status = acfg_wlan_vap_profile_vlan_add(vap_params);
            if(status != A_STATUS_OK){
                acfg_log_errstr("Failed to add %s to vlan\n", vap_params->vap_name);
                return A_STATUS_FAILED;
            }
        }
    }

    //Set the ssid if in Station mode and security mode is open or wep
    if ((vap_params->opmode == ACFG_OPMODE_STA) && (setssid) &&
            ((vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_SHARED) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_AUTO)))
    {
        strncpy((char *)ssid.name, (char *)vap_params->ssid,
                ACFG_MAX_SSID_LEN);
        status = acfg_set_ssid(vap_params->vap_name, &ssid);
        if(status != A_STATUS_OK) {
            acfg_log_errstr("Failed to set the SSID\n");
            return A_STATUS_FAILED;
        }
    }
    return status; 
}

a_status_t
acfg_wlan_vap_profile_delete(acfg_wlan_profile_vap_params_t *vap_params)
{
    a_status_t status = A_STATUS_OK;
    a_uint8_t *vapname = vap_params->vap_name;
    a_char_t *radioname = vap_params->radio_name;
    a_int8_t sec;

    status = acfg_get_opmode(vap_params->vap_name,
            &vap_params->opmode);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: Opmode fetch fail\n", vap_params->vap_name);
    }

    if(acfg_set_security(vap_params, NULL, 
                PROFILE_DELETE, &sec) != A_STATUS_OK) {
        acfg_log_errstr("%s: Failed to delete %s security params\n", __func__,
                vap_params->vap_name);
        return A_STATUS_EINVAL;
    }

    if( (*vapname) && (*radioname)) {

        status = acfg_wlan_iface_present(radioname);

        if(status != A_STATUS_OK) {
            acfg_log_errstr("Radio Interface not present %d \n",  status);
            return A_STATUS_EINVAL;
        }

        status = acfg_wlan_iface_present((a_char_t *)vapname);

        if(status != A_STATUS_OK) {
            acfg_log_errstr("Vap is Not Present!!\n");
            return A_STATUS_FAILED;
        }

        status = acfg_delete_vap((a_uint8_t *)radioname, vapname);

        if(status != A_STATUS_OK) {
            acfg_log_errstr("Failed to delete vap!\n\a\a");
        }
    }
    return status;
}

void 
acfg_mac_to_str(a_uint8_t *addr, a_char_t *str)
{
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0],
            addr[1],
            addr[2],
            addr[3],
            addr[4],
            addr[5]);
}

void 
acfg_wlan_vap_profile_print(acfg_wlan_profile_vap_params_t *vap_params)
{
    a_uint8_t num_nodes, chain;
    a_char_t mac_addr[20];

    printf("Vap Name     : %s \n", (char *)vap_params->vap_name);
    printf("Ssid         : %s \n", (char *)vap_params->ssid);
    acfg_mac_to_str (vap_params->vap_mac, mac_addr);
    printf("Vap Mac      : %s \n", (char *)mac_addr);
    printf("Opmode       : ");
    switch (vap_params->opmode) {
        case 0:
            printf("IBSS  \n");
            break;
        case 1:
            printf("STA  \n");
            break;
        case 2:
            printf("WDS  \n");
            break;
        case 3:
            printf("AHDEMO  \n");
            break;
        case 4:
            printf("RESERVE-0  \n");
            break;
        case 5:
            printf("RESERVE-1  \n");
            break;
        case 6:
            printf("HOSTAP  \n");
            break;
        case 7:
            printf("RESERVE-2  \n");
            break;
        case 8:
            printf("MONITOR  \n");
            break;
        default :
            printf("%s: unknown VAP opmode (%d)\n", __func__, vap_params->opmode);
            break;
    }
    printf("Phymode      : " );
    switch (vap_params->phymode) {
        case 0:
            printf("AUTO \n");
            break;
        case 1:
            printf("11A  \n");
            break;
        case 2:
            printf("11B  \n");
            break;
        case 3:
            printf("11G  \n");
            break;
        case 4:
            printf("FH  \n");
            break;
        case 5:
            printf("TURBO_A  \n");
            break;
        case 6:
            printf("TURBO_G  \n");
            break;
        case 7:
            printf("11NA_HT20  \n");
            break;	
        case 8:
            printf("11NG_HT20  \n");
            break;
        case 9:
            printf("11NA_HT40PLUS  \n");
            break;
        case 10:
            printf("11NA_HT40MINUS  \n");
            break;
        case 11:
            printf("11NG_HT40PLUS  \n");
            break;
        case 12:
            printf("11NG_HT40MINUS  \n");
            break;
        case 13:
            printf("11NG_HT40  \n");
            break;
        case 14:
            printf("11NA_HT40  \n");
            break;
        case 15:
            printf("11AC_VHT20\n");
            break;
        case 16:
            printf("11AC_VHT40PLUS\n");
            break;
        case 17:
            printf("11AC_VHT40MINUS\n");
            break;
        case 18:
            printf("11AC_VHT40\n");
            break;
        case 19:
            printf("11AC_VHT80\n");
            break;
        case 20:
            printf("INVALID  \n");
            break;
        default :
            printf("%s: unknown VAP phymode (%d)\n", __func__, vap_params->phymode);
            break;
    }
    printf("Rate         : %u\n", vap_params->bitrate);
    printf("Beacon Intvl : %u\n", vap_params->beacon_interval);
    if (vap_params->rts_thresh.flags & ACFG_RTS_DISABLED) {
        printf("RTS          : Disabled \n");
    } else {
        printf("RTS          : %u\n", vap_params->rts_thresh.val);
    }
    if (vap_params->frag_thresh.flags & ACFG_FRAG_DISABLED) {
        printf("Fragment thr : Disabled \n");
    } else {
        printf("Fragment thr : %u\n", vap_params->frag_thresh.val);
    }
    if (vap_params->txpow.flags & ACFG_TXPOW_DISABLED) {
        printf("Txpower      : Disabled \n");
    } else {
        printf("Txpower      : %u\n", vap_params->txpow.val);
    }
    printf("DataRssi Avg : %u\n", vap_params->rssi.data_avg_rssi);
    for (chain = 0; chain < ACFG_MAX_ANTENNA; chain++) {
        printf("DataRssi chain%d: [ctl] %d [ext] %d\n", chain,
                vap_params->rssi.data_rssi_ctrl[chain],
                vap_params->rssi.data_rssi_ctrl[chain]);
    }
    printf("BcnRssi Avg : %u\n", vap_params->rssi.bc_avg_rssi);
    for (chain = 0; chain < ACFG_MAX_ANTENNA; chain++) {
        printf("BcnRssi chain%d: [ctl] %d [ext] %d\n", chain,
                vap_params->rssi.bc_rssi_ctrl[chain],
                vap_params->rssi.bc_rssi_ctrl[chain]);
    }
    printf("Security Method :");
    switch (vap_params->security_params.sec_method) {
        case ACFG_WLAN_PROFILE_SEC_METH_OPEN:
            printf("OPEN\n");
            break;
        case ACFG_WLAN_PROFILE_SEC_METH_SHARED:
            printf("SHARED\n");
            break;
        case ACFG_WLAN_PROFILE_SEC_METH_AUTO:
            printf("AUTO\n");
            break;
        case ACFG_WLAN_PROFILE_SEC_METH_WPA:
            printf("WPA\n");
            break;
        case ACFG_WLAN_PROFILE_SEC_METH_WPA2:
            printf("WPA2\n");
            break;
        case ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2:
            printf("WPA/WPA2 \n");
            break;
        case ACFG_WLAN_PROFILE_SEC_METH_WPA_EAP:
            printf("WPA-EAP\n");
            break;
        case ACFG_WLAN_PROFILE_SEC_METH_WPA2_EAP:
            printf("WPA2-EAP \n");
            break;
        case ACFG_WLAN_PROFILE_SEC_METH_WPS:
            printf("WPS \n");
            break;
        default:
            printf("\n");
            break;	
    }
    printf("Cipher       : ");
    if ((vap_params->security_params.cipher_method & 
                ACFG_WLAN_PROFILE_CIPHER_METH_TKIP) && 
            (vap_params->security_params.cipher_method 
             & ACFG_WLAN_PROFILE_CIPHER_METH_AES)) {
        printf("TKIP CCMP   ");
    }
    else if (vap_params->security_params.cipher_method & 
            ACFG_WLAN_PROFILE_CIPHER_METH_TKIP) 
    {
        printf("TKIP ");
    }
    else if (vap_params->security_params.cipher_method & 
            ACFG_WLAN_PROFILE_CIPHER_METH_AES)
    {
        printf("CCMP ");
    }
    else if (vap_params->security_params.cipher_method & 
            ACFG_WLAN_PROFILE_CIPHER_METH_WEP)
    {
        printf("WEP ");
    }

    else if (!(vap_params->security_params.cipher_method &
                ACFG_WLAN_PROFILE_CIPHER_METH_NONE))
    {
        printf("NONE ");
    }
    printf("\n");

    printf("Group Cipher : ");
    if (vap_params->security_params.g_cipher_method &
            ACFG_WLAN_PROFILE_CIPHER_METH_TKIP) {
        printf("TKIP ");
    }
    if (vap_params->security_params.g_cipher_method &
            ACFG_WLAN_PROFILE_CIPHER_METH_AES) {
        printf("CCMP ");
    }
    printf("\n");

    if (vap_params->security_params.cipher_method == 
            ACFG_WLAN_PROFILE_CIPHER_METH_WEP)
    {
        printf("WEP keys:\n");
        printf("[1]: %s\n",vap_params->security_params.wep_key0);
        printf("[2]: %s\n",vap_params->security_params.wep_key1);
        printf("[3]: %s\n",vap_params->security_params.wep_key2);
        printf("[4]: %s\n",vap_params->security_params.wep_key3);

    }
    if (vap_params->node_params.num_node > 0) {
        printf("acl node: ");
    }
    for (num_nodes = 0; num_nodes < vap_params->node_params.num_node;	
            num_nodes++)
    {
        printf("%02x:%02x:%02x:%02x:%02x:%02x ", 
                vap_params->node_params.acfg_acl_node_list[num_nodes][0],
                vap_params->node_params.acfg_acl_node_list[num_nodes][1],
                vap_params->node_params.acfg_acl_node_list[num_nodes][2],
                vap_params->node_params.acfg_acl_node_list[num_nodes][3],
                vap_params->node_params.acfg_acl_node_list[num_nodes][4],
                vap_params->node_params.acfg_acl_node_list[num_nodes][5]
              );
    }
    printf("\n");
    printf("Acl policy: ");
    switch (vap_params->node_params.node_acl) {
        case ACFG_WLAN_PROFILE_NODE_ACL_ALLOW:
            printf("ALLOW\n");
            break;
        case ACFG_WLAN_PROFILE_NODE_ACL_DENY:
            printf("DENY\n");
            break;
        default:
            printf("NONE\n");
            break;	
    }
}


void
acfg_wlan_profile_print(acfg_wlan_profile_t *profile)
{
    a_uint8_t i;
    a_char_t mac_addr[20];

    printf("Radio Name   : %s \n", (char *)profile->radio_params.radio_name);
    acfg_mac_to_str(profile->radio_params.radio_mac, mac_addr);
    printf("Radio Mac    : %s \n", (char *)mac_addr);
    printf("Channel      : %u \n", profile->radio_params.chan);
    printf("Freq         : %u \n", profile->radio_params.freq);
    printf("Countrycode  : %u(0x%x) \n", profile->radio_params.country_code,
            profile->radio_params.country_code);
    printf("VAP cnt      : %u\n", profile->num_vaps);
    for (i = 0; i < profile->num_vaps; i++) { 
        acfg_wlan_vap_profile_print (&profile->vap_params[i]);
        printf("::::::\n");
    }
}

void acfg_set_vap_list(acfg_wlan_profile_t *new_profile,       
        acfg_wlan_profile_t *cur_profile,
        acfg_wlan_profile_vap_list_t *create_list,
        acfg_wlan_profile_vap_list_t *delete_list,
        acfg_wlan_profile_vap_list_t *modify_list)
{
    a_uint8_t num_new_vap = 0, num_cur_vap = 0;
    acfg_wlan_profile_vap_params_t *vap_param;
    a_uint8_t vap_matched = 0;

    if (cur_profile == NULL) {
        acfg_log_errstr("%s()- Error !!Current profile cannot be NULL \n\r",__func__);
        return;
    }	
    for (num_new_vap = 0; num_new_vap < new_profile->num_vaps; 
            num_new_vap++) 	
    {
        vap_param = &new_profile->vap_params[num_new_vap];
        vap_matched = 0;
        for (num_cur_vap = 0; num_cur_vap < cur_profile->num_vaps;
                num_cur_vap++)
        {
            if (ACFG_STR_MATCH(vap_param->vap_name, 
                        cur_profile->vap_params[num_cur_vap].vap_name))
            {
                //put it to modify list
                modify_list->new_vap_idx[modify_list->num_vaps] = num_new_vap;
                modify_list->cur_vap_idx[modify_list->num_vaps] = num_cur_vap;
                modify_list->num_vaps++;	
                vap_matched = 1;
                break;
            }
        }
        if (vap_matched == 0) {
            if(vap_param->vap_name[0] == '\0')
                continue;
            //put it to create list
            create_list->new_vap_idx[create_list->num_vaps] = num_new_vap;
            create_list->num_vaps++;
            modify_list->new_vap_idx[modify_list->num_vaps] = num_new_vap;
            modify_list->cur_vap_idx[modify_list->num_vaps] = num_new_vap;
            modify_list->num_vaps++;	
        }
    }
    //Check if any vap has to be deleted
    for (num_cur_vap = 0; num_cur_vap < cur_profile->num_vaps;
            num_cur_vap++)
    {
        vap_param = &cur_profile->vap_params[num_cur_vap];
        vap_matched = 0;
        for (num_new_vap = 0; num_new_vap < new_profile->num_vaps;
                num_new_vap++)
        {
            if (ACFG_STR_MATCH(vap_param->vap_name, new_profile->vap_params[num_new_vap].vap_name))
            {
                vap_matched = 1;
                break;
            }

        }
        if (vap_matched == 0) {
            //put it to delete list
            delete_list->cur_vap_idx[delete_list->num_vaps] = num_cur_vap;
            delete_list->num_vaps++;
        }
    }
}

a_status_t
acfg_create_vaps(acfg_wlan_profile_vap_list_t *create_list, acfg_wlan_profile_t *new_profile)
{
    a_uint8_t i, vap_index;
    acfg_wlan_profile_vap_params_t *vap_profile;
    a_status_t status = A_STATUS_OK;

    for (i = 0; i < create_list->num_vaps; i++) {
        vap_index = create_list->new_vap_idx[i];
        vap_profile = &new_profile->vap_params[vap_index];
        status = acfg_wlan_vap_create(vap_profile, 
                new_profile->radio_params);

        if (status != A_STATUS_OK) {
            acfg_log_errstr("%s: Failed to create VAP profile (vap=%s status=%d)!\n",
                    __func__, vap_profile->vap_name, status);
            break;
        }
    }
    return status;
}

a_status_t
acfg_modify_profile(acfg_wlan_profile_vap_list_t *modify_list, acfg_wlan_profile_t *new_profile,
        acfg_wlan_profile_t *cur_profile, int *sec)
{
    a_uint8_t vap_index, cur_vap_index;
    acfg_wlan_profile_vap_params_t *vap_profile, *cur_vap_profile;
    a_status_t status = A_STATUS_OK;
    a_int32_t i;

    *sec = 0;
    for (i = 0; i < modify_list->num_vaps; i++) {
        vap_index = modify_list->new_vap_idx[i];
        cur_vap_index = modify_list->cur_vap_idx[i];
        vap_profile = &new_profile->vap_params[vap_index];
        cur_vap_profile = &cur_profile->vap_params[cur_vap_index];
        status = acfg_wlan_vap_profile_modify(vap_profile, cur_vap_profile,
                new_profile->radio_params);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("%s: Failed to modify VAP profile (vap=%s status=%d)!\n",
                    __func__, vap_profile->vap_name, status);
            /* In the event of failure, Undo any configurations completed on previous VAPs. 
               This is to keep the actual configuration & current profile file in sync.
               Note that the current profile will not be written in the event of failure. */
            if(i)
            {
                i--;
                for(;i>=0;i--)
                {
                    vap_index = modify_list->new_vap_idx[i];
                    cur_vap_index = modify_list->cur_vap_idx[i];
                    vap_profile = &cur_profile->vap_params[cur_vap_index];
                    cur_vap_profile = &new_profile->vap_params[vap_index];
                    status = acfg_wlan_vap_profile_modify(vap_profile, cur_vap_profile,
                                new_profile->radio_params);
                    if(status !=A_STATUS_OK)
                    {
                        acfg_log_errstr("\n\n\r*****Restoring vap(%s) configuration failed \n\r",new_profile->vap_params[vap_index].vap_name);
                    }
                    else
                        acfg_log_errstr("\n\n\r****vap(%s) configuration restored\n\r",new_profile->vap_params[vap_index].vap_name);
                
                }
                
            }     
            /* return Failure to ensure current profile file is not over-written */
            status = A_STATUS_FAILED;
            break;
        }
        *sec = 1;
    }
    return status;
}

a_status_t
acfg_set_vap_profile(acfg_wlan_profile_t *new_profile,
        acfg_wlan_profile_t *cur_profile,
        acfg_wlan_profile_vap_list_t *create_list,
        acfg_wlan_profile_vap_list_t *delete_list,
        acfg_wlan_profile_vap_list_t *modify_list)
{
    a_uint8_t i, vap_index;
    a_uint32_t send_wps_event = 0;
    acfg_wlan_profile_vap_params_t *vap_profile;
    acfg_wlan_profile_vap_params_t *cur_vap_params, *new_vap_params;
    a_status_t status;
    int sec;

    //Delete Vaps
    for (i = 0; i < delete_list->num_vaps; i++) {
        vap_index = delete_list->cur_vap_idx[i];
        vap_profile = &cur_profile->vap_params[vap_index];
        status = acfg_wlan_vap_profile_delete(vap_profile);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("%s: Failed to delete profile (status=%d)!\n", __func__, status);
            return status;
        }
        new_profile->num_vaps--;
        if (ACFG_IS_VALID_WPS(vap_profile->security_params)) {
            send_wps_event = 1;
        }
        if (ACFG_IS_SEC_ENABLED(vap_profile->security_params.sec_method)) {
            send_wps_event = 1;
        }
    }

    //Create vaps 
    status = acfg_create_vaps(create_list, new_profile);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: Failed to create profile (status=%d)!\n", __func__, status);
        return status;
    }

    for (i = 0; i < create_list->num_vaps; i++) {
        vap_index = create_list->new_vap_idx[i];
        cur_vap_params = &cur_profile->vap_params[vap_index];
        new_vap_params = &new_profile->vap_params[vap_index];
        cur_vap_params->opmode = new_vap_params->opmode;
    }

    if (cur_profile == NULL) {
        goto done;
    }

    //modify vaps
    status = acfg_modify_profile(modify_list, new_profile, cur_profile, &sec);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s:%s Failed to create profile (status=%d)!\n", __func__, __FILE__, status);
        return status;
    }
    if (sec == 1) {
        send_wps_event =1;
    }
done:
    if (send_wps_event) {
        acfg_send_interface_event(ACFG_APP_EVENT_INTERFACE_MOD, 
                strlen(ACFG_APP_EVENT_INTERFACE_MOD));
    }	
    return status;
}

a_status_t acfg_set_radio_profile_chan(acfg_wlan_profile_radio_params_t 
        *radio_params,
        acfg_wlan_profile_radio_params_t
        *cur_radio_params,
        acfg_wlan_profile_t *profile)
{
    a_status_t status = A_STATUS_OK;
    a_uint8_t i;
    acfg_wlan_profile_vap_params_t *vap_params;	

    if (radio_params->chan != cur_radio_params->chan) {
        for (i = 0; i < profile->num_vaps; i++) {
            vap_params = &profile->vap_params[i];
            if (vap_params->opmode == ACFG_OPMODE_STA) {
                continue;
            }
            status = acfg_wlan_iface_down(vap_params->vap_name);
            if(status != A_STATUS_OK) {
                acfg_log_errstr("%s: Failed to bring VAP down (vap=%s status=%d)!\n",
                        __func__, vap_params->vap_name, status);
                return A_STATUS_FAILED;
            }
        }
        for (i = 0; i < profile->num_vaps; i++) {
            vap_params = &profile->vap_params[i];
            if (vap_params->opmode == ACFG_OPMODE_STA) {
                continue;
            }
            status = acfg_set_channel(vap_params->vap_name,
                    radio_params->chan);
            if(status != A_STATUS_OK){
                acfg_log_errstr("Failed to set channel\n");
                return A_STATUS_FAILED;
            }
        }
        for (i = 0; i < profile->num_vaps; i++) {
            vap_params = &profile->vap_params[i];
            if (vap_params->opmode == ACFG_OPMODE_STA) {
                continue;
            }
            status = acfg_wlan_iface_up(vap_params->vap_name);
            if(status != A_STATUS_OK) {
                acfg_log_errstr("%s: Failed to bring VAP up (vap=%s status=%d)!\n",
                        __func__, vap_params->vap_name, status);
                return A_STATUS_FAILED;
            }
        }
    }	
    return status;
}

a_status_t acfg_set_radio_profile(acfg_wlan_profile_radio_params_t 
        *radio_params,
        acfg_wlan_profile_radio_params_t
        *cur_radio_params)
{
    a_status_t status = A_STATUS_OK;

    status = acfg_wlan_iface_present((a_char_t *)radio_params->radio_name);
    if(status != A_STATUS_OK) {
        acfg_log_errstr("Radio not present\n");
        return A_STATUS_EINVAL;
    }
    if (!ACFG_STR_MATCH((char *)radio_params->radio_mac, 
                (char *)cur_radio_params->radio_mac)) {
        status = acfg_set_ifmac ((char *)radio_params->radio_name,
                (char *)radio_params->radio_mac,
                ARPHRD_IEEE80211);
        if (status != A_STATUS_OK) {
            //return A_STATUS_FAILED;
        }	
    }
    if (radio_params->country_code != cur_radio_params->country_code) {
        if(radio_params->country_code != 0) {
            status = acfg_set_radio_param(radio_params->radio_name,
                    ACFG_PARAM_RADIO_COUNTRYID,
                    radio_params->country_code);
            if (status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }
        }
    }
    if (radio_params->ampdu != cur_radio_params->ampdu) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_AMPDU,
                !!radio_params->ampdu);
        if (status != A_STATUS_OK) {
            return A_STATUS_FAILED;
        }
    }
    if (radio_params->ampdu_limit_bytes != 
            cur_radio_params->ampdu_limit_bytes)
    {
        if (radio_params->ampdu_limit_bytes) {
            status = acfg_set_radio_param(radio_params->radio_name,
                    ACFG_PARAM_RADIO_AMPDU_LIMIT,
                    radio_params->ampdu_limit_bytes);
            if (status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }
        }
        else {
            acfg_log_errstr("Invalid value for ampdu limit \n\r");
            return A_STATUS_FAILED;
        }
    }
    if (radio_params->ampdu_subframes != cur_radio_params->ampdu_subframes)
    {
        if (radio_params->ampdu_subframes) {
            status = acfg_set_radio_param(radio_params->radio_name,
                    ACFG_PARAM_RADIO_AMPDU_SUBFRAMES,
                    radio_params->ampdu_subframes);
            if (status != A_STATUS_OK) {
                return A_STATUS_FAILED;
            }
        }
        else {
            acfg_log_errstr("Invalid value for ampdu subframes \n\r");
            return A_STATUS_FAILED;
        }
    }
    if(radio_params->macreq_enabled != cur_radio_params->macreq_enabled)
    {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_ENABLE_MAC_REQ,
                radio_params->macreq_enabled);
        if (status != A_STATUS_OK) {
            return A_STATUS_FAILED;
        }
    }
    if (radio_params->aggr_burst != cur_radio_params->aggr_burst) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_AGGR_BURST,
                radio_params->aggr_burst);
        if (status != A_STATUS_OK) {
            return A_STATUS_FAILED;
        }
    }
    if (radio_params->aggr_burst_dur != cur_radio_params->aggr_burst_dur) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_AGGR_BURST_DUR,
                radio_params->aggr_burst_dur);
        if (status != A_STATUS_OK) {
            return A_STATUS_FAILED;
        }
    }

    return status;
}

void
acfg_init_profile(acfg_wlan_profile_t *profile)
{
    acfg_wlan_profile_radio_params_t *radio_params;
    acfg_wlan_profile_vap_params_t *vap_params;
    int i;
    radio_params = &profile->radio_params;
    acfg_init_radio_params (radio_params);
    for(i = 0; i < ACFG_MAX_VAPS; i++) {
        vap_params = &profile->vap_params[i];
        acfg_init_vap_params (vap_params);
   }
}

void
acfg_init_radio_params (acfg_wlan_profile_radio_params_t *unspec_radio_params)
{
    unspec_radio_params->chan = 0;
    unspec_radio_params->freq = 0;
    unspec_radio_params->country_code = 0;
    memset(unspec_radio_params->radio_mac, 0, ACFG_MACSTR_LEN);
    unspec_radio_params->macreq_enabled = 0xff;
    unspec_radio_params->ampdu = -1;
    unspec_radio_params->ampdu_limit_bytes = 0;
    unspec_radio_params->ampdu_subframes = 0;
    unspec_radio_params->aggr_burst = -1;
    unspec_radio_params->aggr_burst_dur = 0;
}

void
acfg_init_vap_params (acfg_wlan_profile_vap_params_t *unspec_vap_params)
{
    acfg_wlan_profile_node_params_t *unspec_node_params;
    acfg_wds_params_t *unspec_wds_params;
    acfg_wlan_profile_vendor_param_t *unspec_vendor_params;
    acfg_wlan_profile_security_params_t *unspec_security_params;
    acfg_wlan_profile_sec_eap_params_t *unspec_eap_params;
    acfg_wlan_profile_sec_radius_params_t *unspec_radius_params; 
    acfg_wlan_profile_sec_acct_server_params_t *unspec_acct_params;
    acfg_wlan_profile_sec_hs_iw_param_t *unspec_hs_params;
    int j;
    memset(unspec_vap_params->vap_name, 0, ACFG_MAX_IFNAME);
    unspec_vap_params->vap_name[0]='\0';
    memset(unspec_vap_params->radio_name, 0, ACFG_MAX_IFNAME);
    unspec_vap_params->opmode = ACFG_OPMODE_INVALID;
    unspec_vap_params->vapid = 0xffffffff;
    unspec_vap_params->phymode = ACFG_PHYMODE_INVALID;
    unspec_vap_params->ampdu = -1;
    memset(unspec_vap_params->ssid, 0, ACFG_MAX_SSID_LEN);
    unspec_vap_params->bitrate = -1;
    for(j = 0; j < 16; j++)
        unspec_vap_params->rate[0] = -1;
    unspec_vap_params->retries = -1;
    unspec_vap_params->txpow.val = -1;
    unspec_vap_params->txpow.flags = 0;
    unspec_vap_params->rssi.bc_valid_mask = 0;
    unspec_vap_params->rssi.data_valid_mask = 0;
    unspec_vap_params->beacon_interval = 0;
    unspec_vap_params->rts_thresh.val = ACFG_RTS_INVALID;
    unspec_vap_params->frag_thresh.val = ACFG_FRAG_INVALID;
    memset(unspec_vap_params->vap_mac, 0, ACFG_MACSTR_LEN);
    unspec_node_params = &unspec_vap_params->node_params;
    for(j = 0; j < ACFG_MAX_ACL_NODE; j++) {
        memset(unspec_node_params->acfg_acl_node_list[j], 0, ACFG_MACADDR_LEN);
    }
    unspec_node_params->node_acl = ACFG_WLAN_PROFILE_NODE_ACL_INVALID;
    unspec_wds_params = &unspec_vap_params->wds_params;
    unspec_wds_params->enabled = -1;
    memset(unspec_wds_params->wds_addr, 0, ACFG_MACSTR_LEN);
    unspec_wds_params->wds_flags = ACFG_FLAG_INVALID;
    unspec_vap_params->vlanid = 0xFFFFFFFF;
    memset(unspec_vap_params->bridge, 0 , ACFG_MAX_IFNAME);
    for(j = 0; j < ACFG_MAX_VENDOR_PARAMS; j++) {
        unspec_vendor_params = &unspec_vap_params->vendor_param[j];
    }
    unspec_vap_params->pureg = -1;
    unspec_vap_params->puren = -1;
    unspec_vap_params->hide_ssid = -1;
    unspec_vap_params->doth = -1;
    unspec_vap_params->client_isolation = -1;
    unspec_vap_params->coext = -1;
    unspec_vap_params->uapsd = -1;
    unspec_vap_params->shortgi = -1;
    unspec_vap_params->amsdu = -1;
    unspec_vap_params->max_clients = 0;
    unspec_security_params = &unspec_vap_params->security_params;
    unspec_security_params->sec_method = ACFG_WLAN_PROFILE_SEC_METH_INVALID;
    unspec_security_params->cipher_method = ACFG_WLAN_PROFILE_CIPHER_METH_INVALID;
    unspec_security_params->g_cipher_method = ACFG_WLAN_PROFILE_CIPHER_METH_INVALID;
    memset(unspec_security_params->psk, 0, ACFG_MAX_PSK_LEN);
    memset(unspec_security_params->wep_key0, 0, ACFG_MAX_WEP_KEY_LEN);
    memset(unspec_security_params->wep_key1, 0, ACFG_MAX_WEP_KEY_LEN);
    memset(unspec_security_params->wep_key2, 0, ACFG_MAX_WEP_KEY_LEN);
    memset(unspec_security_params->wep_key3, 0, ACFG_MAX_WEP_KEY_LEN);
    unspec_security_params->wep_key_defidx = 0;
    unspec_security_params->wps_pin = 0;
    unspec_security_params->wps_flag = 0;
    memset(unspec_security_params->wps_manufacturer, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_model_name, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_model_number, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_serial_number, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_device_type, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_config_methods, 0, ACFG_WPS_CONFIG_METHODS_LEN);
    memset(unspec_security_params->wps_upnp_iface, 0, ACFG_MAX_IFNAME);
    memset(unspec_security_params->wps_friendly_name, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_man_url, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_model_desc, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_upc, 0, ACFG_WSUPP_PARAM_LEN);
    unspec_security_params->wps_pbc_in_m1 = 0;
    memset(unspec_security_params->wps_device_name , 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_rf_bands, 0, ACFG_WPS_RF_BANDS_LEN);
    unspec_eap_params = &unspec_security_params->eap_param;
    unspec_eap_params->eap_type = 0;
    memset(unspec_eap_params->identity , 0, EAP_IDENTITY_LEN);
    memset(unspec_eap_params->password , 0, EAP_PASSWD_LEN);
    memset(unspec_eap_params->ca_cert , 0, EAP_FILE_NAME_LEN);
    memset(unspec_eap_params->client_cert , 0, EAP_FILE_NAME_LEN);
    memset(unspec_eap_params->private_key , 0, EAP_FILE_NAME_LEN);
    memset(unspec_eap_params->private_key_passwd , 0, EAP_PVT_KEY_PASSWD_LEN);
    unspec_security_params->radius_retry_primary_interval = 0;
    unspec_radius_params = &unspec_security_params->pri_radius_param;
    memset(unspec_radius_params->radius_ip, 0, IP_ADDR_LEN);
    unspec_radius_params->radius_port = 0;
    memset(unspec_radius_params->shared_secret, 0 , RADIUS_SHARED_SECRET_LEN);
    unspec_radius_params = &unspec_security_params->sec1_radius_param;
    memset(unspec_radius_params->radius_ip, 0, IP_ADDR_LEN);
    unspec_radius_params->radius_port = 0;
    memset(unspec_radius_params->shared_secret, 0 , RADIUS_SHARED_SECRET_LEN);
    unspec_radius_params = &unspec_security_params->sec2_radius_param;
    memset(unspec_radius_params->radius_ip, 0, IP_ADDR_LEN);
    unspec_radius_params->radius_port = 0;
    memset(unspec_radius_params->shared_secret, 0 , RADIUS_SHARED_SECRET_LEN);
    unspec_acct_params = &unspec_security_params->pri_acct_server_param;
    memset(unspec_acct_params->acct_ip, 0, IP_ADDR_LEN);
    unspec_acct_params->acct_port = 0;
    memset(unspec_acct_params->shared_secret, 0 , ACCT_SHARED_SECRET_LEN);
    unspec_acct_params = &unspec_security_params->sec1_acct_server_param;
    memset(unspec_acct_params->acct_ip, 0, IP_ADDR_LEN);
    unspec_acct_params->acct_port = 0;
    memset(unspec_acct_params->shared_secret, 0 , ACCT_SHARED_SECRET_LEN);
    unspec_acct_params = &unspec_security_params->sec2_acct_server_param;
    memset(unspec_acct_params->acct_ip, 0, IP_ADDR_LEN);
    unspec_acct_params->acct_port = 0;
    memset(unspec_acct_params->shared_secret, 0 , ACCT_SHARED_SECRET_LEN);
    unspec_hs_params = &unspec_security_params->hs_iw_param;
    unspec_hs_params->hs_enabled = 0;
    unspec_hs_params->iw_enabled = 0;
}

acfg_wlan_profile_t * acfg_get_profile(char *radioname)
{
    acfg_wlan_profile_t *new_profile, *curr_profile;
    a_char_t curr_profile_file[64];
    int ret = 0;
    FILE *fp;

    acfg_reset_errstr();

    new_profile = malloc(sizeof(acfg_wlan_profile_t));
    if (new_profile == NULL) {
        acfg_log_errstr("%s: mem alloc failure\n", __FUNCTION__);
        return NULL;
    }
    curr_profile = malloc(sizeof(acfg_wlan_profile_t));
    if (curr_profile == NULL) {
        acfg_log_errstr("%s: mem alloc failure\n", __FUNCTION__);
        free(new_profile);
        return NULL;
    }
    memset(new_profile, 0, sizeof(acfg_wlan_profile_t));
    memset(curr_profile, 0, sizeof(acfg_wlan_profile_t));
    sprintf(curr_profile_file, "/etc/acfg_curr_profile_%s.conf.bin",radioname);
    fp =  fopen (curr_profile_file,"rb");
    if(fp == NULL) {
        acfg_log_errstr(" %s not found. Initializing profile \n\r",curr_profile_file);
        acfg_init_profile(curr_profile);
    } else {
        ret = fread(curr_profile ,1,sizeof(acfg_wlan_profile_t),fp);
        if(!ret) {
            acfg_log_errstr("ERROR !! %s could not be read!!\n\r",curr_profile_file);
            free(new_profile);
            free(curr_profile);
            return NULL;
        }
        fclose(fp);
    }
    memcpy(new_profile, curr_profile, sizeof(acfg_wlan_profile_t));
    new_profile->priv = (void*)curr_profile;
    return new_profile;
}

void acfg_free_profile(acfg_wlan_profile_t * profile)
{
    free(profile->priv);
    free(profile);
}

a_status_t 
acfg_set_profile(acfg_wlan_profile_t *new_profile, 
        acfg_wlan_profile_t *cur_profile)
{
    a_status_t   status = A_STATUS_OK;
    acfg_wlan_profile_vap_list_t create_list, modify_list, delete_list;

    memset(&create_list, 0, sizeof (acfg_wlan_profile_vap_list_t));
    memset(&delete_list, 0, sizeof (acfg_wlan_profile_vap_list_t));
    memset(&modify_list, 0, sizeof (acfg_wlan_profile_vap_list_t));

    acfg_set_vap_list(new_profile, cur_profile,
            &create_list, &delete_list, 
            &modify_list);
    if (cur_profile == NULL) {	
        status = acfg_set_radio_profile(&new_profile->radio_params,
                NULL);
    } else {
        status = acfg_set_radio_profile(&new_profile->radio_params,
                &cur_profile->radio_params);
    }
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: Failed to set radio profile (radio=%s status=%d)!\n",
                __func__, new_profile->radio_params.radio_name, status);
        return status;
    }
    status = acfg_set_vap_profile(new_profile, cur_profile,
            &create_list, &delete_list, 
            &modify_list);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: Failed to set VAP profile (vap=%s status=%d)!\n",
                __func__, new_profile->vap_params->vap_name, status);
        return status;
    }
    if (cur_profile != NULL) {
        status = acfg_set_radio_profile_chan(&new_profile->radio_params,
                &cur_profile->radio_params,
                new_profile);
        if (status != A_STATUS_OK) {
            acfg_log_errstr("%s: Failed to set radio profile channel (vap=%s status=%d)!\n",
                    __func__, new_profile->radio_params.radio_name, status);
            return status;
        }
    }
    return status; 
}

a_status_t 
acfg_write_file(acfg_wlan_profile_t *new_profile)
{
    int i=0, ret=0;
    FILE *fp;
    char curr_profile_file[64];
    a_status_t status = A_STATUS_OK;

    sprintf(curr_profile_file, "/etc/acfg_curr_profile_%s.conf.bin",new_profile->radio_params.radio_name);
    fp =  fopen (curr_profile_file,"wb");
    if(fp != NULL) {
        int valid_vaps = 0;

        acfg_print("%s: INFO: '%s' VAP cnt is %u\n", __func__, new_profile->radio_params.radio_name, new_profile->num_vaps);
        /* move valid VAPs to the front, clear all other */
        for(i=0; i < ACFG_MAX_VAPS; i++)
        {
            if (new_profile->vap_params[i].vap_name[0] == '\0')
            {
                acfg_print("%s: INFO: '%s' clearing VAP index %d\n", __func__, new_profile->radio_params.radio_name, i);
                acfg_init_vap_params(&new_profile->vap_params[i]);
                continue;
            }
            acfg_print("%s: INFO: '%s' valid VAP index %d\n", __func__, new_profile->radio_params.radio_name, i);
            if (i > valid_vaps)
            {
                acfg_print("%s: INFO: '%s' moving VAP '%s' to index %d\n", __func__,
                    new_profile->radio_params.radio_name, new_profile->vap_params[i].vap_name, valid_vaps);
                memcpy(&new_profile->vap_params[valid_vaps], &new_profile->vap_params[i], sizeof(acfg_wlan_profile_vap_params_t));
                acfg_init_vap_params(&new_profile->vap_params[i]);
            }
            valid_vaps++;
        }
        acfg_print("%s: INFO: '%s' VAP cnt is %u\n", __func__, new_profile->radio_params.radio_name, new_profile->num_vaps);
        ret = fwrite(new_profile, 1, sizeof(acfg_wlan_profile_t), fp);
        if(!ret)
            status = A_STATUS_FAILED;
        fclose(fp);
    } else {
        acfg_log_errstr("%s could not be opened for writing \n\r",curr_profile_file);
        status = A_STATUS_FAILED;
    }
    return status;
}

a_status_t
acfg_reset_cur_profile(a_char_t *radio_name)
{
    char curr_profile_file[64];
    a_status_t status = A_STATUS_OK;

    sprintf(curr_profile_file, "/etc/acfg_curr_profile_%s.conf.bin", radio_name);

    /* Do not return failure, if current profile file doesn't exist */
    errno = 0;
    if((unlink(curr_profile_file) < 0) && (errno != ENOENT)) {
        status = A_STATUS_FAILED;
    }

    return status;
}

a_status_t 
acfg_apply_profile(acfg_wlan_profile_t *new_profile)
{
    a_status_t  status = A_STATUS_OK;
    acfg_wlan_profile_t * curr_profile = NULL;

    if (new_profile == NULL)
        return A_STATUS_FAILED;

    curr_profile = (acfg_wlan_profile_t *)new_profile->priv;
    status = acfg_set_profile(new_profile, curr_profile);
    if (status == A_STATUS_OK) {
        acfg_write_file(new_profile);
    }
    return status;
}

void 
acfg_get_wep_str(a_char_t *str, a_uint8_t *key, a_uint8_t len)
{
    if (len == 5) {
        sprintf(str, "%02x%02x%02x%02x%02x", key[0],
                key[1],
                key[2],
                key[3],
                key[4]);
    }
    if (len == 13) {
        sprintf(str, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
                key[0], key[1], key[2],
                key[3], key[4], key[5],
                key[6], key[7], key[8],
                key[9], key[10], key[11],
                key[12]);
    }
    if (len == 16) {
        sprintf(str, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
                key[0], key[1], key[2],
                key[3], key[4], key[5],
                key[6], key[7], key[8],
                key[9], key[10], key[11],
                key[12], key[13], key[14],
                key[15]);
    }
}


a_status_t
acfg_get_vap_info(a_uint8_t *ifname, 
        acfg_wlan_profile_vap_params_t *vap_params)
{
    a_status_t status = A_STATUS_OK;
    acfg_macacl_t maclist;
    a_uint8_t i = 0;
    a_uint32_t val;
    acfg_ssid_t ssid;

    status = acfg_get_opmode(ifname, &vap_params->opmode);
    if(status != A_STATUS_OK){
        return status;
    }
    status = acfg_get_ssid(ifname, &ssid);
    if(status != A_STATUS_OK){
        return status;
    }
    strncpy(vap_params->ssid, ssid.name, ssid.len);

    status = acfg_get_rate(ifname, &vap_params->bitrate);
    if(status != A_STATUS_OK){
        return status;
    }
    vap_params->bitrate = vap_params->bitrate / 1000000;

    status = acfg_get_txpow(ifname, &vap_params->txpow);
    if(status != A_STATUS_OK){
        return status;
    }

    status = acfg_get_rssi(ifname, &vap_params->rssi);
    if(status != A_STATUS_OK){
        return status;
    }

    status = acfg_get_rts(ifname, &vap_params->rts_thresh);
    if(status != A_STATUS_OK){
        return status;
    }

    status = acfg_get_frag(ifname, &vap_params->frag_thresh);
    if(status != A_STATUS_OK){
        return status;
    }

    status = acfg_acl_getmac(ifname, &maclist);
    if(status != A_STATUS_OK){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_MACCMD, 
            &vap_params->node_params.node_acl);	
    if(status != A_STATUS_OK){
        return status;
    }
    for (i = 0; i < maclist.num; i++) {
        memcpy(vap_params->node_params.acfg_acl_node_list[i],
                maclist.macaddr[i], ACFG_MACADDR_LEN);
    }
    vap_params->node_params.num_node = maclist.num;

    status = acfg_get_vap_param(ifname, ACFG_PARAM_VAP_SHORT_GI, &val);
    if(status != A_STATUS_OK){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_VAP_AMPDU, &val);
    if(status != A_STATUS_OK){
        return status;
    }
    vap_params->ampdu = !!(val); /* double negation */

    status = acfg_get_vap_param(ifname, ACFG_PARAM_HIDESSID, &vap_params->hide_ssid);
    if(status != A_STATUS_OK){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_APBRIDGE, &val);
    if(status != A_STATUS_OK){
        return status;
    }
    vap_params->client_isolation = !val;

    status = acfg_get_vap_param(ifname, ACFG_PARAM_BEACON_INTERVAL, 
            &vap_params->beacon_interval);
    if(status != A_STATUS_OK){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_PUREG, &vap_params->pureg);
    if(status != A_STATUS_OK){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_UAPSD, &vap_params->uapsd);
    if(status != A_STATUS_OK){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_PUREN, &vap_params->puren);
    if(status != A_STATUS_OK){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_EXTAP, &val);
    if(status != A_STATUS_OK){
        return status;
    }
    if (val) {
        vap_params->wds_params.wds_flags |= ACFG_FLAG_EXTAP;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_DISABLECOEXT, &val);
    if(status != A_STATUS_OK){
        return status;
    }
    vap_params->coext = !!(val);

    status = acfg_get_vap_param(ifname, ACFG_PARAM_DOTH, &vap_params->doth);
    if(status != A_STATUS_OK){
        return status;
    }

    return status;	
}

a_status_t 
acfg_get_current_profile(acfg_wlan_profile_t *profile)
{
    a_status_t status = A_STATUS_FAILED, final_status = A_STATUS_OK;
    acfg_os_req_t	req = {.cmd = ACFG_REQ_GET_PROFILE};			
    acfg_radio_vap_info_t *ptr;
    int i = 0;

    ptr = &req.data.radio_vap_info;
    if (acfg_os_check_str(profile->radio_params.radio_name, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    status = acfg_os_send_req(profile->radio_params.radio_name, &req);
    if (status == A_STATUS_OK) {
        strncpy((char *)profile->radio_params.radio_name, (char *)ptr->radio_name, 32);
        memcpy(profile->radio_params.radio_mac, 
                ptr->radio_mac, 
                ACFG_MACADDR_LEN);
        profile->radio_params.chan = ptr->chan;
        profile->radio_params.freq = ptr->freq;
        profile->radio_params.country_code = ptr->country_code;

        for (i = 0; i <  ptr->num_vaps; i++) {
            status = acfg_get_vap_info(ptr->vap_info[i].vap_name, 
                    &profile->vap_params[i]);
            if(status != A_STATUS_OK){
                acfg_log_errstr("%s: Get vap info failed for %s\n", __func__, 
                        ptr->vap_info[i].vap_name);
                final_status = A_STATUS_FAILED;
                continue;
            }
            strcpy((char *)profile->vap_params[i].vap_name, (char *)ptr->vap_info[i].vap_name);
            memcpy(profile->vap_params[i].vap_mac, 
                    ptr->vap_info[i].vap_mac, 
                    ACFG_MACADDR_LEN);
            profile->vap_params[i].phymode = ptr->vap_info[i].phymode;
            profile->vap_params[i].security_params.sec_method =
                ptr->vap_info[i].sec_method;
            profile->vap_params[i].security_params.cipher_method =  
                ptr->vap_info[i].cipher;
            if (ptr->vap_info[i].wep_key_len) {
                if (ptr->vap_info[i].wep_key_idx == 0) {
                    acfg_get_wep_str(profile->vap_params[i].security_params.wep_key0,  
                            ptr->vap_info[i].wep_key,
                            ptr->vap_info[i].wep_key_len);
                } else if (ptr->vap_info[i].wep_key_idx == 1) {
                    acfg_get_wep_str(profile->vap_params[i].security_params.wep_key1,  
                            ptr->vap_info[i].wep_key,
                            ptr->vap_info[i].wep_key_len);
                } else if (ptr->vap_info[i].wep_key_idx == 2) {
                    acfg_get_wep_str(profile->vap_params[i].security_params.wep_key2,  
                            ptr->vap_info[i].wep_key,
                            ptr->vap_info[i].wep_key_len);
                } else if (ptr->vap_info[i].wep_key_idx == 3) {
                    acfg_get_wep_str(profile->vap_params[i].security_params.wep_key3,  
                            ptr->vap_info[i].wep_key,
                            ptr->vap_info[i].wep_key_len);
                }
            }
        }
        profile->num_vaps = ptr->num_vaps;	
    } else {
        acfg_log_errstr("%s: Error sending cmd\n", __func__);
    }
    return final_status;
}

void
acfg_fill_wps_config(char *ifname, char *buf)
{
    char filename[32];
    FILE *fp;

    sprintf(filename, "%s_%s.conf", ACFG_WPS_CONFIG_PREFIX, ifname);
    fp = fopen(filename, "w");
    fprintf(fp, "%s", buf);
}

a_status_t 
acfg_get_wps_cred(a_uint8_t *ifname, acfg_opmode_t opmode, 
        char *buffer, int *buflen)
{
    char cmd[255], replybuf[4096];
    a_uint32_t len;

    len = sizeof(replybuf);

    memset(cmd, '\0', sizeof(cmd));
    memset(replybuf, '\0', sizeof(replybuf));
    sprintf(cmd, "%s", "WPS_GET_CONFIG");
    if(acfg_ctrl_req(ifname, cmd, strlen(cmd),
                replybuf, &len, opmode) < 0){
        acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                cmd,
                ifname);
        return A_STATUS_FAILED;
    }
    *buflen = len;
    strcpy(buffer, replybuf);

    return A_STATUS_OK;
}


void
acfg_parse_cipher(char *value, acfg_wps_cred_t *wps_cred)
{	
    int last;
    char *start, *end, buf[255];

    sprintf(buf, "%s", value);
    start = buf;
    while (*start != '\0') {
        while (*start == ' ' || *start == '\t')
            start++;
        if (*start == '\0')
            break;
        end = start;
        while (*end != ' ' && *end != '\t' && *end != '\0' && *end != '\n')
            end++;
        last = *end == '\0';
        *end = '\0';
        if (strcmp(start, "CCMP") == 0) {
            wps_cred->enc_type |= 
                ACFG_WLAN_PROFILE_CIPHER_METH_AES;
        }	
        else if (strcmp(start, "TKIP") == 0) {
            wps_cred->enc_type |=
                ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
        }
        if (last) {
            break;
        }
        start = end + 1;
    }
}

a_status_t
acfg_parse_wpa_key_mgmt(char *value, 
        acfg_wps_cred_t *wps_cred)
{	
    int last;
    char *start, *end, *buf;

    buf = strdup(value);
    if (buf == NULL)
        return A_STATUS_FAILED;
    start = buf;
    while (*start != '\0') {
        while (*start == ' ' || *start == '\t')
            start++;
        if (*start == '\0')
            break;
        end = start;
        while (*end != ' ' && *end != '\t' && *end != '\0' && *end != '\n')
            end++;
        last = *end == '\0';
        *end = '\0';
        if (strcmp(start, "WPA-PSK") == 0) {
            wps_cred->key_mgmt = 2;
        }	
        else if (strcmp(start, "WPA-EAP") == 0) {
            wps_cred->key_mgmt = 1;
        }
        if (last) {
            break;
        }
        start = end + 1;
    }
    free(buf);
    return A_STATUS_OK;
}

int
acfg_get_wps_config(a_uint8_t *ifname, acfg_wps_cred_t *wps_cred)
{
    char filename[32];
    FILE *fp;
    char *pos;
    int val = 0, ret = 1, len = 0, buflen = 0;
    char buf[255];

    buflen = sizeof(buf);
    sprintf(filename, "/etc/%s_%s.conf", ACFG_WPS_CONFIG_PREFIX, ifname);
    fp = fopen(filename, "r");
    if (fp == NULL) {
        return -1;
    }	

    while(fgets(buf, buflen, fp)) {
        pos = buf;
        if (strncmp(pos, "wps_state=", 10) == 0) {
            pos = strchr(buf, '=');
            pos++;
            val = atoi(pos);
            if (val == 2) {
                wps_cred->wps_state = val;
                ret = 1;
            }
        } else if (strncmp(pos, "ssid=", 5) == 0) {
            pos = strchr(buf, '=');
            pos++;
            memset(wps_cred->ssid, '\0', sizeof(wps_cred->ssid));
            sprintf(wps_cred->ssid, "%s", pos);
            wps_cred->ssid[strlen(wps_cred->ssid) - 1] = '\0';
        } else if (strncmp(pos, "wpa_key_mgmt=", 13) == 0) {
            pos = strchr(buf, '=');
            pos++;
            acfg_parse_wpa_key_mgmt(pos, wps_cred);
        } else if (strncmp(pos, "wpa_pairwise=", 13) == 0) {
            pos = strchr(buf, '=');
            pos++;
            acfg_parse_cipher(pos, wps_cred);
        } else if (strncmp(pos, "wpa_passphrase=", 15) == 0) {
            pos = strchr(buf, '=');
            pos++;
            len = strlen(pos);
            if (pos[len - 1] == '\n') {
                pos[len - 1] = '\0';
                len--;
            }
            memset(wps_cred->key, '\0', sizeof(wps_cred->key));
            strncpy(wps_cred->key, pos, len);
        } else if (strncmp(pos, "wpa_psk=", 7) == 0) {
            pos = strchr(buf, '=');
            pos++;
            len = strlen(pos);
            if (pos[len - 1] == '\n') {
                pos[len - 1] = '\0';
                len--;
            }
            memset(wps_cred->key, '\0', sizeof(wps_cred->key));
            strncpy(wps_cred->key, pos, len);
        } else if (strncmp(pos, "wpa=", 4) == 0) {
            pos = strchr(buf, '=');
            pos++;
            wps_cred->wpa = atoi(pos);
        } else if (strncmp(pos, "key_mgmt=", 9) == 0) {
            pos = strchr(buf, '=');
            pos++;
            wps_cred->key_mgmt = atoi(pos);
        } else if (strncmp(pos, "auth_alg=", 9) == 0) {
            pos = strchr(buf, '=');
            pos++;
            wps_cred->auth_alg = atoi(pos);
        } else if (strncmp(pos, "proto=", 6) == 0) {
            pos = strchr(buf, '=');
            pos++;
            wps_cred->wpa = atoi(pos);
        } else if (strncmp(pos, "wep_key=", 8) == 0) {
            pos = strchr(buf, '=');
            pos++;
            len = strlen(pos);
            if (pos[len - 1] == '\n') {
                pos[len - 1] = '\0';
                len--;
            }
            memset(wps_cred->wep_key, '\0', sizeof(wps_cred->wep_key));
            strncpy(wps_cred->wep_key, pos, len);
        } else if (strncmp(pos, "wep_default_key=", 16) == 0) {
            pos = strchr(buf, '=');
            pos++;
            wps_cred->wep_key_idx = atoi(pos);
        }
    }
    fclose(fp);
    return ret;
}

void acfg_set_hs_iw_vap_param(acfg_wlan_profile_vap_params_t *vap_params)
{
    acfg_opmode_t opmode;
    acfg_opmode_t mac_addr;

    acfg_get_opmode(vap_params->vap_name, &opmode);
    acfg_get_ap(vap_params->vap_name, (acfg_macaddr_t *)&mac_addr);
    if(opmode == ACFG_OPMODE_HOSTAP)
    {
        if(vap_params->security_params.hs_iw_param.hessid[0] == 0)
            strcpy(vap_params->security_params.hs_iw_param.hessid, (char *)mac_addr);
        if(vap_params->security_params.hs_iw_param.network_type > 15)
            vap_params->security_params.hs_iw_param.network_type = DEFAULT_NETWORK_TYPE;
        if(vap_params->security_params.hs_iw_param.internet > 1)
            vap_params->security_params.hs_iw_param.internet = DEFAULT_INTERNET;
        if(vap_params->security_params.hs_iw_param.asra > 1)
            vap_params->security_params.hs_iw_param.asra = DEFAULT_ASRA;
        if(vap_params->security_params.hs_iw_param.esr > 1)
            vap_params->security_params.hs_iw_param.esr = DEFAULT_ESR;
        if(vap_params->security_params.hs_iw_param.uesa > 1)
            vap_params->security_params.hs_iw_param.uesa = DEFAULT_UESA;
        if(vap_params->security_params.hs_iw_param.venue_group >= VENUE_GROUP_RESERVED_START)
            vap_params->security_params.hs_iw_param.venue_group = DEFAULT_VENUE_GROUP;
        if(vap_params->security_params.hs_iw_param.roaming_consortium[0] == 0)
            vap_params->security_params.hs_iw_param.roaming_consortium[0] = '\0';
        if(vap_params->security_params.hs_iw_param.roaming_consortium2[0] == 0)
            vap_params->security_params.hs_iw_param.roaming_consortium2[0] = '\0';
        if(vap_params->security_params.hs_iw_param.venue_name[0] == 0)
            strcpy(vap_params->security_params.hs_iw_param.venue_name, 
                    "venue_name=eng:Wi-Fi Alliance Labs\x0a 2989 Copper Road\x0aSanta Clara, CA 95051, USA");
    }
}

#define OFFSET(a,b) ((long )&((a *) 0)->b)

struct acfg_wps_params {
    a_uint8_t name[32];
    a_uint32_t offset;
};

struct acfg_wps_params wps_device_info[] =
{
    {"wps_device_name",  OFFSET(acfg_wlan_profile_security_params_t, wps_device_name)}, 
    {"wps_device_type",  OFFSET(acfg_wlan_profile_security_params_t, wps_device_type)},
    {"wps_model_name",  OFFSET(acfg_wlan_profile_security_params_t, wps_model_name)},
    {"wps_model_number",  OFFSET(acfg_wlan_profile_security_params_t, wps_model_number)},
    {"wps_serial_number",  OFFSET(acfg_wlan_profile_security_params_t, wps_serial_number)},
    {"wps_manufacturer",  OFFSET(acfg_wlan_profile_security_params_t, wps_manufacturer)},
    {"wps_config_methods",  OFFSET(acfg_wlan_profile_security_params_t, wps_config_methods)},
};

void acfg_set_wps_default_config(acfg_wlan_profile_vap_params_t *vap_params)
{
    acfg_opmode_t opmode;

    acfg_get_opmode(vap_params->vap_name, &opmode);
    if(opmode == ACFG_OPMODE_STA)
    {
        if(vap_params->security_params.wps_config_methods[0] == 0)
            strcpy(vap_params->security_params.wps_config_methods,
                    "\"ethernet label push_button\"");
        if(vap_params->security_params.wps_device_type[0] == 0)
            strcpy(vap_params->security_params.wps_device_type, "1-0050F204-1");
        if(vap_params->security_params.wps_manufacturer[0] == 0)
            strcpy(vap_params->security_params.wps_manufacturer, "Atheros");
        if(vap_params->security_params.wps_model_name[0] == 0)
            strcpy(vap_params->security_params.wps_model_name, "cmodel");
        if(vap_params->security_params.wps_model_number[0] == 0)
            strcpy(vap_params->security_params.wps_model_number, "123");
        if(vap_params->security_params.wps_serial_number[0] == 0)
            strcpy(vap_params->security_params.wps_serial_number, "12345");
        if(vap_params->security_params.wps_device_name[0] == 0)
            strcpy(vap_params->security_params.wps_device_name, "WirelessClient");
    }
    else
    { 
        if(vap_params->security_params.wps_config_methods[0] == 0)
            strcpy(vap_params->security_params.wps_config_methods,
                    "push_button label virtual_display virtual_push_button physical_push_button");
        if(vap_params->security_params.wps_device_type[0] == 0)
            strcpy(vap_params->security_params.wps_device_type, "6-0050F204-1");
        if(vap_params->security_params.wps_manufacturer[0] == 0)
            strcpy(vap_params->security_params.wps_manufacturer, "Atheros Communications, Inc.");
        if(vap_params->security_params.wps_model_name[0] == 0)
            strcpy(vap_params->security_params.wps_model_name, "APxx");
        if(vap_params->security_params.wps_model_number[0] == 0)
            strcpy(vap_params->security_params.wps_model_number, "APxx-xxx");
        if(vap_params->security_params.wps_serial_number[0] == 0)
            strcpy(vap_params->security_params.wps_serial_number, "87654321");
        if(vap_params->security_params.wps_device_name[0] == 0)
            strcpy(vap_params->security_params.wps_device_name, "AtherosAP");
    }
}

void acfg_get_wps_dev_config(acfg_wlan_profile_vap_params_t *vap_params)
{
    FILE *fp;
    unsigned int i, offset;
    char buf[255], *pos;
    char filename[32];

    sprintf(filename, "/etc/%s_%s.conf", ACFG_WPS_DEV_CONFIG_PREFIX, vap_params->vap_name);

    fp = fopen(filename, "r");
    if(fp == NULL)
        return;

    while(fgets(buf, sizeof(buf), fp))
    {
        if(buf[0] == '#') {
            continue;
        }
        pos = buf;
        while (*pos != '\0') {
            if (*pos == '\n') {
                *pos = '\0';
                break;
            }
            pos++;
        }
        pos = strchr(buf, '=');
        if (pos == NULL) {
            continue;
        }
        *pos = '\0';
        pos++;
        for (i = 0; i < (sizeof (wps_device_info) /
                    sizeof (struct acfg_wps_params)); i++) {
            if (strcmp(buf, (char *)wps_device_info[i].name) == 0) {
                offset = wps_device_info[i].offset;
                strcpy((char *)(&vap_params->security_params) + offset, pos);
                break;
            }
        }
    }
    fclose(fp);
}

void
acfg_update_wps_dev_config_file(acfg_wlan_profile_vap_params_t *vap_params, int force_update)
{
    char filename[32];
    FILE *fp;
    acfg_wlan_profile_security_params_t security_params;
    unsigned int i;

    sprintf(filename, "/etc/%s_%s.conf", ACFG_WPS_DEV_CONFIG_PREFIX, vap_params->vap_name);

    /* Try to open the file for reading */
    fp = fopen(filename, "r");
    if(fp == NULL)
    {
        /* Create file if it doesn't exist*/
        force_update = 1; 
    }
    else
    {
        char buf[255];
        char *pos;
        int offset;

        /* make a copy of initial security_params, so that it can be used for later comparision */
        memcpy(&security_params, &vap_params->security_params, sizeof(acfg_wlan_profile_security_params_t));
        /* Read the contents and get the WPS device info */
        while(fgets(buf, sizeof(buf), fp))
        {
            if(buf[0] == '#') {
                continue;
            }
            pos = buf;
            while (*pos != '\0') {
                if (*pos == '\n') {
                    *pos = '\0';
                    break;
                }
                pos++;
            }
            pos =  strchr(buf, '=');
            if (pos == NULL) {
                continue;
            }
            *pos = '\0';
            pos++;
            for (i = 0; i < (sizeof (wps_device_info) /
                        sizeof (struct acfg_wps_params)); i++) {
                if (strcmp(buf, (char *)wps_device_info[i].name) == 0) {
                    offset = wps_device_info[i].offset;
                    strcpy((char *)(&security_params) + offset, pos);
                    break;
                }
            }
        }
        if(memcmp(&security_params, &vap_params->security_params, sizeof(acfg_wlan_profile_security_params_t)))
        {
            /* Profile is updated, so update the WPS dev file too */
            force_update = 1;
        }
    }
    if(force_update == 1)
    {
        int ret, buflen;
        char str[255], data[ACFG_MAX_WPS_FILE_SIZE];
        int len = 0;

        if(fp)
            fclose(fp);

        buflen = sizeof(data);

        for(i = 0; i < (sizeof (wps_device_info) /
                    sizeof (struct acfg_wps_params)); i++)
        {
            ret = sprintf(str, "\n%s=%s", wps_device_info[i].name, 
                    (((a_uint8_t *)&(vap_params->security_params)) + wps_device_info[i].offset));
            if (ret >= 0 && buflen > ret) 
            {
                strcat(data, str);
                buflen -= ret;
                len += ret; 
            }
        }
        acfg_update_wps_config_file(vap_params->vap_name, ACFG_WPS_DEV_CONFIG_PREFIX, data, len); 
    }
}

void 
acfg_update_wps_config_file(a_uint8_t *ifname, char *prefix, char *data, int len)
{
    char filename[32];
    FILE *fp;
    char *pos, *start;
    int ret = 0;

    sprintf(filename, "/etc/%s_%s.conf", prefix, ifname);
    fp = fopen(filename, "w");
    pos = start = data;
    while (len) {
        start = pos;
        while ((*pos != '\n') && *pos != '\0') {
            pos++;
            len--;
        }
        if (*pos == '\0') {
            ret = 1;
        }
        *pos = '\0';
        fprintf(fp, "%s\n", start);
        if (ret == 1) {
            fclose(fp);
            return;
        }
        pos++;
        len--;
        while(*pos == '\n') {
            pos++;
            len--;
        }
    }
    fclose(fp);
}

a_status_t
acfg_wps_success_cb(a_uint8_t *ifname)
{
    char data[ACFG_MAX_WPS_FILE_SIZE];
    int datalen =  0;
    acfg_wps_cred_t wps_cred;
    a_status_t status = A_STATUS_OK;
    acfg_opmode_t opmode;
    acfg_wlan_profile_vap_params_t vap_params;

    status = acfg_get_opmode(ifname, &opmode);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: Opmode fetch fail for %s\n", __func__, ifname);
        return A_STATUS_FAILED;
    }
    memset(&vap_params, 0, sizeof(acfg_wlan_profile_vap_params_t));
    memset(data, '\0', sizeof(data));
    status = acfg_get_wps_cred(ifname, opmode, data, &datalen);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: Get WPS credentials failed for %s\n", __func__, ifname);
        return A_STATUS_FAILED;
    }
    acfg_update_wps_config_file(ifname, ACFG_WPS_CONFIG_PREFIX, data, datalen);
    strcpy((char *)vap_params.vap_name, (char *)ifname);
    if (opmode == ACFG_OPMODE_STA) {
        return A_STATUS_OK;
    }
    acfg_get_wps_config(ifname, &wps_cred);
    acfg_get_wps_dev_config(&vap_params);
    if (wps_cred.wps_state == WPS_FLAG_CONFIGURED) {
        strcpy((char *)vap_params.vap_name,(char *)ifname);
        status = acfg_set_wps_vap_params(&vap_params, &wps_cred);
        if (status != A_STATUS_OK) {
            return A_STATUS_FAILED;
        }
        status = acfg_config_security(&vap_params);
        if (status != A_STATUS_OK) {
            return A_STATUS_FAILED;
        }
    }
    return A_STATUS_OK;
}

a_status_t acfg_get_iface_list(acfg_vap_list_t *list, int *count)
{
    a_status_t status = A_STATUS_FAILED;
    acfg_os_req_t	req = {.cmd = ACFG_REQ_GET_PROFILE};			
    acfg_radio_vap_info_t *ptr;
    a_uint8_t wifi_iface[4][ACFG_MAX_IFNAME] = {"wifi0", "wifi1", "wifi2", "wifi3"};
    unsigned int n;
    int num_iface = 0, i;

    for (n = 0; n < sizeof (wifi_iface) / sizeof(wifi_iface[0]); n++) {	
        status = acfg_wlan_iface_present((a_char_t *)wifi_iface[n]);
        if(status != A_STATUS_OK) {
            continue;
        }
        ptr = &req.data.radio_vap_info;
        memset(ptr, 0 , sizeof(acfg_radio_vap_info_t));
        if (acfg_os_check_str(wifi_iface[n], ACFG_MAX_IFNAME))
            return A_STATUS_ENOENT;
        status = acfg_os_send_req(wifi_iface[n], &req);

        if (status == A_STATUS_OK) {
            for (i = 0; i <  ptr->num_vaps; i++) {

                strncpy(list->iface[i + num_iface], (char *)ptr->vap_info[i].vap_name, ACFG_MAX_IFNAME);
            }
            num_iface += i;
        }
    }
    *count = num_iface;
    return A_STATUS_OK;
}

a_status_t
acfg_handle_wps_event(a_uint8_t *ifname, enum acfg_event_handler_type event) 
{
    a_status_t status = A_STATUS_OK;
    acfg_opmode_t opmode;

    status = acfg_get_opmode(ifname, &opmode);	
    if (status != A_STATUS_OK) {
        return status;
    }
    switch (event) {
        case ACFG_EVENT_WPS_NEW_AP_SETTINGS:
            if (opmode == ACFG_OPMODE_HOSTAP) {
                status = acfg_wps_success_cb(ifname);
            }
            break;
        case ACFG_EVENT_WPS_SUCCESS:
            if (opmode == ACFG_OPMODE_STA) {
                status = acfg_wps_success_cb(ifname);
            }
            break;
        default:
            return A_STATUS_ENOTSUPP;
    }
    return status;
}

a_status_t
acfg_set_wps_pin(char *ifname, int action, char *pin, char *pin_txt,
        char *bssid)
{
    a_char_t cmd[255];
    a_char_t replybuf[255];
    a_uint32_t len = 0;
    acfg_opmode_t opmode;
    acfg_vap_list_t vap_list;

    if (acfg_get_opmode((a_uint8_t *)ifname, &opmode) != A_STATUS_OK) {
        acfg_log_errstr("Opmode get failed\n");
        return -1;
    }
    acfg_get_ctrl_iface_path(ACFG_CONF_FILE, ctrl_hapd,
            ctrl_wpasupp);
    strncpy(vap_list.iface[0], ifname, ACFG_MAX_IFNAME);
    vap_list.num_iface = 1;
    if (action == ACFG_WPS_PIN_SET) {
        sprintf(cmd, "%s %s", WPA_WPS_CHECK_PIN_CMD_PREFIX, pin);
        len = sizeof (replybuf);
        if(acfg_ctrl_req((a_uint8_t *)ifname, cmd, strlen(cmd), replybuf,
                    &len, opmode) < 0){
            acfg_log_errstr("%s: Failed to set WPS pin for %s\n", __func__, ifname);
            return A_STATUS_FAILED;
        }
        if (!strncmp(replybuf, "FAIL-CHECKSUM", strlen("FAIL-CHECKSUM")) ||
                !strncmp(replybuf, "FAIL", strlen("FAIL"))) 
        {
            acfg_log_errstr("%s: Invalid pin\n", __func__);
            return A_STATUS_FAILED;
        }
    }
    if (opmode == ACFG_OPMODE_HOSTAP) {
        if (action == ACFG_WPS_PIN_SET) {
            memset(replybuf, '\0', sizeof (replybuf));
            len = sizeof (replybuf);
            sprintf(cmd, "%s %s %s %d", WPA_WPS_PIN_CMD_PREFIX,  "any",
                    pin, WPS_TIMEOUT);
            if(acfg_ctrl_req((a_uint8_t *)ifname, cmd, strlen(cmd), replybuf, &len,
                        ACFG_OPMODE_HOSTAP) < 0){
                return A_STATUS_FAILED;
            }
            strncpy(pin_txt, replybuf, len);

            memset(replybuf, '\0', sizeof (replybuf));
            len = sizeof (replybuf);
            sprintf(cmd, "%s %s %s %d", WPA_WPS_AP_PIN_CMD_PREFIX,  "set",
                    pin, WPS_TIMEOUT);
            if(acfg_ctrl_req((a_uint8_t *)ifname, cmd, strlen(cmd), replybuf, &len,
                        ACFG_OPMODE_HOSTAP) < 0){
                return A_STATUS_FAILED;
            }
            strncpy(pin_txt, replybuf, len);
        } else if (action == ACFG_WPS_PIN_RANDOM) {
            memset(replybuf, '\0', sizeof (replybuf));
            len = sizeof (replybuf);
            sprintf(cmd, "%s %s %d", WPA_WPS_AP_PIN_CMD_PREFIX,  "random",
                    WPS_TIMEOUT);
            if(acfg_ctrl_req((a_uint8_t *)ifname, cmd, strlen(cmd), replybuf, &len,
                        ACFG_OPMODE_HOSTAP) < 0){
                return A_STATUS_FAILED;
            }
            acfg_log_errstr("PIN: %s\n", replybuf);
            strncpy(pin_txt, replybuf, len);
        }
    } else if (opmode == ACFG_OPMODE_STA) {
        char bssid_str[20];
        a_uint8_t macaddr[6];

        if (action == ACFG_WPS_PIN_SET) {
            if (hwaddr_aton(bssid, macaddr) == -1) {
                sprintf(bssid_str, "any");
            } else {
                sprintf(bssid_str, "%s", bssid);
            }
            sprintf(cmd, "%s %s %s", WPA_WPS_PIN_CMD_PREFIX,
                    bssid_str, pin);
            if(acfg_ctrl_req((a_uint8_t *)ifname, cmd, strlen(cmd), replybuf, &len,
                        ACFG_OPMODE_STA) < 0){
                return A_STATUS_FAILED;
            }
        }
    }
    return A_STATUS_OK;
}

a_status_t 
acfg_set_wps_pbc(char *ifname)
{
    acfg_opmode_t opmode;
    a_char_t cmd[255], replybuf[255];
    a_uint32_t len = 0;
    a_status_t status = A_STATUS_OK;

    len = sizeof(replybuf);
    memset(cmd, '\0', sizeof(cmd));
    sprintf(cmd, "%s", WPA_WPS_PBC_CMD_PREFIX);

    status = acfg_get_opmode((a_uint8_t *)ifname, &opmode);
    if (status != A_STATUS_OK) {
        acfg_log_errstr("%s: Opmode fetch fail\n", ifname);
        return A_STATUS_FAILED;
    }

    acfg_get_ctrl_iface_path(ACFG_CONF_FILE, ctrl_hapd,
            ctrl_wpasupp);
    if(acfg_ctrl_req((a_uint8_t *)ifname, cmd, strlen(cmd),
                replybuf, &len,
                opmode) < 0){
        return A_STATUS_FAILED;
    }
    if(strncmp(replybuf, "OK", 2) != 0) {
        acfg_log_errstr("set pbc failed for %s\n", ifname);
        return A_STATUS_FAILED;
    }
    return status;
}
