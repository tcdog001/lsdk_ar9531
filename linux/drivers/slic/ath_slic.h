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

#include <linux/dma-mapping.h>

/* Default Module param values */
#define DEF_MAX_SLOTS_8         8
#define DEF_SLOTS_EN_8          8
#define DEF_FS_LONG_1           1
#define DEF_TX_SWEEP_POS_1      1
#define DEF_RX_SWEEP_POS_0      0
#define DEF_MCLK_SEL_1          1

/* Maximum bits in a register */
#define MAX_REG_BITS            32
/* Enable all slots */
#define SLIC_SLOTS_EN_ALL       0xffffffff

/* Mode */
#define SLIC_RX                 1     /* MBOX Tx */   
#define SLIC_TX                 2     /* MBOX Rx */
/* For SRAM access. Use only for Scorpion. 
 * Strictly do not use for WASP or
 * older platforms */
#define USE_SRAM
/* Comment it out for SRAM access */
#undef USE_SRAM

#ifndef USE_SRAM
#define NUM_DESC              384
#define SLIC_BUF_SIZE         2048
#else
/* For SRAM */
#define NUM_DESC                4
#define SLIC_BUF_SIZE           128
#endif

#define SLIC_MIMR_TIMER 	_IOW('N', 0x20, int)
#define SLIC_DDR        	_IOW('N', 0x22, int)
#define SLIC_BITSWAP            _IOW('N', 0x23, int)
#define SLIC_BYTESWAP           _IOW('N', 0x24, int)


#define SLIC_LOCK_INIT(_sc)	spin_lock_init(&(_sc)->slic_lock)
#define SLIC_LOCK_DESTROY(_sc)
#define SLIC_LOCK(_sc)		spin_lock_irqsave(&(_sc)->slic_lock, (_sc)->slic_lockflags)
#define SLIC_UNLOCK(_sc)	spin_unlock_irqrestore(&(_sc)->slic_lock, (_sc)->slic_lockflags)

typedef struct {
	unsigned int OWN        :  1,    /* bit 00 */
		     EOM        :  1,    /* bit 01 */
		     rsvd1      :  6,    /* bit 07-02 */
		     size       : 12,    /* bit 19-08 */
		     length     : 12,    /* bit 31-20 */
		     rsvd2      :  4,    /* bit 00 */
		     BufPtr     : 28,    /* bit 00 */
		     rsvd3      :  4,    /* bit 00 */
		     NextPtr    : 28,    /* bit 00 */
		     Caligned   : 32;
} ath_mbox_dma_desc;

typedef struct slic_buf {
	uint8_t *bf_vaddr;
	unsigned long bf_paddr;
} slic_buf_t;

typedef struct slic_dma_buf {
	ath_mbox_dma_desc *lastbuf;
	ath_mbox_dma_desc *db_desc;
	dma_addr_t db_desc_p;
	slic_buf_t db_buf[NUM_DESC];
	int tail;
} slic_dma_buf_t;

typedef struct ath_slic_softc {
	int ropened;
	int popened;
	slic_dma_buf_t sc_pbuf;
	slic_dma_buf_t sc_rbuf;
	char *sc_pmall_buf;
	char *sc_rmall_buf;
	int sc_irq;
	int ft_value;
	int ppause;
	int rpause;
	spinlock_t slic_lock;   /* lock */
	unsigned long slic_lockflags;
} ath_slic_softc_t;

ath_slic_softc_t sc_buf_var;

/* Function declarations */

/* Description : GPIO initializations and SLIC register
		 configurations 
 * Parameters  : None
 * Returns     : void */
void ath_slic_link_on(void);

/* Descritpion : To configure MBOX DMA Policy 
 * Paramaters  : None
 * Returns     : void */
void ath_slic_request_dma_channel(void);

/* Description : Program Tx and Rx descriptor base
		 addresses and start DMA 
 * Parameters  : desc_buf_p - Pointer to MBOX descriptor
                 mode       - Tx/Rx
 * Returns     : void */
void ath_slic_dma_start(unsigned long, int);

/* Description : Pause MBOX Tx/Rx DMA.
 * Parameters  : Mode - Tx/Rx
 * Returns     : void */
void ath_slic_dma_pause(int);

/* Description : Resume MBOX Tx/Rx DMA
 * Parameters  : Mode - Tx/Rx
 * Returns     : void */
void ath_slic_dma_resume(int);

/* Description : Unused function. Maintained for sanity
 * Parameters  : frac - frac value of PLL Modulation
                 pll - Audio PLL config 
 * Returns    : void */
void ar7242_slic_clk(unsigned long, unsigned long);

/* Description :  ISR to handle and clear MBOX interrupts
   Parameters  :  irq      - IRQ Number
                  dev_id   - Pointer to device data structure
                  regs     - Snapshot of processors context
   Returns     :  IRQ_HANDLED */
irqreturn_t ath_slic_intr(int irq, void *dev_id);

