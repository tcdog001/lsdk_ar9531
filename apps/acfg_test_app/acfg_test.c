#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<ctype.h>
#include<string.h>
#include <libgen.h>

#include<acfg_api.h>


#include<acfg_tool.h>
#include<acfg_event.h>


#define BUFF_INIT_SIZE      500
#define NULLCHAR            '\0'


/*
 * Prototypes
 */
a_uint32_t doapitest(char *argv[]);
int get_tbl_idx(char *name) ;
int display_params(char *name) ;
void usage(void);
int recv_events(char *ifname, int nonblock) ;
void recv_wps_events(void);

/* External Declaration */
extern fntbl_t fntbl[] ;
extern char *type_desc [] ;


/* Options acepted by this tool
 *
 * p - Print description of command line parameters for acfg api
 * e - Wait for events
 */
static char *option_args = "ne::p::w::" ;
char *appname;

int main(int argc , char *argv[])
{

    int c;
    int argvidx = 0 ;
    int ret ;
    int opt_events = 0 ;
    int opt_events_nonblock = 0 ;
    int opt_disp_param = 0 ;
    char *opt_disp_param_arg = NULL;
    char *opt_event_arg = NULL;
    int opt_wps_event = 0;
    acfg_dl_init();

    appname = basename(argv[0]);


    while( (c = getopt(argc , argv , option_args)) != -1 )
    {
        switch (c)
        {
            case 'e':
                opt_events = 1 ;
                opt_event_arg = optarg ;
                break;

            case 'n':
                opt_events_nonblock = 1 ;
                break;

            case 'p':
                opt_disp_param = 1 ;
                opt_disp_param_arg = optarg ;
                break;
            case 'w':
                opt_wps_event = 1 ;
                break;

            case '?':
                /* getopt returns error */
                usage();
                exit(0);
                break;

            default:
                usage();
                exit(0);
                break;
        } //end switch
    }//end while

    argvidx = optind ;

    if(opt_disp_param)
    {
        ret = display_params(opt_disp_param_arg) ;
    }
    else if(opt_events)
    {
        ret = recv_events(opt_event_arg,opt_events_nonblock);
    }
    else if (opt_wps_event)
    {
        recv_wps_events();
    }
    else if (argv[argvidx] != NULL)
        ret = doapitest( &argv[argvidx] );

    if(ret != 0)
    {
        printf("\n<<<<<<<<<< Dumping LOG >>>>>>>>>>>>>\n");
        printf("%s", acfg_get_errstr());
        printf("\n<<<<<<<<<<<<<< End >>>>>>>>>>>>>>>>>\n");
    }

    return ret ;
}

void usage(void)
{
    printf("\n");
    printf("\t%s <acfg api name> <api arguments> \n",appname);
    printf("\t%s -p \n\t\tPrint help for "\
            "all acfg apis\n\n",appname);

    printf("\t%s -p<acfg api name> \n\t\tPrint help for "\
            "one acfg api\n\n",appname);

    printf("\t%s -e <interface name> [-n]"\
            "\n\t\tWait for events on interface. "
            " -n issues a nonblocking call to acfg library\n\n",appname);
}

/** 
 * @brief Get the index into the table of function 
 *        pointers for this acfg api
 * 
 * @param name - Acfg api name 
 * 
 * @return integer representing an index
 */
int get_tbl_idx(char *name)
{
    int j ;

    j = 0 ;
    while( fntbl[j].apiname != NULL)
    {
        if(strcmp(name , fntbl[j].apiname) == 0)
        {
            return j ;
        }
        j++;
    }

    return -1 ;

}


/** 
 * @brief Execute test for a particular acfg api.
 * 
 * @param argv[] - Array of charater pointers to NULL terminated 
 *                 strings. argv[0] is the acfg api name. Command
 *                 line arguments for testing this api begin from argv[1].
 * 
 * @return 
 */
a_uint32_t doapitest(char *argv[])
{
    int param_num ;
    int idx ;
    char **pc ;

    /* Get the index into the table of function pointers
     * which specifies the function to call to test this 
     * acfg api.
     */
    idx = get_tbl_idx(argv[0]) ;

    if(idx < 0)
    {
        dbglog("Incorrect acfg api");
        return -1;
    }

    /* Check for correct number of command line parameters 
     * for this acfg api.
     */
    param_num = 0 ;
    pc = &argv[1] ;
    while(*pc != NULL)
    {
        param_num++ ;
        pc++ ;
    }

    if( param_num != fntbl[idx].num_param )
    {
        dbglog("Incorrect number of parameters");
        return -1;
    }

    /* Call the wrapper function and return the 
     * status.
     */
    return fntbl[idx].wrapper(&argv[1]) ;
}



/**
 * @brief Print the parameter info for one acfg api
 *
 * @param index
 *
 * @return
 */
int print_param_info(int index)
{
    param_info_t *pparam = NULL ;

    pparam = fntbl[index].param_info ;

    if(pparam)
    {
        int count = 0;
        printf("\n\n");
        printf("%s: \n",fntbl[index].apiname);
        printf("\tName\t\t\tType\t\t\tDescription\n");
        printf("\t----\t\t\t----\t\t\t-----------\n");
        while(count < fntbl[index].num_param && (pparam->name != NULL))
        {
            printf("\t%s\t\t\t%s\t\t\t%s\n",pparam->name,
                    type_desc[pparam->type], pparam->desc);
            count++; pparam++;
        }
    }
    else
    {
        dbglog("No param info specified for %s ",fntbl[index].apiname);
    }

    return 0;
}


/**
 * @brief Display parameter info
 *
 * @param name - If this is NULL, display param info
 *               for all acfg apis
 *
 * @return
 */
int display_params(char *name)
{
    int idx = -1 ;

    if(name)
    {
        idx =  get_tbl_idx(name) ;
        print_param_info(idx);
    }
    else
    {
        idx = 0 ;
        while( fntbl[idx].apiname != NULL )
        {
            print_param_info(idx);
            idx++;
        }
    }

    return 0;
}

a_status_t
acfg_logger(a_uint8_t *buf)
{
    FILE *ev_fp;

    ev_fp = fopen(ACFG_EVENT_LOG_FILE, "a+");
    if (ev_fp == NULL) {
        printf("unable to open event log file\n");
        return A_STATUS_FAILED;
    }
    fprintf(ev_fp, "%s\n", buf);
    fclose(ev_fp);

    return A_STATUS_OK;
}


/*
 * Event Callbacks
 */
a_status_t
cb_assoc_sta(a_uint8_t *ifname, acfg_assoc_t *stadone)
{
    a_uint8_t buf[255];

    if (stadone->frame_send == 0) {
        sprintf((char *)buf,
                "%s:Event-assoc AP->STA:status %d  %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status,
                stadone->bssid[0],  stadone->bssid[1],
                stadone->bssid[2],  stadone->bssid[3],
                stadone->bssid[4],  stadone->bssid[5]);
    }
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_disassoc_sta(a_uint8_t *ifname, acfg_disassoc_t *stadone)
{
    a_uint8_t buf[255];

    if (stadone->frame_send == 0) {
        sprintf((char *)buf,
                "%s:Event disssoc AP -> STA: reason %d %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->reason,
                stadone->macaddr[0],  stadone->macaddr[1],
                stadone->macaddr[2],  stadone->macaddr[3],
                stadone->macaddr[4],  stadone->macaddr[5]);
    } else {
        sprintf((char *)buf,
                "%s:Event disassoc STA -> AP:status %d  reason %d %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status,
                stadone->reason,
                stadone->macaddr[0],  stadone->macaddr[1],
                stadone->macaddr[2],  stadone->macaddr[3],
                stadone->macaddr[4],  stadone->macaddr[5]);
    }
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_assoc_ap(a_uint8_t *ifname, acfg_assoc_t *apdone)
{
    a_uint8_t buf[255];

    if (apdone->frame_send == 0) {
        sprintf((char *)buf,
                "%s:Event assoc STA -> AP status %d  %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                apdone->status,
                apdone->bssid[0],  apdone->bssid[1],
                apdone->bssid[2],  apdone->bssid[3],
                apdone->bssid[4],  apdone->bssid[5]);
    } else {
        sprintf((char *)buf,
                "%s:Event assoc AP -> STA: status %d  \n",
                ifname,
                apdone->status);
    }
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_disassoc_ap(a_uint8_t *ifname, acfg_disassoc_t *fail)
{
    a_uint8_t buf[255];

    if (fail->frame_send == 0) {
        sprintf((char *)buf, 
                "%s:Event disassoc STA->AP: reason = %d %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                fail->reason,
                fail->macaddr[0], fail->macaddr[1],
                fail->macaddr[2], fail->macaddr[3],
                fail->macaddr[4], fail->macaddr[5]
               );
    } else {
        sprintf((char *)buf, "%s:Event disassoc AP->STA: reason = %d status = %d %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                fail->reason,
                fail->status,
                fail->macaddr[0], fail->macaddr[1],
                fail->macaddr[2], fail->macaddr[3],
                fail->macaddr[4], fail->macaddr[5]
               );
    }
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_auth_sta(a_uint8_t *ifname, acfg_auth_t *stadone)
{
    a_uint8_t buf[255];

    if (stadone->frame_send == 0) {	
        sprintf((char *)buf, "%s:Event auth AP->STA status %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status, 
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    } else {
        sprintf((char *)buf, "%s:Event auth STA->AP status %d\n",
                ifname,
                stadone->status);
    }
    acfg_logger(buf);
    return A_STATUS_OK ;
}


a_status_t
cb_deauth_sta(a_uint8_t *ifname, acfg_dauth_t *stadone)
{
    a_uint8_t buf[255];

    if (stadone->frame_send == 0) {	
        sprintf((char *)buf, 
                "%s:Event deauth AP->STA reason %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->reason, 
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    } else {
        sprintf((char *)buf, 
                "%s: Event deauth STA->AP status %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status, 
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    }
    acfg_logger(buf);
    return A_STATUS_OK ;
}	

a_status_t
cb_auth_ap(a_uint8_t *ifname, acfg_auth_t *stadone)
{
    a_uint8_t buf[255];

    if (stadone->frame_send == 0) {	
        sprintf((char *)buf, 
                "%s: Event auth STA->AP status %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status, 
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    } else {
        sprintf((char *)buf, "AP -> STA auth status %d\n",
                stadone->status);
    }
    acfg_logger(buf);
    return A_STATUS_OK ;
}


a_status_t
cb_deauth_ap(a_uint8_t *ifname, acfg_dauth_t *stadone)
{
    a_uint8_t buf[255];

    if (stadone->frame_send == 0) {	
        sprintf((char *)buf, 
                "%s: Event deauth STA->AP reason %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->reason, 
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    } else {
        sprintf((char *)buf, 
                "%s: Event deauth AP->STA status %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status, 
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    }
    acfg_logger(buf);
    return A_STATUS_OK ;
}	

a_status_t
cb_scan_done(a_uint8_t *ifname, acfg_scan_done_t *apdone)
{
    msg("Event-scan done: ifname - %s",(char *)ifname);
    return A_STATUS_OK ;
}

a_status_t
cb_wsupp_raw_message(a_uint8_t *ifname, acfg_wsupp_raw_message_t *raw)
{
    a_uint8_t buf[255];

    sprintf((char *)buf, 
            "%s: Wsupp Raw: %s\n", ifname, raw->raw_message);
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_wsupp_ap_sta_conn(a_uint8_t *ifname, acfg_wsupp_ap_sta_conn_t *conn)
{
    a_uint8_t buf[255];

    sprintf((char *)buf, "%s:%s\n", ifname, conn->raw_message);
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_wsupp_ap_sta_disconn(a_uint8_t *ifname, acfg_wsupp_ap_sta_conn_t *conn)
{
    a_uint8_t buf[255];

    sprintf((char *)buf, "%s:%s\n", ifname, conn->raw_message);
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_wsupp_wpa_conn(a_uint8_t *ifname, acfg_wsupp_wpa_conn_t *conn)
{
    a_uint8_t buf[255];

    sprintf((char *)buf, "%s:%s\n", ifname, conn->raw_message);
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_wsupp_wpa_disconn(a_uint8_t *ifname, acfg_wsupp_wpa_conn_t *conn)
{
    a_uint8_t buf[255];

    sprintf((char *)buf, "%s:%s\n", ifname, conn->raw_message);
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_wsupp_wpa_term(a_uint8_t *ifname, acfg_wsupp_wpa_conn_t *conn)
{
    msg("Event: %s: WPA TERMINATING: %s",(char *)ifname, conn->raw_message);
    return A_STATUS_OK ;
}

a_status_t
cb_wsupp_wpa_scan(a_uint8_t *ifname, acfg_wsupp_wpa_conn_t *conn)
{
    msg("Event: %s: WPA SCAN RESULT: %s",(char *)ifname, conn->raw_message);
    return A_STATUS_OK ;
}

a_status_t
cb_wsupp_assoc_reject(a_uint8_t *ifname, acfg_wsupp_assoc_t *assoc)
{
    a_uint8_t buf[255];

    sprintf((char *)buf, "%s:%s\n", ifname, assoc->raw_message);
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_wsupp_eap_success(a_uint8_t *ifname, acfg_wsupp_eap_t *eap)
{
    a_uint8_t buf[255];

    sprintf((char *)buf, "%s:%s\n", ifname, eap->raw_message);
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_wsupp_eap_failure(a_uint8_t *ifname, acfg_wsupp_eap_t *eap)
{
    a_uint8_t buf[255];

    sprintf((char *)buf, "%s:%s\n", ifname, eap->raw_message);
    acfg_logger(buf);
    return A_STATUS_OK ;
}

a_status_t
cb_wsupp_wps_enrollee(a_uint8_t *ifname, acfg_wsupp_wps_enrollee_t *enrollee)
{
    msg("Event: %s: WPS ENROLLEE SEEN: %s",
            (char *)ifname, enrollee->raw_message);
    return A_STATUS_OK ;
}

a_status_t
cb_push_button(a_uint8_t *ifname, acfg_pbc_ev_t *pbc)
{
    a_uint8_t buf[128];

    sprintf((char *)buf, "%s:Event Push button\n", ifname);
    acfg_logger(buf);
    return A_STATUS_OK;
}

a_status_t
cb_wsupp_wps_new_ap_setting(a_uint8_t * ifname, 
        acfg_wsupp_wps_new_ap_settings_t *wps_new_ap)
{
    a_uint8_t buf[128];
    a_status_t status = A_STATUS_OK;

    status = acfg_handle_wps_event(ifname, ACFG_EVENT_WPS_NEW_AP_SETTINGS);	
    sprintf((char *)buf, "%s:Wsupp wps recv new AP settings %s\n",
            ifname, wps_new_ap->raw_message);
    acfg_logger(buf);
    return status;
}

a_status_t
cb_wsupp_wps_success(a_uint8_t * ifname, 
        acfg_wsupp_wps_success_t *wps_succ)
{
    a_uint8_t buf[128];
    a_status_t status = A_STATUS_OK;

    status = acfg_handle_wps_event(ifname, ACFG_EVENT_WPS_SUCCESS);	
    sprintf((char *)buf, "%s:Wsupp wps success %s\n",
            ifname, wps_succ->raw_message);
    acfg_logger(buf);
    return status;
}

acfg_event_t ev ;

/**
 * @brief Receive events
 *
 * @param ifname
 * @param nonblock - 1 for nonblocking call 
 *                   0 for blocking call
 * @return
 */
int recv_events(char *ifname, int nonblock)
{
    a_status_t status ;
    acfg_event_mode_t evmode ;

    if(nonblock == 1)
        evmode = ACFG_EVENT_NOBLOCK ;
    else
        evmode = ACFG_EVENT_BLOCK ;

    msg("Issuing %s call to wait for events",\
            evmode==ACFG_EVENT_NOBLOCK ? "nonblocking " : "blocking");

    ev.assoc_sta = cb_assoc_sta ;
    ev.disassoc_sta = cb_disassoc_sta ;
    ev.assoc_ap = cb_assoc_ap ;
    ev.disassoc_ap = cb_disassoc_ap ;
    ev.auth_sta = cb_auth_sta ;
    ev.deauth_sta = cb_deauth_sta ;
    ev.auth_ap = cb_auth_ap ;
    ev.deauth_ap = cb_deauth_ap ;
    ev.scan_done = cb_scan_done ;
    ev.wsupp_raw_message = cb_wsupp_raw_message;
    ev.wsupp_ap_sta_conn = cb_wsupp_ap_sta_conn;
    ev.wsupp_ap_sta_disconn = cb_wsupp_ap_sta_disconn;
    ev.wsupp_wpa_conn = cb_wsupp_wpa_conn;
    ev.wsupp_wpa_disconn = cb_wsupp_wpa_disconn;
    ev.wsupp_wpa_term = cb_wsupp_wpa_term;
    ev.wsupp_wpa_scan = cb_wsupp_wpa_scan;
    ev.wsupp_wps_enrollee = cb_wsupp_wps_enrollee;
    ev.wsupp_assoc_reject = cb_wsupp_assoc_reject;
    ev.wsupp_eap_success = cb_wsupp_eap_success;
    ev.wsupp_eap_failure = cb_wsupp_eap_failure;
    ev.push_button = cb_push_button;
    ev.wsupp_wps_new_ap_setting = cb_wsupp_wps_new_ap_setting;
    ev.wsupp_wps_success = cb_wsupp_wps_success;

    status = acfg_recv_events(&ev, evmode);
    if(status != A_STATUS_OK && status != A_STATUS_SIG)
    {
        msg("Acfg lib returned error...");
        goto errout;
    }

    if(evmode == ACFG_EVENT_NOBLOCK)
    {
        msg("Returned from acfg lib call. Going to sleep...");
        while(1)
            sleep(1000);
    }

errout: ;
        return acfg_to_os_status(status) ;
}

void recv_wps_events(void)
{
    a_status_t status = A_STATUS_OK;

    ev.wsupp_wps_new_ap_setting = cb_wsupp_wps_new_ap_setting;
    ev.wsupp_wps_success = cb_wsupp_wps_success;
    status = acfg_recv_events(&ev, ACFG_EVENT_BLOCK);
    if(status != A_STATUS_OK && status != A_STATUS_SIG)
    {
        msg("Acfg lib returned error...");
    }
}
