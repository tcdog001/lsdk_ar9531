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
 * 
 */

#include "athrs_phy.h"
#include "athrs_mac.h"

extern void athr_dcm_reset_seq(void);

void
athrs_ar8033_mgmt_init(void)
{
    extern void ath_gpio_config_output(int gpio);
    uint32_t rddata;

    rddata = athr_reg_rd(GPIO_IN_ENABLE3_ADDRESS)&
             ~GPIO_IN_ENABLE3_MII_GE1_MDI_MASK;
    rddata |= GPIO_IN_ENABLE3_MII_GE1_MDI_SET(19);
    athr_reg_wr(GPIO_IN_ENABLE3_ADDRESS, rddata);
    
    ath_gpio_config_output(ATH_GPIO);
    ath_gpio_config_output(ATH_GPIO17);
    
    rddata = athr_reg_rd(GPIO_OUT_FUNCTION4_ADDRESS) &
             ~(GPIO_FUNCTION4_MASK);
                

    rddata |=(GPIO_FUNCTION4_ENABLE);
              
    athr_reg_wr(GPIO_OUT_FUNCTION4_ADDRESS, rddata);

    ath_mdio_gpio14();
}

int
athrs_ar8033_phy_setup(void  *arg)
{

    if (is_emu()) {
        athr_dcm_reset_seq();
    }
    
    return 0;
}

int 
athrs_ar8033_reg_init(void *arg)
{


  athrs_ar8033_mgmt_init();
  if (is_emu()){ 
      phy_reg_write(0x1,0x5, 0x9, 0x0);
  }
  phy_reg_write(0x1,0x5, 0x1f, 0x101 );


#ifdef FORCE_10
  printk ("To advertise only 10Mpbps\n");
  phy_reg_write(0x1,0x0, 0x16,0x0);
  phy_reg_write(0x1,0x0, 0x4, 0x0040 |0x0020);
#endif


  printk("%s: Done\n",__func__);
   
  return 0;
}

unsigned int 
athrs_ar8033_phy_read(int ethUnit, unsigned int phy_addr, unsigned int reg_addr)
{
  
  return phy_reg_read(0x1,phy_addr,reg_addr);
 
}

void 
athrs_ar8033_phy_write(int ethUnit,unsigned int phy_addr, unsigned int reg_addr, unsigned int write_data)
{
    phy_reg_write(0x1,phy_addr,reg_addr,write_data);

}
int athrs_ar8033_phy_register_ops(void *arg)
{
  athr_gmac_t *mac   = (athr_gmac_t *) arg;
  athr_phy_ops_t *ops = mac->phy;

  if(!ops)
     ops = kmalloc(sizeof(athr_phy_ops_t), GFP_KERNEL);

  memset(ops,0,sizeof(athr_phy_ops_t));

  ops->mac            =  mac;
  ops->is_up          =  NULL;
  ops->is_alive       =  NULL;
  ops->speed          =  NULL;
  ops->is_fdx         =  NULL;
  ops->ioctl          =  NULL;
  ops->setup          =  athrs_ar8033_phy_setup;
  ops->stab_wr        =  NULL;
  ops->link_isr       =  NULL;
  ops->en_link_intrs  =  NULL;
  ops->dis_link_intrs =  NULL;
  ops->read_phy_reg   =  athrs_ar8033_phy_read;
  ops->write_phy_reg  =  athrs_ar8033_phy_write;
  ops->read_mac_reg   =  NULL;
  ops->write_mac_reg  =  NULL;
  ops->init           =  athrs_ar8033_reg_init;

  mac->phy = ops;
  ops->port_map       =  0xa0;
  return 0;
}
