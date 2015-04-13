#include<stdio.h>
#include<string.h>
#include <unistd.h>

#include<acfg_api_types.h>
#include<acfg_api.h>
#include <acfg_drv_if.h>
#include<acfg_misc.h>
#include<acfg_tool.h>
#include <acfg_config_file.h>
#include <acfg_wsupp_api.h>


/**
 * @brief Strings describing type of parameter
 */
char *type_desc [] = {
    [PARAM_UINT8]   = "uint8",
    [PARAM_UINT16]  = "uint16",
    [PARAM_UINT32]  = "uint32",
    [PARAM_SINT8]   = "sint8",
    [PARAM_SINT16]  = "sint16",
    [PARAM_SINT32]  = "sint32",
    [PARAM_STRING]  = "string",
};


/* -------------------------------
 *   Wrapper function prototypes
 * -------------------------------
 */
#define wrap_proto(name)    int wrap_##name(char *params[])

wrap_proto(create_vap);
wrap_proto(delete_vap);
wrap_proto(set_ssid);
wrap_proto(get_ssid);
wrap_proto(set_testmode);
wrap_proto(get_testmode);
wrap_proto(get_rssi);
wrap_proto(get_custdata);
wrap_proto(set_channel);
wrap_proto(get_channel);
wrap_proto(set_opmode);
wrap_proto(get_opmode);
wrap_proto(set_freq);
wrap_proto(get_freq);
wrap_proto(get_rts);
wrap_proto(get_frag);
wrap_proto(get_txpow);
wrap_proto(get_ap);
wrap_proto(set_enc);
wrap_proto(set_vap_vendor_param);
wrap_proto(set_vap_param);
wrap_proto(get_vap_param);
wrap_proto(set_radio_param);
wrap_proto(get_radio_param);
wrap_proto(get_rate);
wrap_proto(set_phymode);
wrap_proto(vlgrp_create);
wrap_proto(vlgrp_delete);
wrap_proto(vlgrp_addvap);
wrap_proto(vlgrp_delvap);
wrap_proto(wlan_profile_create);
wrap_proto(wlan_profile_create_from_file);
wrap_proto(wlan_profile_get);
wrap_proto(is_offload_vap);

/* security functions */
wrap_proto(wsupp_init);

/*----------------------------------------------------
  Wrapper Functions
  -----------------------------------------------------
 */

param_info_t wrap_set_profile_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"file", PARAM_STRING, "file name"},
};

int wrap_set_profile(char *params[])
{
    int status = A_STATUS_OK;
    acfg_wlan_profile_t *new_profile;
    int i = 0;

    /* Get New Profile */
    new_profile = acfg_get_profile(params[0]);

    /* Read New profile from user & populate new_profile */
    status = acfg_read_file(params[1], new_profile);	
    if(status < 0 ) {
        printf("New profile could not be read \n\r");
        return A_STATUS_FAILED;
    }
    for (i = 0; i < new_profile->num_vaps; i++) {
        strcpy((char *)new_profile->vap_params[i].radio_name, 
                (char *)new_profile->radio_params.radio_name);
    }
    strcpy(ctrl_hapd, new_profile->ctrl_hapd);
    strcpy(ctrl_wpasupp, new_profile->ctrl_wpasupp);

    /* Apply the new profile */
    status = acfg_apply_profile(new_profile); 			

    if(status == A_STATUS_OK)
        printf("Configuration Completed \n\r");
  
    /* Free cur_profile & new_profile */ 
    acfg_free_profile(new_profile);
    
    return status;	
}

param_info_t wrap_reset_profile_params[] = {
    {"radio", PARAM_STRING, "radio name"},
};

/**
 * @ Force profile reset. 
 * @ Deletes the current profile for the respective radio
 *
 * @param radio name
 *
 * @return
 */
int wrap_reset_profile(char *params[])
{
    int status = A_STATUS_OK;

    /* Takes radioname as input & delete the 
       respective current profile file */
    status = acfg_reset_cur_profile(params[0]);	
    if(status < 0 ) {
        printf("Reset profile failed \n\r");
        return A_STATUS_FAILED;
    }
    return A_STATUS_OK;	
}

param_info_t wrap_create_vap_params[] = {
    {"wifi", PARAM_STRING , "radio name"},
    {"vap", PARAM_STRING , "vap name"},
    {"opmode", PARAM_UINT32 , "opmode"},
    {"vapid", PARAM_SINT32 , "vapid"},
    {"flags", PARAM_UINT32 , "integer representing the bitwise"\
        "OR of vapinfo flags"},
};

/**
 * @brief Wrapper for acfg_create_vap
 *
 * @param params[]
 *
 * @return
 */
int wrap_create_vap(char *params[])
{
    a_uint8_t  *wifi , *vap ;
    a_int32_t   vapid;
    acfg_opmode_t mode ;
    acfg_vapinfo_flags_t flags ;

    a_status_t status = A_STATUS_FAILED ;

    /*printf("%s(): Param passed  - ",__FUNCTION__);*/

    wifi = (a_uint8_t *)params[0] ;
    vap = (a_uint8_t *)params[1] ;
    get_uint32(params[2], (a_uint32_t *)&mode);
    get_uint32(params[3], (a_uint32_t *)&vapid);
    get_uint32(params[4] , (a_uint32_t *)&flags);

    dbg_print_params("wifi - %s; vap - %s; mode - 0x%x; vapid - %d; flags - 0x%x",\
            wifi, vap, mode, vapid, flags);

    status = acfg_create_vap( wifi, vap, mode, vapid, flags);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_delete_vap_params[] = {
    {"radio", PARAM_STRING , "radio name"},
    {"vap", PARAM_STRING , "vap name"},
};


/**
 * @brief Wrapper for acfg_delete_vap
 *
 * @param params[]
 *
 * @return
 */
int wrap_delete_vap(char *params[])
{

    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("radio - %s; vap - %s ",params[0], params[1]);

    status = acfg_delete_vap((a_uint8_t *)params[0], (a_uint8_t *)params[1]);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_ssid_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"ssid", PARAM_STRING , "ssid to set"},
};


/**
 * @brief  Wrapper for acfg_set_ssid
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_ssid(char *params[])
{
    acfg_ssid_t ssid ;
    a_status_t status = A_STATUS_FAILED ;

    strncpy((char *)ssid.name,params[1],ACFG_MAX_SSID_LEN);
    ssid.len = strlen((char *)ssid.name);

    dbg_print_params("vap - %s; ssid - %s; ssid len - %d",\
            params[0], ssid.name, ssid.len);

    status = acfg_set_ssid((a_uint8_t *)params[0], &ssid);
    if (status != A_STATUS_OK)
        printf("%s: setssid failed\n", __func__);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_ssid_params[] = {
    {"vap", PARAM_STRING , "vap name"},
};


/**
 * @brief Wrapper for acfg_get_ssid
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_ssid(char *params[])
{
    acfg_ssid_t ssid ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s",params[0]);

    status = acfg_get_ssid((a_uint8_t *)params[0], &ssid);
    msg("SSID - %s, SSID len - %d",(a_char_t *)ssid.name,ssid.len);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_rssi_params[] = {
    {"vap", PARAM_STRING , "vap name"},
};

/**
 * @brief Wrapper for acfg_get_rssi
 *
 * @param params[]
 *
 * @return 
 */
int wrap_get_rssi(char *params[])
{
    acfg_rssi_t rssi ;
    a_status_t status = A_STATUS_FAILED ;
    a_uint8_t   i;

    dbg_print_params("vap - %s",params[0]);

    status = acfg_get_rssi((a_uint8_t *)params[0], &rssi);

    for(i=0; i<ACFG_MAX_ANTENNA;i++){
        if(!(rssi.bc_valid_mask&(1<<i)))
            rssi.bc_rssi_ctrl[i] = rssi.bc_rssi_ext[i] = 0;

        if(!(rssi.data_valid_mask&(1<<i)))
            rssi.data_rssi_ctrl[i] = rssi.data_rssi_ext[i] = 0;
    }

    msg("Beacon RSSIavg=%d", rssi.bc_avg_rssi);
    msg("Beacon RSSIctl=%d %d %d", rssi.bc_rssi_ctrl[0],
            rssi.bc_rssi_ctrl[1], rssi.bc_rssi_ctrl[2]);
    msg("Beacon RSSIext=%d %d %d", rssi.bc_rssi_ext[0],
            rssi.bc_rssi_ext[1], rssi.bc_rssi_ext[2]);

    msg("Data RSSIavg=%d", rssi.data_avg_rssi);
    msg("Data RSSIctl=%d %d %d", rssi.data_rssi_ctrl[0],
            rssi.data_rssi_ctrl[1],
            rssi.data_rssi_ctrl[2]);
    msg("Data RSSIext=%d %d %d", rssi.data_rssi_ext[0],
            rssi.data_rssi_ext[1], rssi.data_rssi_ext[2]);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_custdata_params[] = {
    {"vap", PARAM_STRING , "vap name"},
};

/**
 * @brief Wrapper for acfg_get_custdata
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_custdata(char *params[])
{
    acfg_custdata_t custdata ;
    a_status_t status = A_STATUS_FAILED ;
    a_uint8_t   i;

    dbg_print_params("vap - %s",params[0]);

    status = acfg_get_custdata((a_uint8_t *)params[0], &custdata);

    msg("CustData=%s", custdata.custdata);
    msg("Rawdata");
    for(i=0; i<ACFG_CUSTDATA_LENGTH;i++){
        printf("%02x ", custdata.custdata[i]);
    }
    printf("\n");
    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_testmode_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"param", PARAM_STRING , "param name"},
};

/**
 * @brief Wrapper for acfg_get_testmode
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_testmode(char *params[])
{
    acfg_testmode_t testmode ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s",params[0]);

    memset(&testmode, 0, sizeof(acfg_testmode_t));

    if (!strcmp(params[1], "bssid")) {
        testmode.operation = ACFG_TESTMODE_BSSID;
    }
    else if (!strcmp(params[1], "chan")) {
        testmode.operation = ACFG_TESTMODE_CHAN;
    }
    else if (!strcmp(params[1], "rx")) {
        testmode.operation = ACFG_TESTMODE_RX;
    }
    else if (!strcmp(params[1], "result")) {
        testmode.operation = ACFG_TESTMODE_RESULT;
    }
    else if (!strcmp(params[1], "ant")) {
        testmode.operation = ACFG_TESTMODE_ANT;
    }
    else {
        msg("!! ERROR !! \n");
        msg("Choose one from the list below -->\n");
        msg("                 bssid \n");
        msg("                 chan \n");
        msg("                 rx \n");
        msg("                 result \n");
        msg("                 ant \n");
        return 0;
    }

    status = acfg_get_testmode((a_uint8_t *)params[0], &testmode);

    if (!strcmp(params[1], "bssid")) {
        msg("bssid=%02x:%02x:%02x:%02x:%02x:%02x\n",
                testmode.bssid[0],
                testmode.bssid[1],
                testmode.bssid[2],
                testmode.bssid[3],
                testmode.bssid[4],
                testmode.bssid[5]);
    }
    else if (!strcmp(params[1], "chan")) {
        msg("chan=%d\n", testmode.chan);
    }
    else if (!strcmp(params[1], "rx")) {
        msg("rx=%d\n", testmode.rx);
    }
    else if (!strcmp(params[1], "result")) {
        msg("rssi=%d, %d, %d, %d\n",
                testmode.rssi_combined,
                testmode.rssi0,
                testmode.rssi1,
                testmode.rssi2);

    }
    else if (!strcmp(params[1], "ant")) {
        msg("ant=%d\n", testmode.antenna);
    }

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_testmode_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"param", PARAM_STRING , "param name"},
    {"setvalue", PARAM_STRING , "set value"},
};

/**
 * @brief Wrapper for acfg_set_testmode
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_testmode(char *params[])
{
    acfg_testmode_t testmode ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s",params[0]);

    memset(&testmode, 0, sizeof(acfg_testmode_t));

    if (!strcmp(params[1], "bssid")) {
        testmode.operation = ACFG_TESTMODE_BSSID;

        if((strlen(params[2]) == ACFG_MAC_STR_LEN)) {
            unsigned int addr[ACFG_MACADDR_LEN];
            int i;

            sscanf(params[2],"%x:%x:%x:%x:%x:%x",(unsigned int *)&addr[0],\
                    (unsigned int *)&addr[1],\
                    (unsigned int *)&addr[2],\
                    (unsigned int *)&addr[3],\
                    (unsigned int *)&addr[4],\
                    (unsigned int *)&addr[5] );

            for(i=0; i<ACFG_MACADDR_LEN; i++) {
                testmode.bssid[i] = addr[i];
            }
        }
    }
    else if (!strcmp(params[1], "chan")) {
        testmode.operation = ACFG_TESTMODE_CHAN;
        testmode.chan = atoi(params[2]);
        msg("wrap_get_testmode: operation=%d, chan=%d\n",
                testmode.operation, testmode.chan);
    }
    else if (!strcmp(params[1], "rx")) {
        testmode.operation = ACFG_TESTMODE_RX;
        testmode.rx = atoi(params[2]);
    }
    else if (!strcmp(params[1], "ant")) {
        testmode.operation = ACFG_TESTMODE_ANT;
        testmode.antenna = atoi(params[2]);
    }
    else {
        msg("!! ERROR !! \n");
        msg("Choose one from the list below -->\n");
        msg("                 bssid [BSSID]\n");
        msg("                 chan [ChanID]\n");
        msg("                 rx [1|0]\n");
        msg("                 ant [0|1|2]\n");
        return 0;
    }

    status = acfg_set_testmode((a_uint8_t *)params[0], &testmode);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_channel_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"channel", PARAM_UINT8, "channel number"},
};


/**
 * @brief Wrapper for acfg_set_channel
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_channel(char *params[])
{
    a_uint32_t chan ;
    a_status_t status = A_STATUS_FAILED ;

    get_uint32(params[1], (a_uint32_t *) &chan);
    dbg_print_params("vap - %s; channel - %d",params[0],chan);

    status = acfg_set_channel((a_uint8_t *)params[0], chan);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_channel_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_channel
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_channel(char *params[])
{
    a_uint8_t chan ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s; channel - %d",params[0],chan);

    status = acfg_get_channel((a_uint8_t *)params[0], &chan);
    msg("Channel - %d",chan);

    return acfg_to_os_status(status) ;
}



param_info_t wrap_set_opmode_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"opmode", PARAM_UINT32, "operation mode to set"},
};


/**
 * @brief Wrapper for acfg_set_opmode
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_opmode(char *params[])
{
    acfg_opmode_t opmode ;
    a_status_t status = A_STATUS_FAILED ;

    get_uint32(params[1], (a_uint32_t *)&opmode);

    dbg_print_params("vap - %s; opmode - %d",params[0],opmode);

    status = acfg_set_opmode((a_uint8_t *)params[0], opmode);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_opmode_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_opmode
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_opmode(char *params[])
{
    acfg_opmode_t opmode ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s;",params[0]);

    status = acfg_get_opmode((a_uint8_t *)params[0], &opmode);
    msg("Opmode - %d",opmode);

    return acfg_to_os_status(status) ;
}



param_info_t wrap_set_freq_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"freq", PARAM_STRING, "frequency in MHz"},
};


/**
 * @brief Wrapper for acfg_set_freq
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_freq(char *params[])
{
    a_uint32_t freq ;
    a_status_t status = A_STATUS_FAILED ;

    get_uint32(params[1], &freq);
    dbg_print_params("vap - %s; frequency - %dMHz",params[0],freq);

    status = acfg_set_freq((a_uint8_t *)params[0], freq);

    return acfg_to_os_status(status) ;
}



param_info_t wrap_get_freq_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_freq
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_freq(char *params[])
{
    a_uint32_t freq ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s;",params[0]);

    status = acfg_get_freq((a_uint8_t *)params[0], &freq);
    msg("Frequency - %dMHz",freq);

    return acfg_to_os_status(status) ;
}




param_info_t wrap_set_rts_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"rts", PARAM_STRING, "rts value"},
    {"fixed", PARAM_STRING, "fixed?"},
};


/**
 * @brief Wrapper for acfg_set_rts
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_rts(char *params[])
{
    acfg_rts_t rts ;
    a_status_t status = A_STATUS_FAILED ;
    rts.flags = 0;
    a_uint32_t temp = 0;

    if(!strncmp(params[1], "off", 3))
        rts.flags |= ACFG_RTS_DISABLED;
    else
        get_uint32(params[1], &rts.val);

    get_uint32(params[2], &temp);

    if(temp == 1)
        rts.flags |= ACFG_RTS_FIXED;

    dbg_print_params("vap - %s; rts - %d",params[0], rts.val);

    if((rts.val == ACFG_RTS_MAX) || (rts.val == 0))
        rts.flags |= ACFG_RTS_DISABLED;

    status = acfg_set_rts((a_uint8_t *)params[0], &rts);

    return acfg_to_os_status(status) ;
}



param_info_t wrap_get_rts_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_rts
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_rts(char *params[])
{
    acfg_rts_t rts ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s;",params[0]);

    status = acfg_get_rts((a_uint8_t *)params[0], &rts);

    if(rts.flags & ACFG_RTS_DISABLED)
        msg("RTS Threshold - Disabled");
    else
        msg("RTS Threshold - %d",rts.val);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_frag_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"frag", PARAM_STRING, "frag value"},
    {"fixed", PARAM_STRING, "fixed?"},
};


/**
 * @brief Wrapper for acfg_set_frag
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_frag(char *params[])
{
    acfg_frag_t frag ;
    a_status_t status = A_STATUS_FAILED ;
    frag.flags = 0;
    a_uint32_t temp = 0;

    if(!strncmp(params[1], "off", 3))
        frag.flags |= ACFG_FRAG_DISABLED;
    else
        get_uint32(params[1], &frag.val);

    get_uint32(params[2], &temp);

    if(temp == 1)
        frag.flags |= ACFG_FRAG_FIXED;

    dbg_print_params("vap - %s; frag - %d",params[0], frag.val);

    if((frag.val >= ACFG_FRAG_MAX) || (frag.val <= 0))
        frag.flags |= ACFG_FRAG_DISABLED;

    status = acfg_set_frag((a_uint8_t *)params[0], &frag);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_frag_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_frag
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_frag(char *params[])
{
    acfg_frag_t frag ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s;",params[0]);

    status = acfg_get_frag((a_uint8_t *)params[0], &frag);

    if(frag.flags & ACFG_FRAG_DISABLED)
        msg("Frag Threshold - Disabled");
    else
        msg("Frag Threshold - %d",frag.val);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_txpow_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"txpow", PARAM_STRING, "txpow value"},
    {"fixed", PARAM_STRING, "fixed?"},
};


/**
 * @brief Wrapper for acfg_set_frag
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_txpow(char *params[])
{
    acfg_txpow_t txpow ;
    a_status_t status = A_STATUS_FAILED ;
    txpow.flags = 0;
    a_uint32_t temp = 0;

    if(!strncmp(params[1], "off", 3))
        txpow.flags |= ACFG_TXPOW_DISABLED;
    else
        get_uint32(params[1], &txpow.val);

    get_uint32(params[2], &temp);

    if(temp == 1)
        txpow.flags |= ACFG_TXPOW_FIXED;

    dbg_print_params("vap - %s; txpow - %d flags %d",params[0], 
            txpow.val, txpow.flags);

    status = acfg_set_txpow((a_uint8_t *)params[0], &txpow);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_txpow_params[] = { 
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_txpow
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_txpow(char *params[])
{
    acfg_txpow_t txp ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s;",params[0]);

    status = acfg_get_txpow((a_uint8_t *)params[0], &txp);

    if(txp.flags & ACFG_TXPOW_DISABLED)
        msg("TxPower - Disabled");
    else
        msg("TxPower Threshold - %d",txp.val);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_ap_params[] = { 
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_ap
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_ap(char *params[])
{
    acfg_macaddr_t macaddr ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s;",params[0]);

    status = acfg_get_ap((a_uint8_t *)params[0], &macaddr);
    msg("AP Macaddr - %x:%x:%x:%x:%x:%x",\
            macaddr.addr[0], macaddr.addr[1], macaddr.addr[2], \
            macaddr.addr[3], macaddr.addr[4], macaddr.addr[5]);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_enc_params[] = { 
    {"vap", PARAM_STRING, "vap name"},
    {"flag",PARAM_STRING, "flags"},
    {"enc", PARAM_STRING, "encode str"},
};


/**
 * @brief Wrapper for acfg_set_enc
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_enc(char *params[])
{
    a_status_t status = A_STATUS_FAILED;
    a_uint32_t flag = 0, len;
    a_uint32_t temp = 0;

    dbg_print_params("vap - %s; flag - %s, encode - %s",
            params[0], params[1], params[2]);

    if(strchr(params[1], '[') == NULL) {
        get_hex(params[1], &flag);
    }
    if(!strncmp(params[2], "off", 3))
        flag |= ACFG_ENCODE_DISABLED;
    if(strchr(params[1], '[') != NULL) {
        if( (sscanf(params[1], "[%i]", &temp) == 1) &&
                (temp > 0) && (temp < ACFG_ENCODE_INDEX)) {
            flag |= temp;
        }
    }
    /* in wep open string case, for test purpose; */
    len = strnlen(params[2], ACFG_ENCODING_TOKEN_MAX);
    if(temp) {
        status = acfg_set_enc((a_uint8_t *)params[0],
                (acfg_encode_flags_t)flag, NULL);
    }
    else {
        status = acfg_set_enc((a_uint8_t *)params[0],
                (acfg_encode_flags_t)flag, params[2]);
    }
    return acfg_to_os_status(status);
}


param_info_t wrap_set_vendor_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"param", PARAM_STRING, "parameter id"},
    {"val", PARAM_STRING, "parameter value"},
    {"param_type", PARAM_STRING, "parameter type str/int/mac"},
    {"reinit", PARAM_STRING, "reinit flag"},
};

/**
 * @brief Wrapper for acfg_set_vap_vendor_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_vap_vendor_param(char *params[])
{
    acfg_param_vap_t paramid ;
    acfg_vendor_param_data_t val ;
    a_status_t status = A_STATUS_FAILED ;
    a_uint32_t len, type, reinit = 0;

    get_uint32(params[1], (a_uint32_t *)&paramid);
    get_uint32(params[4], (a_uint32_t *)&reinit);
    /* convert val to required type */
    if(strcmp(params[3], "str") == 0)
    {
        strcpy((char *)&val, params[2]);
        len = strlen((char *)&val) + 1;
        type = ACFG_TYPE_STR; 
    }
    else if(strcmp(params[3], "int") == 0)
    {
        *(a_uint32_t *)(&val) = atol(params[2]);
        len = sizeof(a_uint32_t);
        type = ACFG_TYPE_INT;
    }
    else if(strcmp(params[3], "mac") == 0)
    {
        acfg_mac_str_to_octet((a_uint8_t *)params[2], (a_uint8_t *)&val);
        len = ACFG_MACADDR_LEN;
        type = ACFG_TYPE_MACADDR;
    }
    else
    {
        dbg_print_params("Invalid type");
        acfg_to_os_status(status);
    } 

    dbg_print_params("vap - %s; paramid - %d; value - %s; paramtype - %s; reinit - %d",\
            params[0], paramid, params[2], params[3], reinit);

    status = acfg_set_vap_vendor_param((a_uint8_t *)params[0], paramid, (a_uint8_t *)&val, len, type, reinit);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_vendor_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"param", PARAM_STRING, "parameter id"},
};


/**
 * @brief Wrapper for acfg_get_vap_vendor_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_vap_vendor_param(char *params[])
{
    acfg_param_vap_t paramid ;
    acfg_vendor_param_data_t val ;
    a_status_t status = A_STATUS_FAILED ;
    a_uint32_t type;

    get_uint32(params[1], (a_uint32_t *)&paramid);

    dbg_print_params("vap - %s; paramid - %d",\
            params[0], paramid);

    status = acfg_get_vap_vendor_param((a_uint8_t *)params[0], paramid,
            (a_uint8_t *)&val, (a_uint32_t *)&type);

    if(type == ACFG_TYPE_INT)
        msg("value - %d", *(a_uint32_t *)&val);
    else if(type == ACFG_TYPE_STR)
        msg("str - %s", (char *)&val);
    else if(type == ACFG_TYPE_MACADDR)
    {
        msg("mac - %02x:%02x:%02x:%02x:%02x:%02x", val.data[0],
                val.data[1],
                val.data[2],
                val.data[3],
                val.data[4],
                val.data[5]);
    }
    else
        msg("Driver returned invalid type");

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_vapprm_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"param", PARAM_STRING, "parameter id"},
    {"val", PARAM_STRING, "parameter value"},
};


/**
 * @brief Wrapper for acfg_set_vap_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_vap_param(char *params[])
{
    acfg_param_vap_t paramid ;
    a_uint32_t val ;
    a_status_t status = A_STATUS_FAILED ;

    get_uint32(params[1], (a_uint32_t *)&paramid);
    get_uint32(params[2], (a_uint32_t *)&val);

    dbg_print_params("vap - %s; paramid - %d; value - %d",\
            params[0], paramid, val);

    status = acfg_set_vap_param((a_uint8_t *)params[0], paramid, val);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_vapprm_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"param", PARAM_STRING, "parameter id"},
};


/**
 * @brief Wrapper for acfg_get_vap_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_vap_param(char *params[])
{
    acfg_param_vap_t paramid ;
    a_uint32_t val ;
    a_status_t status = A_STATUS_FAILED ;

    get_uint32(params[1], (a_uint32_t *)&paramid);
    dbg_print_params("vap - %s; paramid - %d; ",params[0], paramid);

    status = acfg_get_vap_param((a_uint8_t *)params[0], paramid, &val);
    msg("Param value - %d",val);

    return acfg_to_os_status(status) ;
}



param_info_t wrap_set_radioprm_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"param", PARAM_STRING, "parameter id"},
    {"val", PARAM_STRING, "parameter value"},
};


/**
 * @brief Wrapper for acfg_set_radio_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_radio_param(char *params[])
{
    acfg_param_radio_t paramid ;
    a_uint32_t val ;
    a_status_t status = A_STATUS_FAILED ;

    get_uint32(params[1], (a_uint32_t *)&paramid);
    get_uint32(params[2], (a_uint32_t *)&val);
    dbg_print_params("vap - %s; paramid - %d; value - %d",\
            params[0], paramid, val);

    status = acfg_set_radio_param((a_uint8_t *)params[0], paramid, val);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_radioprm_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"param", PARAM_STRING, "parameter id"},
};


/**
 * @brief Wrapper for acfg_get_radio_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_radio_param(char *params[])
{
    acfg_param_radio_t paramid ;
    a_uint32_t val ;
    a_status_t status = A_STATUS_FAILED ;

    get_uint32(params[1], (a_uint32_t *)&paramid);
    dbg_print_params("vap - %s; paramid - %d",params[0], paramid);

    status = acfg_get_radio_param((a_uint8_t *)params[0], paramid, &val);
    msg("Param value - %d",val);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_rate_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"rate", PARAM_STRING, "rate in Mbps"},
};


/**
 * @brief Wrapper for acfg_get_rate
 *
 * @param params[]
 *
 * @return 
 */
int wrap_set_rate(char *params[])
{
    acfg_rate_t rate ;
    a_status_t status = A_STATUS_FAILED ;

    get_uint32(params[1], (a_uint32_t *)&rate.value);
    dbg_print_params("vap - %s; rate - %dMBps",params[0],rate.value);

    if(strchr(params[1], 'M') != NULL)
        rate.value *= 1000000;

    if(rate.value == 0)
        rate.fixed = 0x0;
    else
        rate.fixed = 0x01;

    status = acfg_set_rate((a_uint8_t *)params[0], &rate);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_rate_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_get_rate
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_rate(char *params[])
{
    a_uint32_t rate ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s",params[0]);

    status = acfg_get_rate((a_uint8_t *)params[0], &rate);
    msg("Default bit rate - %d",rate);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_phymode_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"phymode", PARAM_UINT32, "phymode to set"},
};

/**
 * @brief Wrapper for acfg_set_phymode
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_phymode(char *params[])
{
    acfg_phymode_t mode ;
    a_status_t status = A_STATUS_FAILED ;

    get_uint32(params[1], (a_uint32_t *)&mode);
    dbg_print_params("vap - %s; phymode - %d",params[0],mode);

    status = acfg_set_phymode((a_uint8_t *)params[0], mode);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_phymode_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_set_phymode
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_phymode(char *params[])
{
    acfg_phymode_t mode ;
    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vap - %s",params[0]);

    status = acfg_get_phymode((a_uint8_t *)params[0], &mode);

    msg("Phymode %u", mode);

    return acfg_to_os_status(status) ;
}


static const char *
addr_ntoa(const uint8_t mac[ACFG_MACADDR_LEN])
{
    static char a[18];
    int i;

    i = snprintf(a, sizeof(a), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return (i < 17 ? NULL : a);
}


param_info_t wrap_assoc_sta_info_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_assoc_sta_info
 *
 * @param params[]
 *
 * @return
 */
int wrap_assoc_sta_info(char *params[])
{
    a_status_t status = A_STATUS_FAILED ;
    acfg_sta_info_req_t sireq ;
    acfg_sta_info_t sibuf[5] ;
    a_uint8_t *cp ;

    dbg_print_params("vap - %s ",params[0]);

    sireq.len = sizeof(sibuf) ;
    sireq.info = &sibuf[0] ;

    dbglog("Sending buffer of length %d",sireq.len);

    status = acfg_assoc_sta_info((a_uint8_t *)params[0], &sireq);
    if(status != A_STATUS_OK)
        return -1;

    dbglog("Received buffer of length %d",sireq.len);
    msg("%-17.17s %4s %4s %4s %4s %4s %6s %6s %4s %5s %3s %8s %6s\n"
            , "ADDR", "AID", "CHAN", "RATE", "RSSI", "IDLE", "TXSEQ", "RXSEQ"
            , "CAPS", "ACAPS", "ERP", "STATE", "HTCAPS" );

    cp = (a_uint8_t *)&sibuf[0] ;
    do {
        acfg_sta_info_t *psi = (acfg_sta_info_t *)cp;

        msg("%s %4u %4d %3dM %4d %4d %6d %6d %4x %5x %3x %8x %6x", \
                addr_ntoa(psi->macaddr),psi->associd, psi->channel, \
                psi->txrate, psi->rssi, psi->inact, psi->txseq, psi->rxseq, \
                psi->cap, psi->athcap, psi->erp, psi->state, psi->htcap );

        cp += sizeof(acfg_sta_info_t) ;
    }while(cp < (a_uint8_t *)&sibuf[0] + sireq.len);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_wsupp_init_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"flags", PARAM_UINT32, "flags: 1: restart 2: cleanup"},
};

int wrap_wsupp_init(char *params[])
{
    a_status_t status = A_STATUS_FAILED ;
    acfg_wsupp_hdl_t *aptr;
    acfg_wsupp_init_flags_t flags ;

    dbg_print_params("vap - %s",params[0]);

    get_uint32(params[1] , (a_uint32_t *)&flags);
    aptr = acfg_wsupp_init((a_uint8_t *)params[0], flags);
    msg("init wsupp: %p %d", aptr, flags);

    if (aptr) {
        acfg_wsupp_uninit(aptr);
        msg("uninit wsupp: %d", status);
        status = A_STATUS_OK;
    }

    return acfg_to_os_status(status) ;
}

param_info_t wrap_wsupp_if_add_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_wsupp_if_add
 *
 * @param params[]
 *
 * @return
 */

int wrap_wsupp_if_add(char *params[])
{
    a_status_t status = A_STATUS_FAILED;
    acfg_wsupp_hdl_t *aptr;

    dbg_print_params("vap - %s",params[0]);

    aptr = acfg_wsupp_init((a_uint8_t *)params[0], 0);
    msg("init wsupp: %p", aptr);

    if (aptr) {
        status = acfg_wsupp_if_add(aptr, (char *)params[0]);
        msg("if add status %d\n", status);
    }

#if 0
    if (status == A_STATUS_OK) {
        status = acfg_wsupp_if_remove(aptr, (char *)params[0]);
        msg("if remove status %d\n", status);
    }
#endif
    return acfg_to_os_status(status) ;
}

param_info_t wrap_wsupp_if_remove_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_wsupp_if_remove
 *
 * @param params[]
 *
 * @return
 */

int wrap_wsupp_if_remove(char *params[])
{
    a_status_t status = A_STATUS_FAILED;
    acfg_wsupp_hdl_t *aptr;

    dbg_print_params("vap - %s",params[0]);

    aptr = acfg_wsupp_init((a_uint8_t *)params[0], 0);
    msg("init wsupp: %p", aptr);

    if (aptr) {
        status = acfg_wsupp_if_remove(aptr, (char *)params[0]);
        msg("if remove status %d\n", status);
    }

    return acfg_to_os_status(status) ;
}

param_info_t wrap_wsupp_nw_add_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_wsupp_nw_add
 *
 * @param params[]
 *
 * @return
 */

int wrap_wsupp_nw_add(char *params[])
{
    a_status_t status = A_STATUS_FAILED;
    acfg_wsupp_hdl_t *aptr;
    a_int32_t network_id;

    dbg_print_params("vap - %s",params[0]);

    aptr = acfg_wsupp_init((a_uint8_t *)params[0], 0);
    msg("init wsupp: %p", aptr);

    if (aptr) {
        status = acfg_wsupp_nw_create(aptr, (char *)params[0],
                                      &network_id);
        msg("network add status %d, network id %d\n", status, network_id);
    }

    return acfg_to_os_status(status) ;
}

param_info_t wrap_wsupp_nw_remove_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"nw_id", PARAM_UINT32, "network id"},
};

/**
 * @brief Wrapper for acfg_wsupp_nw_remove
 *
 * @param params[]
 *
 * @return
 */

int wrap_wsupp_nw_remove(char *params[])
{
    a_status_t status = A_STATUS_FAILED;
    acfg_wsupp_hdl_t *aptr;
    a_uint32_t network_id;

    get_uint32(params[1], (a_uint32_t *)&network_id);

    dbg_print_params("vap - %s; network id - %d", params[0], network_id);

    aptr = acfg_wsupp_init((a_uint8_t *)params[0], 0);
    msg("init wsupp: %p", aptr);

    if (aptr) {
        status = acfg_wsupp_nw_delete(aptr, (char *)params[0], network_id);
        msg("network remove status %d\n", status);
    }

    return acfg_to_os_status(status) ;
}

param_info_t wrap_wsupp_nw_set_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"nw_id", PARAM_UINT32, "network id"},
    {"item", PARAM_UINT32, "item"},
    {"param", PARAM_STRING, "param"},
};

/**
 * @brief Wrapper for acfg_wsupp_nw_set
 *
 * @param params[]
 *
 * @return 
 */

int wrap_wsupp_nw_set(char *params[])
{
    a_status_t status = A_STATUS_FAILED;
    acfg_wsupp_hdl_t *aptr;
    a_uint32_t network_id;
    a_uint32_t item;

    get_uint32(params[1], (a_uint32_t *)&network_id);
    get_uint32(params[2], (a_uint32_t *)&item);

    dbg_print_params("vap - %s; network id - %d; param %s",
            params[0], network_id, params[3]);

    aptr = acfg_wsupp_init((a_uint8_t *)params[0], 0);
    msg("init wsupp: %p", aptr);

    if (aptr) {
        if (!strncmp(params[3], "NONE", sizeof("NONE")-1))
            status = acfg_wsupp_nw_set(aptr, network_id, item, NULL);
        else
            status = acfg_wsupp_nw_set(aptr, network_id, item, params[3]);
        msg("network set status %d\n", status);
    }

    return acfg_to_os_status(status) ;
}

param_info_t wrap_wsupp_nw_get_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"nw_id", PARAM_UINT32, "network id"},
    {"item", PARAM_UINT32, "item"},
};

/**
 * @brief Wrapper for acfg_wsupp_nw_get
 *
 * @param params[]
 *
 * @return
 */

int wrap_wsupp_nw_get(char *params[])
{
    a_status_t status = A_STATUS_FAILED;
    acfg_wsupp_hdl_t *aptr;
    a_uint32_t network_id;
    a_uint32_t item;
    char reply[512];
    a_uint32_t reply_len=sizeof(reply);

    get_uint32(params[1], (a_uint32_t *)&network_id);
    get_uint32(params[2], (a_uint32_t *)&item);

    dbg_print_params("vap - %s; network id - %d; item %d",
            params[0], network_id, item);

    aptr = acfg_wsupp_init((a_uint8_t *)params[0], 0);
    msg("init wsupp: %p", aptr);

    if (aptr) {
        status = acfg_wsupp_nw_get(aptr, network_id, item, reply, &reply_len);
        msg("network get status %d\n", status);
    }
    printf("reply = %s\n", reply);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_wsupp_nw_list_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_wsupp_nw_list
 *
 * @param params[]
 *
 * @return
 */

int wrap_wsupp_nw_list(char *params[])
{
    a_status_t status = A_STATUS_FAILED;
    acfg_wsupp_hdl_t *aptr;
    char reply[512];
    a_uint32_t reply_len=sizeof(reply);


    dbg_print_params("vap - %s", params[0]);

    aptr = acfg_wsupp_init((a_uint8_t *)params[0], 0);
    msg("init wsupp: %p", aptr);

    if (aptr) {
        memset(reply, 0, sizeof(reply));
        status = acfg_wsupp_nw_list(aptr, reply, &reply_len);
        msg("network list status %d\n", status);
    }
    printf("reply = \n%s\n", reply);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_wsupp_wps_req_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"vap", PARAM_STRING, "vap name"},
    {"item", PARAM_UINT32, "item"},
    {"param", PARAM_STRING, "param"},
    {"param2", PARAM_STRING, "param2"},
};

/**
 * @brief Wrapper for acfg_wsupp_wps_set
 *
 * @param params[]
 *
 * @return
 */

int wrap_wsupp_wps_req(char *params[])
{
    a_status_t status = A_STATUS_FAILED;
    a_uint32_t item;

    get_uint32(params[2], (a_uint32_t *)&item);
    dbg_print_params("vap - %s; param %s; param2 %s",
            params[1], params[3], params[4]);
    status = acfg_set_wps_mode((a_uint8_t *)params[0], (a_uint8_t *)params[1], 
            item, (a_int8_t *)params[3], (a_int8_t *)params[4]);
    return acfg_to_os_status(status) ;
}

param_info_t wrap_wsupp_set_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"item", PARAM_UINT32, "item"},
    {"val", PARAM_UINT32, "param_val"},
    {"str", PARAM_STRING, "param_str"},
};

/**
 * @brief Wrapper for acfg_wsupp_set
 *
 * @param params[]
 *
 * @return
 */

int wrap_wsupp_set(char *params[])
{
    a_status_t status = A_STATUS_FAILED;
    a_uint32_t type, param_val;

    get_uint32(params[1], (a_uint32_t *)&type);
    get_uint32(params[2], (a_uint32_t *)&param_val);

    dbg_print_params("vap - %s; item %d; param_val %s; param_str %s",
            params[0], item, param_val, params[3]);

    status = acfg_set_wps((a_int8_t *)params[0], type,
            (a_int8_t *)params[3]);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_reg_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"addr", PARAM_STRING, "register offset"},
    {"val", PARAM_STRING, "parameter value"},
};

/**
 * @brief Wrapper for acfg_set_reg
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_reg(char *params[])
{
    a_uint32_t offset;
    a_uint32_t value;
    a_status_t status = A_STATUS_FAILED;

    get_hex(params[1], &offset);
    get_hex(params[2], &value);
    dbg_print_params("vap - %s; offset - %x; value - %x",\
            params[0], offset, value);

    status = acfg_set_reg((a_uint8_t *)params[0], offset, value);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_reg_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"addr", PARAM_STRING, "register offset"},
};


/**
 * @brief Wrapper for acfg_get_reg
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_reg(char *params[])
{
    a_uint32_t offset;
    a_uint32_t value;
    a_status_t status = A_STATUS_FAILED ;

    get_hex(params[1], &offset);
    dbg_print_params("vap - %s; offset - %x",params[0], offset);

    status = acfg_get_reg((a_uint8_t *)params[0], offset, &value);
    msg("Param value - %08x",value);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_vlgrp_create_params[] = {
    {"vlan", PARAM_STRING, "vlan id"}
};

/**
 * @brief Wrapper for acfg_vlgrp_create
 *
 * @param params[]
 *
 * @return
 */
int wrap_vlgrp_create(char *params[])
{

    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vlangrp create - %s ",params[0]);

    status = acfg_vlgrp_create((a_uint8_t *)params[0]);

    return acfg_to_os_status(status) ;
}

/** 
 * @brief Wrapper for acfg_vlgrp_delete
 *
 * @param params[]
 *
 * @return
 */
int wrap_vlgrp_delete(char *params[])
{

    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vlangrp delete - %s ",params[0]);

    status = acfg_vlgrp_delete((a_uint8_t *)params[0]);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_vlgrp_addvap_params[] = {
    {"vlan", PARAM_STRING, "vlan id"},
    {"vap", PARAM_STRING, "vap"}
};

/**
 * @brief Wrapper for acfg_vlgrp_addvap
 *
 * @param params[]
 *
 * @return
 */
int wrap_vlgrp_addvap(char *params[])
{

    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vlangrp addvap - %s , %s",params[0], params[1]);

    status = acfg_vlgrp_addvap((a_uint8_t *)params[0], (a_uint8_t *) params[1]);

    return acfg_to_os_status(status) ;
}

/**
 * @brief Wrapper for acfg_vlgrp_delvap
 *
 * @param params[]
 *
 * @return
 */
int wrap_vlgrp_delvap(char *params[])
{

    a_status_t status = A_STATUS_FAILED ;

    dbg_print_params("vlangrp delvap - %s , %s",params[0], params[1]);

    status = acfg_vlgrp_delvap((a_uint8_t *)params[0], (a_uint8_t *) params[1]);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_acl_addmac_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"macadr", PARAM_STRING, "mac address"},
};

/**
 * @brief Wrapper for acfg_acl_addmac
 *
 * @param params[]
 *
 * @return
 */
int wrap_acl_addmac(char *params[])
{
    a_status_t status = A_STATUS_FAILED ;
    a_uint8_t mac[6];

    dbg_print_params("vap - %s; macaddr - %s",params[0], params[1]);
    printf("vap - %s: macaddr - %s",(char *)params[0], (char *)params[1]);

    memcpy(mac, params[1], ACFG_MACSTR_LEN);

    acfg_mac_str_to_octet((a_uint8_t *)params[0], mac);
    status = acfg_acl_addmac((a_uint8_t *)params[0], mac);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_acl_getmac_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_acl_getmac
 *
 * @param params[]
 *
 * @return
 */
int wrap_acl_getmac(char *params[])
{
    a_status_t status = A_STATUS_FAILED ;
    acfg_macacl_t macacl;
    int i, j;

    dbg_print_params("vap - %s: ",params[0]);

    status = acfg_acl_getmac((a_uint8_t *)params[0], &macacl);

    msg("MACADDR's in ACL - %d", macacl.num);

    for(i = 0; i < macacl.num; i++) {
        printf("MAC address %d = ", (i + 1));
        for(j = 0; j < 6; j++) {
            printf("%x", macacl.macaddr[i][j]);
            printf("%c", (j != 5)? ':' : '\0');
        }
        printf("\n");
    }
    return acfg_to_os_status(status) ;
}


param_info_t wrap_acl_delmac_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"macadr", PARAM_STRING, "mac address"},
};

/**
 * @brief Wrapper for acfg_acl_delmac
 *
 * @param params[]
 *
 * @return
 */
int wrap_acl_delmac(char *params[])
{
    a_status_t status = A_STATUS_FAILED ;
    char mac[20];

    dbg_print_params("vap - %s; macaddr - %s",params[0], params[1]);
    printf("vap - %s: macaddr - %s",(char *)params[0], (char *)params[1]);

    memcpy(mac, params[1], ACFG_MACSTR_LEN);

    status = acfg_acl_delmac((a_uint8_t *)params[0], (a_uint8_t *)params[1]);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_wlan_profile_create_from_file_params[] = {
    {"file", PARAM_STRING, "file name"},
};

param_info_t wrap_wlan_profile_create_params[] = { 
    {"radio", PARAM_STRING, "radio name"},
    {"chan", PARAM_STRING, "channel"},
    {"freq", PARAM_STRING, "frequency"},
    {"txpow", PARAM_STRING, "txpower"},
    {"ctrycode", PARAM_STRING, "countrycode"},
    {"radiomac", PARAM_STRING, "mac address: radio"},
    {"vap", PARAM_STRING, "vap name"},
    {"opmode", PARAM_STRING, "operating mode: 1 - STA 6 - AP"},
    {"phymode", PARAM_STRING, "phy mode: 0-AUTO 1-11A 2-11B \
        3-11G 5-11NA 6-11NG"},
    {"ssid", PARAM_STRING, "essid"},
    {"bitrate", PARAM_STRING, "data rate 1M-54M"},
    {"bintval", PARAM_STRING, "for AP only 100, 200, 300"},
    {"rts", PARAM_STRING, "rts threshold 0-2346"},
    {"frag", PARAM_STRING, "frag threshold 0-2346"},
    {"vapmac", PARAM_STRING, "mac address: vap"},
    {"nodemac", PARAM_STRING, "mac address: station"},
    {"nodeacl", PARAM_STRING, "acl policy:0-none 1-accept 2-deny"},
    {"sec_method", PARAM_STRING, "security method"},
    {"ciph_method", PARAM_STRING, "cipher method"},
    {"psk/pasphrase", PARAM_STRING, "wpa-psk/wpa-passphrase"},
    {"WEP-KEY0", PARAM_STRING, "WEP-KEY-0"},
    {"WEP-KEY1", PARAM_STRING, "WEP-KEY-1"},
    {"WEP-KEY2", PARAM_STRING, "WEP-KEY-2"},
    {"WEP-KEY3", PARAM_STRING, "WEP-KEY-3"},
};

param_info_t wrap_wlan_profile_modify_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"chan", PARAM_STRING, "channel"},
    {"freq", PARAM_STRING, "frequency"},
    {"txpow", PARAM_STRING, "txpower"},
    {"ctrycode", PARAM_STRING, "countrycode"},
    {"radiomac", PARAM_STRING, "mac address: radio"},
    {"vap", PARAM_STRING, "vap name"},
    {"opmode", PARAM_STRING, "operating mode: 1 - STA 6 - AP"},
    {"phymode", PARAM_STRING, "phy mode: 0-AUTO 1-11A 2-11B \
        3-11G 5-11NA 6-11NG"},
    {"ssid", PARAM_STRING, "essid"},
    {"bitrate", PARAM_STRING, "data rate 1M-54M"},
    {"bintval", PARAM_STRING, "for AP only 100, 200, 300"},
    {"rts", PARAM_STRING, "rts threshold 0-2346"},
    {"frag", PARAM_STRING, "frag threshold 0-2346"},
    {"vapmac", PARAM_STRING, "mac address: vap"},
    {"nodemac", PARAM_STRING, "mac address: station"},
    {"nodeacl", PARAM_STRING, "acl policy:0-none 1-accept 2-deny"},
    {"sec_method", PARAM_STRING, "security method"},
    {"ciph_method", PARAM_STRING, "cipher method"},
    {"psk/pasphrase", PARAM_STRING, "wpa-psk/wpa-passphrase"},
    {"WEP-KEY0", PARAM_STRING, "WEP-KEY-0"},
    {"WEP-KEY1", PARAM_STRING, "WEP-KEY-1"},
    {"WEP-KEY2", PARAM_STRING, "WEP-KEY-2"},
    {"WEP-KEY3", PARAM_STRING, "WEP-KEY-3"},
};

param_info_t wrap_wlan_profile_get_params[] = {
    {"radio", PARAM_STRING, "radio name"},
};

int wrap_wlan_profile_get(char *params[])
{
    a_status_t status = 0;
    acfg_wlan_profile_t wlan_profile;

    strncpy ((char *)wlan_profile.radio_params.radio_name,
            params[0], ACFG_MAX_IFNAME);
    status = acfg_wlan_profile_get(&wlan_profile);

    if (status == A_STATUS_OK) {
        acfg_wlan_profile_print(&wlan_profile);
    }

    return status;
}


int wrap_wlan_vap_profile_get(char *params[])
{
    a_status_t status = 0;
    acfg_wlan_profile_vap_params_t vap_params;

    strncpy ((char *)vap_params.radio_name,
            params[0], ACFG_MAX_IFNAME);
    strncpy ((char *)vap_params.vap_name,
            params[1], ACFG_MAX_IFNAME);
    status = acfg_wlan_vap_profile_get(&vap_params);

    if (status == A_STATUS_OK) {
        acfg_wlan_vap_profile_print(&vap_params);
    }

    return status;
}

param_info_t wrap_hostapd_getconfig_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


int wrap_hostapd_getconfig(char *params[])
{
    a_status_t status = 0;
    a_char_t buffer[4096];

    status = acfg_hostapd_getconfig((a_uint8_t *)params[0], buffer);

    printf("%s: \nReceived Buffer: \n%s\n", __func__, buffer);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_hostapd_set_wpa_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"wpa", PARAM_STRING, "wpa"},
    {"psk/passphrase", PARAM_STRING, "wpa_psk=/wpa_passphrase=(full)"},
    {"wpa_key_mgmt", PARAM_STRING, "wpa_key_mgmt"},
    {"wpa_pairwise", PARAM_STRING, "wpa_pairwise"},
    {"wpa_group_rekey", PARAM_STRING, "wpa_group_rekey"},
    {"wpa_passphrase", PARAM_STRING, "wpa_passphrase"},
};

int wrap_hostapd_set_wpa(char *params[])
{
    a_status_t status = 0;

    //    status = acfg_hostapd_set_wpa((a_uint8_t **)params);

    return acfg_to_os_status(status) ;
}

int wrap_wps_pbc(char *params[])
{
    a_status_t status = 0;

    status = acfg_set_wps_pbc(params[0]);
    return acfg_to_os_status(status);
}

int wrap_wps_pin(char *params[])
{
    a_status_t status = 0;
    int pin_action;
    char pin[10], pin_txt[10];

    if (strncmp(params[1], "set", 3) == 0) {
        pin_action = ACFG_WPS_PIN_SET;
        strcpy(pin, params[2]);
    } else if (strncmp(params[1], "random", 6) == 0) {
        pin_action = ACFG_WPS_PIN_RANDOM;
        strcpy(pin, "");
    }
    status = acfg_set_wps_pin(params[0], pin_action, pin, pin_txt, params[3]);
    return acfg_to_os_status(status);
}

int wrap_wps_config(char *params[])
{
    a_status_t status = 0;

    status = acfg_wps_config((a_uint8_t *)params[0], params[1],
            params[2], params[3], params[4]);

    return acfg_to_os_status(status);
}



param_info_t wrap_is_offload_vap_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


int wrap_is_offload_vap(char *params[])
{
    a_status_t status = 0;

    status = acfg_is_offload_vap((a_uint8_t *)params[0]);

    return acfg_to_os_status(status) ;
}


/*
 * Wrapper function table
 */
fntbl_t fntbl[] = {
    {"acfg_set_profile", wrap_set_profile, 2, wrap_set_profile_params },
    {"acfg_create_vap", wrap_create_vap, 5, wrap_create_vap_params },
    {"acfg_delete_vap", wrap_delete_vap, 2,  wrap_delete_vap_params },
    {"acfg_set_ssid", wrap_set_ssid, 2, wrap_set_ssid_params },
    {"acfg_get_ssid", wrap_get_ssid, 1, wrap_get_ssid_params },
    {"acfg_set_channel", wrap_set_channel, 2, wrap_set_channel_params },
    {"acfg_get_channel", wrap_get_channel, 1, wrap_get_channel_params },
    {"acfg_set_opmode", wrap_set_opmode, 2, wrap_set_opmode_params },
    {"acfg_get_opmode", wrap_get_opmode, 1, wrap_get_opmode_params },
    {"acfg_set_freq", wrap_set_freq, 2, wrap_set_freq_params },
    {"acfg_get_freq", wrap_get_freq, 1, wrap_get_freq_params },
    {"acfg_set_rts", wrap_set_rts, 3, wrap_set_rts_params },
    {"acfg_get_rts", wrap_get_rts, 1, wrap_get_rts_params },
    {"acfg_set_frag", wrap_set_frag, 3, wrap_set_frag_params },
    {"acfg_get_frag", wrap_get_frag, 1, wrap_get_frag_params },
    {"acfg_set_txpow", wrap_set_txpow, 3, wrap_set_txpow_params },
    {"acfg_get_txpow", wrap_get_txpow, 1, wrap_get_txpow_params },
    {"acfg_get_ap", wrap_get_ap, 1, wrap_get_ap_params },
    {"acfg_set_enc", wrap_set_enc, 3, wrap_set_enc_params },
    {"acfg_set_vap_vendor_param", wrap_set_vap_vendor_param, 5, wrap_set_vendor_params },
    {"acfg_get_vap_vendor_param", wrap_get_vap_vendor_param, 2, wrap_get_vendor_params },
    {"acfg_set_vap_param", wrap_set_vap_param, 3, wrap_set_vapprm_params },
    {"acfg_get_vap_param", wrap_get_vap_param, 2, wrap_get_vapprm_params },
    {"acfg_set_radio_param",wrap_set_radio_param, 3, wrap_set_radioprm_params},
    {"acfg_get_radio_param",wrap_get_radio_param, 2, wrap_get_radioprm_params},
    {"acfg_get_rate",wrap_get_rate, 1, wrap_get_rate_params},
    {"acfg_set_rate",wrap_set_rate, 2, wrap_set_rate_params},
    {"acfg_wsupp_init",wrap_wsupp_init, 2, wrap_wsupp_init_params},
    {"acfg_wsupp_if_add",wrap_wsupp_if_add, 1, wrap_wsupp_if_add_params},
    {"acfg_wsupp_if_remove", wrap_wsupp_if_remove, 1,
        wrap_wsupp_if_remove_params},
    {"acfg_wsupp_nw_add",wrap_wsupp_nw_add, 1, wrap_wsupp_nw_add_params},
    {"acfg_wsupp_nw_remove",wrap_wsupp_nw_remove, 2,
        wrap_wsupp_nw_remove_params},
    {"acfg_wsupp_nw_set",wrap_wsupp_nw_set, 4, wrap_wsupp_nw_set_params},
    {"acfg_wsupp_nw_get",wrap_wsupp_nw_get, 3, wrap_wsupp_nw_get_params},
    {"acfg_wsupp_nw_list",wrap_wsupp_nw_list, 1, wrap_wsupp_nw_list_params},
    {"acfg_wsupp_wps_req",wrap_wsupp_wps_req, 5, wrap_wsupp_wps_req_params},
    {"acfg_wsupp_set",wrap_wsupp_set, 4, wrap_wsupp_set_params},
    {"acfg_get_rssi", wrap_get_rssi, 1, wrap_get_rssi_params },
    {"acfg_get_custdata", wrap_get_custdata, 1, wrap_get_custdata_params },
    {"acfg_get_testmode", wrap_get_testmode, 2, wrap_get_testmode_params },
    {"acfg_set_testmode", wrap_set_testmode, 3, wrap_set_testmode_params },
    {"acfg_set_phymode", wrap_set_phymode, 2, wrap_set_phymode_params},
    {"acfg_get_phymode", wrap_get_phymode, 1, wrap_get_phymode_params},
    {"acfg_assoc_sta_info",wrap_assoc_sta_info, 1, wrap_assoc_sta_info_params},
    {"acfg_set_reg", wrap_set_reg, 3, wrap_set_reg_params},
    {"acfg_get_reg", wrap_get_reg, 2, wrap_get_reg_params},
    {"acfg_vlgrp_create",wrap_vlgrp_create, 1, wrap_vlgrp_create_params},
    {"acfg_vlgrp_delete",wrap_vlgrp_delete, 1, wrap_vlgrp_create_params},
    {"acfg_vlgrp_addvap",wrap_vlgrp_addvap, 2, wrap_vlgrp_addvap_params},
    {"acfg_vlgrp_delvap",wrap_vlgrp_delvap, 2, wrap_vlgrp_addvap_params},
    {"acfg_acl_addmac", wrap_acl_addmac, 2, wrap_acl_addmac_params},
    {"acfg_acl_getmac", wrap_acl_getmac, 1, wrap_acl_getmac_params},
    {"acfg_acl_delmac", wrap_acl_delmac, 2, wrap_acl_delmac_params},
    {"acfg_get_current_profile", wrap_wlan_profile_get,
        1, wrap_wlan_profile_get_params},
    {"acfg_hostapd_getconfig", wrap_hostapd_getconfig,
        1, wrap_hostapd_getconfig_params},
    {"acfg_hostapd_set_wpa", wrap_hostapd_set_wpa,
        6, wrap_hostapd_set_wpa_params},
    {"acfg_wps_pbc", wrap_wps_pbc, 1, NULL},
    {"acfg_wps_pin", wrap_wps_pin, 4, NULL},
    {"acfg_wps_config", wrap_wps_config, 5, NULL},
    {"acfg_is_offload_vap", wrap_is_offload_vap, 1, wrap_is_offload_vap_params},
    {"acfg_reset_profile", wrap_reset_profile, 1, wrap_reset_profile_params },
    {NULL,NULL},
};
