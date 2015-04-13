/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_mib.h"
#include "fal_uk_if.h"

sw_error_t
fal_get_mib_info(a_uint32_t dev_id, fal_port_t port_id,
                 fal_mib_info_t * mib_Info)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_MIB_GET, dev_id, port_id,
                    (a_uint32_t) mib_Info);
    return rv;
}

sw_error_t
fal_mib_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MIB_STATUS_SET, dev_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_mib_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MIB_STATUS_GET, dev_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_mib_port_flush_counters(a_uint32_t dev_id, fal_port_t port_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_MIB_FLUSH_COUNTERS, dev_id, port_id);
    return rv;
}

sw_error_t
fal_mib_cpukeep_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MIB_CPU_KEEP_SET, dev_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_mib_cpukeep_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MIB_CPU_KEEP_GET, dev_id, (a_uint32_t)enable);
    return rv;
}