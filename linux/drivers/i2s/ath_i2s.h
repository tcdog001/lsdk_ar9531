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

#include <linux/delay.h>

#define MASTER                      		1
#define SPDIF
#undef  SPDIFIOCTL

#define MHZ                         1000000
#define SPDIF_CONFIG_CHANNEL(x)     ((3&x)<<20)
#define SPDIF_MODE_LEFT             1
#define SPDIF_MODE_RIGHT            2
#define SPDIF_CONFIG_SAMP_FREQ(x)   ((0xf&x)<<24)
#define SPDIF_SAMP_FREQ_48          2
#define SPDIF_SAMP_FREQ_44          0
#define SPDIF_CONFIG_ORG_FREQ(x)    ((0xf&x)<<4)
#define SPDIF_ORG_FREQ_48           0xd
#define SPDIF_ORG_FREQ_44           0xf
#define SPDIF_CONFIG_SAMP_SIZE(x)   (0xf&x)
#define SPDIF_S_8_16                2
#define SPDIF_S_24_32               0xb
#define MAX_MBOX_VUC                6   /* Refer MBOX Descriptor structure in data sheet */


#define PAUSE				        0x1
#define START				        0x2
#define RESUME				        0x4

#define STATUSMASK1			        (1ul << 31)
#define STATUSMASK			        (1ul << 4)
#define SHIFT_WIDTH			        11
#define ADDR_VALID			        1

#define I2S_RESET                   		1
#define MBOX_RESET  		                (1ul << 1)
#define MBOX_INTR_MASK         			(1ul << 7)

#define MONO           		         	(1ul << 14)
/* For SRAM acess */
#define USE_SRAM 
/* Uncomment this macro for SRAM access. Valid only for 
 * Scorpion. Do not use SRAM for WASP */
#undef  USE_SRAM

#ifndef USE_SRAM
#define ATH_I2S_NUM_DESC       			128
#define ATH_I2S_BUFF_SIZE           		768
#else
/* FOR SRAM */
#define ATH_I2S_NUM_DESC                        4
#define ATH_I2S_BUFF_SIZE                       128
#endif

/* AUDIO PLL Default values. 
 * Sampling frequency = 48000 Hz, Number of bits = 16 
 */
#define AUDIO_PLL_CONFIG_DEF_48_16              0x4202
#define AUDIO_PLL_MOD_DEF_48_16                 0xa4a904e
#define STEREO_CONFIG_DEF_POSEDGE               0x2

/* Stereo Volume Defines */
#define STEREO_VOLUME_MIN                       -16
#define STEREO_VOLUME_MAX                       7

#define ATH_AUD_DPLL3_KD_25			0x3d
#define ATH_AUD_DPLL3_KD_40			0x32

#define ATH_AUD_DPLL3_KI_25			0x4
#define ATH_AUD_DPLL3_KI_40			0x4

#define I2S_LOCK_INIT(_sc)			spin_lock_init(&(_sc)->i2s_lock)
#define I2S_LOCK_DESTROY(_sc)
#define I2S_LOCK(_sc)				spin_lock_irqsave(&(_sc)->i2s_lock, (_sc)->i2s_lockflags)
#define I2S_UNLOCK(_sc)				spin_unlock_irqrestore(&(_sc)->i2s_lock, (_sc)->i2s_lockflags)

#define I2S_VOLUME          _IOW('N', 0x20, int)
#define I2S_FREQ            _IOW('N', 0x21, int)
#define I2S_DSIZE           _IOW('N', 0x22, int)
#define I2S_MODE            _IOW('N', 0x23, int)
#define I2S_FINE            _IOW('N', 0x24, int)
#define I2S_COUNT           _IOWR('N', 0x25, int)
#define I2S_PAUSE           _IOWR('N', 0x26, int)
#define I2S_RESUME          _IOWR('N', 0x27, int)
#define I2S_MCLK            _IOW('N', 0x28, int)
#define I2S_BITSWAP         _IOW('N', 0x29, int)
#define I2S_BYTESWAP        _IOW('N', 0x30, int)
#define I2S_SPDIF_VUC       _IOWR('N', 0x31, int)
#define I2S_MICIN           _IOW('N', 0x32, int)
#define I2S_SPDIF_DISABLE   _IOW('N', 0x33, int)

/* MBOX Descriptor */
typedef struct {
	unsigned int OWN		:  1,    /* bit 00 */
		     EOM		:  1,    /* bit 01 */
		     rsvd1	    :  6,    /* bit 07-02 */
		     size	    : 12,    /* bit 19-08 */
		     length	    : 12,    /* bit 31-20 */
		     rsvd2	    :  4,    /* bit 00 */
		     BufPtr	    : 28,    /* bit 00 */
		     rsvd3	    :  4,    /* bit 00 */
		     NextPtr	: 28;    /* bit 00 */
#ifdef SPDIF
	unsigned int Va[MAX_MBOX_VUC];
	unsigned int Ua[MAX_MBOX_VUC];
	unsigned int Ca[MAX_MBOX_VUC];
	unsigned int Vb[MAX_MBOX_VUC];
	unsigned int Ub[MAX_MBOX_VUC];
	unsigned int Cb[MAX_MBOX_VUC];
#endif
} ath_mbox_dma_desc;

/*
 * XXX : This is the interface between i2s and wlan
 *       When adding info, here please make sure that
 *       it is reflected in the wlan side also
 */
typedef struct i2s_stats {
	unsigned int write_fail;
	unsigned int rx_underflow;
} i2s_stats_t;

/* Structure to store the vitural and physical
 * address of the data buffer */ 
typedef struct i2s_buf {
	uint8_t *bf_vaddr;
	unsigned long bf_paddr;
} i2s_buf_t;

/* DMA buffer structure containing pointers to
   MBOX decriptors and data buffers */
typedef struct i2s_dma_buf {
	ath_mbox_dma_desc *lastbuf;
	ath_mbox_dma_desc *db_desc;
	dma_addr_t db_desc_p;
	i2s_buf_t db_buf[ATH_I2S_NUM_DESC];
	int tail;
} i2s_dma_buf_t;

/* I2S structure to hold variables and locks */
typedef struct ath_i2s_softc {
	int			ropened,
				popened,
				sc_irq,
				ft_value,
				ppause,
				rpause;
	char			*sc_pmall_buf,
				*sc_rmall_buf;
	i2s_dma_buf_t		sc_pbuf,
				sc_rbuf;
	wait_queue_head_t	wq_rx,
				wq_tx;
	spinlock_t		i2s_lock;
	unsigned long           i2s_lockflags;
	struct timer_list	pll_lost_lock;
	unsigned int		pll_timer_inited,
				dsize,
				vol,
	                        freq,
	                        psedge;
	unsigned long           audio_pll,
		                target_pll;
} ath_i2s_softc_t;

/* PLL settings. The structure is passed from the user application
 * and the corresponding PLL registers are written */
typedef struct i2s_pll_t {
        unsigned long audio_pll;      /* Audio PLL Config Reg Value */
        unsigned long target_pll;     /* Audio PLL Modulation Reg Value */
        unsigned int  psedge;              /* Stereo Config Posedge Value */
}pll_t;               

/* SPDIF VUC Values. Passed from the application to write/read SPDIF
 * VUC values of each descriptor */
typedef struct spdif_vuc_t {
        unsigned int write;                      /* Write/Read VUC */
        unsigned int va[MAX_MBOX_VUC];           /* VUC values in accordance with SPDIF format */
        unsigned int ua[MAX_MBOX_VUC];
        unsigned int ca[MAX_MBOX_VUC];
        unsigned int vb[MAX_MBOX_VUC];
        unsigned int ub[MAX_MBOX_VUC];
        unsigned int cb[MAX_MBOX_VUC];
}vuc_t;

ath_i2s_softc_t sc_buf_var;
i2s_stats_t stats;

/* I2S Fucntion declarations */

/* Description :  Initialize GPIOs and set STEREO CONFIG 
                  defaults. 
   Parameters  :  Master/Slave mode 
                  0 - MASTER, 1 - SLAVE
   Returns     :  void */
void ath_i2s_init_reg(int ath_i2s_slave);

/* Description :  Initialize MBOX DMA POLICY. Set SRAM access
                  if required.
   Parameters  :  None
   Returns     :  void */
void ath_i2s_request_dma_channel(void);

/* Description :  Set MBOX Tx/Rx Descriptor base addres
   Parameters  :  Descriptor base Address , mode(Tx/Rx)
   Returns     :  void */
void ath_i2s_dma_desc(unsigned long, int);

/* Description :  Start MBOX Tx/Rx DMA.
   Parameters  :  mode(Tx/Rx)
   Returns     :  void */
void ath_i2s_dma_start(int);

/* Description :  Pause MBOX Tx/Rx DMA.
   Parameters  :  mode(Tx/Rx)
   Returns     :  void */
void ath_i2s_dma_pause(int);

/* Description :  Resume MBOX Tx/Rx DMA.
   Parameters  :  mode(Tx/Rx)
   Returns     :  void */
void ath_i2s_dma_resume(int);

/* Description :  Set Audio PLL config and AUDIO PLL modulation
   Parameters  :  PLL modulation, PLL config
   Returns     :  void */
void ath_i2s_clk(unsigned long, unsigned long);

/* Description :  Set DPLL KD and KI Values. Not used
                  in Scorpion. 
   Parameters  :  kd, ki
   Returns     :  void */
void ath_i2s_dpll(unsigned int, unsigned int );

/* Description :  Exported open function to be used
                  by other modules like USB gadget
   Parameters  :  None
   Returns     :  void */
int  ath_ex_i2s_open(void);

/* Description :  Exported close function to be used
                  by other modules 
   Parameters  :  None
   Returns     :  void */
void ath_ex_i2s_close(void);

/* Description :  Exported set frequency function to be used
                  by other modules like USB gadget for 
                  configuring AUDIO PLL.
   Parameters  :  None
   Returns     :  void */
void ath_ex_i2s_set_freq(unsigned int);

/* Description :  Exported write function to be used
                  by other modules like USB gadget to
 		  write data to the I2S/play.
   Parameters  :  Count  - Number of bytes to write
                  buf    - Pointer to data to be writtten
   Returns     :  void */
void ath_ex_i2s_write(size_t , const char *, int );

/* Description :  ISR to handle and clear MBOX interrupts
   Parameters  :  irq      - IRQ Number
                  dev_id   - Pointer to device data structure  
                  regs     - Snapshot of processors context
   Returns     :  IRQ_HANDLED */
irqreturn_t ath_i2s_intr(int irq, void *dev_id, struct pt_regs *regs);

/* Description :  I2S Open function called from applications open call and 
                  from ath_ex_i2s_open
   Parameters  :  inode    - Inode structure
                  filp     - File pointer structuressors context
   Returns     :  0/EBUSY/ENOMEM */

int ath_i2s_open(struct inode *inode, struct file *filp);

