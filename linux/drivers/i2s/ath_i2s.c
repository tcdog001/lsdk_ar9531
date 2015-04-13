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

//#define here()        do { printk("%s:%d\n", __func__, __LINE__); } while(0)
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <asm/system.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */
#include <asm/io.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/interrupt.h>

#include "atheros.h"
#include "ath_i2s.h"

#undef 	AOW 
#undef 	USE_MEMCPY
#define MAX_I2S_WRITE_RETRIES 2
#define MAX_I2S_READ_RETRIES  2
#undef  GMRWAR

/* Module parameter initial values */
int ath_i2s_major = 253;
int ath_i2s_minor = 0;
int ath_i2s_slave = 0;
int ath_i2s_mclk_sel = 0; /* Internal MCLK */

static int i2s_start;

module_param(ath_i2s_major,     int, S_IRUGO);
module_param(ath_i2s_minor,     int, S_IRUGO);
module_param(ath_i2s_slave,     int, S_IRUGO);
module_param(ath_i2s_mclk_sel,  int, S_IRUGO);

MODULE_AUTHOR("Mughilan@Atheros");
MODULE_LICENSE("Dual BSD/GPL");

/* Follow the sequence while writing to stereo config . Set
   stereo_config bits in the stereo config register */
static inline void ath_i2s_write_stereo(unsigned int stereo_config)
{
	ath_reg_rmw_set(ATH_STEREO_CONFIG, (stereo_config |
                                            ATH_STEREO_CONFIG_RESET));

	udelay(100);
	ath_reg_rmw_clear(ATH_STEREO_CONFIG, ATH_STEREO_CONFIG_RESET);
}

/* Clear the stereo_config bits in the stereo config register */
static inline void ath_i2s_clear_stereo(unsigned int stereo_config)
{
	unsigned int rd_data;
	rd_data = ath_reg_rd(ATH_STEREO_CONFIG);
	rd_data = (rd_data & ~stereo_config);

	ath_reg_wr(ATH_STEREO_CONFIG, (rd_data |
                                       ATH_STEREO_CONFIG_RESET));

	udelay(100);
	ath_reg_rmw_clear(ATH_STEREO_CONFIG, ATH_STEREO_CONFIG_RESET);
}

void i2s_get_stats(i2s_stats_t *p)
{
	memcpy(p, &stats, sizeof(struct i2s_stats));

}
EXPORT_SYMBOL(i2s_get_stats);    

void i2s_clear_stats(void)
{
	stats.write_fail = 0; 
	stats.rx_underflow = 0;
}
EXPORT_SYMBOL(i2s_clear_stats);


int ath_i2s_desc_busy(struct file *filp)
{
	int mode;
	int own;
	int ret = 0;
	int j = 0;

	ath_i2s_softc_t *sc = &sc_buf_var;
	ath_mbox_dma_desc *desc;
	i2s_dma_buf_t *dmabuf;

	if (!filp)
		mode = 0;
	else
		mode = filp->f_mode;

	if (mode & FMODE_READ) {
		dmabuf = &sc->sc_rbuf;
		own = sc->rpause;
	} else {
		dmabuf = &sc->sc_pbuf;
		own = sc->ppause;
	}        

	desc = dmabuf->db_desc;

	for (j = 0; j < ATH_I2S_NUM_DESC; j++) {
		if (desc[j].OWN) {
			desc[j].OWN = 0;
		}
		ath_i2s_dma_resume(0);
	}        

	return ret;
}
EXPORT_SYMBOL(ath_i2s_desc_busy);

/* Allocate MBOX buffer descriptors and buffers and initialize then */
int ath_i2s_init_desc(struct file *filp)
{
	ath_i2s_softc_t *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf;
	i2s_buf_t *scbuf;
	uint8_t *bufp = NULL;
	int j, k, byte_cnt, tail = 0, mode = 1;
	ath_mbox_dma_desc *desc;
	unsigned long desc_p;

	if (!filp) {
		mode = FMODE_WRITE;
	} else {
		mode = filp->f_mode;
	}

	if (mode & FMODE_READ) {
#ifndef AOW
		init_waitqueue_head(&sc->wq_tx);
#endif
		dmabuf = &sc->sc_rbuf;
		sc->ropened = 1;
		sc->rpause = 0;
	} else {
#ifndef AOW
		init_waitqueue_head(&sc->wq_rx);
#endif
		dmabuf = &sc->sc_pbuf;
		sc->popened = 1;
		sc->ppause = 0;
	}

#ifdef USE_SRAM
	dmabuf->db_desc = (ath_mbox_dma_desc *)(KSEG1ADDR(ATH_SRAM_BASE));
	dmabuf->db_desc_p = dmabuf->db_desc;
#else
	dmabuf->db_desc = (ath_mbox_dma_desc *)
	dma_alloc_coherent(NULL, ATH_I2S_NUM_DESC *
				sizeof(ath_mbox_dma_desc),
				&dmabuf->db_desc_p, GFP_DMA);

	if (dmabuf->db_desc == NULL) {
		printk(KERN_CRIT "DMA desc alloc failed for %d\n", mode);
		return ENOMEM;
	}
#endif

	for (j = 0; j < ATH_I2S_NUM_DESC; j++) {
		dmabuf->db_desc[j].OWN = 0;
#ifdef SPDIF
		for (k = 0; k < 6; k++) {
			dmabuf->db_desc[j].Va[k] = 0;
			dmabuf->db_desc[j].Ua[k] = 0;
			dmabuf->db_desc[j].Ca[k] = 0;
			dmabuf->db_desc[j].Vb[k] = 0;
			dmabuf->db_desc[j].Ub[k] = 0;
			dmabuf->db_desc[j].Cb[k] = 0;
		}
		
		/* For Dynamic Conf */
		dmabuf->db_desc[j].Ca[0] |= SPDIF_CONFIG_CHANNEL(SPDIF_MODE_LEFT);
		dmabuf->db_desc[j].Cb[0] |= SPDIF_CONFIG_CHANNEL(SPDIF_MODE_RIGHT);
#ifdef SPDIFIOCTL
		dmabuf->db_desc[j].Ca[0] = 0x00100000;
		dmabuf->db_desc[j].Ca[1] = 0x02100000;
		dmabuf->db_desc[j].Cb[0] = 0x00200000;
		dmabuf->db_desc[j].Cb[1] = 0x02100000;
#endif
#endif
	}

	/* Allocate data buffers */
	scbuf = dmabuf->db_buf;
#ifdef USE_SRAM
	bufp = (uint8_t *)((KSEG1ADDR(ATH_SRAM_BASE)) +
				(ATH_I2S_NUM_DESC * sizeof(ath_mbox_dma_desc)));
#else
	if (!(bufp = kmalloc(ATH_I2S_NUM_DESC * ATH_I2S_BUFF_SIZE, GFP_KERNEL))) {
		printk(KERN_CRIT "Buffer allocation failed for \n");
		goto fail3;
	}
#endif
	if (mode & FMODE_READ) {
		sc->sc_rmall_buf = bufp;
	} else {
		sc->sc_pmall_buf = bufp;
	}

	for (j = 0; j < ATH_I2S_NUM_DESC; j++) {
		scbuf[j].bf_vaddr = &bufp[j * ATH_I2S_BUFF_SIZE];
#ifdef USE_SRAM
		scbuf[j].bf_paddr = scbuf[j].bf_vaddr;
#else
		scbuf[j].bf_paddr = dma_map_single(NULL, scbuf[j].bf_vaddr,
				                   ATH_I2S_BUFF_SIZE,
				                   DMA_BIDIRECTIONAL);
#endif
	}
	dmabuf->tail = 0;

	// Initialize desc
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
	byte_cnt = ATH_I2S_NUM_DESC * ATH_I2S_BUFF_SIZE;
	tail = dmabuf->tail;

	while (byte_cnt && (tail < ATH_I2S_NUM_DESC)) {
		desc[tail].rsvd1 = 0;
		desc[tail].size = ATH_I2S_BUFF_SIZE;
		if (byte_cnt > ATH_I2S_BUFF_SIZE) {
			desc[tail].length = ATH_I2S_BUFF_SIZE;
			byte_cnt -= ATH_I2S_BUFF_SIZE;
			desc[tail].EOM = 0;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
			desc[tail].EOM = 0;
		}
		desc[tail].rsvd2 = 0;
		desc[tail].rsvd3 = 0;
		desc[tail].BufPtr = (unsigned int) scbuf[tail].bf_paddr;
		desc[tail].NextPtr = (desc_p +
				((tail +
				  1) *
				 (sizeof(ath_mbox_dma_desc))));
		if (mode & FMODE_READ) {
			desc[tail].OWN = 1;
		} else {
			desc[tail].OWN = 0;
		}
		tail++;
	}
	tail--;
	desc[tail].NextPtr = desc_p;

	dmabuf->tail = 0;

	return 0;

fail3:
	if (mode & FMODE_READ) {
		dmabuf = &sc->sc_rbuf;
	} else {
		dmabuf = &sc->sc_pbuf;
	}
#ifndef USE_SRAM
	dma_free_coherent(NULL,
			ATH_I2S_NUM_DESC * sizeof(ath_mbox_dma_desc),
			dmabuf->db_desc, dmabuf->db_desc_p);
	if (mode & FMODE_READ) {
		if (sc->sc_rmall_buf) {
			kfree(sc->sc_rmall_buf);
		}
	} else {
		if (sc->sc_pmall_buf) {
			kfree(sc->sc_pmall_buf);
		}
	}
#endif
	return -ENOMEM;
}

/* Exported functions to be used by other modules */
int ath_ex_i2s_open()
{
	ath_i2s_softc_t *sc = &sc_buf_var;

	i2s_start = 1;

	/* I2S is always configured as master for use by
         * other modules */
	ath_i2s_init_reg(ath_i2s_slave);

	if (sc->popened) {
		printk("%s, %d I2S speaker busy\n", __func__, __LINE__);
		return -EBUSY;
	}

	return ath_i2s_open(NULL, NULL);

	return (0);
}
EXPORT_SYMBOL(ath_ex_i2s_open);

int ath_i2s_open(struct inode *inode, struct file *filp)
{
	ath_i2s_softc_t *sc = &sc_buf_var;
	int opened = 0;

	if (filp && (filp->f_mode & FMODE_READ) && (sc->ropened)) {
		printk("%s, %d I2S mic busy\n", __func__, __LINE__);
		return -EBUSY;
	}
	if (filp && (filp->f_mode & FMODE_WRITE) && (sc->popened)) {
		printk("%s, %d I2S speaker busy\n", __func__, __LINE__);
		return -EBUSY;
	}

	opened = (sc->ropened | sc->popened);

	/* Reset MBOX FIFO's */
	if (!opened) {
		ath_reg_wr(ATH_MBOX_FIFO_RESET, 0xff); // virian
		udelay(500);
	}

	/* Allocate and initialize descriptors */
	if (ath_i2s_init_desc(filp) == ENOMEM) {
		return -ENOMEM;
	}

	if (!opened) {
		ath_i2s_request_dma_channel();
	}

	if (!sc->pll_timer_inited) {
		init_timer(&sc->pll_lost_lock);
		sc->pll_timer_inited = 1;
	}

	return (0);
}

/* I2S rd fucntion with the complete read logic. Added to 
 * avoid many switches between the application and the 
 * driver */
ssize_t ath_i2s_rd(struct file * filp, char __user * buf,
                     size_t count, loff_t * f_pos)
{
#define prev_tail(t) ({ (t == 0) ? (ATH_I2S_NUM_DESC - 1) : (t - 1); })
#define next_tail(t) ({ (t == (ATH_I2S_NUM_DESC - 1)) ? 0 : (t + 1); })

	uint8_t *data;
	ssize_t retval;
	struct ath_i2s_softc *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf = &sc->sc_rbuf;
	i2s_buf_t *scbuf;
	ath_mbox_dma_desc *desc;
	unsigned int byte_cnt, mode = 1, offset = 0, tail = dmabuf->tail;
	unsigned long desc_p;
	int need_start = 0;
	
	byte_cnt = count;

	if (sc->ropened < 2) {
		ath_reg_rmw_set(ATH_MBOX_INT_ENABLE, ATH_MBOX_TX_DMA_COMPLETE);
		need_start = 1;
	}

	sc->ropened = 2;

	scbuf = dmabuf->db_buf;
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
	data = scbuf[0].bf_vaddr;

	desc_p += tail * sizeof(ath_mbox_dma_desc);

#ifndef AOW
	if (!need_start) {
		wait_event_interruptible(sc->wq_tx, desc[tail].OWN != 1);
	}
#endif

	while (byte_cnt && !desc[tail].OWN) {
		if (byte_cnt >= ATH_I2S_BUFF_SIZE) {
			desc[tail].length = ATH_I2S_BUFF_SIZE;
			byte_cnt -= ATH_I2S_BUFF_SIZE;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
		}
	       
		dma_cache_sync(NULL, scbuf[tail].bf_vaddr, desc[tail].length, DMA_FROM_DEVICE);

		retval = copy_to_user(buf + offset, scbuf[tail].bf_vaddr,
				      desc[tail].length);
		if (retval) {
			return retval;
		}
		desc[tail].BufPtr = (unsigned int) scbuf[tail].bf_paddr;
		/* Hand over the ownership of the descriptor to MBOX */	
		desc[tail].OWN = 1;
	
		offset += desc[tail].length;	
		tail = next_tail(tail);
		
	}

	dmabuf->tail = tail;

	if (need_start) {
		ath_i2s_dma_desc((unsigned long) desc_p, mode);
		if (filp) {
			ath_i2s_dma_start(mode);
		}
	} else if (!sc->rpause) {
		ath_i2s_dma_resume(mode);
	}

	return offset;
}

ssize_t ath_i2s_read(struct file * filp, char __user * buf,
                         size_t count, loff_t * f_pos)
{
	int tmpcount, ret = 0;
	int cnt = 0;
	char *data;
#ifdef I2S_DEBUG
	static int myVectCnt = 0;
	u_int16_t *locBuf = (u_int16_t *)buf;
	for( tmpcount = 0; tmpcount < (count>>1); tmpcount++) {
		*locBuf++ = myTestVect[myVectCnt++];
		if( myVectCnt >= (myTestVectSz *2) ) myVectCnt = 0;
	}
#endif
eagain:
	tmpcount = count;
	data = (char *) buf;
	ret = 0;

	do {
		ret = ath_i2s_rd(filp, data, tmpcount, NULL);
		cnt++;
		if (ret == -ERESTART) {
			return ret;
		}
		if (ret == EAGAIN) {
			printk("%s:%d %d\n", __func__, __LINE__, ret);
			goto eagain;
		}


		tmpcount = tmpcount - ret;
		data += ret;
#ifdef I2S_DEBUG
		if (cnt > MAX_I2S_READ_RETRIES) {
			if (i2s_start) {
				//ath_i2s_dma_start(0);
				i2s_start = 0;
			}
			stats.write_fail++;
			printk("... %s, %d: stats %d....\n", __func__, __LINE__, stats.write_fail);
			return 0;
                }
#endif
	} while(tmpcount);
        
	return count;
}

ssize_t ath_i2s_wr(struct file * filp, const char __user * buf,
			 size_t count, loff_t * f_pos)
{
#define prev_tail(t) ({ (t == 0) ? (ATH_I2S_NUM_DESC - 1) : (t - 1); })
#define next_tail(t) ({ (t == (ATH_I2S_NUM_DESC - 1)) ? 0 : (t + 1); })

	uint8_t *data;
	ssize_t retval;
	int byte_cnt, offset, need_start = 0;
	int mode = 0;
	struct ath_i2s_softc *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf = &sc->sc_pbuf;
	i2s_buf_t *scbuf;
	ath_mbox_dma_desc *desc;
	int tail = dmabuf->tail;
	unsigned long desc_p;
	
	byte_cnt = count;

	if (sc->popened < 2) {
		ath_reg_rmw_set(ATH_MBOX_INT_ENABLE, ATH_MBOX_RX_DMA_COMPLETE);
		need_start = 1;
	}

	sc->popened = 2; 

	scbuf = dmabuf->db_buf;
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
	offset = 0;
	data = scbuf[0].bf_vaddr;

	desc_p += tail * sizeof(ath_mbox_dma_desc);

#ifndef AOW
	if (!need_start) {
		retval = wait_event_interruptible(sc->wq_rx, desc[tail].OWN != 1);
		if (retval == -ERESTARTSYS) {
			return -ERESTART;
		}
	}
#endif

	while (byte_cnt && !desc[tail].OWN) {
		if (byte_cnt >= ATH_I2S_BUFF_SIZE) {
			desc[tail].length = ATH_I2S_BUFF_SIZE;
			byte_cnt -= ATH_I2S_BUFF_SIZE;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
		}
		//#ifdef USE_MEMCPY        
		//        memcpy(scbuf[tail].bf_vaddr, buf + offset, ATH_I2S_BUFF_SIZE);
		//#else        
		//        retval = copy_from_user(scbuf[tail].bf_vaddr, buf + offset,
		//                                ATH_I2S_BUFF_SIZE);
		//        if (retval)
		//            return retval;
		//#endif            
#ifdef GMRWAR 
		retval = 0;
		memcpy(scbuf[tail].bf_vaddr, buf + offset,
				ATH_I2S_BUFF_SIZE);
#else        
		  retval = copy_from_user(scbuf[tail].bf_vaddr, buf + offset,
                                desc[tail].length);

		if (retval) 
		  return retval;
		
#endif          
		dma_cache_sync(NULL, scbuf[tail].bf_vaddr, desc[tail].length, DMA_TO_DEVICE);
		desc[tail].BufPtr = (unsigned int) scbuf[tail].bf_paddr;
		/* Hand over the ownership of the descriptor to MBOX */
		desc[tail].OWN = 1;

		tail = next_tail(tail);
		offset += ATH_I2S_BUFF_SIZE;
	}

	
	dmabuf->tail = tail;

	if (need_start) {
		ath_i2s_dma_desc((unsigned long) desc_p, mode);
		ath_i2s_dma_start(mode);
	} else if (!sc->ppause) {
		ath_i2s_dma_resume(mode);
	}
	return count - byte_cnt;
}


void ath_ex_i2s_write(size_t count, const char * buf, int resume)
{
	int tmpcount, ret = 0;
	int cnt = 0;
	char *data;
#ifdef I2S_DEBUG
	static int myVectCnt = 0; 
	u_int16_t *locBuf = (u_int16_t *)buf;
	for( tmpcount = 0; tmpcount < (count>>1); tmpcount++) {
		*locBuf++ = myTestVect[myVectCnt++];        
		if( myVectCnt >= (myTestVectSz *2) ) myVectCnt = 0;
	}
#endif
eagain:
	tmpcount = count;
	data = (char *) buf;
	ret = 0;

	do {
		ret = ath_i2s_wr(NULL, data, tmpcount, NULL);
		cnt++;
		if (ret == -ERESTART) {
#ifdef I2S_DEBUG
			printk("%s:%d %d\n", __func__, __LINE__, ret);
#endif
			return;
		}
		if (ret == EAGAIN) {
#ifdef I2S_DEBUG
			printk("%s:%d %d\n", __func__, __LINE__, ret);
#endif
			goto eagain;
		}

		tmpcount = tmpcount - ret;
		data += ret;
#ifdef AOW
		if (cnt > MAX_I2S_WRITE_RETRIES) {
			if (i2s_start) {
				//ath_i2s_dma_start(0);
				i2s_start = 0;
			}
			stats.write_fail++;
			printk("... %s, %d: stats %d....\n", __func__, __LINE__, stats.write_fail);
			return;
		}            
#endif
	} while(tmpcount);
}
EXPORT_SYMBOL(ath_ex_i2s_write);


ssize_t ath_i2s_write(struct file * filp, const char __user * buf,
			 size_t count, loff_t * f_pos)
{
	int tmpcount, ret = 0;
	int cnt = 0;
	char *data;
#ifdef I2S_DEBUG
	static int myVectCnt = 0;
	u_int16_t *locBuf = (u_int16_t *)buf;
	for( tmpcount = 0; tmpcount < (count>>1); tmpcount++) {
		*locBuf++ = myTestVect[myVectCnt++];
		if( myVectCnt >= (myTestVectSz *2) ) myVectCnt = 0;
	}
#endif
#ifdef GMRWAR
	static char buffer[16384];
	static int intbuf[16384/4];
	int buffersize,i;
	int bigdata;
	short int smalldata;
	int off=0;
	static int re_13=0;


	if(1) {
		buffersize = count > 16384?16384:count;
		copy_from_user(buffer, buf, buffersize);
		if(count > sizeof(buffer))
			printk(KERN_ERR "size=%d, cnt=%d\n", buffersize,count);
		for(i=0;i<buffersize/4;i++) { 
			bigdata = *(int *)(buffer + i*4);
			smalldata = (short int) (bigdata>>16);
			smalldata = ((smalldata & 0xFF) << 8) + ((smalldata >> 8) & 0xFF);
			*(short int *)(buffer + i*2) = smalldata;

		}
		buffersize /= 2;
	}
#if 0
	for(i=0;i<buffersize/4;i++) { 
		intbuf[i+off] = ((int *)buffer)[i];
		re_13++;
		if(re_13 == 13) {
			intbuf[i+off+1] = ((int *)buffer)[i];
			off++;
			re_13 = 0;
		}
	}
	buffersize += off*4;
#endif
	memcpy(intbuf, buffer, buffersize);
#endif

eagain:
#ifdef GMRWAR
	tmpcount = buffersize;
	data = intbuf;
#else
	tmpcount = count;
	data = (char *) buf;
#endif
	ret = 0;

	do {
		ret = ath_i2s_wr(NULL, data, tmpcount, NULL);
		cnt++;
		if (ret == -ERESTART) {
			return ret;
		}
		if (ret == EAGAIN) {
			printk("%s:%d %d\n", __func__, __LINE__, ret);
			goto eagain;
		}


		tmpcount = tmpcount - ret;
		data += ret;
#ifdef I2S_DEBUG
		if (cnt > MAX_I2S_WRITE_RETRIES) {
			if (i2s_start) {
				//ath_i2s_dma_start(0);
				i2s_start = 0;
			}
			stats.write_fail++;
			printk("... %s, %d: stats %d....\n", __func__, __LINE__, stats.write_fail);
			return 0;
		}
#endif
	} while(tmpcount);

	return count;
}


int ath_i2s_close(struct inode *inode, struct file *filp)
{
	int j, own, mode;
	ath_i2s_softc_t *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf;
	ath_mbox_dma_desc *desc;

	del_timer(&sc->pll_lost_lock);
	sc->pll_timer_inited = 0;

	if (!filp) {
		mode  = 0;
	} else {
		mode = filp->f_mode;
	}

	if (mode & FMODE_READ) {
		dmabuf = &sc->sc_rbuf;
		own = sc->rpause;
	} else {
		dmabuf = &sc->sc_pbuf;
		own = sc->ppause;
	}

	desc = dmabuf->db_desc;
	if (own) {
		for (j = 0; j < ATH_I2S_NUM_DESC; j++) {
			desc[j].OWN = 0;
		}
		ath_i2s_dma_resume(mode);
	} else { 
	  /* Wait for MBOX to give up the descriptors */
	  for (j = 0; j < ATH_I2S_NUM_DESC; j++) {
	    while (desc[j].OWN) {
				schedule_timeout_interruptible(HZ);
			}
		}
	}

#ifndef USE_SRAM	
	for (j = 0; j < ATH_I2S_NUM_DESC; j++) {
		dma_unmap_single(NULL, dmabuf->db_buf[j].bf_paddr,
				ATH_I2S_BUFF_SIZE, DMA_BIDIRECTIONAL);
	}

	if (mode & FMODE_READ) {
		kfree(sc->sc_rmall_buf);
	} else {
		kfree(sc->sc_pmall_buf);
	}

	dma_free_coherent(NULL,
			ATH_I2S_NUM_DESC * sizeof(ath_mbox_dma_desc),
			dmabuf->db_desc, dmabuf->db_desc_p);
#endif
	if (mode & FMODE_READ) {
		sc->ropened = 0;
		sc->rpause = 0;
	} else {
		sc->popened = 0;
		sc->ppause = 0;
	}
	return (0);
}

void ath_ex_i2s_close()          
{                                
	ath_i2s_close(NULL, NULL);
}
EXPORT_SYMBOL(ath_ex_i2s_close); 

int ath_i2s_release(struct inode *inode, struct file *filp)
{
	printk(KERN_CRIT "release\n");
	return 0;
}

int ath_i2s_set_freq(unsigned long audio_pll, unsigned long target_pll,
                     unsigned int psedge)
{
	ath_i2s_clk(target_pll, audio_pll);
	ath_i2s_dpll(ATH_AUD_DPLL3_KD_40, ATH_AUD_DPLL3_KI_40);
	
	ath_i2s_clear_stereo(ATH_STEREO_CONFIG_PSEDGE(0xff));
	
	ath_i2s_write_stereo((ATH_STEREO_CONFIG_PSEDGE(psedge)));
	return 0;
}

void ath_ex_i2s_set_freq(unsigned int data)
{
	        ath_i2s_set_freq( AUDIO_PLL_CONFIG_DEF_48_16, AUDIO_PLL_MOD_DEF_48_16,
				  STEREO_CONFIG_DEF_POSEDGE);

}
EXPORT_SYMBOL(ath_ex_i2s_set_freq);

/* Set Data Word Size and I2S Word Size in stereo
 * Config */
int ath_i2s_set_dsize(unsigned int data)
{
	unsigned int	st_cfg = 0;

	switch (data) {
	case 8:
		//ath_reg_wr(ATH_STEREO_CONFIG, 0xa00302);
		st_cfg = (ATH_STEREO_CONFIG_DATA_WORD_SIZE(ATH_STEREO_WS_8B));
		break;
	case 16:
		//ath_reg_wr(ATH_STEREO_CONFIG, 0xa21302);
		st_cfg = (ATH_STEREO_CONFIG_PCM_SWAP |
			  ATH_STEREO_CONFIG_DATA_WORD_SIZE(ATH_STEREO_WS_16B));
		break;
	case 24:
		//ath_reg_wr(ATH_STEREO_CONFIG, 0xa22b02);
		st_cfg = (ATH_STEREO_CONFIG_PCM_SWAP |
			  ATH_STEREO_CONFIG_DATA_WORD_SIZE(ATH_STEREO_WS_24B) |
			  ATH_STEREO_CONFIG_I2S_32B_WORD);
		break;
	case 32:
		//ath_reg_wr(ATH_STEREO_CONFIG, 0xa23b02);
		st_cfg = (ATH_STEREO_CONFIG_PCM_SWAP |
				ATH_STEREO_CONFIG_DATA_WORD_SIZE(ATH_STEREO_WS_32B) |
				ATH_STEREO_CONFIG_I2S_32B_WORD);
		break;
	default:
		printk(KERN_CRIT "Data size %d not supported \n", data);
		return -ENOTSUPP;
	}

	ath_i2s_clear_stereo((ATH_STEREO_CONFIG_PCM_SWAP |
                                              ATH_STEREO_CONFIG_DATA_WORD_SIZE(3) |
                                              ATH_STEREO_CONFIG_I2S_32B_WORD));

	ath_i2s_write_stereo(st_cfg);

	return 0;
}

/* Increase/Decrease the volume of the sample
 * being played */
int ath_i2s_set_volume(int data)
{
	int	mask;
	
	if (data > STEREO_VOLUME_MAX || data < STEREO_VOLUME_MIN) {
		printk("Volume not supported \n");
		return -ENOTSUPP;
	} 
	else {
		/* Negative numbers are represented as twos complement */
		if (data < 0 ) {
			mask = (~(data - 0x1))&0x0f;
			data = mask | 0x10;
			
		}
		else {
			data = (data & 0x0f);
		}
		/* Clear all stereo volume bits */
		ath_reg_rmw_clear(ATH_STEREO_VOLUME, STEREO0_VOLUME_CHANNEL1_SET(0x1f));
		ath_reg_rmw_clear(ATH_STEREO_VOLUME, STEREO0_VOLUME_CHANNEL0_SET(0x1f));
		/* Set Stereo volume bits */
		ath_reg_rmw_set(ATH_STEREO_VOLUME, STEREO0_VOLUME_CHANNEL1_SET(data));
		ath_reg_rmw_set(ATH_STEREO_VOLUME, STEREO0_VOLUME_CHANNEL0_SET(data));
	}

	return 0;
}

/* Only for debug purpose */
int ath_spdif_set_vuc(vuc_t *spdif_vuc)
{
	ath_i2s_softc_t *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf;
	int j, k = 0;

	dmabuf = &sc->sc_pbuf;
	for (j = 0; j < ATH_I2S_NUM_DESC; j++) {
		dmabuf->db_desc[j].OWN = 0;
#ifdef SPDIF
		if (spdif_vuc->write == 1) {
			for (k = 0; k < 6; k++) {
				dmabuf->db_desc[j].Va[k] = spdif_vuc->va[k];
				dmabuf->db_desc[j].Ua[k] = spdif_vuc->ua[k];
				dmabuf->db_desc[j].Ca[k] = spdif_vuc->ca[k];
				dmabuf->db_desc[j].Vb[k] = spdif_vuc->vb[k];
				dmabuf->db_desc[j].Ub[k] = spdif_vuc->ub[k];
				dmabuf->db_desc[j].Cb[k] = spdif_vuc->cb[k];
				printk(KERN_DEBUG "spdif %x %x %x %x %x %x \n", dmabuf->db_desc[j].Va[k],  dmabuf->db_desc[j].Ua[k], dmabuf->db_desc[j].Ca[k],  dmabuf->db_desc[j].Vb[k] , dmabuf->db_desc[j].Ub[k] , dmabuf->db_desc[j].Cb[k]);
			}
		}
		else {
			for (k = 0; k < 6; k++) {
				spdif_vuc->va[k] = dmabuf->db_desc[j].Va[k];
				spdif_vuc->ua[k] = dmabuf->db_desc[j].Ua[k];
				spdif_vuc->ca[k] = dmabuf->db_desc[j].Ca[k];
				spdif_vuc->vb[k] = dmabuf->db_desc[j].Vb[k];
				spdif_vuc->ub[k] = dmabuf->db_desc[j].Ub[k];
				spdif_vuc->cb[k] = dmabuf->db_desc[j].Cb[k];
				printk(KERN_DEBUG "spdif read %x %x %x %x %x %x \n", spdif_vuc->va[k],  spdif_vuc->ua[k], spdif_vuc->ca[k],  spdif_vuc->vb[k] , spdif_vuc->ub[k] , spdif_vuc->cb[k]);
			}
		}
	}
#endif
        return 0;

                                           
}
int ath_i2s_ioctl(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	int data;
	struct ath_i2s_softc *sc = &sc_buf_var;
	pll_t i2s_pll;
	vuc_t spdif_vuc;
	int ret;
	unsigned long fine;
	unsigned int rd_data;

	switch (cmd) {
	case I2S_PAUSE:
		data = arg;
		ath_i2s_dma_pause(data);
		if (data) {
			sc->rpause = 1;
		} else {
			sc->ppause = 1;
		}
		return 0;
	case I2S_RESUME:
		data = arg;
		ath_i2s_dma_resume(data);
		if (data) {
			sc->rpause = 0;
		} else {
			sc->ppause = 0;
		}
		return 0;
	case I2S_VOLUME:
		sc->vol = arg;
		ret = ath_i2s_set_volume(sc->vol);
		return ret;

	case I2S_FREQ:
		ret =  copy_from_user(&i2s_pll, (pll_t *)arg, sizeof(pll_t));
		if (ret) {
			return ret;
		} 
		else {
			sc->audio_pll = i2s_pll.audio_pll;
			sc->target_pll = i2s_pll.target_pll;
			sc->psedge = i2s_pll.psedge;
			return ath_i2s_set_freq(i2s_pll.audio_pll, i2s_pll.target_pll,
						i2s_pll.psedge);       
		}
		

	case I2S_FINE:
		fine = arg; 
		ath_reg_rmw_clear(AUDIO_PLL_MODULATION_ADDRESS, AUDIO_PLL_MODULATION_TGT_DIV_FRAC_MASK);
		ath_reg_rmw_set(AUDIO_PLL_MODULATION_ADDRESS, AUDIO_PLL_MODULATION_TGT_DIV_FRAC_SET(fine));
		ath_reg_rmw_set(AUDIO_PLL_MOD_STEP_ADDRESS, AUDIO_PLL_MOD_STEP_UPDATE_CNT_SET(0x1));
		ath_reg_rmw_set(AUDIO_PLL_MOD_STEP_ADDRESS, AUDIO_PLL_MOD_STEP_FRAC_SET(0x1));
		ath_reg_rmw_set(AUDIO_PLL_MODULATION_ADDRESS, AUDIO_PLL_MODULATION_START_SET(1));
		do {
			rd_data = ath_reg_rd(CURRENT_AUDIO_PLL_MODULATION_ADDRESS);
			rd_data = CURRENT_AUDIO_PLL_MODULATION_FRAC_GET(rd_data); 
		} while(rd_data != fine);
		if (rd_data == fine) {
			printk("Fine freq done \n");
		}

		return 0;

	case I2S_DSIZE:
		sc->dsize = arg;
		return ath_i2s_set_dsize(sc->dsize);

	case I2S_MODE:		/* mono or stereo */
		data = arg;
		ath_i2s_clear_stereo(ATH_STEREO_CONFIG_MODE(3));
		ath_i2s_write_stereo(ATH_STEREO_CONFIG_MODE(data));
		
		/*Otherwise default settings. Nothing to do */
		return 0;

	case I2S_MICIN:
		data = arg;
		if (data == 1) {
			/* Write MIC IN mode to 32 bit */
			ath_i2s_write_stereo(ATH_STEREO_CONFIG_MIC_WORD_SIZE);
		}
		return 0;

	case I2S_MCLK:
		data = arg;
		ath_i2s_clear_stereo(ATH_STEREO_CONFIG_I2S_MCLK_SEL);
		ath_i2s_write_stereo(data << 10);
		return 0;

	case I2S_SPDIF_DISABLE:
		data = arg;
		/* Clear SPDIF Enable bit */
		if (data) {
			 ath_i2s_clear_stereo(ATH_STEREO_CONFIG_SPDIF_ENABLE);
		}
		/* Write SPDIF Enable */
			 ath_i2s_write_stereo(ATH_STEREO_CONFIG_SPDIF_ENABLE);
		return 0;	
	case I2S_BITSWAP:
		data = arg;
		/* Rx bit swap */
		if (data) {
			ath_reg_rmw_set(ATH_MBOX_DMA_POLICY,
					MBOX_DMA_POLICY_RXD_16BIT_SWAP_SET(1));
		}
		else {

			/* Tx Bit Swap */
			ath_reg_rmw_set(ATH_MBOX_DMA_POLICY,
					MBOX_DMA_POLICY_TXD_16BIT_SWAP_SET(1));
		}
		return 0;

	case I2S_BYTESWAP:
		data = arg;
		/* Rx byte swap */
		if (data) {
			ath_reg_rmw_set(ATH_MBOX_DMA_POLICY,
					MBOX_DMA_POLICY_RXD_END_SWAP_SET(1));
		}
		else {
			/* Tx Byte Swap */
			ath_reg_rmw_set(ATH_MBOX_DMA_POLICY,
					MBOX_DMA_POLICY_TXD_END_SWAP_SET(1));
		}
		return 0;
		
	case I2S_SPDIF_VUC:
		ret =  copy_from_user(&spdif_vuc, (vuc_t *)arg, sizeof(vuc_t));
		if (ret) {
			return ret;
		}
		else {
			ret = ath_spdif_set_vuc(&spdif_vuc);
			if (spdif_vuc.write == 0) 
				copy_to_user(&spdif_vuc, (vuc_t *)arg, sizeof(vuc_t));
			return ret;
		}

	case I2S_COUNT:
		data = arg;
		return 0;


		default:
			return -ENOTSUPP;
	}

}

irqreturn_t ath_i2s_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int int_status;
	ath_i2s_softc_t *sc = &sc_buf_var;

	int_status = ath_reg_rd(ATH_MBOX_INT_STATUS); 
#ifndef AOW
	if (int_status & ATH_MBOX_RX_DMA_COMPLETE) {
		wake_up_interruptible(&sc->wq_rx);
	}
	if (int_status & ATH_MBOX_TX_DMA_COMPLETE) {
		wake_up_interruptible(&sc->wq_tx);
	}
#endif

	if (int_status & ATH_MBOX_RX_UNDERFLOW) {
		printk("ath_i2s: Underflow Encountered....\n");
	}
	if (int_status & ATH_MBOX_TX_OVERFLOW) {
		printk("ath_i2s: Overflow Encountered....\n");
	}

	/* Ack the interrupts */
	if (is_qca955x()) {
		ath_reg_wr(ATH_MBOX_INT_STATUS, 
				int_status & (~(ATH_MBOX_RX_DMA_COMPLETE | ATH_MBOX_TX_DMA_COMPLETE | 
						ATH_MBOX_TX_OVERFLOW | ATH_MBOX_RX_UNDERFLOW)));
	} else {
		ath_reg_wr(ATH_MBOX_INT_STATUS, int_status);
	}

	return IRQ_HANDLED;
}

/* The I2S initialization fn carries hard-coded values.
 * Register descriptions are to be defined and updated.
 * TODO list
 */
void ath_i2s_init_reg(int ath_i2s_slave)
{
#define ATH_STEREO_CONFIG_DEFAULT ( \
                ATH_STEREO_CONFIG_SPDIF_ENABLE | \
                ATH_STEREO_CONFIG_ENABLE | \
                ATH_STEREO_CONFIG_PCM_SWAP | \
                ATH_STEREO_CONFIG_DATA_WORD_SIZE(ATH_STEREO_WS_16B) | \
                ATH_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE | \
		ATH_STEREO_CONFIG_PSEDGE(2))

	unsigned int rddata;
	unsigned int st_cfg = 0;

	/* Reset I2S Controller */
	ath_reset(ATH_RESET_I2S);
	udelay(1000);

	if (is_qca955x()) {
/* Scorpion Emulation GPIO settings */
#ifdef CONFIG_ATH_EMULATION
	// MIC IN as i/p on GPIO_14
	ath_reg_wr(ATH_GPIO_IN_ENABLE1, ((ath_reg_rd(ATH_GPIO_IN_ENABLE1) & ~(0xff00))|0xe00));

	//SD is o/p on GPIO 17 and has fn index 15
	ath_reg_wr(ATH_GPIO_OUT_FUNCTION4, (ath_reg_rd(ATH_GPIO_OUT_FUNCTION4)|0xf00));

	//SPDIF is on GPIO 13 and has fn index 17
	ath_reg_wr(ATH_GPIO_OUT_FUNCTION3, (ath_reg_rd(ATH_GPIO_OUT_FUNCTION3)|0x1100));
	
	if (ath_i2s_slave) {
		printk("Debug:: I2S in slave mode....\n");
		// SCLK is an input on GPIO12
		rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE1) & ~(0x00ff0000));
		ath_reg_wr(ATH_GPIO_IN_ENABLE1, rddata | GPIO_IN_ENABLE1_I2SEXTCLK_SET(12));

		// WS is an input on GPIO11
		rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE1) & ~(0x000000ff));
		ath_reg_wr(ATH_GPIO_IN_ENABLE1, rddata | GPIO_IN_ENABLE1_I2S0__WS_SET(11));

		ath_reg_wr(ATH_GPIO_OE, ath_reg_rd(ATH_GPIO_OE) & ~(0x22000));

		ath_reg_wr(ATH_GPIO_OE, ath_reg_rd(ATH_GPIO_OE) | 0xd800);

	} else {
		 printk("Debug:: I2s in Master mode....\n");
		//SCLK and WS are o/p
		ath_reg_wr(ATH_GPIO_OUT_FUNCTION3, (ath_reg_rd(ATH_GPIO_OUT_FUNCTION3)|0x0d));

		//WS is on GPIO 11 and has fn index 14
		ath_reg_wr(ATH_GPIO_OUT_FUNCTION2, (ath_reg_rd(ATH_GPIO_OUT_FUNCTION2)|0xe000000));

		ath_reg_wr(ATH_GPIO_OE, (ath_reg_rd(ATH_GPIO_OE)&(~(0x23800))));
	}
#else
		/* MIC IN as i/p on GPIO_14 */
		rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE1) & (~GPIO_IN_ENABLE1_I2S0__MIC_SD_MASK));
		rddata = (rddata | GPIO_IN_ENABLE1_I2S0__MIC_SD_SET(14));
		ath_reg_wr(ATH_GPIO_IN_ENABLE1, rddata);
		
		/* Speaker out is o/p on GPIO 11 and has fn index 15 */
		rddata = (ath_reg_rd(ATH_GPIO_OUT_FUNCTION2) & (~GPIO_OUT_FUNCTION2_ENABLE_GPIO_11_MASK));
		rddata = (rddata | GPIO_OUT_FUNCTION2_ENABLE_GPIO_11_SET(15));
		ath_reg_wr(ATH_GPIO_OUT_FUNCTION2, rddata);
				      
		/* SPDIF is on GPIO 15 and has fn index 17 */
		rddata = (ath_reg_rd(ATH_GPIO_OUT_FUNCTION3) & (~GPIO_OUT_FUNCTION3_ENABLE_GPIO_15_MASK));
		rddata = (rddata | GPIO_OUT_FUNCTION3_ENABLE_GPIO_15_SET(17));
		ath_reg_wr(ATH_GPIO_OUT_FUNCTION3, rddata);

		if (ath_i2s_slave) { 
			printk("Debug:: I2S in slave mode....\n");
			if (ath_i2s_mclk_sel == 1) {
				/* MCLK is on 4 and is input */
				rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE1) & (~GPIO_IN_ENABLE1_I2SEXT_MCLK_MASK));
				rddata = (rddata | GPIO_IN_ENABLE1_I2SEXT_MCLK_SET(4));
        	      	      ath_reg_wr(ATH_GPIO_IN_ENABLE1, rddata);
			}

			else {
				/* MCLK is on GPIO 4 and is output */
				rddata = (ath_reg_rd(ATH_GPIO_OUT_FUNCTION1) & (~GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_MASK));

				rddata = (rddata | GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_SET(16));
                                ath_reg_wr(ATH_GPIO_OUT_FUNCTION1, rddata);
			}
	

			/* SCLK is an input on GPIO13 */
			rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE1) & ~(GPIO_IN_ENABLE1_I2SEXTCLK_MASK));
			ath_reg_wr(ATH_GPIO_IN_ENABLE1, (rddata | GPIO_IN_ENABLE1_I2SEXTCLK_SET(13)));

			/* WS is an input on GPIO12 */
			rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE1) & ~(GPIO_IN_ENABLE1_I2S0__WS_MASK));
			ath_reg_wr(ATH_GPIO_IN_ENABLE1, (rddata | GPIO_IN_ENABLE1_I2S0__WS_SET(12)));

			if (ath_i2s_mclk_sel == 0) {
				ath_reg_wr(ATH_GPIO_OE, ((ath_reg_rd(ATH_GPIO_OE)) & (~(0x8810))));

				ath_reg_wr(ATH_GPIO_OE, ((ath_reg_rd(ATH_GPIO_OE)) | (0x7000)));
			}

			else {
				ath_reg_wr(ATH_GPIO_OE, ((ath_reg_rd(ATH_GPIO_OE)) & (~(0x8800))));

				ath_reg_wr(ATH_GPIO_OE, ((ath_reg_rd(ATH_GPIO_OE)) | (0x7010)));
			}



		} else {
			printk("Debug:: I2s in Master mode....\n");

			if (ath_i2s_mclk_sel == 0) {
				/* MCLK is on GPIO 4 and is output */
				rddata = (ath_reg_rd(ATH_GPIO_OUT_FUNCTION1) & (~GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_MASK));

				rddata = (rddata | GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_SET(16));
				ath_reg_wr(ATH_GPIO_OUT_FUNCTION1, rddata);
			}
			else {
			 	/* MCLK is on 4 and is input */
				 rddata = (ath_reg_rd(ATH_GPIO_IN_ENABLE1) & (~GPIO_IN_ENABLE1_I2SEXT_MCLK_MASK));
				 rddata = (rddata | GPIO_IN_ENABLE1_I2SEXT_MCLK_SET(4));
				 ath_reg_wr(ATH_GPIO_IN_ENABLE1, rddata);
			}

			/* SCLK and WS are o/p on 13 and 12 */
			rddata = (ath_reg_rd(ATH_GPIO_OUT_FUNCTION3) & (~GPIO_OUT_FUNCTION3_ENABLE_GPIO_13_MASK));
			rddata = (rddata | GPIO_OUT_FUNCTION3_ENABLE_GPIO_13_SET(13));
			ath_reg_wr(ATH_GPIO_OUT_FUNCTION3, rddata);

			/* WS is on GPIO 12 and has fn index 14 */
			rddata = (ath_reg_rd(ATH_GPIO_OUT_FUNCTION3) & (~GPIO_OUT_FUNCTION3_ENABLE_GPIO_12_MASK));
			rddata = (rddata | GPIO_OUT_FUNCTION3_ENABLE_GPIO_12_SET(14));
			ath_reg_wr(ATH_GPIO_OUT_FUNCTION3, rddata);

			if (ath_i2s_mclk_sel == 0) { 
				ath_reg_wr(ATH_GPIO_OE, (ath_reg_rd(ATH_GPIO_OE) & (~(0x0b810))));

				ath_reg_wr(ATH_GPIO_OE, ath_reg_rd(ATH_GPIO_OE) | 0x4000);
			}
			else {
				ath_reg_wr(ATH_GPIO_OE, (ath_reg_rd(ATH_GPIO_OE) & (~(0x0b800))));

				ath_reg_wr(ATH_GPIO_OE, ath_reg_rd(ATH_GPIO_OE) | 0x4010);
			}

		}
#endif 
	} else {
		/* MIC IN as i/p on GPIO_14 */
		rddata = ath_reg_rd(ATH_GPIO_IN_ENABLE1);
                rddata = rddata |  GPIO_IN_ENABLE1_I2S0__MIC_SD_SET(14);
                ath_reg_wr(ATH_GPIO_IN_ENABLE1, rddata);

		rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION2);
		rddata = rddata & (~GPIO_OUT_FUNCTION2_ENABLE_GPIO_11_MASK);
		rddata = rddata | ATH_GPIO_OUT_FUNCTION2_ENABLE_GPIO_11(0x0e);
		ath_reg_wr(ATH_GPIO_OUT_FUNCTION2, rddata);

		rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION3);
		rddata = rddata | (ATH_GPIO_OUT_FUNCTION3_ENABLE_GPIO_12(0x0d) |
				ATH_GPIO_OUT_FUNCTION3_ENABLE_GPIO_13(0x0c) |
				ATH_GPIO_OUT_FUNCTION3_ENABLE_GPIO_15(0x19));
		ath_reg_wr(ATH_GPIO_OUT_FUNCTION3, rddata);
	
		rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION1);
		rddata = rddata & (~GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_MASK);;
		rddata = rddata | ATH_GPIO_OUT_FUNCTION1_ENABLE_GPIO_4(0x0f);
		ath_reg_wr(ATH_GPIO_OUT_FUNCTION1, rddata);
	
		rddata = ath_reg_rd(ATH_GPIO_OUT_FUNCTION4);
		rddata = rddata & 0xffffff00;
		ath_reg_wr(ATH_GPIO_OUT_FUNCTION4, rddata);

		rddata = ath_reg_rd(ATH_GPIO_OE);
		rddata = rddata | ATH_GPIO_OE_EN(0x4000);
		rddata = rddata & 0xffff47ef;
		ath_reg_wr(ATH_GPIO_OE, rddata);
	}
	/* Clear Mode bit. It is set by default */
	ath_i2s_clear_stereo(ATH_STEREO_CONFIG_MASTER);
	if (!ath_i2s_slave) {
		// Program Stereo Config Register for master mode.
		st_cfg = (ATH_STEREO_CONFIG_DEFAULT | ATH_STEREO_CONFIG_MASTER);
	}
	else {
		// Program Stereo Config Register for slave mode.
		st_cfg = (ATH_STEREO_CONFIG_DEFAULT);
	}

	/* Select MCLK source */
	if (ath_i2s_mclk_sel) {
		st_cfg |= ATH_STEREO_CONFIG_I2S_MCLK_SEL;
	} 

	/* Clear I2S Delay. It is set by default */
	ath_i2s_clear_stereo(ATH_STEREO_CONFIG_DELAY);

#ifdef CONFIG_ATH_EMULATION
	st_cfg |=  ATH_STEREO_CONFIG_I2S_MCLK_SEL;
#endif
	/* Writing stereo */
	ath_i2s_write_stereo(st_cfg);

	/* Set SRAM access bit */
#ifdef USE_SRAM
	ath_reg_rmw_set(ATH_MBOX_DMA_POLICY, ATH_MBOX_DMA_POLICY_SRAM_AC);
#endif
#undef ATH_STEREO_CONFIG_DEFAULT
}

void ath_i2s_request_dma_channel(void)
{
	ath_reg_wr(ATH_MBOX_DMA_POLICY, 
			(ATH_MBOX_DMA_POLICY_RX_QUANTUM |
			 ATH_MBOX_DMA_POLICY_TX_QUANTUM |
			 ATH_MBOX_DMA_POLICY_TX_FIFO_THRESH(6)));

	/* Enable Overflow underrun interrupts */
	ath_reg_rmw_set(ATH_MBOX_INT_ENABLE, (ATH_MBOX_TX_OVERFLOW | ATH_MBOX_RX_UNDERFLOW));
#ifdef USE_SRAM
	ath_reg_rmw_set(ATH_MBOX_DMA_POLICY, ATH_MBOX_DMA_POLICY_SRAM_AC);
#endif
}

void ath_i2s_dma_desc(unsigned long desc_buf_p, int mode)
{
	/*
	 * Program the device to generate interrupts
	 * RX_DMA_COMPLETE for mbox 0
	 */
	if (mode) {
		ath_reg_wr(ATH_MBOX_DMA_TX_DESCRIPTOR_BASE0, desc_buf_p);
	} else {
		ath_reg_wr(ATH_MBOX_DMA_RX_DESCRIPTOR_BASE0, desc_buf_p);
	}
}

void ath_i2s_dma_start(int mode)
{
	/*
	 * Start
	 */
	if (mode) {
		ath_reg_wr(ATH_MBOX_DMA_TX_CONTROL0, ATH_MBOX_DMA_START);
	} else {
		ath_reg_wr(ATH_MBOX_DMA_RX_CONTROL0, ATH_MBOX_DMA_START);
	}
}
EXPORT_SYMBOL(ath_i2s_dma_start);

void ath_i2s_dma_pause(int mode)
{
	/*
	 * Pause
	 */
	if (mode) {
		ath_reg_wr(ATH_MBOX_DMA_TX_CONTROL0, ATH_MBOX_DMA_PAUSE);
	} else {
		ath_reg_wr(ATH_MBOX_DMA_RX_CONTROL0, ATH_MBOX_DMA_PAUSE);
	}
}
EXPORT_SYMBOL(ath_i2s_dma_pause);

void ath_i2s_dma_resume(int mode)
{
	/*
	 * Resume
	 */
	if (mode) {
		ath_reg_wr(ATH_MBOX_DMA_TX_CONTROL0, ATH_MBOX_DMA_RESUME);
	} else {
		ath_reg_wr(ATH_MBOX_DMA_RX_CONTROL0, ATH_MBOX_DMA_RESUME);
	}
}
EXPORT_SYMBOL(ath_i2s_dma_resume);

void ath_i2s_dpll_lost_lock(unsigned long arg)
{
	ath_i2s_softc_t		*sc = (ath_i2s_softc_t *)arg;

	if (DPLL3_SQSUM_DVC_GET(ath_reg_rd(DPLL3_ADDRESS)) < 0x40000) {
		return;
	}

	ath_i2s_clear_stereo(ATH_STEREO_CONFIG_ENABLE);
	ath_i2s_set_freq(sc->audio_pll, sc->target_pll, sc->psedge);
	ath_i2s_write_stereo(ATH_STEREO_CONFIG_ENABLE);
}

void ath_i2s_dpll(unsigned int kd, unsigned int ki)
{
	unsigned int	i = 0;
	/* DPLL Configuration not required for Scorpion */
 	if (is_ar934x()) {
		do {
			ath_reg_rmw_clear(DPLL3_ADDRESS, DPLL3_DO_MEAS_SET(1));
			ath_reg_rmw_set(AUDIO_PLL_CONFIG_ADDRESS, AUDIO_PLL_CONFIG_PLLPWD_SET(1));
			udelay(100);
			// Configure AUDIO DPLL
			ath_reg_rmw_clear(DPLL2_ADDRESS, (DPLL2_KI_MASK | DPLL2_KD_MASK));
			ath_reg_rmw_set(DPLL2_ADDRESS, (DPLL2_KI_SET(ki) | DPLL2_KD_SET(kd)));
			ath_reg_rmw_clear(DPLL3_ADDRESS, DPLL3_PHASE_SHIFT_MASK);
			ath_reg_rmw_set(DPLL3_ADDRESS, DPLL3_PHASE_SHIFT_SET(0x6));
			if (!is_ar934x_10()) {
				ath_reg_rmw_clear(DPLL2_ADDRESS, DPLL2_RANGE_SET(1));
				ath_reg_rmw_set(DPLL2_ADDRESS, DPLL2_RANGE_SET(1));
			}
			ath_reg_rmw_clear(AUDIO_PLL_CONFIG_ADDRESS, AUDIO_PLL_CONFIG_PLLPWD_SET(1));

			ath_reg_rmw_clear(DPLL3_ADDRESS, DPLL3_DO_MEAS_SET(1));
			udelay(100);
			ath_reg_rmw_set(DPLL3_ADDRESS, DPLL3_DO_MEAS_SET(1));
			udelay(100);
			
			while ((ath_reg_rd(DPLL4_ADDRESS) & DPLL4_MEAS_DONE_SET(1)) == 0) {
				udelay(10);
			}
			udelay(100);

		i ++;
	} while (DPLL3_SQSUM_DVC_GET(ath_reg_rd(DPLL3_ADDRESS)) >= 0x40000);

	printk("\tAud:	0x%x 0x%x\n", KSEG1ADDR(DPLL3_ADDRESS),
			DPLL3_SQSUM_DVC_GET(ath_reg_rd(DPLL3_ADDRESS)));

	if (is_ar934x_11_or_later()) {
		ath_i2s_softc_t *sc = &sc_buf_var;

			sc->pll_lost_lock.expires = jiffies + HZ;
			sc->pll_lost_lock.function = ath_i2s_dpll_lost_lock;
			sc->pll_lost_lock.data = (int)sc;
			add_timer(&sc->pll_lost_lock);
		}		
	}
}
EXPORT_SYMBOL(ath_i2s_dpll);

void ath_i2s_clk(unsigned long frac, unsigned long pll)
{
	/*
	 * Tick...Tick...Tick
	 */
	ath_reg_wr(AUDIO_PLL_MODULATION_ADDRESS, frac);
	ath_reg_wr(AUDIO_PLL_CONFIG_ADDRESS, (pll | AUDIO_PLL_CONFIG_PLLPWD_SET(1)));
	ath_reg_wr(AUDIO_PLL_CONFIG_ADDRESS, (pll & ~AUDIO_PLL_CONFIG_PLLPWD_SET(1)));
}
EXPORT_SYMBOL(ath_i2s_clk);


loff_t ath_i2s_llseek(struct file *filp, loff_t off, int whence)
{
	printk(KERN_CRIT "llseek\n");
	return off;
}


struct file_operations ath_i2s_fops = {
	.owner   = THIS_MODULE,
	.llseek  = ath_i2s_llseek,
	.read    = ath_i2s_read,
	.write   = ath_i2s_write,
	.ioctl   = ath_i2s_ioctl,
	.open    = ath_i2s_open,
	.release = ath_i2s_close,
};

void ath_i2s_cleanup_module(void)
{
	ath_i2s_softc_t *sc = &sc_buf_var;

	printk(KERN_CRIT "unregister\n");

	free_irq(sc->sc_irq, NULL);
	unregister_chrdev(ath_i2s_major, "ath_i2s");
}

int ath_i2s_init_module(void)
{
	ath_i2s_softc_t *sc = &sc_buf_var;
	int result = -1;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (ath_i2s_major) {
		result = register_chrdev(ath_i2s_major, "ath_i2s",
				&ath_i2s_fops);
	}
	if (result < 0) {
		printk(KERN_WARNING "ath_i2s: can't get major %d\n",
				ath_i2s_major);
		return result;
	}

	sc->sc_irq = ATH_MISC_IRQ_DMA;

	/* Establish ISR would take care of enabling the interrupt */
	result = request_irq(sc->sc_irq, (void *) ath_i2s_intr, IRQF_DISABLED,
			"ath_i2s", NULL);
	if (result) {
		printk(KERN_INFO
				"i2s: can't get assigned irq %d returns %d\n",
				sc->sc_irq, result);
	}

	ath_i2s_init_reg(ath_i2s_slave);

	I2S_LOCK_INIT(&sc_buf_var);

	return 0;		/* succeed */
}

module_init(ath_i2s_init_module);
module_exit(ath_i2s_cleanup_module);
