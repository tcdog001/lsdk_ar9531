/*
* Copyright (c) 2012 Qualcomm Atheros, Inc..
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
*/

#include <dirent.h>
#include <sys/un.h>

#include "includes.h"
#include "eloop.h"
#include "common.h"
#include "wpa_ctrl.h"
#include "version.h"
#include "wrapd_api.h"

#define HOSTAPD_CONN_TIMES      3
#define WPA_S_CONN_TIMES        3

#define CONFIG_CTRL_IFACE_CLIENT_DIR "/tmp"
#define CONFIG_CTRL_IFACE_CLIENT_PREFIX "wrap_ctrl_"


static const char *hostapd_ctrl_iface_dir = "/var/run/hostapd";
static const char *wpa_s_ctrl_iface_dir = "/var/run/wpa_supplicant";

static const char *wrapd_ctrl_iface_path = "/var/run/wrapd-global";
static const char *global_wpa_s_ctrl_iface_path = "/var/run/wpa_supplicant-global";

static wrapd_hdl_t *wrapd_handle;
static struct wrapd_ctrl *wrapd_conn;

struct wrapd_ctrl *wrapd_wpa_s_conn = NULL;
struct wrapd_ctrl *wrapd_hostapd_conn[HOSTAPD_CNT];

char *ap_ifname[HOSTAPD_CNT];

char *mpsta_ifname = NULL;
char *dbdc_ifname = NULL;


struct wrapd_ctrl {
	int sock;
	struct sockaddr_un local;
	struct sockaddr_un dest;
};

static void usage(void)
{
    printf("wrapd [-g<wrapd ctrl intf>] [-a<ap ifname>] [-p<psta ifname>] "
        "[-w<global wpa_s ctrl intf>] \\\n"
        "      [-A<psta oma>] [-R<psta oma>] [-c<wpa_s conf file>] [-v<vma list file>] "
        "[-d<DBDC ifname> ] \\\n"        
        "      [command..]\n"
        "        -h = help (show this usage text)\n"
		"        -B = run as daemon in the background\n"        
        "        -M = do MAT while adding PSTA\n"
        "        -L = list oma->vma for active PSTAs\n" 
        "        -I = enable isolation\n"      
        "        -S = run in slave mode, send msg to wrapd\n"    
        "        -T = use timer to connect detected PSTAs\n"         
        "      default wrapd ctrl intf: /var/run/wrapd-global\n"
        "      default hostapd ctrl path: /var/run/hostapd/\n"
        "      default wpa_s ctrl path: /var/run/hostapd/\n"
        "      default global wpa_s ctrl intf: /var/run/wpa_supplicant-global\n");
}

struct wrapd_ctrl *
wrapd_ctrl_open(const char *ctrl_iface, wrapd_hdl_t *handle)
{
	struct wrapd_ctrl *priv;
    struct sockaddr_un addr;

	if (ctrl_iface == NULL)
		return NULL;
    
	priv = os_zalloc(sizeof(*priv));
	if (priv == NULL)
		return NULL;
    
	priv->sock = -1;
	priv->sock = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (priv->sock < 0) {
		wrapd_printf("Fail to create socket");
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	os_strlcpy(addr.sun_path, ctrl_iface, sizeof(addr.sun_path));
    
    if (bind(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {      
        wrapd_printf("1st, fail to bind socket");

        if (connect(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {

            if (unlink(ctrl_iface) < 0) {
                wrapd_printf("Intf exists but does not allow to connect");
                goto fail;
            }
            
            if (bind(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
                wrapd_printf("2st, fail to bind socket");
                goto fail;
            }
            wrapd_printf("2st, success to bind socket");
        } else {
            wrapd_printf("Intf exists");
            goto fail;
        }
    }
    
    eloop_register_read_sock(priv->sock, wrapd_ctrl_iface_receive, handle, NULL);
    return priv;

fail:
	if (priv->sock >= 0)
		close(priv->sock);
	os_free(priv);
	return NULL;

}

struct wrapd_ctrl *
wrapd_conn_to_hostapd(const char *ifname)
{
    struct wpa_ctrl *priv = NULL;
	char *cfile;
	int flen;

	if (ifname == NULL) 
		return NULL;

	flen = os_strlen(hostapd_ctrl_iface_dir) + strlen(ifname) + 2;
	cfile = os_malloc(flen);
	if (cfile == NULL)
		return NULL;
    
	snprintf(cfile, flen, "%s/%s", hostapd_ctrl_iface_dir, ifname);
	priv = wpa_ctrl_open(cfile);
    os_free(cfile);
    
    return (struct wrapd_ctrl *)priv;
}

struct wrapd_ctrl *
wrapd_conn_to_mpsta_wpa_s(const char *ifname)
{
    struct wpa_ctrl *priv = NULL;
	char *cfile;
	int flen;

	if (ifname == NULL) 
		return NULL;

	flen = strlen(wpa_s_ctrl_iface_dir) + strlen(ifname) + 2;
	cfile = malloc(flen);
	if (cfile == NULL)
		return NULL;
    
	snprintf(cfile, flen, "%s/%s", wpa_s_ctrl_iface_dir, ifname);
	priv = wpa_ctrl_open(cfile);
    free(cfile);
    
    return (struct wrapd_ctrl *)priv;
}

void wrapd_send_msg(const char *msg, int len, const char *dest_path)
{
    int sock;
    struct sockaddr_un local;
	struct sockaddr_un dest;
    socklen_t destlen = sizeof(dest);
	static int counter = 0;
	int ret;
	size_t res;
	int tries = 0;

	sock = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0) {
		return;
	}

	local.sun_family = AF_UNIX;
	counter++;
try_again:
	ret = os_snprintf(local.sun_path, sizeof(local.sun_path),
			  CONFIG_CTRL_IFACE_CLIENT_DIR "/"
			  CONFIG_CTRL_IFACE_CLIENT_PREFIX "%d-%d",
			  (int) getpid(), counter);
	if (ret < 0 || (size_t) ret >= sizeof(local.sun_path)) {
		close(sock);
		return;
	}
    
	tries++;
	if (bind(sock, (struct sockaddr *) &local, sizeof(local)) < 0) {
		if (errno == EADDRINUSE && tries < 2) {
			/*
			 * getpid() returns unique identifier for this instance
			 * of wpa_ctrl, so the existing socket file must have
			 * been left by unclean termination of an earlier run.
			 * Remove the file and try again.
			 */
			unlink(local.sun_path);
			goto try_again;
		}
		close(sock);
		return;
	}

	dest.sun_family = AF_UNIX;
	res = os_strlcpy(dest.sun_path, dest_path, sizeof(dest.sun_path));
	if (res >= sizeof(dest.sun_path)) {
		close(sock);
		return;
	}
	if (connect(sock, (struct sockaddr *) &dest, sizeof(dest)) < 0) {
		close(sock);
		unlink(local.sun_path);
		return;
	}
    sendto(sock, msg, len, 0, (struct sockaddr *) &dest, destlen);
    
    close(sock);
	return;
}

int main(int argc, char *argv[])
{
    int i, j; 
    int c, res, ret = -1;
    const char *global_wpa_s_ctrl_intf = NULL;
    const char *wrapd_ctrl_intf = NULL;
    const char *vma_conf_file = NULL;
    const char *wpa_s_conf_file = NULL;
    const char *add_psta_addr = NULL;
    const char *remove_psta_addr = NULL;

    int daemonize = 0;
    int hostapd_num = 0;
    int list_psta_addr = 0;
    int do_mat = 0;
    int do_isolation = 0;
    int do_timer = 0;
    int conn_cnt = 0;
    int slave_mode = 0;
    char msg[128] = {0};
    
    if (os_program_init())
        return -1;

    for (i = 0; i < HOSTAPD_CNT; i ++) { 
        ap_ifname[i] = NULL;
        wrapd_hostapd_conn[i] = NULL;
    }

    for (;;) {
        c = getopt(argc, argv, "g:a:p:w:A:R:BLMSITc:v:d:h");
        if (c < 0)
            break;
        switch (c) {
            case 'g':  
                wrapd_ctrl_intf = optarg;
                break;            
            case 'w':
                global_wpa_s_ctrl_intf = optarg;
                break;
            case 'a':
                if (hostapd_num >= HOSTAPD_CNT) {
                    usage(); 
                    goto out;
                }                    
                ap_ifname[hostapd_num ++] = os_strdup(optarg);
                break;
            case 'p':
                mpsta_ifname = os_strdup(optarg);
                break;       
            case 'd':
                dbdc_ifname = os_strdup(optarg);
                break;                      
		    case 'B':
			    daemonize++;
			    break;                
            case 'A':
                add_psta_addr = optarg;
                break;
            case 'R':
                remove_psta_addr = optarg;
                break;  
            case 'L':     
                list_psta_addr = 1;
                break;  
            case 'M':     
                do_mat = 1;
                break;    
            case 'I':     
                do_isolation = 1;
                break;  
            case 'S':     
                slave_mode = 1;
                break;    
            case 'T':
                do_timer = 1;
                break;               
            case 'c':
                wpa_s_conf_file = optarg;
                break;
            case 'v':
                vma_conf_file = optarg;
                break;   
            case 'h':
                usage(); 
                ret = 0;
                goto out;
            default:
                usage();
                goto out;

        }
    }

    for (i = 0; i < hostapd_num - 1; i ++) {
        for (j = i + 1; j < hostapd_num; j ++) {
            if (os_strcmp(ap_ifname[i], ap_ifname[j]) == 0) {
                wrapd_printf("duplicated ap_ifname[%d] of ap_ifname[%d]", i, j);
                goto out;
            }
        }
    }

    if(NULL == wrapd_ctrl_intf)
        wrapd_ctrl_intf = wrapd_ctrl_iface_path;

    if (slave_mode) {
        if(add_psta_addr) {
            if (do_mat) {
                if (dbdc_ifname) {
                    wrapd_printf("Invalid MAT option, DBDC is enabled");
                    goto out;                  
                }
                res = os_snprintf(msg, sizeof(msg),"ETH_PSTA_ADD MAT %s %s", ap_ifname[0], add_psta_addr);
            } else
                res = os_snprintf(msg, sizeof(msg),"ETH_PSTA_ADD %s %s", ap_ifname[0], add_psta_addr);
                
            if (res < 0 || res >= sizeof(msg)){
                wrapd_printf("Fail to build ETH_PSTA_ADD msg"); 
                goto out;
            }
            wrapd_send_msg(msg, 128, wrapd_ctrl_intf);
            ret = 0;
            goto out;
            
        } else if (remove_psta_addr) {
            res = os_snprintf(msg, sizeof(msg),"ETH_PSTA_REMOVE %s", remove_psta_addr);
            if (res < 0 || res >= sizeof(msg)){
                wrapd_printf("Fail to build ETH_PSTA_REMOVE msg"); 
                goto out;
            }
            wrapd_send_msg(msg, (16 + 17), wrapd_ctrl_intf);
            ret = 0;
            goto out;

        } else if (list_psta_addr) {
            wrapd_send_msg("PSTA_LIST", 9, wrapd_ctrl_intf);
            ret = 0;
            goto out;
        }
    } 

    if(NULL == global_wpa_s_ctrl_intf)
        global_wpa_s_ctrl_intf = global_wpa_s_ctrl_iface_path;

    if (eloop_init()) {
        wrapd_printf("Failed to initialize event loop");
        goto out;
    }
    
    wrapd_handle = wrapd_conn_to_global_wpa_s(global_wpa_s_ctrl_intf, wpa_s_conf_file, do_isolation, do_timer);
    if (wrapd_handle == NULL) 
        goto out;

    wrapd_conn = wrapd_ctrl_open(wrapd_ctrl_intf, wrapd_handle);

    for (i = 0; i < HOSTAPD_CNT; i ++) { 
        if(ap_ifname[i]) {
            for (conn_cnt = 0; conn_cnt < HOSTAPD_CONN_TIMES; conn_cnt ++) {
                wrapd_hostapd_conn[i] = wrapd_conn_to_hostapd(ap_ifname[i]);
                if (wrapd_hostapd_conn[i]) {
                    wrapd_printf("WRAP hostapd(%s) connected", ap_ifname[i]);
                    break;
                }
                os_sleep(1, 0);
            }   
            if(wrapd_hostapd_conn[i]) {
                if (wpa_ctrl_attach((struct wpa_ctrl *)wrapd_hostapd_conn[i]) != 0) {
                    wrapd_printf("Failed to attach to WRAP hostapd(%s)", ap_ifname[i]);;
                    goto out;
                }
                wrapd_printf("WRAP hostapd(%s) attached", ap_ifname[i]);
                eloop_register_read_sock(wrapd_hostapd_conn[i]->sock, wrapd_hostapd_ctrl_iface_receive, wrapd_handle, (void *)ap_ifname[i]); 
            } else {
                wrapd_printf("WRAP hostapd(%s) not exists", ap_ifname[i]);
            }
        }
    }

    if(mpsta_ifname == NULL) {
        wrapd_printf("Failed to connect to MPSTA wpa_s - mpsta_ifname == NULL");
        goto out;
    }    

    for (conn_cnt = 0; conn_cnt < WPA_S_CONN_TIMES; conn_cnt ++) {
        /*
         * Delay to ensure scan doesn't overlap with ht40 intol acs scan, else will cause 
         * scan to fail and will take more time for MPSTA to associate.
         * EV 131644 
         */
        if(conn_cnt == 0)
            os_sleep(3, 0);
        wrapd_wpa_s_conn = wrapd_conn_to_mpsta_wpa_s(mpsta_ifname);
        if (wrapd_wpa_s_conn) {
            wrapd_printf("MPSTA wpa_s(%s) connected", mpsta_ifname);
            break;
        }
        os_sleep(1, 0);
    }   
    if(wrapd_wpa_s_conn) {
        if (wpa_ctrl_attach((struct wpa_ctrl *)wrapd_wpa_s_conn) != 0) {
            wrapd_printf("Failed to attach to MPSTA wpa_s(%s)", mpsta_ifname);
            goto out;
        }
        wrapd_printf("MPSTA wpa_s(%s) attached", mpsta_ifname);
        eloop_register_read_sock(wrapd_wpa_s_conn->sock, wrapd_wpa_s_ctrl_iface_receive, wrapd_handle, NULL); 
    } else {
        wrapd_printf("MPSTA wpa_s(%s) not exists", mpsta_ifname);
    }

    if (vma_conf_file) {
        wrapd_load_vma_list(vma_conf_file, wrapd_handle);
    }

	if (daemonize && os_daemonize(NULL)) {
		wrapd_printf("daemon");
		goto out;
	}

    eloop_run();

out:

    for (i = 0; i < HOSTAPD_CNT; i ++) { 
        if (ap_ifname[i])
            os_free(ap_ifname[i]);
            
        if (wrapd_hostapd_conn[i])
            wpa_ctrl_close((struct wpa_ctrl *)wrapd_hostapd_conn[i]);
    }
        
    if (dbdc_ifname)
        os_free(dbdc_ifname);
    if (mpsta_ifname)
        os_free(mpsta_ifname);

    if (wrapd_wpa_s_conn)
        wpa_ctrl_close((struct wpa_ctrl *)wrapd_wpa_s_conn); 
    
	os_program_deinit();    
    
    return ret;  
    
}

