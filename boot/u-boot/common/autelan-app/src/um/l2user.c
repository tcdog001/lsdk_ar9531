#include "utils.h"
#include "um.h"
#define SPRINT_MAX_LEN  256
#define MAC80211
static void 
l2user_update(bool created, struct apuser *dst, struct apuser *src)
{
    time_t now = time(NULL);
    
    /*
    * dst is new created
    */
    if (created) {
        dst->wifi.uptime = now;
    } else {
        /*
        * call update callback, before change dst
        */
        um_ubus_update_cb(dst, src);
    }
    #if 0
    if (dst->wifi.uptime) {
        dst->wifi.livetime = now - dst->wifi.uptime;
    }
	#endif
	
    
    os_maccpy(dst->ap, src->ap);
    os_strdcpy(dst->ifname, src->ifname);
    os_maccpy(dst->vap, src->vap);
    
    dst->local = src->local;
    dst->radioid = src->radioid;
    dst->wlanid = src->wlanid;
	dst->ip = src->ip;
	dst->wifi.uptime = src->wifi.uptime;
	dst->wifi.livetime = src->wifi.livetime;

    os_objdcpy(&dst->wifi.rx, &src->wifi.rx);
    os_objdcpy(&dst->wifi.tx, &src->wifi.tx);
    
    dst->wifi.signal = src->wifi.signal;

	dst->portal.type = src->portal.type;
	dst->portal.state = src->portal.state;

    debug_l2timer_trace("l2user %s %s", um_macstring(src->mac), created?"create":"update");    
}


char*  get_ap_macaddr()
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));	
	#ifdef MAC80211
		strcpy( str_tmp_cmd,  "/sbin/ifconfig br-lan| /bin/grep 'HWaddr'|/usr/bin/awk '{print $5}'");
	#else
      strcpy(str_tmp_cmd,"echo 0000");
	#endif	

	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			pclose(fp);
	 }
	pclose(fp);

	return szVal;		
	

}


char*  get_vap_macaddr(char * ifname)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));	
	#ifdef MAC80211
		sprintf( str_tmp_cmd,  "/sbin/ifconfig %s| /bin/grep 'HWaddr'|/usr/bin/awk '{print $5}'",ifname);
	#else
	      strcpy(str_tmp_cmd,"echo 0000");
	#endif

	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }

	return (szVal);		
	

}

char*  get_sta_macaddr(char * ifname,int stanum)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));	
	#ifdef MAC80211
			sprintf( str_tmp_cmd, " /usr/sbin/iw dev %s station dump|/bin/grep 'Station'|/usr/bin/awk '{print $2}'| /bin/sed -n '%dp'",ifname,stanum);
	#else
	      strcpy(str_tmp_cmd,"echo 0000");
	#endif


	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }

	return (szVal);		


}


char*  get_sta_ipaddr(char * stamacaddr)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));	
	#ifdef MAC80211
	
	sprintf( str_tmp_cmd, "/bin/cat /var/dhcp.leases |/bin/grep  '%s'|/usr/bin/awk '{print $3}'",stamacaddr);
	#else
		     ;;
	#endif

	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
	return (szVal);		
;

}


uint32_t  get_sta_uptime(char* phy ,char* ifname,char * stamacaddr)
{
	FILE *fp;
	uint32_t year,month,day,hour,minute,second;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));	
	#ifdef MAC80211
	sprintf( str_tmp_cmd, "/bin/cat /sys/kernel/debug/ieee80211/%s/netdev:%s/stations/%s/connected_time |/usr/bin/awk  '{print  $3 }'|/bin/sed -n '1p'",phy,ifname,stamacaddr);
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
     year  =(atoi(szVal))*365*24*3600;

	 
	sprintf( str_tmp_cmd, "/bin/cat /sys/kernel/debug/ieee80211/%s/netdev:%s/stations/%s/connected_time |/usr/bin/awk  '{print  $3 }'|/bin/sed -n '2p'",phy,ifname,stamacaddr);
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
     month  =(atoi(szVal))*30*24*3600; 


	sprintf( str_tmp_cmd, "/bin/cat /sys/kernel/debug/ieee80211/%s/netdev:%s/stations/%s/connected_time |/usr/bin/awk  '{print  $3 }'|/bin/sed -n '3p'",phy,ifname,stamacaddr);	
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
     day  =(atoi(szVal))*24*3600; 


	sprintf( str_tmp_cmd, "/bin/cat /sys/kernel/debug/ieee80211/%s/netdev:%s/stations/%s/connected_time |/usr/bin/awk  '{print  $3 }'|/bin/sed -n '4p'|/usr/bin/cut -d: -f1",phy,ifname,stamacaddr);
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
     hour=(atoi(szVal))*3600; 


	sprintf( str_tmp_cmd, "/bin/cat /sys/kernel/debug/ieee80211/%s/netdev:%s/stations/%s/connected_time |/usr/bin/awk  '{print  $3 }'|/bin/sed -n '4p'|/usr/bin/cut -d: -f2",phy,ifname,stamacaddr);
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
     hour=(atoi(szVal))*60; 

	sprintf( str_tmp_cmd, "/bin/cat /sys/kernel/debug/ieee80211/%s/netdev:%s/stations/%s/connected_time |/usr/bin/awk  '{print  $3 }'|/bin/sed -n '4p'|/usr/bin/cut -d: -f3",phy,ifname,stamacaddr);
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
     second=(atoi(szVal))+ minute + hour + day + month + year; 
	#else
			  ;;
	#endif
	return second;		
	

}

int get_sta_livetime(char * ifname,int stanum)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));
	#ifdef MAC80211
			sprintf( str_tmp_cmd,  "/usr/sbin/iw dev %s station dump|/bin/grep 'inactive time'|/usr/bin/awk '{print $3}'| /bin/sed -n '%dp'",ifname,stanum);
	#else
		     ;;
	#endif

	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			pclose(fp);
	 }
	return atoi(szVal);
}


long long  get_rx_bytes(char * ifname, int stanum)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));	
	
	#ifdef MAC80211
		sprintf( str_tmp_cmd,  "/usr/sbin/iw dev %s station dump|/bin/grep 'rx bytes'|/usr/bin/cut -d: -f2| /bin/sed -n '%dp'",ifname,stanum);
	#else
		     ;;
	#endif

	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
	return atoll(szVal);		


}

int get_rx_packets(char * ifname,int stanum)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));
	#ifdef MAC80211
			sprintf( str_tmp_cmd,  "/usr/sbin/iw dev %s station dump|/bin/grep 'rx packets'|/usr/bin/cut -d: -f2| /bin/sed -n '%dp'",ifname,stanum);
	#else
		     ;;
	#endif

	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			pclose(fp);
	 }
	return atoi(szVal);

}


int get_rx_bitrate(char * ifname, int stanum)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));
	#ifdef MAC80211

	sprintf( str_tmp_cmd,  "/usr/sbin/iw dev %s station dump|/bin/grep 'rx bitrate'|/usr/bin/awk '{print $3}'|/bin/sed -n '%dp'",ifname,stanum);

	#else
		     ;;
	#endif

	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			pclose(fp);
	 }
	return (atof(szVal)*1000);
	pclose(fp);

}



long long get_tx_bytes(char * ifname, int stanum)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));	
	#ifdef MAC80211

		sprintf( str_tmp_cmd,  "/usr/sbin/iw dev %s station dump|/bin/grep 'tx bytes'|/usr/bin/cut -d: -f2| /bin/sed -n '%dp'",ifname,stanum);

	#else
		 ;;
	#endif

	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';	
			 pclose(fp);
	 }
	return atoll(szVal);
	pclose(fp);

}


int get_tx_packets(char * ifname, int stanum)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));
	#ifdef MAC80211

		sprintf( str_tmp_cmd,  "/usr/sbin/iw dev %s station dump|/bin/grep 'tx packets'|/usr/bin/cut -d: -f2| /bin/sed -n '%dp'",ifname,stanum);

	#else
		 ;;
	#endif
	
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
	return atoi(szVal);
	pclose(fp);

}

int  get_tx_bitrate(char * ifname, int stanum)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));
	#ifdef MAC80211
		sprintf( str_tmp_cmd,  "/usr/sbin/iw dev %s station dump|/bin/grep 'tx bitrate'|/usr/bin/awk '{print $3}'|/bin/sed -n '%dp'",ifname,stanum);	
	#else
		 ;;
	#endif	
	
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
	return (atof(szVal)*1000) ;
	pclose(fp);

}


int  get_sta_signal(char * ifname, int stanum)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));
	#ifdef MAC80211
		sprintf( str_tmp_cmd,  "/usr/sbin/iw dev %s station dump|/bin/grep 'signal:'|/usr/bin/awk '{print $2}'|/usr/bin/cut -d- -f2|/bin/sed -n '%dp'",ifname,stanum);	
	#else
		 ;;
	#endif	
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
	return atoi(szVal);
	pclose(fp);

}


int  get_sta_signalavg(char * ifname, int stanum)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));
	#ifdef MAC80211
		sprintf( str_tmp_cmd,  "/usr/sbin/iw dev %s station dump|/bin/grep 'signal avg:'|/usr/bin/awk '{print $3}'|/usr/bin/cut -d- -f2|/bin/sed -n '%dp'",ifname,stanum);	
	#else
		 ;;
	#endif	
	
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
			fgets(szVal,sizeof(szVal),fp);
			szVal[strlen(szVal)-1] = '\0';
			 pclose(fp);
	 }
	return atoi(szVal);
	pclose(fp);

}


int  get_sta_portalstate()
{

	return  UM_WIFIDOG_STATE_KNOW;

}


int  get_sta_total(char * ifname)
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));
	#ifdef MAC80211
			sprintf( str_tmp_cmd,  "/usr/sbin/iw dev %s station dump|/bin/grep 'Station'|/usr/bin/wc -l",ifname);
	#else
		 ;;
	#endif	
	
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
		fgets(szVal,sizeof(szVal),fp);
		szVal[strlen(szVal)-1] = '\0';
		pclose(fp);
	}
	return atoi(szVal);
	pclose(fp);
}


/*static int 
l2user_timer(struct um_intf *intf)*/
static int 
l2user_timer(struct um_intf *intf)
{
    struct apuser info, *user;
   //struct apuser info;

	int statotal = 0;
	int stanum = 0;
	int a0 = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0;
    char stamacaddr[18]={0};  	

    
    um_user_init(&info, true);
    
    /*
    * TODO: get l2user from iw/wlanconfig
    *   mac80211: iw <dev> <wlan> station dump
    *   madwifi:  wlanconfig <wlan> list
    */
    statotal = get_sta_total(intf->ifname);

    
	if(0 == statotal)
	{
	    printf("=============no  no   no   no  sta  sta  sta ===================\n");
        return 0; 
	}
	else
		{

	for(stanum =1; stanum <= statotal; stanum++)
	{


		sscanf(get_sta_macaddr(intf->ifname, stanum), "%x:%x:%x:%x:%x:%x",&a0,&a1 ,&a2 ,&a3 ,&a4 ,&a5);
			info.mac[0] = a0;
			info.mac[1] = a1;
			info.mac[2] = a2;
			info.mac[3] = a3;
			info.mac[4] = a4;
			info.mac[5] = a5;	

		sscanf(get_ap_macaddr(), "%x:%x:%x:%x:%x:%x",&a0,&a1 ,&a2 ,&a3 ,&a4 ,&a5);
			info.ap[0] = a0;
			info.ap[1] = a1;
			info.ap[2] = a2;
			info.ap[3] = a3;
			info.ap[4] = a4;
			info.ap[5] = a5;

		sscanf(get_vap_macaddr(intf->ifname), "%x:%x:%x:%x:%x:%x",&a0,&a1 ,&a2 ,&a3 ,&a4 ,&a5);
			info.vap[0] = a0;
			info.vap[1] = a1;
			info.vap[2] = a2;
			info.vap[3] = a3;
			info.vap[4] = a4;
			info.vap[5] = a5;
			
		sprintf(stamacaddr,"%02x:%02x:%02x:%02x:%02x:%02x",info.mac[0],info.mac[1],info.mac[2],info.mac[3]
			,info.mac[4],info.mac[5]);
		
		info.ip = inet_addr(get_sta_ipaddr(stamacaddr));			
		strcpy(info.ifname, intf->ifname)	;
		info.wifi.rx.bytes = get_rx_bytes(intf->ifname,stanum);	
		info.wifi.rx.packets = get_rx_packets(intf->ifname,stanum);
		info.wifi.rx.rate= get_rx_bitrate(intf->ifname,stanum);
		info.wifi.tx.bytes = get_tx_bytes(intf->ifname,stanum);	
		info.wifi.tx.packets = get_tx_packets(intf->ifname,stanum);
		info.wifi.tx.rate = get_tx_bitrate(intf->ifname,stanum);
		info.wifi.signal = get_sta_signal(intf->ifname,stanum);
		info.wlanid = intf->wlanid;
		info.radioid = intf->radioid;
		info.wifi.uptime = get_sta_uptime("phy0", intf->ifname, stamacaddr);
		info.wifi.livetime = get_sta_livetime(intf->ifname, stanum);
		info.portal.type = UM_PORTAL_TYPE_WIFIDOG;
		info.portal.state = get_sta_portalstate();
	
	    #if 0
#if 1 /* test data */
    static int imac = 0;
    
   // os_strdcpy(info.ifname, intf->ifname);

    while(0==(info.mac[5] = (++imac) & os_mask(3))) {
        ;
    }
#endif
#endif


    
    user = um_user_update(&info, l2user_update);
    if (NULL==user) {
        return -ENOMEM;
    }
		
		}
		}

    return 0;
}

int  get_wlan_total()
{
	FILE *fp;
	char str_tmp_cmd[SPRINT_MAX_LEN];
	char szVal[SPRINT_MAX_LEN];
	memset( str_tmp_cmd, 0, SPRINT_MAX_LEN );
	memset(szVal, 0x00, sizeof(szVal));
	#ifdef MAC80211
		strcpy( str_tmp_cmd,  "/usr/bin/iwinfo | /bin/grep 'wlan' |/usr/bin/wc -l");
	#else
		 ;;
	#endif	
	
	fp=popen(str_tmp_cmd,"r");
	if(fp)
	{
		fgets(szVal,sizeof(szVal),fp);
		szVal[strlen(szVal)-1] = '\0';
		pclose(fp);
	}
	return atoi(szVal);
	pclose(fp);

}


int
um_l2user_timer(void)
{

    struct um_intf *intf;
    int err = 0;
   
   //foreach intf in wlan cfg
    
    list_for_each_entry(intf, &umc.uci.wlan.cfg, node) {

        err = l2user_timer(intf);
        if (err<0) {
            return err;
        }
    }

    return 0;
}

/******************************************************************************/
