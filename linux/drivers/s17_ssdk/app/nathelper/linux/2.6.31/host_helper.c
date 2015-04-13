/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc. All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <linux/autoconf.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/netfilter_arp.h>
#include <linux/inetdevice.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <net/netfilter/nf_conntrack.h>
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#include <linux/if_vlan.h>
#endif
#if defined (CONFIG_BRIDGE)
#include <net/bridge/br_private.h>
#endif
#include <include/net/ip_fib.h>
#include "hsl_api.h"
#include "fal_nat.h"
#include "fal_ip.h"
#include "hsl.h"
#include "nat_helper.h"
#include "napt_acl.h"
#include "lib/nat_helper_hsl.h"
#include "lib/nat_helper_dt.h"

#ifdef ISISC
#define CONFIG_IPV6_HWACCEL 1
#include "isisc_port_ctrl.h"
#include "isisc_reg.h"
#include "isisc_misc.h"
#include "isisc_fdb.h"
#include "isisc_ip.h"
#include "isisc_nat.h"
#else
#undef CONFIG_IPV6_HWACCEL
#include "isis_port_ctrl.h"
#include "isis_reg.h"
#include "isis_misc.h"
#include "isis_fdb.h"
#include "isis_ip.h"
#include "isis_nat.h"
#endif

#ifdef CONFIG_IPV6_HWACCEL
#include <linux/ipv6.h>
#include <linux/netfilter_ipv6.h>
#endif

extern void simple_strtohex(uint8_t *hex, uint8_t *str, uint8_t len);

#define MAC_LEN 6
#define IP_LEN 4
#define ARP_HEADER_LEN 8

#define ARP_ENTRY_MAX 128

/* P6 is used by loop dev. */
#define S17_P6PAD_MODE_REG_VALUE 0x01000000

#define MULTIROUTE_WR

extern struct net init_net;

#ifdef AP152
char *nat_wan_dev_list = "eth0.2";
char *nat_lan_dev_list = "br0";
#else
char *nat_wan_dev_list = "eth1.2";
char *nat_lan_dev_list = "eth1.1";
#endif

static int wan_fid = 0xffff;
static fal_pppoe_session_t pppoetbl = {0};
static uint32_t pppoe_gwid = 0;
static char nat_bridge_dev[IFNAMSIZ*4];
static uint8_t lanip[4] = {0}, wanip[4] = {0};

#ifdef MULTIROUTE_WR
#define MAX_HOST 8
struct wan_next_hop
{
    u_int32_t host_ip;
    u_int32_t entry_id;
    u_int32_t in_acl;
    u_int32_t in_use;
    u_int8_t  host_mac[6];
};
static struct net_device *multi_route_indev = NULL;
static struct wan_next_hop wan_nh_ent[MAX_HOST] = {{0}};

static int wan_nh_get(u_int32_t host_ip)
{
    int i;

    for (i=0; i<MAX_HOST; i++)
    {
        if ((wan_nh_ent[i].host_ip != 0) && !memcmp(&wan_nh_ent[i].host_ip, &host_ip, 4))
        {
            // printk("%s %d\n", __FUNCTION__, __LINE__);
            // if ((wan_nh_ent[i].entry_id != 0) && (wan_nh_ent[i].in_acl != 1))
            if (wan_nh_ent[i].in_acl != 1)
            {
                printk("%s %d\n", __FUNCTION__, __LINE__);
                wan_nh_ent[i].in_acl = 1;

                return i;
            }
            // printk("%s %d\n", __FUNCTION__, __LINE__);
        }
        printk("%s %d wan_nh_ent 0x%08x host_ip 0x%08x\n", __FUNCTION__, __LINE__, wan_nh_ent[i].host_ip, host_ip);
    }

    return -1;
}

static void wan_nh_add(u_int8_t *host_ip , u_int8_t *host_mac, u_int32_t id)
{
    int i;

    for( i = 0 ; i < MAX_HOST ; i++ )
    {
        if((wan_nh_ent[i].host_ip != 0) && !memcmp(&wan_nh_ent[i].host_ip, host_ip, 4))
        {
            if (host_mac == NULL) break;

            if(!memcmp(&wan_nh_ent[i].host_mac, host_mac,6))
                return;
            else
                break ;
        }

        if(wan_nh_ent[i].host_ip == 0)
            break;
    }

    if (i < MAX_HOST)
    {
        if ((wan_nh_ent[i].in_use) && (wan_nh_ent[i].in_acl)) return;

        memcpy(&wan_nh_ent[i].host_ip, host_ip, 4);
        if (host_mac != NULL)
        {
            memcpy(wan_nh_ent[i].host_mac, host_mac, 6);
            wan_nh_ent[i].entry_id = id;
            if ((wan_nh_ent[i].in_use) && !(wan_nh_ent[i].in_acl))
            {
                droute_add_acl_rules(*(uint32_t *)&lanip, id);
                /* set the in_acl flag */
                wan_nh_ent[i].in_acl = 1;
            }
        }
        else
        {
            /* set the in_use flag */
            wan_nh_ent[i].in_use = 1;
        }
        aos_printk("%s: ip %08x (%d)\n" ,__func__, wan_nh_ent[i].host_ip, i);
    }
}

static uint32_t get_next_hop( uint32_t daddr , uint32_t saddr )
{
    struct fib_result res;
    struct flowi fl = { .nl_u = { .ip4_u =
            {
                .daddr = daddr,
                .saddr = saddr,
                .tos = 0,
                .scope = RT_SCOPE_UNIVERSE,
            }
        },
        .mark = 0,
        .iif = multi_route_indev->ifindex
    };
    struct net    * net = dev_net(multi_route_indev);
    struct fib_nh *mrnh;

    fib_lookup(net, &fl, &res);
    mrnh = res.fi->fib_nh;

    return mrnh->nh_gw;
}

uint32_t napt_set_default_route(fal_ip4_addr_t dst_addr, fal_ip4_addr_t src_addr)
{
    sw_error_t rv;

    /* search for the next hop (s) */
    if (!(get_aclrulemask() & (1 << S17_ACL_LIST_DROUTE)))
    {
        if (multi_route_indev && \
                (nf_athrs17_hnat_wan_type != NF_S17_WAN_TYPE_PPPOE) && (nf_athrs17_hnat_wan_type != NF_S17_WAN_TYPE_PPPOES0))
        {
            uint32_t next_hop = get_next_hop(dst_addr, src_addr);
            aos_printk("Next hop: %08x\n", next_hop);
            if (next_hop != 0)
            {
                fal_host_entry_t arp_entry;

                memset(&arp_entry, 0, sizeof(arp_entry));
                arp_entry.ip4_addr = next_hop;
                arp_entry.flags = FAL_IP_IP4_ADDR;
                rv = isis_ip_host_get(0, FAL_IP_ENTRY_IPADDR_EN, &arp_entry);
                if (rv != SW_OK)
                {
                    printk("%s: isis_ip_host_get error... (non-existed host: %08x?) \n", __func__, next_hop);
                    /* add into the nh_ent */
                    wan_nh_add((u_int8_t *)&next_hop, (u_int8_t *)NULL, 0);
                }
                else
                {
                    printk("%s %d\n", __FUNCTION__, __LINE__);
                    if (wan_nh_get(next_hop) != -1)
                        droute_add_acl_rules(*(uint32_t *)&lanip, arp_entry.entry_id);
                    else
                        printk("%s %d\n", __FUNCTION__, __LINE__);
                }
            }
            else
            {
                aos_printk("no need to set the default route... \n");
                set_aclrulemask (S17_ACL_LIST_DROUTE);
            }
        }
    }
    /* end next hop (s) */

    return SW_OK;
}
#endif /* MULTIROUTE_WR */

static sw_error_t setup_interface_entry(char *list_if, int is_wan)
{
    char temp[IFNAMSIZ*4]; /* Max 4 interface entries right now. */
    char *dev_name, *list_all;
    struct net_device *nat_dev;
    struct in_device *in_device_lan = NULL;
    uint8_t *devmac, if_mac_addr[MAC_LEN];
    char *br_name;
    uint32_t vid = 0;
    sw_error_t setup_error;
    uint32_t ipv6 = 0;

    memcpy(temp, list_if, strlen(list_if)+1);
    list_all = temp;

    setup_error = SW_OK;
    while ((dev_name = strsep(&list_all, " ")) != NULL)
    {
        nat_dev = dev_get_by_name(&init_net, dev_name);
        if (NULL == nat_dev)
        {
            // printk("%s: Cannot get device %s by name!\n", __FUNCTION__, dev_name);
            setup_error = SW_FAIL;
            continue;
        }
#if defined (CONFIG_BRIDGE)
        if (NULL != nat_dev->br_port) /* under bridge interface. */
        {
            /* Get bridge interface name */
            br_name = (char *)nat_dev->br_port->br->dev->name;
            memcpy (nat_bridge_dev, br_name, sizeof(br_name));
            /* Get dmac */
            devmac = (uint8_t *)nat_dev->br_port->br->dev->dev_addr;
        }
        else
#endif /* CONFIG_BRIDGE */
        {
            devmac = (uint8_t *)nat_dev->dev_addr;
        }
        /* get vid */
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
        vid = vlan_dev_vlan_id(nat_dev);
#else
        vid = 0;
#endif
#ifdef CONFIG_IPV6_HWACCEL
        ipv6 = 1;
        if (is_wan)
        {
            wan_fid = vid;
        }
#else
        ipv6 = 0;
        if (is_wan)
        {
            if (NF_S17_WAN_TYPE_PPPOEV6 == nf_athrs17_hnat_wan_type)
                ipv6 = 1;
            wan_fid = vid;
        }
#endif
#ifdef ISISC
        if (0 == is_wan) /* Not WAN -> LAN */
        { /* Setup private and netmask as soon as possible */
            in_device_lan = (struct in_device *) nat_dev->ip_ptr;
            nat_hw_prv_mask_set((a_uint32_t)(in_device_lan->ifa_list->ifa_mask));
            nat_hw_prv_base_set((a_uint32_t)(in_device_lan->ifa_list->ifa_address));
        }
#endif
        memcpy(if_mac_addr, devmac, MAC_LEN);
        devmac = if_mac_addr;
        dev_put(nat_dev);

        HNAT_PRINTK("DMAC: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                    devmac[0], devmac[1], devmac[2],
                    devmac[3], devmac[4], devmac[5]);
        HNAT_PRINTK("VLAN id: %d\n", vid);

        if(if_mac_add(devmac, vid, ipv6) != 0)
        {
            setup_error = SW_FAIL;
            continue;
        }
        else
        {
            setup_error = SW_OK;
        }
    }

    return setup_error;
}

static int setup_all_interface_entry(void)
{
    static int setup_wan_if = 0;
    static int setup_lan_if=0;
    static int setup_default_vid = 0;
    int i = 0;

    if (0 == setup_default_vid)
    {
        for (i=0; i<7; i++) /* For AR8327/AR8337, only 7 port */
        {
            isis_port_route_defv_set(0, i);
        }
        setup_default_vid = 1;
    }

    if (0 == setup_lan_if)
    {
#ifdef ISISC
        isis_arp_cmd_set(0, FAL_MAC_FRWRD); /* Should be put in init function. */
#endif
        if (SW_OK == setup_interface_entry(nat_lan_dev_list, 0))
        {
            setup_lan_if = 1; /* setup LAN interface entry success */
            printk("Setup LAN interface entry done!\n");
        }
    }

    if (0 == setup_wan_if)
    {
        if (SW_OK == setup_interface_entry(nat_wan_dev_list, 1))
        {
            setup_wan_if = 1; /* setup WAN interface entry success */
            printk("Setup WAN interface entry done!\n");
        }
    }
#ifndef ISISC /* For S17c only */
    if ((nf_athrs17_hnat_wan_type == NF_S17_WAN_TYPE_PPPOE) ||
            (nf_athrs17_hnat_wan_type == NF_S17_WAN_TYPE_PPPOEV6))
    {
        uint8_t buf[6];

        HNAT_PRINTK("Peer MAC: %s ", nf_athrs17_hnat_ppp_peer_mac);
        simple_strtohex(buf, nf_athrs17_hnat_ppp_peer_mac, 6);
        /* add the peer interface with VID */
        if_mac_add(buf, wan_fid, 0);
        HNAT_PRINTK(" --> (%.2x-%.2x-%.2x-%.2x-%.2x-%.2x)\n", \
                    buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
        simple_strtohex(buf, nf_athrs17_hnat_wan_ip, 4);
        memcpy(&wanip, buf, 4);
    }
#endif

    return 1;
}

/* check for pppoe session change */
static void isis_pppoe_check_for_redial(void)
{
    if (nf_athrs17_hnat_wan_type == NF_S17_WAN_TYPE_IP)
        return;

    if(((nf_athrs17_hnat_wan_type == NF_S17_WAN_TYPE_PPPOE) \
            || (nf_athrs17_hnat_wan_type == NF_S17_WAN_TYPE_PPPOEV6)) \
            && (pppoetbl.session_id != 0))
    {
        if(pppoetbl.session_id != nf_athrs17_hnat_ppp_id)
        {
            aos_printk("%s: PPPoE session ID changed... \n", __func__);
            if (nf_athrs17_hnat_wan_type != NF_S17_WAN_TYPE_PPPOEV6)
            {
                if (isis_pppoe_session_table_del(0, &pppoetbl) != SW_OK)
                {
                    aos_printk("delete old pppoe session %d entry_id %d failed.. \n", pppoetbl.session_id, pppoetbl.entry_id);
                    return;
                }

                /* force PPPoE parser for multi- and uni-cast packets; for v1.0.7+ */
                pppoetbl.session_id = nf_athrs17_hnat_ppp_id;
                pppoetbl.multi_session = 1;
                pppoetbl.uni_session = 1;
                pppoetbl.entry_id = 0;
                /* set the PPPoE edit reg (0x2200), and PPPoE session reg (0x5f000) */
                if (isis_pppoe_session_table_add(0, &pppoetbl) == SW_OK)
                {
                    isis_pppoe_session_id_set(0, pppoetbl.entry_id, pppoetbl.session_id);
                    printk("%s: new pppoe session id: %x, entry_id: %x\n", __func__, pppoetbl.session_id, pppoetbl.entry_id);
                }
            }
            else  /* nf_athrs17_hnat_wan_type == NF_S17_WAN_TYPE_PPPOEV6 */
            {
                /* reset the session Id only */
                aos_printk("IPV6 PPPOE mode... \n");
                pppoetbl.session_id = nf_athrs17_hnat_ppp_id;
                isis_pppoe_session_id_set(0, pppoetbl.entry_id, pppoetbl.session_id);
                printk("%s: new pppoe session id: %x, entry_id: %x\n", __func__, pppoetbl.session_id, pppoetbl.entry_id);
            }
            /* read back the WAN IP */
            uint8_t buf[4];

            simple_strtohex(buf, nf_athrs17_hnat_wan_ip, 4);
            memcpy(&wanip, buf, 4);
            aos_printk("Read the WAN IP back... %.8x\n", *(uint32_t *)&wanip);
            /* change the PPPoE ACL to ensure the packet is correctly forwarded by the HNAT engine */
            pppoe_add_acl_rules(*(uint32_t *)&wanip, *(uint32_t *)&lanip, pppoe_gwid);
        }
    }
}

#ifdef ISIS
static void pppoev6_mac6_loop_dev(void)
{
#define PPPOEV6_SESSION_ID  0xfffe
    fal_pppoe_session_t ptbl;

    memset(&ptbl, 0, sizeof(fal_pppoe_session_t));

    aos_printk("%s: set MAC6 as loopback device\n", __func__);

    ptbl.session_id = PPPOEV6_SESSION_ID;
    ptbl.multi_session = 1;
    ptbl.uni_session = 1;
    ptbl.entry_id = 0xe;

    /* set the PPPoE edit reg (0x2200), and PPPoE session reg (0x5f000) */
    if (isis_pppoe_session_table_add(0, &ptbl) == SW_OK)
    {
        isis_pppoe_session_id_set(0, ptbl.entry_id, ptbl.session_id);
        aos_printk("%s: pppoe session id: %d added into entry: %d \n", __func__, ptbl.session_id, ptbl.entry_id);
    }
    else
    {
        aos_printk("%s: failed on adding pppoe session id: %d\n", __func__, ptbl.session_id);
    }

    /* PPPoE entry 0 */
    athrs17_reg_write(0x2200, PPPOEV6_SESSION_ID);

    aos_printk("%s: end of function... \n", __func__);
}

static void pppoev6_remove_parser(uint32_t entry_id)
{
    aos_printk("%s: clear entry id: %d\n", __func__, entry_id);
    /* clear the session id in the PPPoE parser engine */
    athrs17_reg_write(PPPOE_SESSION_OFFSET + PPPOE_SESSION_E_OFFSET * entry_id, 0);
}

static void pppoev6_mac6_stop_learning(void)
{
    /* do not disable this port if some other registers are already filled in
       to prevent setting conflict */
    int val = S17_P6PAD_MODE_REG_VALUE;

    if ( val != (1<<24))
    {
        aos_printk("%s: MAC 6 already being used!\n", __FUNCTION__);
        return;
    }


    /* clear the MAC6 learning bit */
    athrs17_reg_write(0x6a8, athrs17_reg_read(0x6a8) & ~(1<<20));

    /* force loopback mode */
    athrs17_reg_write(0x94, 0x7e);
    athrs17_reg_write(0xb4, 0x10);
}
#endif

static int add_pppoe_host_entry(uint32_t sport, a_int32_t arp_entry_id)
{
    a_bool_t ena;
    int rv = SW_OK;
    // int wport = 0xff;

    if (0xffff == wan_fid)
    {
        printk("%s: Cannot get WAN vid!\n", __FUNCTION__);
        return SW_FAIL;
    }

    aos_printk("Wan type: PPPoE, session: %d\n", nf_athrs17_hnat_ppp_id);
    aos_printk("Peer MAC: %s Peer IP: %s\n", nf_athrs17_hnat_ppp_peer_mac, nf_athrs17_hnat_ppp_peer_ip);

    if (isis_pppoe_status_get(0, &ena) != SW_OK)
    {
        aos_printk("Cannot get the PPPoE mode\n");
        ena = 0;
    }

    if (!ena)
    {
        if (isis_pppoe_status_set(0, A_TRUE) != SW_OK)
            aos_printk("Cannot enable the PPPoE mode\n");

        aos_printk("PPPoE enable mode: %d\n", ena);

        pppoetbl.session_id = nf_athrs17_hnat_ppp_id;
        pppoetbl.multi_session = 1;
        pppoetbl.uni_session = 1;
        pppoetbl.entry_id = 0;

        /* set the PPPoE edit reg (0x2200), and PPPoE session reg (0x5f000) */
        rv = isis_pppoe_session_table_add(0, &pppoetbl);
        if (rv == SW_OK)
        {
            uint8_t mbuf[6], ibuf[4];
            a_int32_t a_entry_id = -1;

            isis_pppoe_session_id_set(0, pppoetbl.entry_id, pppoetbl.session_id);
            aos_printk("pppoe session: %d, entry_id: %d\n", pppoetbl.session_id, pppoetbl.entry_id);

            /* create the peer host ARP entry */
            simple_strtohex(ibuf, nf_athrs17_hnat_ppp_peer_ip, 4);
            simple_strtohex(mbuf, nf_athrs17_hnat_ppp_peer_mac, 6);

            a_entry_id = arp_hw_add(S17_WAN_PORT, wan_fid, ibuf, mbuf, 0);
            if (a_entry_id >= 0) /* hostentry creation okay */
            {
                aos_printk("(1)Bind PPPoE session ID: %d, entry_id: %d to host entry: %d\n", \
                           pppoetbl.session_id, pppoetbl.entry_id, a_entry_id);

                rv = isis_ip_host_pppoe_bind(0, a_entry_id, pppoetbl.entry_id, A_TRUE);
                if ( rv != SW_OK)
                {
                    aos_printk("isis_ip_host_pppoe_bind failed (entry: %d, rv: %d)... \n", a_entry_id, rv);
                }

                aos_printk("adding ACLs \n");
                pppoe_gwid = a_entry_id;
                pppoe_add_acl_rules(*(uint32_t *)&wanip, *(uint32_t *)&lanip, a_entry_id);
                aos_printk("ACL creation okay... \n");
            }
        }
        else
        {
            aos_printk("PPPoE session add failed.. (id: %d)\n", pppoetbl.session_id);
            aos_printk("rv: %d\n", rv);
        }

#ifdef ISIS
        if (nf_athrs17_hnat_wan_type == NF_S17_WAN_TYPE_PPPOEV6)
        {
            aos_printk("IPV6 PPPOE mode... (share the same ID with IPV4's)\n");
            pppoev6_mac6_loop_dev();
            pppoev6_remove_parser(pppoetbl.entry_id);

            /* bind the first LAN host to the pseudo PPPoE ID */
            rv = isis_ip_host_pppoe_bind(0, arp_entry_id, 0, A_TRUE);
            if ( rv != SW_OK)
            {
                aos_printk("isis_ip_host_pppoe_bind failed (entry: %d, rv: %d)... \n", arp_entry_id, rv);
            }
        }
#endif
    }
    else  /* ena */
    {
#ifdef ISIS
        if ((nf_athrs17_hnat_wan_type == NF_S17_WAN_TYPE_PPPOEV6) &&
                (sport != S17_WAN_PORT)&& (arp_entry_id != 0))
        {
            aos_printk("IPV6 PPPoE mode\n");
            /* bind LAN hosts to the pseudo PPPoE ID */
            rv = isis_ip_host_pppoe_bind(0, arp_entry_id, 0, A_TRUE);
            if ( rv != SW_OK)
            {
                aos_printk("isis_ip_host_pppoe_bind failed (entry: %d, rv: %d)... \n", arp_entry_id, rv);
            }
        }
#endif
    }
    
    return SW_OK;
}

static int
arp_is_reply(struct sk_buff *skb)
{
    struct arphdr *arp = arp_hdr(skb);

    if (!arp)
    {
        HNAT_PRINTK("%s: Packet has no ARP data\n", __func__);
        return 0;
    }

    if (skb->len < sizeof(struct arphdr))
    {
        HNAT_PRINTK("%s: Packet is too small to be an ARP\n", __func__);
        return 0;
    }

    if (arp->ar_op != htons(ARPOP_REPLY))
    {
        return 0;
    }

    return 1;
}

static int
dev_check(char *in_dev, char *dev_list)
{
    char *list_dev;
    char temp[100] = {0};
    char *list;

    if(!in_dev || !dev_list)
    {
        return 0;
    }

    strcpy(temp, dev_list);
    list = temp;

    HNAT_PRINTK("%s: list:%s\n", __func__, list);
    while ((list_dev = strsep(&list, " ")) != NULL)
    {
        HNAT_PRINTK("%s: strlen:%d list_dev:%s in_dev:%s\n",
                    __func__, strlen(list_dev), list_dev, in_dev);

        if (!strncmp(list_dev, in_dev, strlen(list_dev)))
        {
            HNAT_PRINTK("%s: %s\n", __FUNCTION__, list_dev);
            return 1;
        }
    }

    return 0;
}

static uint32_t get_netmask_from_netdevice(const struct net_device *in_net_dev)
{
    struct in_device *my_in_device = NULL;
    uint32_t result = 0xffffff00;

    if((in_net_dev) && (in_net_dev->ip_ptr != NULL))
    {
        my_in_device = (struct in_device *)(in_net_dev->ip_ptr);
        if(my_in_device->ifa_list != NULL)
        {
            result = my_in_device->ifa_list->ifa_mask;
        }
    }

    return result;
}

static unsigned int
arp_in(unsigned int hook,
       struct sk_buff *skb,
       const struct net_device *in,
       const struct net_device *out,
       int (*okfn) (struct sk_buff *))
{
    struct arphdr *arp = NULL;
    uint8_t *sip, *dip, *smac, *dmac;
    uint8_t dev_is_lan = 0;
    uint32_t sport = 0, vid = 0;
#ifdef ISIS
    uint32_t lan_netmask = 0;
    a_bool_t prvbasemode = 1;
#endif
    a_int32_t arp_entry_id = -1;

    /* check for PPPoE redial here, to reduce overheads */
    isis_pppoe_check_for_redial();

    /* do not write out host table if HNAT is disabled */
    if (!nf_athrs17_hnat)
        return NF_ACCEPT;

    setup_all_interface_entry();

    if(dev_check((char *)in->name, (char *)nat_wan_dev_list))
    {

    }
    else if (dev_check((char *)in->name, (char *)nat_bridge_dev))
    {
        dev_is_lan = 1;
    }
    else
    {
        printk("Not Support device: %s\n",  nat_bridge_dev);
        return NF_ACCEPT;
    }

    if(!arp_is_reply(skb))
    {
        return NF_ACCEPT;
    }

    if(arp_if_info_get((void *)(skb->head), &sport, &vid) != 0)
    {
        return NF_ACCEPT;
    }

    arp = arp_hdr(skb);
    smac = ((uint8_t *) arp) + ARP_HEADER_LEN;
    sip = smac + MAC_LEN;
    dmac = sip + IP_LEN;
    dip = dmac + MAC_LEN;

    arp_entry_id = arp_hw_add(sport, vid, sip, smac, 0);
    if(arp_entry_id < 0)
    {
        return NF_ACCEPT;
    }

    if (0 == dev_is_lan)
    {
        memcpy(&wanip, dip, 4);
#ifdef MULTIROUTE_WR
        wan_nh_add(sip, smac, arp_entry_id);
#endif
    }

    if(dev_is_lan && nat_hw_prv_base_can_update())
    {
        nat_hw_flush();
        nat_hw_prv_base_set(*((uint32_t*)dip));
        nat_hw_prv_base_update_disable();
        memcpy(&lanip, dip, 4); /* copy Lan port IP. */
#ifndef ISISC
        lan_netmask = get_netmask_from_netdevice(in);
        redirect_internal_ip_packets_to_cpu_on_wan_add_acl_rules(*(uint32_t *)&lanip, lan_netmask);
#endif
#ifdef MULTIROUTE_WR
        multi_route_indev = in;
#endif
    }

    if ((nf_athrs17_hnat_wan_type == NF_S17_WAN_TYPE_PPPOE) ||
            (nf_athrs17_hnat_wan_type == NF_S17_WAN_TYPE_PPPOEV6))
    {
        add_pppoe_host_entry(sport, arp_entry_id);
    }

#ifdef ISIS
    /* check for SIP and DIP range */
    if ((lanip[0] != 0) && (wanip[0] != 0))
    {
        if (isis_nat_prv_addr_mode_get(0, &prvbasemode) != SW_OK)
        {
            aos_printk("Private IP base mode check failed: %d\n", prvbasemode);
        }

        if (!prvbasemode) /* mode 0 */
        {
            if ((lanip[0] == wanip[0]) && (lanip[1] == wanip[1]))
            {
                if ((lanip[2] & 0xf0) == (wanip[2] & 0xf0))
                {
                    if (get_aclrulemask()& (1 << S17_ACL_LIST_IPCONF))
                        return NF_ACCEPT;

                    aos_printk("LAN IP and WAN IP conflict... \n");
                    /* set h/w acl to filter out this case */
#ifdef MULTIROUTE_WR
                    // if ( (wan_nh_ent[0].host_ip != 0) && (wan_nh_ent[0].entry_id != 0))
                    if ( (wan_nh_ent[0].host_ip != 0))
                        ip_conflict_add_acl_rules(*(uint32_t *)&wanip, *(uint32_t *)&lanip, wan_nh_ent[0].entry_id);
#endif
                    return NF_ACCEPT;
                }
            }
        }
        else  /* mode 1*/
        {
            ;; /* do nothing */
        }
    }
#endif /* ifdef ISIS */

    return NF_ACCEPT;
}

static struct
        nf_hook_ops arpinhook =
{
    .hook = arp_in,
    .hooknum = NF_ARP_IN,
    .owner = THIS_MODULE,
    .pf = NFPROTO_ARP,
    .priority = NF_IP_PRI_FILTER,
};

#define HOST_AGEOUT_STATUS 1
void host_check_aging(void)
{
    fal_host_entry_t *host_entry_p, host_entry= {0};
    sw_error_t rv;
    int cnt = 0;
    unsigned long flags;
    fal_napt_entry_t src_napt = {0}, pub_napt = {0};

    host_entry_p = &host_entry;
    host_entry_p->entry_id = FAL_NEXT_ENTRY_FIRST_ID;

    local_irq_save(flags);
    while (1)
    {
        host_entry_p->status = HOST_AGEOUT_STATUS;
        /* FIXME: now device id is set to 0. */
        rv = isis_ip_host_next (0, FAL_IP_ENTRY_STATUS_EN, host_entry_p);
        // rv = isis_ip_host_next (0, 0, host_entry_p);
        if (SW_OK != rv)
            break;
        if (cnt >= ARP_ENTRY_MAX) // arp entry number
            break;

        if (ARP_AGE_NEVER == host_entry_p->status)
            continue;

        if ((S17_WAN_PORT == host_entry_p->port_id) &&
                (host_entry_p->counter_en))
        {
            if (0 != host_entry_p->packet)
            {
                // arp entry is using, update it.
                host_entry.status = ARP_AGE;
                printk("Update WAN port hostentry!\n");
                isis_ip_host_add(0, host_entry_p);
            }
            else
            {
                printk("Del WAN port hostentry!\n");
                isis_ip_host_del(0, FAL_IP_ENTRY_IPADDR_EN, host_entry_p);
            }
            continue;
        }

        src_napt.entry_id = FAL_NEXT_ENTRY_FIRST_ID;
        memcpy(&src_napt.src_addr, &host_entry_p->ip4_addr, sizeof(fal_ip4_addr_t));
        pub_napt.entry_id = FAL_NEXT_ENTRY_FIRST_ID;
        memcpy(&pub_napt.trans_addr, &host_entry_p->ip4_addr, sizeof(fal_ip4_addr_t));
        if((isis_napt_next(0, FAL_NAT_ENTRY_SOURCE_IP_EN ,&src_napt) !=0) && \
                (isis_napt_next(0, FAL_NAT_ENTRY_PUBLIC_IP_EN ,&pub_napt) != 0))
        {
            /* Cannot find naptentry */
            printk("ARP id 0x%x: Cannot find NAPT entry!\n", host_entry_p->entry_id);
            isis_ip_host_del(0, FAL_IP_ENTRY_IPADDR_EN, host_entry_p);
            continue;
        }
        // arp entry is using, update it.
        host_entry_p->status = ARP_AGE;
        isis_ip_host_add(0, host_entry_p);
        printk("update entry 0x%x port %d\n", host_entry_p->entry_id, host_entry_p->port_id);
        cnt++;
    }
    local_irq_restore(flags);
}

#ifdef CONFIG_IPV6_HWACCEL
#define IPV6_LEN 16
#define MAC_LEN 6
#define PROTO_ICMPV6 0x3a
#define NEIGHBOUR_AD 136

struct icmpv6_option
{
    __u8 type;
    __u8 len;
    __u8 mac[MAC_LEN];
};

static unsigned int ipv6_handle(unsigned   int   hooknum,
                                struct   sk_buff   *skb,
                                const   struct   net_device   *in,
                                const   struct   net_device   *out,
                                int   (*okfn)(struct   sk_buff   *))
{
    struct ipv6hdr *iph6 = ipv6_hdr(skb);
    struct icmp6hdr *icmp6 = icmp6_hdr(skb);
    __u8 *sip = ((__u8 *)icmp6)+sizeof(struct icmp6hdr);
    struct icmpv6_option *icmpv6_opt = (struct icmpv6_option *)(sip+IPV6_LEN);
    __u8 *sa = icmpv6_opt->mac;

    uint32_t sport = 0, vid = 0;

    if(PROTO_ICMPV6 == iph6->nexthdr)
    {
        if(NEIGHBOUR_AD == icmp6->icmp6_type)
        {
            setup_all_interface_entry();
            HNAT_PRINTK("ND Reply %x %x\n",icmpv6_opt->type,icmpv6_opt->len);
            HNAT_PRINTK("isis_v6: incoming packet, sip = %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\n"
                       ,sip[0],sip[1],sip[2],sip[3],sip[4],sip[5],sip[6],sip[7]
                       ,sip[8],sip[9],sip[10],sip[11],sip[12],sip[13],sip[14],sip[15]
                      );
            HNAT_PRINTK("isis_v6: incoming packet, sa  = %.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n", sa[0], sa[1], sa[2], sa[3], sa[4], sa[5]);

            if(arp_if_info_get((void *)(skb->head), &sport, &vid) != 0)
            {
                return NF_ACCEPT;
            }

            //add nd entry
            if((2 == icmpv6_opt->type) && (1 == icmpv6_opt->len))
            {
                arp_hw_add(sport, vid, sip, sa, 1);
            }
        }
    }


    return NF_ACCEPT;
}

static struct nf_hook_ops ipv6_inhook =
{
    .hook = ipv6_handle,
    .owner = THIS_MODULE,
    .pf = PF_INET6,
    .hooknum = NF_INET_PRE_ROUTING,
    .priority = NF_IP6_PRI_CONNTRACK,
};
#endif /* CONFIG_IPV6_HWACCEL */

extern int napt_procfs_init(void);
extern void napt_procfs_exit(void);

void host_helper_init(void)
{
    int i;
    sw_error_t rv;
    a_uint32_t entry;

    /* header len 4 with type 0xaaaa */
    isis_header_type_set(0, A_TRUE, 0xaaaa);
#ifdef ISISC
    /* For S17c (ISISC), it is not necessary to make all frame with header */
    isis_port_txhdr_mode_set(0, 0, FAL_ONLY_MANAGE_FRAME_EN);
    /* Fix tag disappear problem, set TO_CPU_VID_CHG_EN, 0xc00 bit1 */
    isis_cpu_vid_en_set(0, A_TRUE);
    /* set RM_RTD_PPPOE_EN, 0xc00 bit0 */
    isis_rtd_pppoe_en_set(0, A_TRUE);
    /* Enable ARP ack frame as management frame. */
    for (i=1; i<6; i++)
    {
        isis_port_arp_ack_status_set(0, i, A_TRUE);
    }
    isis_arp_cmd_set(0, FAL_MAC_FRWRD);
    /* set VLAN_TRANS_TEST register bit, to block packets from WAN port has private dip */
    isis_netisolate_set(0, A_TRUE);
#else
    isis_port_txhdr_mode_set(0, 0, FAL_ALL_TYPE_FRAME_EN);
#endif
    isis_cpu_port_status_set(0, A_TRUE);
    isis_ip_route_status_set(0, A_TRUE);

    /* CPU port with VLAN tag, others w/o VLAN */
    entry = 0x01111112;
    HSL_REG_ENTRY_SET(rv, 0, ROUTER_EG, 0, (a_uint8_t *) (&entry), sizeof (a_uint32_t));

    napt_procfs_init();
    
    nf_register_hook(&arpinhook);
#ifdef CONFIG_IPV6_HWACCEL
    aos_printk("Registering IPv6 hooks... \n");
    nf_register_hook(&ipv6_inhook);
#endif
    /* Enable ACLs to handle MLD packets */
    upnp_ssdp_add_acl_rules();
    ipv6_snooping_solicted_node_add_acl_rules();
    ipv6_snooping_sextuple0_group_add_acl_rules();
    ipv6_snooping_quintruple0_1_group_add_acl_rules();
}

void host_helper_exit(void)
{
    napt_procfs_exit();
    
    nf_unregister_hook(&arpinhook);
#ifdef CONFIG_IPV6_HWACCEL
    nf_unregister_hook(&ipv6_inhook);
#endif
}

