/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_interface_ctrl.h"
#include "fal_uk_if.h"

sw_error_t
fal_port_3az_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PORT_3AZ_STATUS_SET, dev_id, port_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_port_3az_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PORT_3AZ_STATUS_GET, dev_id, port_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_interface_mac_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MAC_MODE_SET, dev_id, port_id, (a_uint32_t)config);
    return rv;
}

sw_error_t
fal_interface_mac_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MAC_MODE_GET, dev_id, port_id, (a_uint32_t)config);
    return rv;
}

sw_error_t
fal_interface_phy_mode_set(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PHY_MODE_SET, dev_id, phy_id, (a_uint32_t)config);
    return rv;
}

sw_error_t
fal_interface_phy_mode_get(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PHY_MODE_GET, dev_id, phy_id, (a_uint32_t)config);
    return rv;
}

sw_error_t
fal_interface_fx100_ctrl_set(a_uint32_t dev_id, fal_fx100_ctrl_config_t * config)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FX100_CTRL_SET, dev_id, (a_uint32_t)config);
    return rv;
}

sw_error_t
fal_interface_fx100_ctrl_get(a_uint32_t dev_id, fal_fx100_ctrl_config_t * config)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FX100_CTRL_GET, dev_id, (a_uint32_t)config);
    return rv;
}

sw_error_t
fal_interface_fx100_status_get(a_uint32_t dev_id, a_uint32_t *status)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FX100_STATUS_GET, dev_id, (a_uint32_t)status);
    return rv;
}

sw_error_t
fal_interface_mac06_exch_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MAC06_EXCH_SET, dev_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_interface_mac06_exch_get(a_uint32_t dev_id, a_bool_t* enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MAC06_EXCH_GET, dev_id, (a_uint32_t)enable);
    return rv;
}
