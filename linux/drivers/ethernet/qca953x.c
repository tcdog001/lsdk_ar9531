/* 
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "athrs_mac.h"
#define MODULE_NAME "953x_GMAC"
#define MAX_SIZE 10


#ifdef CONFIG_ATHR_PHY_SWAP
#define athr_swap_phy()   							\
	athr_reg_rmw_set(ATHR_GMAC_ETH_CFG, ATHR_GMAC_ETH_CFG_SW_PHY_SWAP);
#define athr_gmac_force_check_link()					\
        athr_gmac_check_link(mac, 0);
#else
#define athr_swap_phy() /* do nothing */
#define athr_gmac_force_check_link()					\
        athr_gmac_check_link(mac, 4);
#endif



extern int frame_sz;
extern athr_gmac_t *athr_gmacs[ATHR_GMAC_NMACS];

extern unsigned int athr_gmac_get_link_st(athr_gmac_t *mac, int *link, int *fdx, 
         athr_phy_speed_t *speed,int phyUnit);

extern void athr_gmac_fast_reset(athr_gmac_t *mac,athr_gmac_desc_t *ds,int ac);
extern void athr_remove_hdr(athr_gmac_t *mac, struct sk_buff *skb);
extern void athr_gmac_add_hdr(athr_gmac_t *mac,struct sk_buff *skb);

static char mii_intf[][MAX_SIZE]={"GMII","MII","RGMII","RMII","SGMII"};

static inline void athr_gmac_set_mac_speed(athr_gmac_t *mac, int is100)
{
    if (is100) {
        athr_gmac_reg_rmw_set(mac, ATHR_GMAC_IFCTL, ATHR_GMAC_IFCTL_SPEED);
    }
    else {
        athr_gmac_reg_rmw_clear(mac, ATHR_GMAC_IFCTL, ATHR_GMAC_IFCTL_SPEED);
    }
}


static inline void athr_gmac_set_mac_if(athr_gmac_t *mac, int is_1000)
{
    athr_gmac_reg_rmw_clear(mac, ATHR_GMAC_CFG2, (ATHR_GMAC_CFG2_IF_1000|
                         ATHR_GMAC_CFG2_IF_10_100));
    if (is_1000) {
        athr_gmac_reg_rmw_set(mac, ATHR_GMAC_CFG2, ATHR_GMAC_CFG2_IF_1000);
        athr_gmac_reg_rmw_set(mac, ATHR_GMAC_FIFO_CFG_5, ATHR_GMAC_BYTE_PER_CLK_EN);
    }
    else {
        athr_gmac_reg_rmw_set(mac, ATHR_GMAC_CFG2, ATHR_GMAC_CFG2_IF_10_100);
        athr_gmac_reg_rmw_clear(mac,ATHR_GMAC_FIFO_CFG_5, ATHR_GMAC_BYTE_PER_CLK_EN);
    }
}

static inline void qca953x_soc_gmac_set_mac_duplex(athr_gmac_t *mac, int fdx)
{ 
    if (fdx) {
        athr_gmac_reg_rmw_set(mac, MAC_CONFIGURATION_2_ADDRESS, ATHR_GMAC_CFG2_FDX);
    }
    else {
        athr_gmac_reg_rmw_clear(mac, MAC_CONFIGURATION_2_ADDRESS, ATHR_GMAC_CFG2_FDX);
    }
} 






int qca953x_gmac_poll_link(athr_gmac_t *mac)
{ 
    struct net_device  *dev     = mac->mac_dev;
    int                 carrier = netif_carrier_ok(dev), fdx, phy_up;
    athr_phy_speed_t  speed;

    int  rc,phyUnit = 0;
#if HYBRID_LINK_CHANGE_EVENT
    int i;
#endif


    assert(mac->mac_ifup);
 
    if (mac->mac_unit == 0)
        phyUnit = 4;

    rc = athr_gmac_get_link_st(mac, &phy_up, &fdx, &speed, phyUnit);
    
    if (mac->mac_unit == 1 && mac_has_flag(mac,ETH_FORCE_SPEED) && is_s27()) {
       speed = ATHR_PHY_SPEED_1000T;
       fdx =  1;
    }

#if HYBRID_LINK_CHANGE_EVENT
    for(i=0; i<HYBRID_MAX_VLAN; i++){
        if(athr_hybrid_lce[i].vlanid == 0)
            continue;
        /* For hybrid with vlan -- 
         * rc is divided into (HYBRID_MAX_VLAN)units, each unit includs 3(HYBIRD_COUNT_BIT) bits, 
         * these 3 bits are uesed to count the port which is UP in the vlan.
         */
        if((((rc>>(i*HYBIRD_COUNT_BIT))&HYBIRD_COUNT_BITMAP) != 0) && 
                (((rc_old>>(i*HYBIRD_COUNT_BIT))&HYBIRD_COUNT_BITMAP) == 0)){
            hybrid_netif_on(athr_hybrid_lce[i].name);
        }else if(((rc>>(i*HYBIRD_COUNT_BIT))&HYBIRD_COUNT_BITMAP) == 0){
            hybrid_netif_off(athr_hybrid_lce[i].name);
        }
    }
    rc_old = rc;
#endif
    if (!phy_up)
    {
        if (carrier)
        {
            printk(MODULE_NAME ": unit %d: phy %0d not up carrier %d\n", mac->mac_unit, phyUnit, carrier);

            /* A race condition is hit when the queue is switched on while tx interrupts are enabled.
             * To avoid that disable tx interrupts when phy is down.
             */
            mac->link_up = 0;
            athr_gmac_intr_disable_tx(mac);
            netif_carrier_off(dev);
            netif_stop_queue(dev);

            if (mac->mac_unit == 0 )
                athr_gmac_reg_rmw_clear(mac, ATHR_GMAC_CFG1, (ATHR_GMAC_CFG1_RX_EN | ATHR_GMAC_CFG1_TX_EN));


       }
       goto done;
    }

    if(!mac->mac_ifup) {
        goto done;
    }

    if ((fdx < 0) || (speed < 0))
    {
        printk(MODULE_NAME ": phy not connected?\n");
        return 0;
    }

    if (athr_chk_phy_in_rst(mac))
        goto done;
    if (carrier && (speed == mac->mac_speed ) && (fdx == mac->mac_fdx)) {
        goto done;
    }

    printk(MODULE_NAME ": enet unit:%d is up...\n", mac->mac_unit);

    printk("eth%d  %s  %s  %s\n", mac->mac_unit, mii_intf[mac->mii_intf],
       spd_str[speed], dup_str[fdx]);

    mac->ops->soc_gmac_set_link(mac, speed, fdx);
    

    printk(MODULE_NAME ": done cfg2 %#x ifctl %#x miictrl  \n",
       athr_gmac_reg_rd(mac, ATHR_GMAC_CFG2),
       athr_gmac_reg_rd(mac, ATHR_GMAC_IFCTL));

    if (is_f1e() || is_s27())
       athr_gmac_fast_reset(mac,NULL,0);
   /* 
    * only Enable Tx and Rx  afeter configuring the mac speed.
    */

        athr_gmac_reg_rmw_set(mac, ATHR_GMAC_CFG1, (ATHR_GMAC_CFG1_RX_EN | ATHR_GMAC_CFG1_TX_EN));

    /*
     * in business
     */
    netif_carrier_on(dev);
    netif_start_queue(dev);
    mac->link_up = 1;

done:
    
    return 0;
}

void qca953x_soc_gmac_set_link(void *arg, athr_phy_speed_t speed, int fdx)
{ 
    athr_gmac_t *mac = (athr_gmac_t *)arg;
    athr_gmac_ops_t *ops = mac->ops;
    int fifo_3 = 0x1f00140;

    if (mac_has_flag(mac,ETH_FORCE_SPEED)) {
        speed = mac->forced_speed;
        fdx   = 1;
    }

    mac->mac_speed =  speed;
    mac->mac_fdx   =  fdx;
    qca953x_soc_gmac_set_mac_duplex(mac, fdx);
    athr_gmac_reg_wr(mac, ATHR_GMAC_FIFO_CFG_3, fifo_3);
    athr_gmac_reg_wr(mac, ATHR_GMAC_DMA_FIFO_THRESH ,0x01d80160);

    switch (speed)
    {
    case ATHR_PHY_SPEED_1000T:
        athr_gmac_set_mac_if(mac, 1);
        athr_gmac_reg_rmw_set(mac, ATHR_GMAC_FIFO_CFG_5, (1 << 19));

        if (ops->set_pll)
	   ops->set_pll(mac,mac->mac_speed);


        if (mac->mac_unit == 0 && is_f1e() )
            athr_reg_wr(ATHR_GMAC_XMII_CONFIG,0x0e000000);

        if ( mac->mac_unit == 0 && is_vir_phy() ){
#ifdef CONFIG_VIR_XMII_CFG
            athr_reg_wr(ATHR_GMAC_XMII_CONFIG,CONFIG_VIR_XMII_CFG);
#else
            athr_reg_wr(ATHR_GMAC_XMII_CONFIG,0x82000000);
#endif

            athr_reg_wr(ATHR_GMAC_ETH_CFG,0x000c0001);
	}

        if (mac->mac_unit == 0 && is_f1e() ) {
            athr_reg_rmw_set(ATHR_GMAC_ETH_CFG,ATHR_GMAC_ETH_CFG_RXD_DELAY);
            athr_reg_rmw_set(ATHR_GMAC_ETH_CFG,ATHR_GMAC_ETH_CFG_RDV_DELAY);
        }

        break;

    case ATHR_PHY_SPEED_100T:
        athr_gmac_set_mac_if(mac, 0);
        athr_gmac_set_mac_speed(mac, 1);
        athr_gmac_reg_rmw_clear(mac, ATHR_GMAC_FIFO_CFG_5, (1 << 19));

        if (mac->mac_unit == 0 && !mac->mac_fdx) {
           athr_gmac_reg_wr(mac, ATHR_GMAC_DMA_FIFO_THRESH ,0x00880060);
           athr_gmac_reg_wr(mac, ATHR_GMAC_FIFO_CFG_3, 0x00f00040);
           /* In Honybee, Inline ingress checksum engine is enabled by default.
              so disable it incase of Half duplex mode */ 
           athr_gmac_reg_rmw_clear(mac, DMA_RESET_OFFSET, (1 << 27));
        }

	if (is_emu() && is_f1e()) {
            athr_reg_wr(0xb804006c,0x2);
            athr_reg_rmw_clear(ATHR_GPIO_OUT_FUNCTION1, (0xff << 16));
            athr_reg_rmw_set(ATHR_GPIO_OE, 0x4);
            athr_reg_rmw_set(ATHR_GPIO_OUT, 0x4);
            udelay(10);
            athr_reg_rmw_clear(ATHR_GPIO_OUT, 0x4);
        }
        if (ops->set_pll)
	   ops->set_pll(mac,mac->mac_speed);

        if ( mac->mac_unit == 0 && is_f1e())
            athr_reg_wr(ATHR_GMAC_XMII_CONFIG,0x0101);

#ifndef CONFIG_ATH_EMULATION
        if (is_ar934x() && mac->mac_unit == 0 && is_f1e()) {
            athr_reg_rmw_clear(ATHR_GMAC_ETH_CFG,ATHR_GMAC_ETH_CFG_RXD_DELAY);
            athr_reg_rmw_clear(ATHR_GMAC_ETH_CFG,ATHR_GMAC_ETH_CFG_RDV_DELAY);
        }
#endif
        break;

    case ATHR_PHY_SPEED_10T:
        athr_gmac_set_mac_if(mac, 0);
        athr_gmac_set_mac_speed(mac, 0);
        athr_gmac_reg_rmw_clear(mac, ATHR_GMAC_FIFO_CFG_5, (1 << 19));

        if (mac->mac_unit == 0 && !mac->mac_fdx) {
           athr_gmac_reg_wr(mac, ATHR_GMAC_DMA_FIFO_THRESH ,0x00880060);
           athr_gmac_reg_wr(mac, ATHR_GMAC_FIFO_CFG_3, 0x00f00040);
           /* In Honybee, Inline ingress checksum engine is enabled by default.
              So disable it incase of Half duplex mode */ 
           athr_gmac_reg_rmw_clear(mac, DMA_RESET_OFFSET, (1 << 27));
        }

        if (ops->set_pll)
	   ops->set_pll(mac,mac->mac_speed);

        if ( mac->mac_unit == 0 && is_f1e()) {
            athr_reg_wr(ATHR_GMAC_XMII_CONFIG,0x1313);
        }
        break;

    default:
        assert(0);
    }
    DPRINTF(MODULE_NAME ": cfg_1: %#x\n", athr_gmac_reg_rd(mac, ATHR_GMAC_FIFO_CFG_1));
    DPRINTF(MODULE_NAME ": cfg_2: %#x\n", athr_gmac_reg_rd(mac, ATHR_GMAC_FIFO_CFG_2));
    DPRINTF(MODULE_NAME ": cfg_3: %#x\n", athr_gmac_reg_rd(mac, ATHR_GMAC_FIFO_CFG_3));
    DPRINTF(MODULE_NAME ": cfg_4: %#x\n", athr_gmac_reg_rd(mac, ATHR_GMAC_FIFO_CFG_4));
    DPRINTF(MODULE_NAME ": cfg_5: %#x\n", athr_gmac_reg_rd(mac, ATHR_GMAC_FIFO_CFG_5));


} 

static int inline athr_get_portno(int phyUnit)
{
#ifdef CONFIG_ATHR_PHY_SWAP
    if (phyUnit == 4)
        return 1;
    else if(phyUnit == 0)
        return 5;
    else
        return (phyUnit +1);
#else
    return ((phyUnit +1));
#endif
}


int qca953x_gmac_check_link(void *arg, int phyUnit)
{
     athr_phy_speed_t   speed;
    athr_gmac_t        *mac = (athr_gmac_t *) arg;
    struct net_device  *dev = mac->mac_dev;
    athr_phy_ops_t     *phy = mac->phy;
    int                carrier = netif_carrier_ok(dev);
    int                fdx, phy_up;
    int                rc;

    assert(mac);
    assert(phy);

    rc = athr_gmac_get_link_st(mac, &phy_up, &fdx, &speed, phyUnit);
    
    if (mac->mac_unit == 1 && mac_has_flag(mac,ETH_FORCE_SPEED) && is_s27()) {
       speed = ATHR_PHY_SPEED_1000T;
       fdx =  1;
    }

    if(phy->stab_wr)
        phy->stab_wr(phyUnit,phy_up,speed);

    if (rc < 0)
        goto done;

    if (!phy_up)
    {
        if (carrier)
        {
            printk(MODULE_NAME ":unit %d: phy %0d not up carrier %d\n", mac->mac_unit, phyUnit, carrier);
            /* A race condition is hit when the queue is switched on while tx interrupts are enabled.
             * To avoid that disable tx interrupts when phy is down.
             */
            mac->link_up = 0;
            athr_gmac_intr_disable_tx(mac);
            netif_carrier_off(dev);
            netif_stop_queue(dev);
        }
        /*
         * Flush the unicast entries of the port.
         */
        if (is_s27())
            phy->write_mac_reg(0x50, ((athr_get_portno(phyUnit) << 8 ) + 0x0d));

        if (mac->mac_unit == 0)
            athr_gmac_reg_rmw_clear(mac, ATHR_GMAC_CFG1, (ATHR_GMAC_CFG1_RX_EN | ATHR_GMAC_CFG1_TX_EN));


        goto done;
    }

    if(!mac->mac_ifup)
        goto done;
    /*
     * phy is up. Either nothing changed or phy setttings changed while we
     * were sleeping.
     */

    if ((fdx < 0) || (speed < 0))
    {
        printk(MODULE_NAME ": phy not connected?\n");
        return 0;
    }


    /*
     * Configure the  pause off and pause
     * on for Global and port threshold
     */
    if (mac->mac_unit == 1 && (is_s27())) {

        phy->write_mac_reg(0x38, 0x16162020);

        switch(phy_up) {

           case 1 :
               phy->write_mac_reg(0x34, 0x16602090);
               break;
           case 2 :
               phy->write_mac_reg(0x34, 0x90bcc0ff);
               break;
           case 3 :
               phy->write_mac_reg(0x34, 0x78bca0ff);
               break;
           case 4 :
               phy->write_mac_reg(0x34, 0x60bc80ff);
               break;
           default :
               break;

        }

    }

    if (carrier && (speed == mac->mac_speed) && (fdx == mac->mac_fdx)) 
        goto done;

    if (phy->is_alive(phyUnit))
    {
        printk(MODULE_NAME ": Enet Unit:%d PHY:%d is UP ", mac->mac_unit,phyUnit);

    printk("eth%d  %s  %s  %s\n", mac->mac_unit, mii_intf[mac->mii_intf],
               spd_str[speed], dup_str[fdx]);



         mac->ops->soc_gmac_set_link(mac, speed, fdx);

        printk(MODULE_NAME ": done cfg2 %#x ifctl %#x miictrl  \n",
           athr_gmac_reg_rd(mac, ATHR_GMAC_CFG2),
           athr_gmac_reg_rd(mac, ATHR_GMAC_IFCTL));

        if (!mac_has_flag(mac,WAN_QOS_SOFT_CLASS))
           athr_gmac_fast_reset(mac,NULL,0);



       /* 
        * only Enable Tx and Rx  afeter configuring the mac speed.
        */

       athr_gmac_reg_rmw_set(mac, ATHR_GMAC_CFG1, (ATHR_GMAC_CFG1_RX_EN | ATHR_GMAC_CFG1_TX_EN));


        /*
         * in business
         */
        netif_carrier_on(dev);
        netif_start_queue(dev);
        mac->link_up = 1;

    }
    else {
        printk(MODULE_NAME ": Enet Unit:%d PHY:%d is Down.\n", mac->mac_unit,phyUnit);
    }

done:
    return 0;
}


int qca953x_gmac_mii_setup(void *arg)
{
    uint8_t mgmt_cfg_val = 0;
   athr_gmac_t *mac = (athr_gmac_t *)arg;



    if ((is_f1e() || is_vir_phy())) {
        if (is_vir_phy()) {
		    printk("HONEYBEE ----> VIR PHY \n");
	}
        else {
		    printk("HONEYBEE ----> F1e PHY\n");
	}

        mgmt_cfg_val = 6;

	/* HONEYBEE Uses GMAC1 for configuration */ 
	mac = athr_gmac_unit2mac(1);
#ifndef CONFIG_ATH_EMULATION
        if(mac->mac_unit == 0)
            athr_reg_wr(ATHR_GMAC_ETH_CFG, ATHR_GMAC_ETH_CFG_RGMII_GE0);  // External RGMII Mode on GE0
#endif
        athr_gmac_reg_wr(mac, ATHR_GMAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
        athr_gmac_reg_wr(mac, ATHR_GMAC_MII_MGMT_CFG, mgmt_cfg_val);

        return 0;
    }

    if (is_s27()) {
        if (!is_emu()) {

	    printk("HONEYBEE ----> S27 PHY MDIO\n");

            mgmt_cfg_val = 7;
            athr_swap_phy();
            athr_reg_wr(ATHR_SWITCH_CLK_SPARE,(athr_reg_rd(ATHR_SWITCH_CLK_SPARE)|0x40));
            athr_gmac_reg_wr(athr_gmacs[1], ATHR_GMAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
            athr_gmac_reg_wr(athr_gmacs[1], ATHR_GMAC_MII_MGMT_CFG, mgmt_cfg_val);

            if (mac_has_flag(mac,ETH_SWONLY_MODE)) {
                athr_reg_rmw_set(ATHR_GMAC_ETH_CFG, ATHR_GMAC_ETH_CFG_SW_ONLY_MODE);
                athr_gmac_reg_rmw_set(athr_gmacs[0], ATHR_GMAC_CFG1, ATHR_GMAC_CFG1_SOFT_RST);;
            }
#ifdef CFG_ATHRS27_APB_ACCESS
          athr_reg_rmw_set(ATHR_GMAC_ETH_CFG, (ATHR_GMAC_ETH_CFG_SW_APB_ACCESS | ATHR_GMAC_ETH_CFG_SW_ACC_MSB_FIRST));
#endif 
        }
        else {
           printk(" HONEYBEE  EMULATION ----> S27 PHY\n");
           mgmt_cfg_val = 7;
           athr_gmac_reg_wr(athr_gmacs[1], ATHR_GMAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
           athr_gmac_reg_wr(athr_gmacs[1], ATHR_GMAC_MII_MGMT_CFG, mgmt_cfg_val);
        }
        return 0;

    }
} 


int qca953x_gmac_hw_setup(void *arg) 
{ 
   athr_gmac_t *mac = (athr_gmac_t *)arg;
    athr_gmac_ring_t *tx, *rx = &mac->mac_rxring;
    athr_gmac_desc_t *r0, *t0;
    uint8_t ac;

    if (mac == NULL) {
        printk (MODULE_NAME "Error while setting up the G-mac\n");
       return -1;
    } 



    /* clear the rx fifo state if any */
    athr_gmac_reg_wr(mac, ATHR_GMAC_DMA_RX_STATUS,
                        athr_gmac_reg_rd(mac, ATHR_GMAC_DMA_RX_STATUS));


    if ((mac->mac_unit == 1))
       athr_gmac_reg_rmw_set(mac, ATHR_GMAC_CFG2, (ATHR_GMAC_CFG2_PAD_CRC_EN |
               ATHR_GMAC_CFG2_LEN_CHECK | ATHR_GMAC_CFG2_IF_1000));

    if (mac->mac_unit == 0)
       athr_gmac_reg_rmw_set(mac, ATHR_GMAC_CFG2, (ATHR_GMAC_CFG2_PAD_CRC_EN |
               ATHR_GMAC_CFG2_LEN_CHECK | ATHR_GMAC_CFG2_IF_10_100));

    athr_gmac_reg_wr(mac, ATHR_GMAC_FIFO_CFG_0, 0x1f00);

    if (mac_has_flag(mac,ATHR_RX_FLCTL))
       athr_gmac_reg_rmw_set(mac,ATHR_GMAC_CFG1,ATHR_GMAC_CFG1_RX_FCTL);

    if (mac_has_flag(mac,ATHR_TX_FLCTL)) {
        /* 
         * If it is GMAC0, set flow control based on half or full duplex.
         * If it is GMAC1, no changes.
         */
        if (mac->mac_unit == 0 && (is_s27() || is_f1e())) {
            if (mac->mac_fdx == 0) {
                /* half duplex.*/
                athr_gmac_reg_rmw_clear(mac, ATHR_GMAC_CFG1, ATHR_GMAC_CFG1_TX_FCTL);
            }
            else {
                /* full duplex */
                athr_gmac_reg_rmw_set(mac, ATHR_GMAC_CFG1, ATHR_GMAC_CFG1_TX_FCTL);
            }
        }
        else {
           athr_gmac_reg_rmw_set(mac, ATHR_GMAC_CFG1, ATHR_GMAC_CFG1_TX_FCTL);
        }
    }

    /* Disable ATHR_GMAC_CFG2_LEN_CHECK to fix the bug that
     * the mac address is mistaken as length when enabling Atheros header
     */
    if (mac_has_flag(mac,ATHR_HEADER_ENABLED))
       athr_gmac_reg_rmw_clear(mac, ATHR_GMAC_CFG2, ATHR_GMAC_CFG2_LEN_CHECK)


    athr_gmac_reg_wr(mac, ATHR_GMAC_FIFO_CFG_1, 0x10ffff);

    if (is_s27()) {
        athr_gmac_reg_wr(mac, ATHR_GMAC_FIFO_CFG_2, 0x03ff0155);
    } else {
        athr_gmac_reg_wr(mac, ATHR_GMAC_FIFO_CFG_2, 0x015500aa);
    }
    /*
     * Weed out junk frames (CRC errored, short collision'ed frames etc.)
     */
    athr_gmac_reg_wr(mac, ATHR_GMAC_FIFO_CFG_4, 0x3ffff);

    /*
     * Drop CRC Errors, Pause Frames ,Length Error frames, Truncated Frames
     * dribble nibble and rxdv error frames.
     */
    printk("Setting Drop CRC Errors, Pause Frames and Length Error frames \n");

    if (mac_has_flag(mac,ATHR_HEADER_ENABLED)) {
       athr_gmac_reg_wr(mac, ATHR_GMAC_FIFO_CFG_5, 0xe6be2);
    }
    else if (mac_has_flag(mac,ATHR_JUMBO_FR)){
       athr_gmac_reg_wr(mac, ATHR_GMAC_FIFO_CFG_5, 0xe6be2);
    }
    else {
       /*
        * Need to set the Out-of-range bit to receive the packets with 1518 bytes, when
        * VLAN is enabled.
        */
       athr_gmac_reg_wr(mac, ATHR_GMAC_FIFO_CFG_5, 0x66bc2);
    }

    if (mac_has_flag(mac,ATHR_JUMBO_FR)) {
       athr_gmac_reg_rmw_set(mac, ATHR_GMAC_CFG2,
                      ATHR_GMAC_CFG2_HUGE_FRAME_EN);
       athr_gmac_reg_wr(mac, ATHR_GMAC_MAX_PKTLEN,frame_sz);
           athr_gmac_reg_rmw_set(mac, DMA_RESET_OFFSET, (1 << 28) | (1 << 29));
    }

    if (mac_has_flag(mac,WAN_QOS_SOFT_CLASS)) {
        athr_gmac_reg_wr(mac,ATHR_GMAC_DMA_TX_ARB_CFG,ATHR_GMAC_TX_QOS_MODE_WEIGHTED
                    | ATHR_GMAC_TX_QOS_WGT_0(0x7)
                    | ATHR_GMAC_TX_QOS_WGT_1(0x5)
                    | ATHR_GMAC_TX_QOS_WGT_2(0x3)
                    | ATHR_GMAC_TX_QOS_WGT_3(0x1));

        for(ac = 0;ac < mac->mac_noacs; ac++) {
            tx = &mac->mac_txring[ac];
            t0  =  &tx->ring_desc[0];
            switch(ac) {
                case ENET_AC_VO:
                    athr_gmac_reg_wr(mac, ATHR_GMAC_DMA_TX_DESC_Q0, athr_gmac_desc_dma_addr(tx, t0));
                    break;
                case ENET_AC_VI:
                    athr_gmac_reg_wr(mac, ATHR_GMAC_DMA_TX_DESC_Q1, athr_gmac_desc_dma_addr(tx, t0));
                    break;
                case ENET_AC_BK:
                    athr_gmac_reg_wr(mac, ATHR_GMAC_DMA_TX_DESC_Q2, athr_gmac_desc_dma_addr(tx, t0));
                    break;
                case ENET_AC_BE:
                    athr_gmac_reg_wr(mac, ATHR_GMAC_DMA_TX_DESC_Q3, athr_gmac_desc_dma_addr(tx, t0));
                    break;
            }
        }
    }
    else {
        tx = &mac->mac_txring[0];
        t0  =  &tx->ring_desc[0];
        athr_gmac_reg_wr(mac, ATHR_GMAC_DMA_TX_DESC_Q0, athr_gmac_desc_dma_addr(tx, t0));
    }

    r0  =  &rx->ring_desc[0];
    athr_gmac_reg_wr(mac, ATHR_GMAC_DMA_RX_DESC, athr_gmac_desc_dma_addr(rx, r0));

    DPRINTF(MODULE_NAME ": cfg1 %#x cfg2 %#x\n", athr_gmac_reg_rd(mac, ATHR_GMAC_CFG1),
        athr_gmac_reg_rd(mac, ATHR_GMAC_CFG2));

    return 0; 
} 

#ifdef CONFIG_ATH_GPIO_LED
void qca953x_gmac_led_setup(void)
{
	athr_reg_rmw_clear(GPIO_OE_ADDRESS,(ATH_GPIO_FUNCTION_OVERCURRENT_EN| ATH_GPIO_FUNCTION_CLK_OBS4_ENABLE | 
						ATH_GPIO_FUNCTION_SPI_CS_1_EN | ATH_GPIO_FUNCTION_S26_UART_DISABLE|ATH_GPIO_FUNCTION_PCIEPHY_TST_EN));

	ath_reg_rmw_clear(GPIO_OUT_FUNCTION1_ADDRESS,GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_SET(0xff));
	ath_reg_rmw_set(GPIO_OUT_FUNCTION1_ADDRESS, GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_SET(0x2d));
	ath_reg_rmw_set(GPIO_OUT_FUNCTION2_ADDRESS, GPIO_OUT_FUNCTION2_ENABLE_GPIO_11_SET(0x2c));
	ath_reg_rmw_set(GPIO_OUT_FUNCTION3_ADDRESS,(GPIO_OUT_FUNCTION3_ENABLE_GPIO_15_SET(0x2a)| GPIO_OUT_FUNCTION3_ENABLE_GPIO_14_SET(0x2b))) ;
	ath_reg_rmw_set(GPIO_OUT_FUNCTION4_ADDRESS,GPIO_OUT_FUNCTION4_ENABLE_GPIO_16_SET(0x29));
	printk(" GPIO LED SETTINGS ....done \n");
}
#endif 
static int
qca953x_set_gmac_caps(void *arg)
{
   athr_gmac_t *mac = (athr_gmac_t *)arg;
#ifdef CONFIG_ATHEROS_HEADER_EN
    if (mac->mac_unit == 1) {
        if (is_s27())
           mac_set_flag(mac,ATHR_S27_HEADER);
    }
#endif

#ifdef CONFIG_ATHRS_QOS
#if !HYBRID_APH126_128_S17_WAR
    if (mac->mac_unit == 0)
        mac_set_flag(mac,WAN_QOS_SOFT_CLASS);
#endif
    if (is_s27()  && mac->mac_unit == 1)
         mac_set_flag(mac,ATHR_SWITCH_QOS);

#endif

#ifdef CONFIG_ATHR_VLAN_IGMP
         mac_set_flag(mac,ATHR_VLAN_IGMP);
#endif


#ifdef CONFIG_GMAC0_RXFCTL
    if (mac->mac_unit == 0)
        mac_set_flag(mac,ATHR_RX_FLCTL);
#endif

#ifdef CONFIG_GMAC0_TXFCTL
    if (mac->mac_unit == 0)
        mac_set_flag(mac,ATHR_TX_FLCTL);
#endif

#ifdef CONFIG_GMAC1_RXFCTL
    if (mac->mac_unit == 1)
        mac_set_flag(mac,ATHR_RX_FLCTL);
#endif

#ifdef CONFIG_ATHR_SWITCH_ONLY_MODE
            mac_set_flag(mac,ETH_SWONLY_MODE);
#endif

#ifdef CONFIG_ATHR_SUPPORT_DUAL_PHY
	mac_set_flag(mac,ATHR_DUAL_PHY);
#endif

#ifdef CONFIG_GMAC1_TXFCTL
    if (mac->mac_unit == 1)
        mac_set_flag(mac,ATHR_TX_FLCTL);
#endif

#ifdef CONFIG_ATHR_RX_TASK
    mac_set_flag(mac,ATHR_RX_TASK);
#else
    mac_set_flag(mac,ATHR_RX_POLL);
#endif

#ifdef ATHR_PORT1_LED_GPIO
    athr_reg_wr(ATHR_GPIO_OE,
          (athr_reg_rd(ATHR_GPIO_OE) & (~(0x1<<ATHR_PORT1_LED_GPIO))));
    athr_reg_wr(ATHR_GPIO_OUT_FUNCTION0+((ATHR_PORT1_LED_GPIO/4) << 2), 
            ((athr_reg_rd(ATHR_GPIO_OUT_FUNCTION0+((ATHR_PORT1_LED_GPIO/4) << 2)) 
            & (~(0xff<<((ATHR_PORT1_LED_GPIO%4) << 3))) | (0x29 << ((ATHR_PORT1_LED_GPIO%4) << 3)))));
#endif

    if (mac->mac_unit == 1) {
       mac_set_flag(mac,ETH_FORCE_SPEED);
       mac->forced_speed = ATHR_PHY_SPEED_1000T;
    }

#ifdef CONFIG_ATH_LINK_INTR 
    mac_set_flag(mac,ETH_LINK_INTERRUPT);
    printk("Link Int Enabled \n");
#else
    printk(" Link poll  Enabled \n");
    mac_set_flag(mac,ETH_LINK_POLL);
    mac_clear_flag(mac,ETH_LINK_INTERRUPT);
#endif 

#ifdef CONFIG_ATH_GPIO_LED
   if (mac->mac_unit == 1) {
      qca953x_gmac_led_setup();
    } 
#endif 
     /* Enable check for DMA */ 
     printk("%s  CHECK DMA STATUS \n",__func__);
     mac_set_flag(mac, CHECK_DMA_STATUS);
    if ( mac_has_flag(mac,ATHR_S27_HEADER)) {
         mac_set_flag(mac,ATHR_HEADER_ENABLED);
    }
    return 0;
} 
static int
check_dma_status_pause(athr_gmac_t *mac) {

    int RxFsm,TxFsm,RxFD,RxCtrl,TxCtrl;

    /*
     * If DMA is in pause state update the watchdog
     * timer to avoid MAC reset.
     */
    RxFsm = athr_gmac_reg_rd(mac,ATHR_GMAC_DMA_RXFSM);
    TxFsm = athr_gmac_reg_rd(mac,ATHR_GMAC_DMA_TXFSM);
    RxFD  = athr_gmac_reg_rd(mac,ATHR_GMAC_DMA_XFIFO_DEPTH);
    RxCtrl = athr_gmac_reg_rd(mac,ATHR_GMAC_DMA_RX_CTRL);
    TxCtrl = athr_gmac_reg_rd(mac,ATHR_GMAC_DMA_TX_CTRL);



    if (((RxFsm & ATHR_GMAC_DMA_DMA_STATE) == 0x3) && ( (((RxFsm >> 4) & ATHR_GMAC_DMA_AHB_STATE) == 0x1) ||  (((RxFsm >> 4) & ATHR_GMAC_DMA_AHB_STATE) == 0x0) || (((RxFsm >> 4) & ATHR_GMAC_DMA_AHB_STATE) == 0x6)) && (RxCtrl == 0x1) && (((RxFsm >> 11) & 0x1ff) == 0x1ff))  {
        DPRINTF("mac:%d RxFsm:%x TxFsm:%x\n",mac->mac_unit,RxFsm,TxFsm);
        return 0;
 
    }
    else if (((((TxFsm >> 4) & ATHR_GMAC_DMA_AHB_STATE) <= 0x4) &&
            ((RxFsm & ATHR_GMAC_DMA_DMA_STATE) == 0x0) &&
            (((RxFsm >> 4) & ATHR_GMAC_DMA_AHB_STATE) == 0x0)) ||
            (((RxFD >> 16) <= 0x20) && (RxCtrl == 1)) ) {
        return 1;
    }
    else {
        DPRINTF(" FIFO DEPTH = %x",RxFD);
        DPRINTF(" RX DMA CTRL = %x",RxCtrl);
        DPRINTF("mac:%d RxFsm:%x TxFsm:%x\n",mac->mac_unit,RxFsm,TxFsm);
        return 2;
    }
}

static int
check_for_dma_status(void *arg,int ac) {

    athr_gmac_t *mac         = (athr_gmac_t *)arg;
    athr_gmac_ring_t   *r    = &mac->mac_txring[ac];
    int                head  = r->ring_head, tail = r->ring_tail;
    athr_gmac_desc_t   *ds;
    athr_gmac_buffer_t *bp;

    /* If Tx hang is asserted reset the MAC and restore the descriptors
     * and interrupt state.
     */
    while (tail != head) // Check for TX DMA.
    {
        ds   = &r->ring_desc[tail];
        bp   =  &r->ring_buffer[tail];

        if(athr_gmac_tx_owned_by_dma(ds)) {
            if ((athr_gmac_get_diff(bp->trans_start,jiffies)) > ((1 * HZ/10))) {

                 /*
                  * If the DMA is in pause state reset kernel watchdog timer
                  */
                if(check_dma_status_pause(mac)) {
                    mac->mac_dev->trans_start = jiffies;
                    return 0;
                }

                printk(MODULE_NAME ": Tx Dma status eth%d : %s\n",mac->mac_unit,
                            athr_gmac_tx_stopped(mac) ? "inactive" : "active");

                athr_gmac_fast_reset(mac,ds,ac);

                break;
            }
        }
        athr_gmac_ring_incr(tail);
    }

    if (check_dma_status_pause(mac) == 0)  //Check for RX DMA
    {
        if (mac->rx_dma_check) { // see if we holding the rx for 100ms
            uint32_t RxFsm;

            if (check_dma_status_pause(mac) == 0) {
                RxFsm = athr_gmac_reg_rd(mac,ATHR_GMAC_DMA_RXFSM);
                printk(MODULE_NAME ": Rx Dma status eth%d : %X\n",mac->mac_unit,RxFsm);
                athr_gmac_fast_reset(mac,NULL,ac);
            }
            mac->rx_dma_check = 0;
        }
        else {
            mac->rx_dma_check = 1;
        }
    }

    return 0;
}
/* dummy definition for honybee */ 
void serdes_pll_lock_detect_st( ) { }  
void mdio_init_device( ){} 


int qca953x_gmac_attach(void *arg )
{

    athr_gmac_t     *mac = (athr_gmac_t *)arg;
    athr_gmac_ops_t *ops = mac->ops;

    if (!ops) {
       ops = (athr_gmac_ops_t *)kmalloc(sizeof(athr_gmac_ops_t), GFP_KERNEL);
    }
     
    printk(MODULE_NAME ": %s\n",__func__); 

    memset(ops,0,sizeof(athr_gmac_ops_t));

    ops->set_caps           = qca953x_set_gmac_caps;
    ops->soc_gmac_hw_setup  = qca953x_gmac_hw_setup;
    ops->soc_gmac_mii_setup = qca953x_gmac_mii_setup;
    ops->check_link 	    = qca953x_gmac_check_link;
    ops->check_dma_st = check_for_dma_status ;
    ops->soc_gmac_set_link  = qca953x_soc_gmac_set_link;
    ops->soft_led           = NULL;
    ops->set_pll            = NULL;

    mac->ops = ops;

    return 0;
}
