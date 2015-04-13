/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/un.h>

#include "includes.h"
#include "common.h"
#include "linux_ioctl.h"
#include "wpa_ctrl.h"
#include "ieee802_11_defs.h"
#include "linux_wext.h"
#include "eloop.h"
#include "netlink.h"
#include "priv_netlink.h"

#include "wrapd_api.h"

#define _BYTE_ORDER _BIG_ENDIAN
#include "ieee80211_external.h"


#define WRAP_MAX_PSTA_NUM   30
#define WRAP_MAX_CMD_LEN    128
#define WRAP_PSTA_START_OFF 4


#define BRCTL_FLAG_NORMAL_WIRED         4099
#define BRCTL_FLAG_NORMAL_WIRELESS      4100
#define BRCTL_FLAG_ISOLATION_WIRED      69635
#define BRCTL_FLAG_ISOLATION_WIRELESS   69636

#define HOSTAPD_MSG_ADDR_OFF    3
#define WPA_S_MSG_ADDR_OFF      3

#define MAX(a, b) (((a) > (b)) ? a : b)
#define MIN(a, b) (((a) < (b)) ? a : b)

extern char *ap_ifname[HOSTAPD_CNT];
extern char *dbdc_ifname;

extern struct wrapd_ctrl *wrapd_hostapd_conn[HOSTAPD_CNT];


#define WRAPD_PSTA_FLAG_WIRED   (1 << 0) 
#define WRAPD_PSTA_FLAG_MAT     (1 << 1)
#define WRAPD_PSTA_FLAG_OPEN    (1 << 2)

struct proxy_sta {
    u8 oma[IEEE80211_ADDR_LEN];
    u8 vma[IEEE80211_ADDR_LEN];
    char parent[IFNAMSIZ];   
    char child[IFNAMSIZ];
    int vma_loaded;
    int connected;
    int added;
    u_int32_t flags;

};

struct wrapd_ctrl {
	int sock;
	struct sockaddr_un local;
	struct sockaddr_un dest;
};

struct wrap_demon {
    int ioctl_sock;
    struct wpa_ctrl *ctrl;
    struct wpa_ctrl *global;  
    struct wpa_ctrl *to_hostapd;    
    struct wrapd_ctrl *wrapd;   
    struct netlink_data *netlink;
    struct proxy_sta psta[WRAP_MAX_PSTA_NUM];
    char *wpa_conf_file; 
    int do_isolation;
    int do_timer;
    int in_timer;
    int mpsta_conn;
};


static int char2addr(char* addr)
{
    int i, j=2;

    for(i=2; i<17; i+=3) {
        addr[j++] = addr[i+1];
        addr[j++] = addr[i+2];
    }

    for(i=0; i<12; i++) {
        /* check 0~9, A~F */
        addr[i] = ((addr[i]-48) < 10) ? (addr[i] - 48) : (addr[i] - 55);
        /* check a~f */
        if ( addr[i] >= 42 )
            addr[i] -= 32;
        if ( addr[i] > 0xf )
            return -1;
    }

    for(i=0; i<6; i++)
        addr[i] = (addr[(i<<1)] << 4) + addr[(i<<1)+1];

    return 0;
}

static int 
wrapd_get_80211param(struct wrap_demon *aptr, char *ifname, int op, int *data)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, ifname, IFNAMSIZ);

	iwr.u.mode = op;

	if (ioctl(aptr->ioctl_sock, IEEE80211_IOCTL_GETPARAM, &iwr) < 0) {
		wrapd_printf("ioctl IEEE80211_IOCTL_GETPARAM err, ioctl(%d) op(%d)", 
                IEEE80211_IOCTL_GETPARAM, op); 
		return -1;
	}

	*data = iwr.u.mode;
	return 0;
}

static void 
wrapd_ifname_to_parent_ifname(struct wrap_demon *aptr, char *child, char *parent)
{
    struct ifreq ifr;
    int parent_index;
    
    wrapd_get_80211param(aptr, child, IEEE80211_PARAM_PARENT_IFINDEX, &parent_index);
  
    os_memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifindex = parent_index;
    if (ioctl(aptr->ioctl_sock, SIOCGIFNAME, &ifr) != 0) {
        wrapd_printf("ioctl SIOCGIFNAME err"); 
        return;
    }

    os_memcpy(parent, ifr.ifr_name, IFNAMSIZ);
}

static void 
wrapd_ifindex_to_parent_ifname(struct wrap_demon *aptr, int index, char *ifname)
{
    struct ifreq ifr;
    int parent_index;
    
    os_memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifindex = index;
    if (ioctl(aptr->ioctl_sock, SIOCGIFNAME, &ifr) != 0) {
        wrapd_printf("ioctl SIOCGIFNAME err"); 
        return;
    }
    
    wrapd_get_80211param(aptr, ifr.ifr_name, IEEE80211_PARAM_PARENT_IFINDEX, &parent_index);
  
    os_memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifindex = parent_index;
    if (ioctl(aptr->ioctl_sock, SIOCGIFNAME, &ifr) != 0) {
        wrapd_printf("ioctl SIOCGIFNAME err"); 
        return;
    }

    os_memcpy(ifname, ifr.ifr_name, IFNAMSIZ);
}

static void 
wrapd_ifindex_to_ifname(struct wrap_demon *aptr, int index, char *ifname)
{
    struct ifreq ifr;
    
    os_memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifindex = index;
    if (ioctl(aptr->ioctl_sock, SIOCGIFNAME, &ifr) != 0) {
        wrapd_printf("ioctl SIOCGIFNAME err"); 
        return;
    }
    os_memcpy(ifname, ifr.ifr_name, IFNAMSIZ);
}

static wrapd_status_t 
wrapd_wpa_s_cmd(wrapd_hdl_t *mctx, char *cmd)
{
    struct wrap_demon *aptr = (void *) mctx;
    char buf[2048];
    size_t len = sizeof(buf);
    int ret;

    if (aptr->global == NULL) {
        wrapd_printf("Not connected to global wpa_supplicant");
        return -1;
    }

    ret = wpa_ctrl_request(aptr->global, cmd, os_strlen(cmd), buf, &len, NULL);
    if (ret == -2) {
        wrapd_printf("'%s' command timed out", cmd);
        return WRAPD_STATUS_ETIMEDOUT;
    } else if (ret < 0) {
        wrapd_printf("'%s' command failed", cmd);
        return WRAPD_STATUS_FAILED;
    }
    buf[len] = '\0';
    //wrapd_printf("%s", buf);

    return WRAPD_STATUS_OK;
}

static wrapd_status_t 
wrapd_psta_if_add(wrapd_hdl_t *mctx, void *if_uctx, char *ifname_plus)
{
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    size_t res;
        
    res = snprintf(cmd, sizeof(cmd), "INTERFACE_ADD %s", ifname_plus);

    if (res < 0 || res >= sizeof(cmd))
        return WRAPD_STATUS_BAD_ARG;

    cmd[sizeof(cmd) - 1] = '\0';
        
    return wrapd_wpa_s_cmd(mctx, cmd);
}

static wrapd_status_t  
wrapd_psta_if_remove(wrapd_hdl_t *mctx, char *ifname)
{
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    size_t res;
        
    res = snprintf(cmd, sizeof(cmd), "INTERFACE_REMOVE %s", ifname);
    if (res < 0 || res >= sizeof(cmd))
        return WRAPD_STATUS_BAD_ARG;

    cmd[sizeof(cmd) - 1] = '\0';

    return wrapd_wpa_s_cmd(mctx, cmd);
}

static void 
wrapd_sta_list(struct wrap_demon *aptr)
{
    int i;
    u8 *oma, *vma;

    i = 0;
    printf("PSTA\tAP/wired\tstatus\t\tOMA\t\t\tVMA\n"); 
    while (i < WRAP_MAX_PSTA_NUM) {
        if(aptr->psta[i].added) {
            oma = aptr->psta[i].oma;
            vma = aptr->psta[i].vma;
            printf("ath%d\t%s\t\t%s\t%02x:%02x:%02x:%02x:%02x:%02x\t%02x:%02x:%02x:%02x:%02x:%02x\n",
                i + WRAP_PSTA_START_OFF, 
                (aptr->psta[i].flags & WRAPD_PSTA_FLAG_WIRED)? "wired" : aptr->psta[i].child, 
                (aptr->psta[i].connected)? "connected" : "disconnected",
                oma[0],oma[1],oma[2],oma[3],oma[4],oma[5],
                vma[0],vma[1],vma[2],vma[3],vma[4],vma[5]);
        }
        i ++;
    }
}

static void
wrapd_psta_conn(struct wrap_demon *aptr, int psta_off)
{
    struct proxy_sta *psta = NULL;
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    int res, ifname_num;
    int32_t brctl_flag;

    psta = &aptr->psta[psta_off];
    
    psta->connected = 1;
    ifname_num = psta_off + WRAP_PSTA_START_OFF;
       
    //add wpa_supplicant iface
    res = os_snprintf(cmd, sizeof(cmd),"ath%d\t%s\t%s\t%s\t%s\t%s",
                        ifname_num, 
                        aptr->wpa_conf_file,
                        "",
                        "",
                        "",
                        "");
    if (res < 0 || res >= sizeof(cmd)){
        wrapd_printf("Fail to build wpa_s msg"); 
        return;
    }
    cmd[sizeof(cmd) - 1] = '\0';
    
    wrapd_psta_if_add(aptr, NULL, cmd);

    //add into bridge intf
    if (psta->flags & WRAPD_PSTA_FLAG_MAT) {
        if (aptr->do_isolation)
            brctl_flag = BRCTL_FLAG_ISOLATION_WIRELESS;
        else
            brctl_flag = BRCTL_FLAG_NORMAL_WIRELESS;
        
    } else {
        if (aptr->do_isolation)
            brctl_flag = BRCTL_FLAG_ISOLATION_WIRED;
        else
            brctl_flag = BRCTL_FLAG_NORMAL_WIRED;
    }

    res = os_snprintf(cmd, sizeof(cmd),"brctl addif br0 ath%d %d", ifname_num, brctl_flag);
    if (res < 0 || res >= sizeof(cmd)){
        wrapd_printf("Fail to build brctl cmd"); 
        return;
    }
    cmd[sizeof(cmd) - 1] = '\0';   
    system(cmd);

    wrapd_printf("proxySTA %d is conn", psta_off);

}

static void 
wrapd_psta_disconn(struct wrap_demon *aptr, int psta_off)
{
    struct proxy_sta *psta = NULL;
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    int res, ifname_num;
    
    wrapd_printf("proxySTA %d is disconn", psta_off);
    
    psta = &aptr->psta[psta_off];

    psta->connected = 0;
    ifname_num = psta_off + WRAP_PSTA_START_OFF;

    //del from bridge intf
    res = os_snprintf(cmd, sizeof(cmd),"brctl delif br0 ath%d", ifname_num);
    if (res < 0 || res >= sizeof(cmd)){
        wrapd_printf("Fail to build brctl cmd"); 
        return;
    }
    cmd[sizeof(cmd) - 1] = '\0';
    system(cmd);

    //remove wpa_supplicant iface
    res = os_snprintf(cmd, sizeof(cmd), "ath%d", ifname_num);
    if (res < 0 || res >= sizeof(cmd)){
        wrapd_printf("Fail to build wpa_s del msg"); 
        return;
    }
    cmd[sizeof(cmd) - 1] = '\0';
    wrapd_psta_if_remove(aptr, cmd);

}

static int
wrapd_vap_create(struct wrap_demon *aptr, struct proxy_sta *psta, int ifname_num, const char *parent)
{
    struct ieee80211_clone_params cp;
    struct ifreq ifr;
	int res;

    os_memset(&ifr, 0, sizeof(ifr));
    os_memset(&cp, 0, sizeof(cp));

    res = os_snprintf(cp.icp_name, sizeof(cp.icp_name), "ath%d", ifname_num);
    if (res < 0 || res >= sizeof(cp.icp_name)) {
        wrapd_printf("os_snprintf err");
        return -1;
    }    
    cp.icp_name[IFNAMSIZ - 1] = '\0';
    cp.icp_opmode = IEEE80211_M_STA;
    cp.icp_flags = 0;
    
    os_strncpy(ifr.ifr_name, parent, IFNAMSIZ);

    if (psta->flags & WRAPD_PSTA_FLAG_MAT) {
        os_memcpy(cp.icp_bssid, psta->vma, IEEE80211_ADDR_LEN);        
        os_memcpy(cp.icp_mataddr, psta->oma, IEEE80211_ADDR_LEN);          
        cp.icp_flags |= IEEE80211_CLONE_MACADDR;
        cp.icp_flags |= IEEE80211_CLONE_MATADDR;
    } else {
        os_memcpy(cp.icp_bssid, psta->oma, IEEE80211_ADDR_LEN);
        cp.icp_flags |= IEEE80211_CLONE_MACADDR;
    }

    ifr.ifr_data = (void *) &cp;

/*
    wrapd_printf("cp.icp_name(%s), cp.icp_opmode(%d), cp.icp_flags(0x%04x), ifr.ifr_name(%s)", 
        cp.icp_name, cp.icp_opmode, cp.icp_flags, ifr.ifr_name);
    wrapd_printf("cp.icp_bssid(%02x:%02x:%02x:%02x:%02x:%02x) icp_mataddr(%02x:%02x:%02x:%02x:%02x:%02x)",
        cp.icp_bssid[0],cp.icp_bssid[1],cp.icp_bssid[2],cp.icp_bssid[3],cp.icp_bssid[4],cp.icp_bssid[5],
        cp.icp_mataddr[0],cp.icp_mataddr[1],cp.icp_mataddr[2],cp.icp_mataddr[3],cp.icp_mataddr[4],cp.icp_mataddr[5]);
*/

	if (ioctl(aptr->ioctl_sock, SIOC80211IFCREATE, &ifr) < 0) {
        wrapd_printf("ioctl(SIOC80211IFCREATE)");
        return -1;
    }
    
    return 0;
}

static int
wrapd_vap_destroy(struct wrap_demon *aptr, int ifname_num)
{
	struct ifreq ifr;
	int res;
    
	os_memset(&ifr, 0, sizeof(ifr));
    res = os_snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "ath%d", ifname_num);
    if (res < 0 || res >= sizeof(ifr.ifr_name)) {
        wrapd_printf("os_snprintf err");
        return -1;
    }  

	if (ioctl(aptr->ioctl_sock, SIOC80211IFDESTROY, &ifr) < 0){
        wrapd_printf("ioctl(SIOC80211IFDESTROY)");
        return -1;
    }

    return 0;
}

static void 
wrapd_conn_timer(void *eloop_ctx, void *timeout_ctx)
{
    struct wrap_demon *aptr = (struct wrap_demon *)eloop_ctx;
    struct proxy_sta *psta = NULL;
    int i, sec;
    static int cnt = 0;

    if (0 == aptr->mpsta_conn) {
        wrapd_printf("stop conn cause 0 == aptr->mpsta_conn");
        goto conn_done;
    }

    for (i = 0; i < WRAP_MAX_PSTA_NUM; i ++) {
        psta = &aptr->psta[i];
        if ((psta->added) && (0 == psta->connected)) {
            wrapd_psta_conn(aptr, i);
            if (cnt < 15)
                sec = (i/3) * 2 + 1;
            else
                sec = 6;
            wrapd_printf("<========delay %ds to connect PSTA %d========>", sec, i);
            eloop_register_timeout(sec, 0, wrapd_conn_timer, aptr, NULL);
            aptr->in_timer = 1;
            cnt ++;
            return;
        }
    }

    wrapd_printf("%d connected",cnt);
    
conn_done:  
    cnt = 0;
    aptr->in_timer = 0;    
    eloop_cancel_timeout(wrapd_conn_timer, aptr, NULL);
}

static void 
wrapd_conn_all(struct wrap_demon *aptr)
{
    struct proxy_sta *psta = NULL;
    int i, cnt;

    if (0 == aptr->mpsta_conn) {
        wrapd_printf("stop conn cause 0 == aptr->mpsta_conn");
        return;
    }

    for (i = 0; i < WRAP_MAX_PSTA_NUM; i ++) {
        psta = &aptr->psta[i];
        if ((psta->added) && (0 == psta->connected)) {
            wrapd_psta_conn(aptr, i);
            cnt ++;
        }
    }

    wrapd_printf("%d connected",cnt);
}

static void 
wrapd_disconn_all(struct wrap_demon *aptr)
{
    struct proxy_sta *psta = NULL;
    int i;
      
    if (1 == aptr->mpsta_conn)
        return;

    for (i = 0; i < WRAP_MAX_PSTA_NUM; i ++) {
        psta = &aptr->psta[i];
        if ((psta->added) && (1 == psta->connected)) {         
            wrapd_psta_disconn(aptr, i);
        }
    }
}

static void
wrapd_psta_add(struct wrap_demon *aptr, const char *parent, const char *child, const u8 *addr, u_int32_t flags)
{
    struct proxy_sta *psta = NULL;
    int res, ifname_num;
    int i;
    int fst_unused = -1;

    if(addr == NULL) {
        wrapd_printf("IWEVREGISTERED with NULL addr"); 
        return;
    }

    //wrapd_printf("addr(%02x:%02x:%02x:%02x:%02x:%02x)",
        //addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]); 

    i = 0;
    while (i < WRAP_MAX_PSTA_NUM) {
       if((aptr->psta[i].added) && (os_memcmp(addr, aptr->psta[i].oma, IEEE80211_ADDR_LEN) == 0)) {
            wrapd_printf("oma already exists");
            return;
        } else {   
            if ((fst_unused == -1) && (0 == aptr->psta[i].added)) {
                if ((flags & WRAPD_PSTA_FLAG_MAT) || (0 == aptr->psta[i].vma_loaded))
                    fst_unused = i; 
            }
        }
        i ++;
    }

    if (fst_unused == -1) {
        wrapd_printf("proxySTA num is up to limit");
        return;
    }

    psta = &aptr->psta[fst_unused];
    psta->added = 1;
    
    os_memcpy(psta->oma, addr, IEEE80211_ADDR_LEN);
    
    if (1 != psta->vma_loaded) 
        os_memcpy(psta->vma, addr, IEEE80211_ADDR_LEN);

    if (flags & WRAPD_PSTA_FLAG_MAT) {
        psta->flags |= WRAPD_PSTA_FLAG_MAT;
        if (1 != psta->vma_loaded) 
            psta->vma[0] |= 0x02;
    }

    if (flags & WRAPD_PSTA_FLAG_WIRED) 
        psta->flags |= WRAPD_PSTA_FLAG_WIRED;

    if (flags & WRAPD_PSTA_FLAG_OPEN) 
        psta->flags |= WRAPD_PSTA_FLAG_OPEN;

    ifname_num = fst_unused + WRAP_PSTA_START_OFF;
    
    //create ProxySTA VAP
    res = wrapd_vap_create(aptr, psta, ifname_num, parent);
    if (res < 0){
        wrapd_printf("Fail to create ProxySTA VAP"); 
        psta->added = 0;
        psta->flags = 0;
        os_memset(psta->oma, 0, IEEE80211_ADDR_LEN);
        return;
    }

    os_strncpy(psta->parent, parent, IFNAMSIZ);
    if (child)
        os_strncpy(psta->child, child, IFNAMSIZ);

    wrapd_printf("proxySTA %d is added", fst_unused);

    if (1 == aptr->mpsta_conn) {
        if ((flags & WRAPD_PSTA_FLAG_OPEN) || (0 == aptr->do_timer) ) {
            wrapd_psta_conn(aptr, fst_unused);
            
        } else {
            if((0 == aptr->in_timer) ) {
                eloop_register_timeout(1, 0, wrapd_conn_timer, aptr, NULL);
                aptr->in_timer = 1;
            }
        }
    }

}

static void 
wrapd_psta_remove(struct wrap_demon *aptr, const u8 *addr)
{
    struct proxy_sta *psta = NULL;
    int res, i, ifname_num;

    if(addr == NULL) {
        wrapd_printf("IWEVEXPIRED with NULL addr"); 
        return;
    }
    
    //wrapd_printf("addr(%02x:%02x:%02x:%02x:%02x:%02x)",
        //addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]); 

    i = 0;
    while (i < WRAP_MAX_PSTA_NUM) {
        if((aptr->psta[i].added) && (os_memcmp(addr, aptr->psta[i].oma, IEEE80211_ADDR_LEN) == 0)) {
            psta = &aptr->psta[i];
            break;
        }   
        i ++;
    }

    if(i == WRAP_MAX_PSTA_NUM) {
        wrapd_printf("proxySTA not found");
        return;
    }
    
    wrapd_psta_disconn(aptr, i);

    ifname_num = i + WRAP_PSTA_START_OFF;
    
    //destory ProxySTA VAP
    res = wrapd_vap_destroy(aptr, ifname_num);
    if (res < 0) {
        wrapd_printf("Fail to destroy ProxySTA VAP"); 
        return;
    }

    psta->added = 0;
    psta->flags = 0;
    
    os_memset(psta->oma, 0, IEEE80211_ADDR_LEN);
    os_memset(psta->parent, 0, IFNAMSIZ);
    os_memset(psta->child, 0, IFNAMSIZ);

    wrapd_printf("proxySTA %d is removed", i);

}

static void
wrapd_wireless_event_custom(struct wrap_demon *aptr, int opcode, char *buf, int len)
{
	switch (opcode) {
	case IEEE80211_EV_AUTH_IND_AP:
		break;
	case IEEE80211_EV_DEAUTH_IND_AP:
		break;
	default:
        //wrapd_printf("opcode(%d)", opcode);
		break;
	}
}

static void
wrapd_wireless_event_wireless(struct wrap_demon *aptr, struct ifinfomsg *ifi,
				                        char *data, int len)
{
    struct iw_event iwe_buf, *iwe = &iwe_buf;
    char *pos, *end, *custom;
    char parent[IFNAMSIZ] = {0};
    char child[IFNAMSIZ] = {0};    
    u_int32_t flags = WRAPD_PSTA_FLAG_OPEN;  
    int i;

    wrapd_ifindex_to_ifname(aptr, ifi->ifi_index, child);
    for (i = 0; i < HOSTAPD_CNT; i ++) {
        if ((wrapd_hostapd_conn[i]) && (os_strcmp(child, ap_ifname[i]) == 0))
            return;
    }
    
    pos = data;
    end = data + len;

    while (pos + IW_EV_LCP_LEN <= end) { //+4
        os_memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
        if (iwe->len <= IW_EV_LCP_LEN)
            return;

        custom = pos + IW_EV_POINT_LEN;
        if (iwe->cmd == IWEVEXPIRED || iwe->cmd == IWEVREGISTERED ){
            os_memcpy(&iwe_buf, pos, sizeof(struct iw_event));
            custom += IW_EV_POINT_OFF;
        }



        switch (iwe->cmd) {
        case IWEVEXPIRED:
            wrapd_psta_remove(aptr,(u8 *) iwe->u.addr.sa_data);               
            break;
            
        case IWEVREGISTERED:
            if (dbdc_ifname) {
                os_strncpy(parent, dbdc_ifname, IFNAMSIZ);
                parent[IFNAMSIZ - 1] = '\0';
                flags &= ~WRAPD_PSTA_FLAG_MAT;
            } else {
                wrapd_ifindex_to_parent_ifname(aptr, ifi->ifi_index, parent);
                flags |= WRAPD_PSTA_FLAG_MAT;
            }
            wrapd_psta_add(aptr, parent, child, (u8 *)(iwe->u.addr.sa_data), flags);           
            break;
            
        case IWEVASSOCREQIE:            
        case IWEVCUSTOM: 
			break;
        }

        pos += iwe->len;
    }
}

static void 
wrapd_event_rtm_newlink(void *ctx, struct ifinfomsg *ifi, u8 *buf, size_t len)
{
    struct wrap_demon *aptr = ctx;
    int attrlen, rta_len;
    struct rtattr *attr;

    attrlen = len;
    attr = (struct rtattr *) buf;

    rta_len = RTA_ALIGN(sizeof(struct rtattr));
    while (RTA_OK(attr, attrlen)) {
        if (attr->rta_type == IFLA_WIRELESS) {
            wrapd_wireless_event_wireless(aptr, ifi, ((char *) attr) + rta_len,
                attr->rta_len - rta_len);
        }
        attr = RTA_NEXT(attr, attrlen);
    }

}

static void 
wrapd_event_link(struct wrap_demon *aptr, char *buf, size_t len, int del)
{
}

static void 
wrapd_event_rtm_dellink(void *ctx, struct ifinfomsg *ifi, u8 *buf, size_t len)
{
    struct wrap_demon *aptr = ctx;
	int attrlen, rta_len;
	struct rtattr *attr;

	attrlen = len;
	attr = (struct rtattr *) buf;

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_IFNAME) {
			wrapd_event_link(aptr, ((char *) attr) + rta_len, attr->rta_len - rta_len, 1);
		}
		attr = RTA_NEXT(attr, attrlen);
	}

}

char *
wrapd_ctrl_iface_process(struct wrap_demon *aptr, char *buf, size_t *resp_len)
{
	char *reply;
	const int reply_size = 2048;
	int reply_len, addr_off;
    int mat = 0;
    char parent_ifname[IFNAMSIZ] = {0};
    u_int32_t flags = 0;

	reply = os_malloc(reply_size);
	if (reply == NULL) {
		*resp_len = 1;
		return NULL;
	}

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
        
	} else if (os_strncmp(buf, "ETH_PSTA_ADD ", 13) == 0) {	
        if (os_strncmp(buf + 13, "MAT ", 4) == 0) {
            if (dbdc_ifname) {
                wrapd_printf("Invalid MAT option, DBDC is enabled");
                return NULL;
            }
            
            mat = 1;
            flags |= (WRAPD_PSTA_FLAG_MAT | WRAPD_PSTA_FLAG_WIRED);
            os_memcpy(parent_ifname, (buf + 13 + 4), 5);
            addr_off = 13 + 4 + 5 + 1;
            
        } else {
            if (dbdc_ifname) {
                os_strncpy(parent_ifname, dbdc_ifname, IFNAMSIZ);
                parent_ifname[IFNAMSIZ - 1] = '\0';
            } else {   
                os_memcpy(parent_ifname, (buf + 13), 5);
                parent_ifname[5] = '\0';
            }
            
            mat = 0;
            flags |= WRAPD_PSTA_FLAG_WIRED;
            addr_off = 13 + 5 + 1;
        } 
        if (char2addr(buf + addr_off) != 0) {
            wrapd_printf("Invalid MAC addr");	
            return NULL;
        }

		wrapd_psta_add(aptr, parent_ifname, NULL, (u8 *)(buf + addr_off), flags);
        
	} else if (os_strncmp(buf, "ETH_PSTA_REMOVE ", 16) == 0) {
        if (char2addr(buf + 16) != 0) {
            wrapd_printf("Invalid MAC addr");	
            return NULL;
        }
		wrapd_psta_remove(aptr, (u8 *)(buf + 16));

	} else if (os_strcmp(buf, "PSTA_LIST") == 0) {
		wrapd_sta_list(aptr);
        
	} else {
		os_memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	*resp_len = reply_len;
	return reply;
}

void 
wrapd_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
    struct wrap_demon *aptr = (struct wrap_demon *)eloop_ctx;
    char buf[256];
    int res;
    struct sockaddr_un from;
    socklen_t fromlen = sizeof(from);
    char *reply;
    size_t reply_len;
    
    res = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *) &from, &fromlen);
    if (res < 0) {
        wrapd_printf("recvfrom err");
        return;
    }
    buf[res] = '\0';

    reply = wrapd_ctrl_iface_process(aptr, buf, &reply_len);
    os_free(reply);

/*
    if (reply) {
        sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from, fromlen);
        os_free(reply);
    } else if (reply_len) {
        sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from, fromlen);
    }
*/
}

void
wrapd_hostapd_ctrl_iface_process(struct wrap_demon *aptr, char *msg, char *child_ifname)
{
	int addr_off;
    char parent_ifname[IFNAMSIZ] = {0};
    u_int32_t flags = 0;

	if (os_strncmp(msg + HOSTAPD_MSG_ADDR_OFF, "AP-STA-CONNECTED ", 17) == 0) {	
        flags |= WRAPD_PSTA_FLAG_MAT ;
        addr_off = HOSTAPD_MSG_ADDR_OFF + 17;

        if (char2addr(msg + addr_off) != 0) {
            wrapd_printf("Invalid MAC addr");	
            return;
        }

        if (dbdc_ifname) {
            os_strncpy(parent_ifname, dbdc_ifname, IFNAMSIZ);
            parent_ifname[IFNAMSIZ - 1] = '\0';
            flags &= ~WRAPD_PSTA_FLAG_MAT;
        } else {
            wrapd_ifname_to_parent_ifname(aptr, child_ifname, parent_ifname);
        }
        
		wrapd_psta_add(aptr, parent_ifname, child_ifname, (u8 *)(msg + addr_off), flags);
        
	} else if (os_strncmp(msg + HOSTAPD_MSG_ADDR_OFF, "AP-STA-DISCONNECTED ", 20) == 0) {
	    addr_off = HOSTAPD_MSG_ADDR_OFF + 20;
        if (char2addr(msg + addr_off) != 0) {
            wrapd_printf("Invalid MAC addr");	
            return;
        }
		wrapd_psta_remove(aptr, (u8 *)(msg + addr_off));

	} else {
		wrapd_printf("Unknow msg(%s)", msg);
	}

}

void 
wrapd_hostapd_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
    struct wrap_demon *aptr = (struct wrap_demon *)eloop_ctx;
    char msg[256];
    int res;
    struct sockaddr_un from;
    socklen_t fromlen = sizeof(from);
   
    res = recvfrom(sock, msg, sizeof(msg) - 1, 0, (struct sockaddr *) &from, &fromlen);
    if (res < 0) {
        wrapd_printf("recvfrom err");
        return;
    }
    msg[res] = '\0';

    wrapd_hostapd_ctrl_iface_process(aptr, msg, (char *)sock_ctx);
}

void
wrapd_wpa_s_ctrl_iface_process(struct wrap_demon *aptr, char *msg)
{
	if (os_strncmp(msg + WPA_S_MSG_ADDR_OFF, "CTRL-EVENT-DISCONNECTED ", 24) == 0) {
        aptr->mpsta_conn = 0;
        wrapd_disconn_all(aptr);
        
	} else if (os_strncmp(msg + WPA_S_MSG_ADDR_OFF, "CTRL-EVENT-CONNECTED ", 21) == 0) {
        aptr->mpsta_conn = 1;
        if ((NULL == wrapd_hostapd_conn) || (0 == aptr->do_timer) ){
            wrapd_conn_all(aptr);
        } else {
            if (0 == aptr->in_timer) {
                eloop_register_timeout(1, 0, wrapd_conn_timer, aptr, NULL);
                aptr->in_timer = 1;
            }
        }
        
	} else {
		//wrapd_printf("Unknow msg(%s)", msg);
	}

}

void 
wrapd_wpa_s_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
    struct wrap_demon *aptr = (struct wrap_demon *)eloop_ctx;
    char msg[256];
    int res;
    struct sockaddr_un from;
    socklen_t fromlen = sizeof(from);
   
    res = recvfrom(sock, msg, sizeof(msg) - 1, 0, (struct sockaddr *) &from, &fromlen);
    if (res < 0) {
        wrapd_printf("recvfrom err");
        return;
    }
    msg[res] = '\0';   

    wrapd_wpa_s_ctrl_iface_process(aptr, msg);
}

//addr format: xx:xx:xx:xx:xx:xx
void
wrapd_load_vma_list(const char *conf_file, wrapd_hdl_t *handle)
{
    struct wrap_demon *aptr = (struct wrap_demon *)handle;
	FILE *f;
	char buf[256], *pos, *start;
	int line = 0, off =0;

    wrapd_printf("oma conf file(%s)", conf_file);

    f = fopen(conf_file, "r");
    if (f == NULL) {
        wrapd_printf("Cant open oma conf file(%s)", conf_file);
        return;
    }
    
	while ((fgets(buf, sizeof(buf), f)) && (off < WRAP_MAX_PSTA_NUM)) {
        line ++;
		if (buf[0] == '#')
			continue;
        
        pos = buf;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos ++;
		}

        pos = os_strchr(buf, ':');
        if ((pos == NULL) || (pos - buf < 2) || (os_strlen(pos) < 15)) {
            wrapd_printf("Invalid addr in line %d", line);
            continue;
        }

        start = pos - 2;
        start[17] = '\0';
        if((start[5] != ':') ||(start[8] != ':') ||(start[11] != ':')|| (start[14] != ':')){
            wrapd_printf("Invalid addr in line %d", line);
            continue;
        }
        
        if (char2addr(start) != 0) {
            wrapd_printf("Invalid addr in line %d", line);	
            continue;
        }
        
        os_memcpy(aptr->psta[off].vma, start, IEEE80211_ADDR_LEN);
        aptr->psta[off].vma_loaded = 1;
        aptr->psta[off].added = 0;
        aptr->psta[off].connected = 0;

        wrapd_printf("Load VMA(%02x:%02x:%02x:%02x:%02x:%02x) to off(%d) in line %d",
            aptr->psta[off].vma[0], aptr->psta[off].vma[1], aptr->psta[off].vma[2],
            aptr->psta[off].vma[3], aptr->psta[off].vma[4], aptr->psta[off].vma[5],
            off, line); 
        off ++;
    }

    close((int)f);
}

wrapd_hdl_t *
wrapd_conn_to_global_wpa_s(const char *ifname, const char *confname, int isolation, int timer)
{
    struct wrap_demon *aptr;
    char *realname = (void *)ifname;
    struct netlink_config *cfg;

    if (!realname) 
        return NULL;

    aptr = os_zalloc(sizeof(*aptr));
    if (!aptr)
        return NULL;
        
    aptr->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (aptr->ioctl_sock < 0) {
        wrapd_printf("socket[PF_INET,SOCK_DGRAM]");
        return NULL;
    }

    aptr->global = wpa_ctrl_open(realname);
    if (!aptr->global) {
        close(aptr->ioctl_sock);
        os_free(aptr);          
        wrapd_printf("Fail to connect global wpa_s");
        return NULL;
    }

    cfg = os_zalloc(sizeof(*cfg));
    if (cfg == NULL) {
        close(aptr->ioctl_sock);
        os_free(aptr);  
        return NULL;
    }    
    cfg->ctx = aptr;
    cfg->newlink_cb = wrapd_event_rtm_newlink;

    aptr->netlink = netlink_init(cfg);
    if (aptr->netlink == NULL) {
        close(aptr->ioctl_sock);
        os_free(cfg);
        os_free(aptr);            
        return NULL;
    }

    if (confname)
        aptr->wpa_conf_file = os_strdup(confname); 

    if (isolation == 1)
        aptr->do_isolation = 1;
    else 
        aptr->do_isolation = 0;

     if (timer == 1)
        aptr->do_timer = 1;
    else 
        aptr->do_timer = 0;   

    return (void *) aptr;
}

