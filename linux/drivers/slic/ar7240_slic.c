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
#include <linux/delay.h>

#include "ar7240.h"
#include "ar7240_slic.h"		/* local definitions */

//#define here()        do { printk("%s:%d ... 0x%08x \n", __func__, __LINE__, ar7240_reg_rd(AR7240_MBOX_SLIC_FRAME)); } while(0)
#define here()

int ar7240_slic_major = 252;
int ar7240_slic_minor = 0;

static DECLARE_WAIT_QUEUE_HEAD(wq_tx);
static DECLARE_WAIT_QUEUE_HEAD(wq_rx);

module_param(ar7240_slic_major, int, S_IRUGO);
module_param(ar7240_slic_minor, int, S_IRUGO);

MODULE_AUTHOR("Jacob@Atheros");
MODULE_LICENSE("Dual BSD/GPL");

void ar7240_slic_link_on(int master);
void ar7240_slic_request_dma_channel(int);
void ar7240_slic_dma_start(unsigned long, int);
void ar7240_slic_dma_pause(int);
void ar7240_slic_dma_resume(int);
void ar7242_slic_clk(unsigned long, unsigned long);
irqreturn_t ar7240_slic_intr(int irq, void *dev_id, struct pt_regs *regs);

int ar7240_slic_init(struct file *filp)
{
	ar7240_slic_softc_t *sc = &sc_buf_var;
	slic_dma_buf_t *dmabuf;
	slic_buf_t *scbuf;
	uint8_t *bufp = NULL;
	int j, byte_cnt, tail = 0;
	ar7240_mbox_dma_desc *desc;
	unsigned long desc_p;

	if (filp->f_mode & FMODE_READ) {
		dmabuf = &sc->sc_rbuf;
		sc->ropened = 1;
		sc->rpause = 0;
	} else {
		dmabuf = &sc->sc_pbuf;
		sc->popened = 1;
		sc->ppause = 0;
	}

	dmabuf->db_desc = (ar7240_mbox_dma_desc *)
	    dma_alloc_coherent(NULL,
			       NUM_DESC *
			       sizeof(ar7240_mbox_dma_desc),
			       &dmabuf->db_desc_p, GFP_DMA);

	if (dmabuf->db_desc == NULL) {
		printk(KERN_CRIT "DMA desc alloc failed for %d\n",
		       filp->f_mode);
		return ENOMEM;
	}

	for (j = 0; j < NUM_DESC; j++) {
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
		dmabuf->db_desc[j].Ca[0] = 0x02100000;
		dmabuf->db_desc[j].Ca[1] = 0x021000d2;
		dmabuf->db_desc[j].Cb[0] = 0x02200000;
		dmabuf->db_desc[j].Cb[1] = 0x021000d2;
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

	if (!(bufp = kmalloc(NUM_DESC * SLIC_BUF_SIZE, GFP_KERNEL))) {
		printk(KERN_CRIT
		       "Buffer allocation failed for \n");
		goto fail3;
	}

	if (filp->f_mode & FMODE_READ)
		sc->sc_rmall_buf = bufp;
	else
		sc->sc_pmall_buf = bufp;

	for (j = 0; j < NUM_DESC; j++) {
		scbuf[j].bf_vaddr = &bufp[j * SLIC_BUF_SIZE];
		scbuf[j].bf_paddr =
		    dma_map_single(NULL, scbuf[j].bf_vaddr,
				   SLIC_BUF_SIZE,
				   DMA_BIDIRECTIONAL);
	}
	dmabuf->tail = 0;

	// Initialize desc
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
	byte_cnt = NUM_DESC * SLIC_BUF_SIZE;
	tail = dmabuf->tail;

	while (byte_cnt && (tail < NUM_DESC)) {
		desc[tail].rsvd1 = 0;
		desc[tail].size = SLIC_BUF_SIZE;
		if (byte_cnt > SLIC_BUF_SIZE) {
			desc[tail].length = SLIC_BUF_SIZE;
			byte_cnt -= SLIC_BUF_SIZE;
			desc[tail].EOM = 0;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
			desc[tail].EOM = 0;
		}
		desc[tail].rsvd2 = 0;
		desc[tail].rsvd3 = 0;
		desc[tail].BufPtr =
		    (unsigned int) scbuf[tail].bf_paddr;
		desc[tail].NextPtr =
		    (desc_p +
		     ((tail +
		       1) *
		      (sizeof(ar7240_mbox_dma_desc))));
		if (filp->f_mode & FMODE_READ) {
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
	if (filp->f_mode & FMODE_READ)
		dmabuf = &sc->sc_rbuf;
	else
		dmabuf = &sc->sc_pbuf;
	dma_free_coherent(NULL,
			  NUM_DESC * sizeof(ar7240_mbox_dma_desc),
			  dmabuf->db_desc, dmabuf->db_desc_p);

	if (filp->f_mode & FMODE_READ) {
		if (sc->sc_rmall_buf)
			kfree(sc->sc_rmall_buf);
	} else {
		if (sc->sc_pmall_buf)
			kfree(sc->sc_pmall_buf);
	}

	return -ENOMEM;

}

int ar7240_slic_open(struct inode *inode, struct file *filp)
{

	ar7240_slic_softc_t *sc = &sc_buf_var;
	int opened = 0, mode = MASTER;

	if ((filp->f_mode & FMODE_READ) && (sc->ropened)) {
        printk("%s, %d SLIC mic busy\n", __func__, __LINE__);
		return -EBUSY;
	}
	if ((filp->f_mode & FMODE_WRITE) && (sc->popened)) {
        printk("%s, %d SLIC speaker busy\n", __func__, __LINE__);
		return -EBUSY;
	}

	opened = (sc->ropened | sc->popened);

    here();
	/* Reset MBOX FIFO's */
	if (!opened) {
		ar7240_reg_wr(AR7240_MBOX_SLIC_FIFO_RESET, 0x1); // virian
		udelay(500);
	}

    here();
	/* Allocate and initialize descriptors */
	if (ar7240_slic_init(filp) == ENOMEM)
		return -ENOMEM;

    here();
	if (!opened) {
	    ar7240_slic_request_dma_channel(mode);
    }

    here();
	return (0);

}


ssize_t ar7240_slic_read(struct file * filp, char __user * buf,
			 size_t count, loff_t * f_pos)
{
#define prev_tail(t) ({ (t == 0) ? (NUM_DESC - 1) : (t - 1); })
#define next_tail(t) ({ (t == (NUM_DESC - 1)) ? 0 : (t + 1); })

	uint8_t *data;
	ssize_t retval;
	struct ar7240_slic_softc *sc = &sc_buf_var;
	slic_dma_buf_t *dmabuf = &sc->sc_rbuf;
	slic_buf_t *scbuf;
	ar7240_mbox_dma_desc *desc;
	unsigned int byte_cnt, mode = 1, offset = 0, tail = dmabuf->tail;
	unsigned long desc_p;
	int need_start = 0;

	byte_cnt = count;

	if (sc->ropened < 2) {
		ar7240_reg_rmw_set(AR7240_MBOX_SLIC_INT_ENABLE, 
                            ( AR7240_MBOX_SLIC_TX_DMA_COMPLETE ));
		need_start = 1;
	}

	sc->ropened = 2;

	scbuf = dmabuf->db_buf;
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
	data = scbuf[0].bf_vaddr;

	desc_p += tail * sizeof(ar7240_mbox_dma_desc);

    if (!need_start) {
        wait_event_interruptible(wq_tx, desc[tail].OWN != 1);
    }

	while (byte_cnt && !desc[tail].OWN) {
		desc[tail].rsvd1 = 0;
		desc[tail].size = SLIC_BUF_SIZE;
		if (byte_cnt >= SLIC_BUF_SIZE) {
			desc[tail].length = SLIC_BUF_SIZE;
			byte_cnt -= SLIC_BUF_SIZE;
			desc[tail].EOM = 0;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
			desc[tail].EOM = 0;
		}
        dma_cache_sync(NULL, scbuf[tail].bf_vaddr, desc[tail].length, DMA_FROM_DEVICE);
		desc[tail].rsvd2 = 0;
		retval =
		    copy_to_user(buf + offset, scbuf[tail].bf_vaddr, 
				   SLIC_BUF_SIZE);
		if (retval)
			return retval;
		desc[tail].BufPtr = (unsigned int) scbuf[tail].bf_paddr;
		desc[tail].rsvd3 = 0;
		desc[tail].OWN = 1;

		tail = next_tail(tail);
		offset += SLIC_BUF_SIZE;
	}

	dmabuf->tail = tail;

	if (need_start) {
		ar7240_slic_dma_start((unsigned long) desc_p, mode);
	} else if (!sc->rpause) {
		ar7240_slic_dma_resume(mode);
	}

	return offset;
}


ssize_t ar7240_slic_write(struct file * filp, char __user * buf,
			 size_t count, loff_t * f_pos)
{
#define prev_tail(t) ({ (t == 0) ? (NUM_DESC - 1) : (t - 1); })
#define next_tail(t) ({ (t == (NUM_DESC - 1)) ? 0 : (t + 1); })

	uint8_t *data;
	ssize_t retval;
	int byte_cnt, offset, need_start = 0;
	int mode = 0;
	struct ar7240_slic_softc *sc = &sc_buf_var;
	slic_dma_buf_t *dmabuf = &sc->sc_pbuf;
	slic_buf_t *scbuf;
	ar7240_mbox_dma_desc *desc;
	int tail = dmabuf->tail;
	unsigned long desc_p;

	byte_cnt = count;

	if (sc->popened < 2) {
        ar7240_reg_rmw_set(AR7240_MBOX_SLIC_INT_ENABLE, 
                            ( AR7240_MBOX_SLIC_RX_DMA_COMPLETE ));
		need_start = 1;
	}

	sc->popened = 2;

	scbuf = dmabuf->db_buf;
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
	offset = 0;
	data = scbuf[0].bf_vaddr;

	desc_p += tail * sizeof(ar7240_mbox_dma_desc);

    if (!need_start) {
        wait_event_interruptible(wq_rx, desc[tail].OWN != 1);
    }

	while (byte_cnt && !desc[tail].OWN) {
		desc[tail].rsvd1 = 0;
		desc[tail].size = SLIC_BUF_SIZE;
		if (byte_cnt >= SLIC_BUF_SIZE) {
			desc[tail].length = SLIC_BUF_SIZE;
			byte_cnt -= SLIC_BUF_SIZE;
			desc[tail].EOM = 0;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
			desc[tail].EOM = 0;
		}
		desc[tail].rsvd2 = 0;
		retval =
		    copy_from_user(scbuf[tail].bf_vaddr, buf + offset,
				   SLIC_BUF_SIZE);
		if (retval)
			return retval;
        dma_cache_sync(NULL, scbuf[tail].bf_vaddr, desc[tail].length, DMA_TO_DEVICE);
		desc[tail].BufPtr = (unsigned int) scbuf[tail].bf_paddr;
		desc[tail].rsvd3 = 0;
		desc[tail].OWN = 1;

		tail = next_tail(tail);
		offset += SLIC_BUF_SIZE;
	}

	dmabuf->tail = tail;

	if (need_start) {
		ar7240_slic_dma_start((unsigned long) desc_p, mode);
	} else if (!sc->ppause) {
		ar7240_slic_dma_resume(mode);
	}

	return count - byte_cnt;
}


int ar7240_slic_close(struct inode *inode, struct file *filp)
{
	int j, own, mode;
	ar7240_slic_softc_t *sc = &sc_buf_var;
	slic_dma_buf_t *dmabuf;
	ar7240_mbox_dma_desc *desc;

	if (filp->f_mode & FMODE_READ) {
		dmabuf = &sc->sc_rbuf;
		own = sc->rpause;
		mode = 1;
	} else {
		dmabuf = &sc->sc_pbuf;
		own = sc->ppause;
		mode = 0;
	}

	desc = dmabuf->db_desc;
	if (own) {
		for (j = 0; j < NUM_DESC; j++) {
			desc[j].OWN = 0;
		}
		ar7240_slic_dma_resume(mode);
	} else { 
		for (j = 0; j < NUM_DESC; j++) {
			while (desc[j].OWN) {
				schedule_timeout_interruptible(HZ);
                
			}
		}
	}

    here();
	for (j = 0; j < NUM_DESC; j++) {
		dma_unmap_single(NULL, dmabuf->db_buf[j].bf_paddr,
				 SLIC_BUF_SIZE, DMA_BIDIRECTIONAL);
	}

	if (filp->f_mode & FMODE_READ)
		kfree(sc->sc_rmall_buf);
	else
		kfree(sc->sc_pmall_buf);

	dma_free_coherent(NULL,
			  NUM_DESC * sizeof(ar7240_mbox_dma_desc),
			  dmabuf->db_desc, dmabuf->db_desc_p);

    here();
	if (filp->f_mode & FMODE_READ) {
		sc->ropened = 0;
		sc->rpause = 0;
	} else {
		sc->popened = 0;
		sc->ppause = 0;
	}

    here();

	return (0);
}

int ar7240_slic_release(struct inode *inode, struct file *filp)
{
	printk(KERN_CRIT "release\n");
	return 0;
}

int ar7240_slic_ioctl(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
#ifndef CONFIG_WASP_SUPPORT
	int data, mask = 0;
	struct ar7240_slic_softc *sc = &sc_buf_var;

	switch (cmd) {
    case SLIC_PAUSE:
        data = arg;
	ar7240_slic_dma_pause(data);
	if (data) {
		sc->rpause = 1;
	} else {
		sc->ppause = 1;
	}
        return 0;
    case SLIC_RESUME:
        data = arg;
	ar7240_slic_dma_resume(data);
	if (data) {
		sc->rpause = 0;
	} else {
		sc->ppause = 0;
	}
        return 0;
	case SLIC_VOLUME:
		data = arg;
		if (data < 15) {
			if (data < 0) {
				mask = 0xf;
			} else {
				mask = (~data) & 0xf;
				mask = mask | 0x10;
			}
		} else {
			if (data <= 22) {
				if (data == 15) {
					data = 0;
				} else {
					mask = data - 15;
				}
			} else {
				mask = 7;
			}
		}
		data = mask | (mask << 8);
		ar7240_reg_wr(STEREO0_VOLUME, data);
		return 0;

	case SLIC_FREQ:		/* Frequency settings */
		data = arg;
#if 0
        switch (data) {
            case 44100:
                ar7242_slic_clk(0x0a47f028, 0x2383);
                break;
            case 48000:
                ar7242_slic_clk(0x03c9f02c, 0x2383);
                break;
            default:
                printk(KERN_CRIT "Freq %d not supported \n",
                   data);
                return -ENOTSUPP;
        }
#endif
		return 0;

	case SLIC_FINE:
		data = arg;
		return 0;

	case SLIC_DSIZE:
		data = arg;
		switch (data) {
		case 8:
            ar7240_reg_wr(AR7240_STEREO_CONFIG,
                (AR7240_STEREO_CONFIG_SPDIF_ENABLE |
                AR7240_STEREO_CONFIG_ENABLE |
                AR7240_STEREO_CONFIG_RESET |
                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_8B) |
                AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE |
                AR7240_STEREO_CONFIG_MASTER |
                AR7240_STEREO_CONFIG_PSEDGE(2)));
            udelay(100);
            ar7240_reg_wr(AR7240_STEREO_CONFIG,
                (AR7240_STEREO_CONFIG_SPDIF_ENABLE |
                AR7240_STEREO_CONFIG_ENABLE |
                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_8B) |
                AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE |
                AR7240_STEREO_CONFIG_MASTER |
                AR7240_STEREO_CONFIG_PSEDGE(2)));
			return 0;
		case 16:
			/*default settings. Nothing to do */
			//ar7240_reg_wr(AR7240_STEREO_CONFIG, 0xa21302);
            ar7240_reg_wr(AR7240_STEREO_CONFIG,
                (AR7240_STEREO_CONFIG_SPDIF_ENABLE |
                AR7240_STEREO_CONFIG_ENABLE |
                AR7240_STEREO_CONFIG_RESET |
                AR7240_STEREO_CONFIG_PCM_SWAP |
                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_16B) |
                AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE |
                AR7240_STEREO_CONFIG_MASTER |
                AR7240_STEREO_CONFIG_PSEDGE(2)));
            udelay(100);
            ar7240_reg_wr(AR7240_STEREO_CONFIG,
                (AR7240_STEREO_CONFIG_SPDIF_ENABLE |
                AR7240_STEREO_CONFIG_ENABLE |
                AR7240_STEREO_CONFIG_PCM_SWAP |
                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_16B) |
                AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE |
                AR7240_STEREO_CONFIG_MASTER |
                AR7240_STEREO_CONFIG_PSEDGE(2)));
			return 0;
		case 24:
			//ar7240_reg_wr(AR7240_STEREO_CONFIG, 0xa02b02);
            ar7240_reg_wr(AR7240_STEREO_CONFIG,
                (AR7240_STEREO_CONFIG_SPDIF_ENABLE |
                AR7240_STEREO_CONFIG_ENABLE |
                AR7240_STEREO_CONFIG_RESET |
                AR7240_STEREO_CONFIG_PCM_SWAP |
                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_24B) |
                AR7240_STEREO_CONFIG_SLIC_32B_WORD |    
                AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE |
                AR7240_STEREO_CONFIG_MASTER |
                AR7240_STEREO_CONFIG_PSEDGE(2)));
            udelay(100);
            ar7240_reg_wr(AR7240_STEREO_CONFIG,
                (AR7240_STEREO_CONFIG_SPDIF_ENABLE |
                AR7240_STEREO_CONFIG_ENABLE |
                AR7240_STEREO_CONFIG_PCM_SWAP |
                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_24B) |
                AR7240_STEREO_CONFIG_SLIC_32B_WORD |    
                AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE |
                AR7240_STEREO_CONFIG_MASTER |
                AR7240_STEREO_CONFIG_PSEDGE(2)));
			return 0;
		case 32:
			//ar7240_reg_wr(AR7240_STEREO_CONFIG, 0xa02b02);
            ar7240_reg_wr(AR7240_STEREO_CONFIG,
                (AR7240_STEREO_CONFIG_SPDIF_ENABLE |
                AR7240_STEREO_CONFIG_ENABLE |
                AR7240_STEREO_CONFIG_RESET |
                AR7240_STEREO_CONFIG_PCM_SWAP |
                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_32B) |
                AR7240_STEREO_CONFIG_SLIC_32B_WORD |    
                AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE |
                AR7240_STEREO_CONFIG_MASTER |
                AR7240_STEREO_CONFIG_PSEDGE(2)));
            udelay(100);
            ar7240_reg_wr(AR7240_STEREO_CONFIG,
                (AR7240_STEREO_CONFIG_SPDIF_ENABLE |
                AR7240_STEREO_CONFIG_ENABLE |

                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_32B) |
                AR7240_STEREO_CONFIG_SLIC_32B_WORD |    
                AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE |
                AR7240_STEREO_CONFIG_MASTER |
                AR7240_STEREO_CONFIG_PSEDGE(2)));
			return 0;
		default:
			printk(KERN_CRIT "Data size %d not supported \n",
			       data);
			return -ENOTSUPP;
		}

	case SLIC_MODE:		/* mono or stereo */
		data = arg;
	    /* For MONO */
		if (data != 2) {
	        ar7240_reg_rmw_set(AR7240_STEREO_CONFIG, MONO);      
        } else {
	        ar7240_reg_rmw_clear(AR7240_STEREO_CONFIG, MONO);      
        }
		/*Otherwise default settings. Nothing to do */
		return 0;
	case SLIC_COUNT:
		data = arg;
		return 0;

	default:
		return -ENOTSUPP;
	}
#endif
    return 0;
}


irqreturn_t ar7240_slic_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	uint32_t r;

	r = ar7240_reg_rd(AR7240_MBOX_SLIC_INT_STATUS);

    if (r & AR7240_MBOX_SLIC_RX_DMA_COMPLETE) {
        wake_up_interruptible(&wq_rx);
    }
    if (r & AR7240_MBOX_SLIC_TX_DMA_COMPLETE) {
        wake_up_interruptible(&wq_tx);
    }

	/* Ack the interrupts */
	ar7240_reg_wr(AR7240_MBOX_SLIC_INT_STATUS, r);

	return IRQ_HANDLED;
}

/* The initialization sequence carries hardcoded values.
 * Register descriptions are to be defined and updated.
 * TODO list.
 */
void ar7240_slic_link_on(int master)
{
    // Reset SLIC controller
    ar7240_reset(AR7240_RESET_SLIC);
    udelay(1000);

    // Disable JTAG for SLIC functionality
    ar7240_reg_wr(AR7240_GPIO_FUNCTION, 
                    AR7240_GPIO_FUNCTION_JTAG_DISABLE);

    // Program GPIO for SPI MISO and UART SIN
    ar7240_reg_wr(AR7240_GPIO_IN_ENABLE0, 
                    (AR7240_GPIO_IN_ENABLE0_SPI_DATA_IN(6) |
                     AR7240_GPIO_IN_ENABLE0_UART_SIN(9)));

    // Program GPIO 7
    ar7240_reg_wr(AR7240_GPIO_OUT_FUNCTION1, 
                    AR7240_GPIO_OUT_FUNCTION1_ENABLE_GPIO_7(0xa));

    // Program GPIO 8 and GPIO 10
    ar7240_reg_wr(AR7240_GPIO_OUT_FUNCTION2, 
                    (AR7240_GPIO_OUT_FUNCTION2_ENABLE_GPIO_10(0x18) | 
                     AR7240_GPIO_OUT_FUNCTION2_ENABLE_GPIO_8(7)));

    // Program GPIO for SLIC Data In
    ar7240_reg_wr(AR7240_GPIO_IN_ENABLE4, 
                    AR7240_GPIO_IN_ENABLE4_SLIC_DATA_IN(0xB));

    // Enable GPIO 13, 14 and 15
    ar7240_reg_wr(AR7240_GPIO_OUT_FUNCTION3, 
                    (AR7240_GPIO_OUT_FUNCTION3_ENABLE_GPIO_12(0xB) |
                     AR7240_GPIO_OUT_FUNCTION3_ENABLE_GPIO_13(5)   |
                     AR7240_GPIO_OUT_FUNCTION3_ENABLE_GPIO_14(4)));

    // Enable General Purpose I/O register
    ar7240_reg_wr(AR7240_GPIO_OE, AR7240_GPIO_OE_EN(0x18a43));

    // Stereo Config Register
    ar7240_reg_wr(AR7240_STEREO_CONFIG, 
                    (AR7240_STEREO_CONFIG_RESET |
                     AR7240_STEREO_CONFIG_MCK_SEL));

    //ar7240_reg_wr(AR7240_SLIC_CLOCK_CTRL, 0xf); //divider value of 25
    ar7240_reg_wr(AR7240_SLIC_CLOCK_CTRL, 0x0); //divider value of 25

    // Slic Timing Control
    //ar7240_reg_wr(AR7240_SLIC_TIMING_CTRL, 0x843);
    ar7240_reg_wr(AR7240_SLIC_TIMING_CTRL, 
                    (AR7240_SLIC_TIMING_CTRL_RXDATA_SAMPLE_POS_EXTEND | 
                    AR7240_SLIC_TIMING_CTRL_TXDATA_FS_SYNC(0x2) |
                    AR7240_SLIC_TIMING_CTRL_FS_POS |
                    AR7240_SLIC_TIMING_CTRL_LONG_FS));

    // Enable 32 Slots
    ar7240_reg_wr(AR7240_SLIC_SLOT, 0x20); //total no.of slots=16

    // Activate Slot 1 and Slot 2
    ar7240_reg_wr(AR7240_SLIC_TX_SLOTS1, 0x3); 
    ar7240_reg_wr(AR7240_SLIC_RX_SLOTS1, 0x3); 

    // Program SLIC Controller
    ar7240_reg_wr(AR7240_SLIC_CTRL, 
                    (AR7240_SLIC_CTRL_CLK_EN |
                     AR7240_SLIC_CTRL_MASTER |
                     AR7240_SLIC_CTRL_EN));
}

void ar7240_slic_request_dma_channel(int mode)
{
	ar7240_reg_wr(AR7240_MBOX_SLIC_DMA_POLICY, 
                    (AR7240_MBOX_DMA_POLICY_RX_QUANTUM |
                     AR7240_MBOX_DMA_POLICY_RX_QUANTUM |
                     AR7240_MBOX_DMA_POLICY_TX_FIFO_THRESH(4)));
}

void ar7240_slic_dma_start(unsigned long desc_buf_p, int mode)
{
	/*
	 * Program the device to generate interrupts
	 * RX_DMA_COMPLETE for mbox 0
	 */
	if (mode) {
        printk("Tx Desc Base 0x%08x\n", desc_buf_p);
		ar7240_reg_wr(AR7240_MBOX_DMA_TX_DESCRIPTOR_BASE1, desc_buf_p);
		ar7240_reg_wr(AR7240_MBOX_DMA_TX_CONTROL1, START);
	} else {
        printk("Rx Desc Base 0x%08x\n", desc_buf_p);
		ar7240_reg_wr(AR7240_MBOX_DMA_RX_DESCRIPTOR_BASE1, desc_buf_p);
		ar7240_reg_wr(AR7240_MBOX_DMA_RX_CONTROL1, START);
	}
}

void ar7240_slic_dma_pause(int mode)
{
    /*
     * Pause
     */
    if (mode) {
            ar7240_reg_wr(AR7240_MBOX_DMA_TX_CONTROL1, PAUSE);
    } else {
            ar7240_reg_wr(AR7240_MBOX_DMA_RX_CONTROL1, PAUSE);
    }
}

void ar7240_slic_dma_resume(int mode)
{
    /*
     * Resume
     */
    if (mode) {
            ar7240_reg_wr(AR7240_MBOX_DMA_TX_CONTROL1, RESUME);
    } else {
            ar7240_reg_wr(AR7240_MBOX_DMA_RX_CONTROL1, RESUME);
    }
}

void ar7242_slic_clk(unsigned long frac, unsigned long pll)
{
    /*
     * Tick...Tick...Tick
     * ar7240_reg_wr(FRAC_FREQ, frac);
     * ar7240_reg_wr(AUDIO_PLL, pll);
     */
}

loff_t ar7240_slic_llseek(struct file *filp, loff_t off, int whence)
{
	printk(KERN_CRIT "llseek\n");
	return off;
}


struct file_operations ar7240_slic_fops = {
	.owner = THIS_MODULE,
	.llseek = ar7240_slic_llseek,
	.read = ar7240_slic_read,
	.write = ar7240_slic_write,
	.ioctl = ar7240_slic_ioctl,
	.open = ar7240_slic_open,
	.release = ar7240_slic_close,
};

void ar7240_slic_cleanup_module(void)
{
	ar7240_slic_softc_t *sc = &sc_buf_var;

	printk(KERN_CRIT "unregister\n");

	free_irq(sc->sc_irq, NULL);
	unregister_chrdev(ar7240_slic_major, "ath_slic");
}

int ar7240_slic_init_module(void)
{
	ar7240_slic_softc_t *sc = &sc_buf_var;
	int result = -1;
	int master = 1;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (ar7240_slic_major) {
		result =
		    register_chrdev(ar7240_slic_major, "ath_slic",
				    &ar7240_slic_fops);
	}
	if (result < 0) {
		printk(KERN_WARNING "ar7240_slic: can't get major %d\n",
		       ar7240_slic_major);
		return result;
	}

	sc->sc_irq = AR7240_MISC_IRQ_DMA;

	/* Establish ISR would take care of enabling the interrupt */
	result = request_irq(sc->sc_irq, ar7240_slic_intr, IRQF_DISABLED,
			     "ar7240_slic", NULL);
	if (result) {
		printk(KERN_INFO
		       "slic: can't get assigned irq %d returns %d\n",
		       sc->sc_irq, result);
	}

	ar7240_slic_link_on(master);

	SLIC_LOCK_INIT(&sc_buf_var);

	return 0;		/* succeed */
}

module_init(ar7240_slic_init_module);
module_exit(ar7240_slic_cleanup_module);
