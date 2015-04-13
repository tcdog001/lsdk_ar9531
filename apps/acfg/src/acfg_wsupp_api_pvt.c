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
#include <string.h>
#include <stdarg.h>

#include <acfg_api_pvt.h>
#include <acfg_wsupp_api.h>

#include "acfg_wsupp_api_pvt.h"

/* common libraries */
void *zalloc(a_uint32_t size)
{
    return calloc(1, size);
}

a_int32_t
get_random(a_uint8_t *buf, a_int32_t len)
{
    FILE *f;
    a_int32_t rc;

    f = fopen("/dev/urandom", "rb");
    if (f == NULL) {
        acfg_log_errstr("Could not open /dev/urandom.\n");
        return -1;
    }

    rc = fread(buf, 1, len, f);
    fclose(f);

    return rc != len ? -1 : 0;
}

a_int32_t
snprintf_hex(a_uint8_t *buf, a_uint32_t buf_size,
        a_uint8_t *data, a_uint32_t len,
        a_uint8_t uppercase)
{
    a_uint32_t i;
    a_uint8_t *pos = buf, *end = buf + buf_size;
    a_int32_t ret;
    if (buf_size == 0)
        return 0;
    for (i = 0; i < len; i++) {
        ret = snprintf((char *) pos, end - pos, uppercase ? "%02X" : "%02x", data[i]);
        if (ret < 0 || ret >= end - pos) {
            end[-1] = '\0';
            return pos - buf;
        }
        pos += ret;
    }
    end[-1] = '\0';
    return pos - buf;
}



/* APIS */
acfg_wsupp_hdl_t *acfg_wsupp_init(a_uint8_t *ifname, acfg_wsupp_init_flags_t flags)
{
    a_status_t status = A_STATUS_FAILED;
    acfg_os_req_t req = {.cmd = ACFG_REQ_WSUPP_INIT};
    acfg_wsupp_info_t *ptr;
    acfg_wsupp_init_flags_t *init_flags;
    struct acfg_wsupp *aptr;

    /* sanity check */
    if (!ifname) 
        ifname = (a_uint8_t *) WSUPP_DEFAULT_IFNAME;
    if (acfg_os_check_str(ifname, ACFG_MAX_IFNAME))
        return NULL;

    aptr = zalloc(sizeof(*aptr));
    if (!aptr)
        return NULL;

    ptr = &req.data.wsupp_info;
    init_flags = &ptr->u.init_flags;

    /* initialize wsupp context */
    get_random(aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(aptr->ifname, ifname, ACFG_MAX_IFNAME);

    /* initialize wsupp request */
    memcpy(ptr->unique, aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(ptr->ifname, aptr->ifname, ACFG_MAX_IFNAME);
    *init_flags = flags;

    /* issue wsupp request */
    status = acfg_os_send_req(ifname, &req);
    if (status != A_STATUS_OK) {
        free(aptr);
        return NULL;
    }

    return (void *) aptr;
}

void acfg_wsupp_uninit(acfg_wsupp_hdl_t *mctx)
{
    acfg_os_req_t req = {.cmd = ACFG_REQ_WSUPP_FINI};
    acfg_wsupp_info_t *ptr;
    struct acfg_wsupp *aptr;

    aptr = (struct acfg_wsupp *) mctx;
    if (!aptr)
        return;
    ptr = &req.data.wsupp_info;

    /* initialize wsupp request */
    memcpy(ptr->unique, aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(ptr->ifname, aptr->ifname, ACFG_MAX_IFNAME);

    /* issue wsupp request */
    acfg_os_send_req(aptr->ifname, &req);

    free(aptr);
}

/*
 * @brief Add a WLAN n/w interface for configuration
 * This only works if there is a global, control interface working.
 *
 * @return status of operation
 */

a_status_t acfg_wsupp_if_add(acfg_wsupp_hdl_t *mctx, 
        char *ifname)
{
    a_status_t status = A_STATUS_FAILED;
    struct acfg_wsupp *aptr;
    acfg_os_req_t req = {.cmd = ACFG_REQ_WSUPP_IF_ADD};
    acfg_wsupp_info_t *ptr;

    aptr = (struct acfg_wsupp *)mctx;
    if (!aptr)
        return A_STATUS_EINVAL;

    /* sanity check */
    if (!ifname)
        return A_STATUS_EINVAL;
    if (acfg_os_check_str((a_uint8_t *)ifname, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    ptr = &req.data.wsupp_info;
    acfg_os_strcpy(ptr->unique, aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(ptr->ifname, (a_uint8_t *)ifname, ACFG_MAX_IFNAME);

    /* issue wsupp request */
    status = acfg_os_send_req(aptr->ifname, &req);
    if (status != A_STATUS_OK) {
        return A_STATUS_FAILED;
    }

    return status;
}

/*
 * @brief remove a WLAN n/w interface for configuration
 * This only works if there is a global, control interface working.
 *
 * @return status of operation
 */
a_status_t acfg_wsupp_if_remove(acfg_wsupp_hdl_t *mctx,
        char *ifname)
{
    a_status_t status = A_STATUS_FAILED;
    struct acfg_wsupp *aptr;
    acfg_os_req_t req = {.cmd = ACFG_REQ_WSUPP_IF_RMV};
    acfg_wsupp_info_t *ptr;

    aptr = (struct acfg_wsupp *)mctx;
    if (!aptr)
        return A_STATUS_EINVAL;

    /* sanity check */
    if (!ifname)
        return A_STATUS_EINVAL;
    if (acfg_os_check_str((a_uint8_t *)ifname, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    ptr = &req.data.wsupp_info;
    acfg_os_strcpy(ptr->unique, aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(ptr->ifname, (a_uint8_t *)ifname, ACFG_MAX_IFNAME);

    /* issue wsupp request */
    status = acfg_os_send_req(aptr->ifname, &req);
    if (status != A_STATUS_OK) {
        return A_STATUS_FAILED;
    }

    return status;
}

/*
 * @brief Add a new network to an interface - it must later be configured
 * and enabled before use.
 *
 * @return status of operation
 * @return also the new network_id
 */
a_status_t acfg_wsupp_nw_create(acfg_wsupp_hdl_t *mctx,
        char *ifname,
        int *network_id)
{
    a_status_t status = A_STATUS_FAILED;
    struct acfg_wsupp *aptr;
    acfg_os_req_t req = {.cmd = ACFG_REQ_WSUPP_NW_CRT};
    acfg_wsupp_info_t *ptr;

    aptr = (struct acfg_wsupp *)mctx;
    if (!aptr)
        return A_STATUS_EINVAL;

    /* sanity check */
    if (!ifname)
        return A_STATUS_EINVAL;
    if (acfg_os_check_str((a_uint8_t *)ifname, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    ptr = &req.data.wsupp_info;
    acfg_os_strcpy(ptr->unique, aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(ptr->ifname, (a_uint8_t *)ifname, ACFG_MAX_IFNAME);

    /* issue wsupp request */
    status = acfg_os_send_req(aptr->ifname, &req);

    if(status == A_STATUS_OK)
        *network_id = req.data.wsupp_info.u.nw_cfg.networkid;

    return status;
}

/*
 * @brief disconnects, and removes a network
 *
 * @return status of operation
 */
a_status_t acfg_wsupp_nw_delete(acfg_wsupp_hdl_t *mctx,
        char *ifname,
        int network_id)
{
    a_status_t status = A_STATUS_FAILED;
    struct acfg_wsupp *aptr;
    acfg_os_req_t req = {.cmd = ACFG_REQ_WSUPP_NW_DEL};
    acfg_wsupp_info_t *ptr;

    aptr = (struct acfg_wsupp *)mctx;
    if (!aptr)
        return A_STATUS_EINVAL;

    /* sanity check */
    if (!ifname)
        return A_STATUS_EINVAL;
    if (acfg_os_check_str((a_uint8_t *)ifname, ACFG_MAX_IFNAME))
        return A_STATUS_ENOENT;

    ptr = &req.data.wsupp_info;
    acfg_os_strcpy(ptr->unique, aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(ptr->ifname, (a_uint8_t *)ifname, ACFG_MAX_IFNAME);
    ptr->u.nw_cfg.networkid = network_id;

    /* issue wsupp request */
    status = acfg_os_send_req(aptr->ifname, &req);
    if (status != A_STATUS_OK) {
        return A_STATUS_FAILED;
    }

    return status;
}

/**
 * @brief sets an item number to the passed string.
 * once a network is enabled, some attributes may no longer be set.
 *
 * @return status of operation
 */
a_status_t acfg_wsupp_nw_set(acfg_wsupp_hdl_t *mctx,
        int network_id,
        enum acfg_wsupp_nw_item item,
        char *in_string)
{
    a_status_t status = A_STATUS_FAILED;
    struct acfg_wsupp *aptr;
    acfg_os_req_t req = {.cmd = ACFG_REQ_WSUPP_NW_SET};
    acfg_wsupp_info_t *ptr;

    aptr = (struct acfg_wsupp *)mctx;
    if (!aptr)
        return A_STATUS_EINVAL;

    ptr = &req.data.wsupp_info;
    acfg_os_strcpy(ptr->unique, aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(ptr->ifname, aptr->ifname, ACFG_MAX_IFNAME);
    ptr->u.nw_cfg.networkid = network_id;
    ptr->u.nw_cfg.item = item;
    if (in_string)
        acfg_os_strcpy(ptr->u.nw_cfg.param_str, (a_uint8_t *)in_string, ACFG_WSUPP_PARAM_LEN);

    /* issue wsupp request */
    status = acfg_os_send_req(aptr->ifname, &req);
    if (status != A_STATUS_OK) {
        return A_STATUS_FAILED;
    }

    return status;
}

/**
 * @brief gets an item attribute of certain network id
 *
 * @return status of operation
 */
a_status_t acfg_wsupp_nw_get(acfg_wsupp_hdl_t *mctx,
        int network_id,
        enum acfg_wsupp_nw_item item,
        char *reply, size_t *reply_len)
{
    a_status_t status = A_STATUS_FAILED;
    struct acfg_wsupp *aptr;
    acfg_os_req_t req = {.cmd = ACFG_REQ_WSUPP_NW_GET};
    acfg_wsupp_info_t *ptr;

    aptr = (struct acfg_wsupp *)mctx;
    if (!aptr)
        return A_STATUS_EINVAL;

    ptr = &req.data.wsupp_info;
    acfg_os_strcpy(ptr->unique, aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(ptr->ifname, aptr->ifname, ACFG_MAX_IFNAME);
    ptr->u.nw_cfg.networkid = network_id;
    ptr->u.nw_cfg.item = item;
    ptr->u.nw_cfg.reply = (a_uint8_t *)reply;

    /* issue wsupp request */
    status = acfg_os_send_req(aptr->ifname, &req);
    if (status == A_STATUS_OK)
        *reply_len = ptr->u.nw_cfg.reply_len;

    return status;
}

/*
 * @brief Lists the configured networks, including stored information for
 * persistent groups. The identifier in this list is used with
 * p2p_group_add and p2p_invite to indicate which persistent group is to be
 * reinvoked.
 *
 * @return  status of operation
 */
a_status_t acfg_wsupp_nw_list(acfg_wsupp_hdl_t *mctx,
        char *reply, size_t *reply_len)
{
    a_status_t status = A_STATUS_FAILED;
    struct acfg_wsupp *aptr;
    acfg_os_req_t req = {.cmd = ACFG_REQ_WSUPP_NW_LIST};
    acfg_wsupp_info_t *ptr;

    aptr = (struct acfg_wsupp *)mctx;
    if (!aptr)
        return A_STATUS_EINVAL;

    ptr = &req.data.wsupp_info;
    acfg_os_strcpy(ptr->unique, aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(ptr->ifname, aptr->ifname, ACFG_MAX_IFNAME);
    ptr->u.nw_cfg.reply = (a_uint8_t *)reply;

    /* issue wsupp request */
    status = acfg_os_send_req(aptr->ifname, &req);
    if(status == A_STATUS_OK)
        *reply_len = ptr->u.nw_cfg.reply_len;

    return status;
}

/*
 * @brief Request a wps call to handle various security requests
 *
 * @return  status of operation
 */
a_status_t acfg_wsupp_wps_req(acfg_wsupp_hdl_t *mctx, enum acfg_wsupp_wps_type wps,
        const char *params,
        char *reply, size_t *reply_len)
{ 
    a_status_t status = A_STATUS_OK;
    struct acfg_wsupp *aptr;
    acfg_os_req_t req = {.cmd = ACFG_REQ_WSUPP_WPS_REQ};
    acfg_wsupp_info_t *ptr;

    aptr = (struct acfg_wsupp *)mctx;
    if (!aptr)
        return A_STATUS_EINVAL;

    ptr = &req.data.wsupp_info;
    acfg_os_strcpy(ptr->unique, aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(ptr->ifname, aptr->ifname, ACFG_MAX_IFNAME);
    ptr->u.wps_cfg.type = wps;
    if (!params)
        return A_STATUS_EINVAL;
    acfg_os_strcpy(ptr->u.wps_cfg.param_str, (a_uint8_t *)params, ACFG_WSUPP_PARAM_LEN);
    ptr->u.wps_cfg.reply = (a_uint8_t *)reply;

    /* issue wsupp request */
    status = acfg_os_send_req(aptr->ifname, &req);
    if (status != A_STATUS_OK) {
        return A_STATUS_FAILED;
    }

    *reply_len = ptr->u.wps_cfg.reply_len;

    return status;
}

/*
 * @brief set some supplicant parameter at run time.
 */
a_status_t acfg_wsupp_set(acfg_wsupp_hdl_t *mctx, enum acfg_wsupp_set_type type,
        int val, char *str)
{
    a_status_t status = A_STATUS_OK;
    struct acfg_wsupp *aptr;
    acfg_os_req_t req = {.cmd = ACFG_REQ_WSUPP_SET};
    acfg_wsupp_info_t *ptr;

    aptr = (struct acfg_wsupp *)mctx;
    if (!aptr)
        return A_STATUS_EINVAL;

    ptr = &req.data.wsupp_info;
    acfg_os_strcpy(ptr->unique, aptr->unique, ACFG_WSUPP_UNIQUE_LEN);
    acfg_os_strcpy(ptr->ifname, aptr->ifname, ACFG_MAX_IFNAME);
    ptr->u.rt_cfg.type = type;
    if (val)
        ptr->u.rt_cfg.param_val = val;
    if (str)
        acfg_os_strcpy(ptr->u.rt_cfg.param_str, (a_uint8_t *)str, ACFG_WSUPP_PARAM_LEN);

    /* issue wsupp request */
    status = acfg_os_send_req(aptr->ifname, &req);
    if (status != A_STATUS_OK) {
        return A_STATUS_FAILED;
    }

    return status;
}

