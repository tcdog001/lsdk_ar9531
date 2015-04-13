/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _SW_API_US_H
#define _SW_API_US_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "common/sw.h"

    sw_error_t sw_uk_init(a_uint32_t nl_prot);

    sw_error_t sw_uk_cleanup(void);

    sw_error_t sw_uk_if(a_uint32_t arg_val[SW_MAX_API_PARAM]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SW_API_INTERFACE_H */
