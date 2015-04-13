/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _SD_H_
#define _SD_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

    sw_error_t
    sd_reg_mdio_set(a_uint32_t dev_id, a_uint32_t phy, a_uint32_t reg,
                    a_uint16_t data);

    sw_error_t
    sd_reg_mdio_get(a_uint32_t dev_id, a_uint32_t phy, a_uint32_t reg,
                    a_uint16_t * data);

    sw_error_t
    sd_reg_hdr_set(a_uint32_t dev_id, a_uint32_t reg_addr,
                   a_uint8_t * reg_data, a_uint32_t len);

    sw_error_t
    sd_reg_hdr_get(a_uint32_t dev_id, a_uint32_t reg_addr,
                   a_uint8_t * reg_data, a_uint32_t len);

    sw_error_t sd_init(a_uint32_t dev_id, ssdk_init_cfg * cfg);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _SD_H_ */
