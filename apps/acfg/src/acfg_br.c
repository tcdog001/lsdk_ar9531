#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_bridge.h>
#include <acfg_api_pvt.h>
#include "acfg_br.h"

struct bridge *bridge_list = NULL;

int br_ioctl(int sock, unsigned long arg1, unsigned long arg2, 
        unsigned long arg3)
{
    unsigned long arg[3];

    arg[0] = arg1;
    arg[1] = arg2;
    arg[2] = arg3;

    return ioctl(sock, SIOCGIFBR, arg);
}

int br_device_ioctl(struct bridge *br, unsigned long arg0,
        unsigned long arg1, unsigned long arg2,
        unsigned long arg3, int sock)
{
    unsigned long args[4];
    struct ifreq ifr;

    args[0] = arg0;
    args[1] = arg1;
    args[2] = arg2;
    args[3] = arg3;

    memcpy(ifr.ifr_name, br->ifname, IFNAMSIZ);
    ifr.ifr_data = (void *)args;

    return ioctl(sock, SIOCDEVPRIVATE, &ifr);
}


a_status_t 
br_make_port_list(struct bridge *br, int sock)
{
    int i;
    int ifindices[256];

    if (br_device_ioctl(br, BRCTL_GET_PORT_LIST, (unsigned long) ifindices,
                0, 0, sock) < 0) 
    {
        return A_STATUS_FAILED;
    }
    for (i = 255; i >= 0; i--) {
        struct port *p;

        if (!ifindices[i])
            continue;

        p = malloc(sizeof(struct port));
        p->index = i;
        p->ifindex = ifindices[i];
        if (if_indextoname(p->ifindex, p->ifname) == NULL) {
            return A_STATUS_FAILED;
        }
        p->parent = br;
        br->ports[i] = p;
        p->next = br->firstport;
        br->firstport = p;
    }
    return 0;
}

a_status_t acfg_get_br_list()
{
    int i, err;
    int ifindices[32];
    int num, br_socket_fd;
    struct bridge *br;
    struct port *p;

    br_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (br_socket_fd  < 0) {
        return A_STATUS_FAILED;
    }
    num = br_ioctl(br_socket_fd, BRCTL_GET_BRIDGES, 
            (unsigned long) ifindices, 32);
    if (num < 0) {
        return A_STATUS_FAILED;
    }
    for (i = 0; i < num; i++) {

        br = malloc(sizeof(struct bridge));
        memset(br, 0, sizeof(struct bridge));
        br->ifindex = ifindices[i];
        br->firstport = NULL;
        if (if_indextoname(br->ifindex, br->ifname) == NULL) {
            return A_STATUS_FAILED;
        }

        br->next = bridge_list;
        bridge_list = br;
        if ((err = br_make_port_list(br, br_socket_fd)) != 0) {
            return A_STATUS_FAILED;
        }

    }
    br = bridge_list;
    acfg_print("num br = %d\n", num);	
    for (i = 0; i < num; i++) {
        acfg_print("br = %s\n", br->ifname);
        p = br->firstport;
        while (p != NULL) {
            acfg_print("Ii = %s\n", p->ifname);
            p = p->next;
        }		
        br = br->next;		
    }
    return A_STATUS_OK;
}

a_status_t
acfg_get_br_name(a_uint8_t *ifname, a_char_t *brname)
{
    struct bridge *br = NULL;
    struct port *p = NULL;

    acfg_get_br_list();

    br = bridge_list;
    while (br != NULL) {
        p = br->firstport;
        while (p != NULL) {
            if (strncmp((a_char_t *)ifname, p->ifname, ACFG_MAX_IFNAME) == 0) {
                strncpy(brname, br->ifname, ACFG_MAX_IFNAME);
                return A_STATUS_OK;
            }
            p = p->next;
        }		
        br = br->next;		
    }
    return A_STATUS_FAILED;
}
