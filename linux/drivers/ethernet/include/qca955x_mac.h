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

#ifndef _QCA_955X_H
#define _QCA_955X_H

#include "atheros.h"

#if defined(ATH_GE1_IS_CONNECTED)
    #define ATHR_GMAC_NMACS            2
#else
    #define ATHR_GMAC_NMACS            1
    #define ATHR_GMAC_MII1_INTERFACE   0xff        /*not connected*/
#endif  /*ATH_GE1_IS_CONNECTED*/


/* Fifo Values */
#define AMCXFIF_CFG_1_VAL		0x10ffff
#define AMCXFIF_CFG_2_VAL	        0x027001aa
#define AMCXFIF_CFG_4_VAL		0x3ffff
#define AMCXFIF_CFG_3_VAL		0x1f00140
#define TXFIFO_TH_VAL			0x01d80160
#define TXFIFO_TH_HD_VAL		0x00880060
#define AMCXFIF_CFG_3_HD_VAL		0x00f00040
#define AMCXFIF_CFG_0_VAL		0x1f00
#define ATH_MDIO_PHY_ADDR 		0x7
#define ATH_MDC_GPIO_PIN  		11
#define ATH_MDIO_GPIO_PIN 		18

#define ATH_GPIO_OUT_FUNC_ADDR          GPIO_OUT_FUNCTION4_ADDRESS
#define ATH_GPIO_OUT_FUNC_EN_MASK       GPIO_OUT_FUNCTION4_ENABLE_GPIO_18_MASK
#define ATH_GPIO_OUT_FUNC_EN_SET        GPIO_OUT_FUNCTION4_ENABLE_GPIO_18_SET
#define ATH_MII_EXT_MDI                 1
                

#define MII_UNKNOWN			-1
#define MII_PHY_UNKNOWN			-1

#define CONFIG_ATHR_GMAC_LOCATION            CONFIG_ATH_MAC_LOCATION

#define CONFIG_ATHR_GMAC_LEN_PER_TX_DS       CONFIG_ATH_LEN_PER_TX_DS

#define CONFIG_ATHR_GMAC_NUMBER_TX_PKTS      CONFIG_ATH_NUMBER_TX_PKTS
#define CONFIG_ATHR_GMAC_NUMBER_RX_PKTS      CONFIG_ATH_NUMBER_RX_PKTS
#define CFG_ATHR_GMAC0_MII                   ATHR_GMAC0_MII
#define CFG_ATHR_GMAC0_MII_PHY               ATHR_GMAC0_MII_PHY

#if ATHR_GMAC_NMACS > 1
#define CONFIG_ATHR_GMAC_NUMBER_TX_PKTS_1    CONFIG_ATH_NUMBER_TX_PKTS_1
#define CONFIG_ATHR_GMAC_NUMBER_RX_PKTS_1    CONFIG_ATH_NUMBER_RX_PKTS_1
#define CFG_ATHR_GMAC1_MII                   ATHR_GMAC1_MII 
#define CFG_ATHR_GMAC1_MII_PHY               ATHR_GMAC1_MII_PHY  
#else
#define CONFIG_ATHR_GMAC_NUMBER_TX_PKTS_1
#define CONFIG_ATHR_GMAC_NUMBER_RX_PKTS_1
#define CFG_ATHR_GMAC1_MII                   MII_UNKNOWN   
#define CFG_ATHR_GMAC1_MII_PHY               MII_PHY_UNKNOWN
#endif

#define ATHR_EEPROM_GE0_MAC_ADDR             ATH_EEPROM_GE0_MAC_ADDR
#define ATHR_EEPROM_GE1_MAC_ADDR             ATH_EEPROM_GE1_MAC_ADDR

#define ATHR_IRQ_ENET_LINK                   ATH_MISC_IRQ_ENET_LINK
#define ATHR_GE0_BASE                        ATH_GE0_BASE
#define ATHR_GE1_BASE                        ATH_GE1_BASE
#define ATHR_CPU_IRQ_GE0                     ATH_CPU_IRQ_GE0
#define ATHR_CPU_IRQ_GE1                     ATH_CPU_IRQ_GE1
#define ATHR_SRAM_START 		     0xbd000000

#define ATHR_IRQ_GE0_GLOBAL_TIMER           ATH_MISC_IRQ_TIMER2
#define ATHR_IRQ_GE1_GLOBAL_TIMER           ATH_MISC_IRQ_TIMER3

#define ATHR_GE0_GLOBAL_TIMER_INT_LOAD     RST_GENERAL_TIMER2_RELOAD_ADDRESS
#define ATHR_GE1_GLOBAL_TIMER_INT_LOAD     RST_GENERAL_TIMER3_RELOAD_ADDRESS
/* GPIO defines */

#define ATHR_GPIO_FUNCTIONS                  ATH_GPIO_FUNCTIONS
#define ATHR_GPIO_OUT_FUNCTION1              ATH_GPIO_OUT_FUNCTION1
#define ATHR_GPIO_OUT_FUNCTION4              ATH_GPIO_OUT_FUNCTION4
#define ATHR_GPIO_OUT_FUNCTION5              ATH_GPIO_OUT_FUNCTION5
#define ATHR_GPIO_OE			     ATH_GPIO_OE
#define ATHR_GPIO_OUT			     ATH_GPIO_OUT
#define ATHR_GPIO_IRQn(n)                    ATH_GPIO_IRQn(n)
#define ATHR_GPIO_INT_ENABLE                 ATH_GPIO_INT_ENABLE
#define ATHR_GPIO_INT_TYPE                   ATH_GPIO_INT_TYPE

#define athr_reg_rmw_set                     ath_reg_rmw_set
#define athr_reg_rmw_clear                   ath_reg_rmw_clear
#define athr_reg_wr_nf                       ath_reg_wr_nf
#define athr_reg_wr                          ath_reg_wr
#define athr_reg_rd                          ath_reg_rd

#define  athrs_ahb_freq                      ath_ahb_freq
#define  athrs_flush_ge                      ath_flush_ge

#if defined(AP136_BOOTROM_HOST)
#define set_gmac_delay()  do{  ath_reg_wr(ETH_XMII_ADDRESS, \
                                 ETH_XMII_TX_INVERT_SET(1) \
                               | ETH_XMII_RX_DELAY_SET(2)  \
                               | ETH_XMII_TX_DELAY_SET(1)  \
                               | ETH_XMII_GIGE_SET(1) ); }while(0)
#else
#define set_gmac_delay()  do{ }while(0)
#endif

extern uint32_t ath_ahb_freq;
int qca955x_gmac_attach(void *arg);
void serdes_pll_lock_detect_st(void);
#endif //_ATHR_934x_H
