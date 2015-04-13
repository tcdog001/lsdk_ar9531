#
# Copyright (c) 2013 Qualcomm Atheros, Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

# Makefile for Ar7240 gigabit MAC and Phys
#



include ${ENETDIR}/Makefile.inc 

ifeq ($(GMAC_QCA955x),1)
obj-m						:= athrs_gmac.o 
athrs_gmac-objs					:= qca_soc_mac.o $(ACCEL-OBJS) $(PHY-OBJS) $(MAC-OBJS) athrs_mac_timer.o athrs_flowmac.o
else 
ifeq ($(GMAC_QCA953x),1)
obj-m						:= athrs_gmac.o 
ifeq ($(HYBRID_VLAN_IGMP), 1)
PHY-OBJS	+= athrs_qos.o
PHY-OBJS	+= vlan_igmp.o
endif

athrs_gmac-objs					:= qca_soc_mac.o $(ACCEL-OBJS) $(PHY-OBJS) $(MAC-OBJS) athrs_mac_timer.o athrs_flowmac.o


else
ifeq ($(ATH_GMAC_AR934x),1)

obj-m					:= athrs_gmac.o 
ifeq ($(HYBRID_VLAN_IGMP), 1)
ifneq ($(findstring _s17,$(ETH_CONFIG2)_$(ETH_CONFIG)),)
PHY-OBJS	+= vlan_igmp_s17.o
else    
PHY-OBJS	+= athrs_qos.o
PHY-OBJS	+= vlan_igmp.o
endif
endif
athrs_gmac-objs				:= athrs_mac.o $(ACCEL-OBJS) $(PHY-OBJS) $(MAC-OBJS) athrs_mac_timer.o athrs_flowmac.o

ifeq ($(HYBRID_PLC_FILTER),1)
athrs_gmac-objs += athrs_plc_filter.o
endif

else

obj-$(CONFIG_AG7240)			+= athrs_gmac.o
obj-phy-$(CONFIG_MACH_AR7240)		+= ag7240.o
obj-phy-$(CONFIG_AR7240_S26_PHY)	+= phys/ar7240_s26_phy.o
obj-phy-$(CONFIG_ATHRF1_PHY)            += phys/athrf1_phy.o
obj-phy-$(CONFIG_ATHRS27_PHY)           += phys/athrs27_phy.o
obj-phy-$(CONFIG_ATHRS16_PHY)		+= phys/athrs16_phy.o
obj-phy-$(CONFIG_ATHRS16_PHY_CONNECT_GE0)		+= phys/athrs16_phy.o
obj-phy-$(CONFIG_AR7242_S16_PHY)	+= phys/athrs16_phy.o
obj-phy-$(CONFIG_AG7240_QOS)		+= athrs_qos.o vlan_igmp.o
obj-phy-$(CONFIG_AR7240_S26_VLAN_IGMP)	+= vlan_igmp.o
obj-phy-$(CONFIG_AR7240_S27_VLAN_IGMP)	+= vlan_igmp.o
obj-hw-nat-$(CONFIG_ATHRS_HW_NAT)	+= athrs_nf_nat.o athrs_hw_nat.o
obj-hw-acl-$(CONFIG_ATHRS_HW_ACL)	+= athrs_nf_acl.o athrs_hw_acl.o

obj-phy-$(CONFIG_MACH_AR934x) 	        += ag934x.o
obj-phy-$(CONFIG_MACH_HORNET)   	+= ag7240.o
obj-phy-$(CONFIG_AR8021_PHY)        	+= phys/ar8021_phy.o 
obj-phy-$(CONFIG_ATHRS_VIR_PHY)        	+= phys/athrs_vir_phy.o 

ifdef ATH_GMAC_TXQUEUELEN
EXTRA_CFLAGS = -DATH_GMAC_TXQUEUELEN=$$ATH_GMAC_TXQUEUELEN
else
# refer to ether_setup
EXTRA_CFLAGS += -DATH_GMAC_TXQUEUELEN=1000
endif

athrs_gmac-objs				:= $(obj-phy-y) athrs_mac.o athrs_mac_timer.o  athrs_phy_ctrl.o athrs_gmac_ctrl.o athrs_flowmac.o $(obj-hw-nat-y) $(obj-hw-acl-y)

ifeq ($(HYBRID_PLC_FILTER),1)
athrs_gmac-objs += athrs_plc_filter.o
endif
endif
endif
endif 


ifeq ($(GMAC_QCA956x),1)
obj-m					:= athrs_gmac.o
athrs_gmac-objs				:= qca_soc_mac.o $(ACCEL-OBJS) $(PHY-OBJS) $(MAC-OBJS) athrs_mac_timer.o athrs_flowmac.o
#PHY-OBJS	+= athrs_qos.o
#PHY-OBJS	+= vlan_igmp.o
endif

EXTRA_CFLAGS += -I$(ENETDIR)/include -I$(ENETDIR)/include/phys -I$(KERNELPATH)/arch/mips/include

ifdef FLOWMACDIR
EXTRA_CFLAGS+= -I ${FLOWMACDIR}
endif

ifndef NO_PUSH_BUTTON
export NO_PUSH_BUTTON=1
endif

ifeq ($(strip ${NO_PUSH_BUTTON}), 1)
EXTRA_CFLAGS+= -DNO_PUSH_BUTTON=1
else
EXTRA_CFLAGS+= -DNO_PUSH_BUTTON=0
endif


ifeq ($(strip ${AP136_BOOTROM_TGT}), 1)
EXTRA_CFLAGS+= -DAP136_BOOTROM_TGT
else
EXTRA_CFLAGS+= -UAP136_BOOTROM_TGT
endif

ifeq ($(strip ${AP136_BOOTROM_HOST}), 1)
EXTRA_CFLAGS+= -DAP136_BOOTROM_HOST
else
EXTRA_CFLAGS+= -UAP136_BOOTROM_HOST
endif

ifeq ($(strip ${HYBRID_VLAN_COMMUNICATE}), 1)
EXTRA_CFLAGS+= -DHYBRID_VLAN_COMMUNICATE=1
else
EXTRA_CFLAGS+= -DHYBRID_VLAN_COMMUNICATE=0
endif

ifeq ($(strip ${HYBRID_PATH_SWITCH}), 1)
EXTRA_CFLAGS+= -DATH_HY_PATH_SWITCH
endif

ifeq ($(strip ${HYBRID_PLC_FILTER}), 1)
EXTRA_CFLAGS+= -DHYBRID_PLC_FILTER
endif

ifeq ($(strip ${HYBRID_LINK_CHANGE_EVENT}), 1)
EXTRA_CFLAGS+= -DHYBRID_LINK_CHANGE_EVENT=1
else
EXTRA_CFLAGS+= -DHYBRID_LINK_CHANGE_EVENT=0
endif

ifeq ($(strip ${HYBRID_SWITCH_PORT6_USED}), 1)
EXTRA_CFLAGS+= -DHYBRID_SWITCH_PORT6_USED=1
else
EXTRA_CFLAGS+= -DHYBRID_SWITCH_PORT6_USED=0
endif

ifeq ($(strip ${HYBRID_APH126_128_S17_WAR}), 1)
EXTRA_CFLAGS+= -DHYBRID_APH126_128_S17_WAR=1
else
EXTRA_CFLAGS+= -DHYBRID_APH126_128_S17_WAR=0
endif

ifneq ($(strip ${ATHR_PORT1_LED_GPIO}), )
EXTRA_CFLAGS+= -DATHR_PORT1_LED_GPIO=${ATHR_PORT1_LED_GPIO}
endif

ifdef STAGING_DIR
ifneq ($(strip $(ATH_HEADER)),)
EXTRA_CFLAGS+= -DCONFIG_ATHEROS_HEADER_EN=1
endif
ifneq ($(strip $(CONFIG_AR7240_S26_VLAN_IGMP)),)
EXTRA_CFLAGS+= -DCONFIG_AR7240_S26_VLAN_IGMP=1
ifneq ($(strip $(CONFIG_QCAGMAC_ATH_SNOOPING_V6)),)
EXTRA_CFLAGS+= -DCONFIG_IPV6=1
endif
endif
ifneq ($(strip $(CONFIG_AR7240_S27_VLAN_IGMP)),)
EXTRA_CFLAGS+= -DCONFIG_AR7240_S27_VLAN_IGMP=1
ifneq ($(strip $(CONFIG_QCAGMAC_ATH_SNOOPING_V6)),)
EXTRA_CFLAGS+= -DCONFIG_IPV6=1
endif
endif
ifneq ($(strip $(CONFIG_AR7240_S17_VLAN_IGMP)),)
EXTRA_CFLAGS+= -DCONFIG_AR7240_S17_VLAN_IGMP=1
ifneq ($(strip $(CONFIG_QCAGMAC_ATH_SNOOPING_V6)),)
EXTRA_CFLAGS+= -DCONFIG_IPV6=1
endif
endif
endif

clean:
	rm -f *.o *.ko 
	rm -f phys/*.o *.ko
ifneq ($(ACCEL-OBJS),)
	rm -f hwaccels/*.o
endif


