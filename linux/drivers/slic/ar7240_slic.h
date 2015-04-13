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

#define NUM_DESC        4
#define SLIC_BUF_SIZE   128

#define MASTER          1

#define PAUSE           0x1
#define START           0x2
#define RESUME          0x4

#define AR7240_MBOX_SLIC_RX_DMA_COMPLETE       (1ul << 6)
#define AR7240_MBOX_SLIC_TX_DMA_COMPLETE       (1ul << 4)
#define AR7240_MBOX_SLIC_TX_OVERFLOW           (1ul << 3)
#define AR7240_MBOX_SLIC_RX_UNDERFLOW          (1ul << 2)
#define AR7240_MBOX_SLIC_TX_NOT_EMPTY          (1ul << 1)
#define AR7240_MBOX_SLIC_RX_NOT_FULL           (1ul << 0)

#define SLIC_LOCK_INIT(_sc)      spin_lock_init(&(_sc)->slic_lock)
#define SLIC_LOCK_DESTROY(_sc)
#define SLIC_LOCK(_sc)           spin_lock_irqsave(&(_sc)->slic_lock, (_sc)->slic_lockflags)
#define SLIC_UNLOCK(_sc)         spin_unlock_irqrestore(&(_sc)->slic_lock, (_sc)->slic_lockflags)

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
} ar7240_mbox_dma_desc;

typedef struct slic_buf {
    uint8_t *bf_vaddr;
    unsigned long bf_paddr;
} slic_buf_t;

typedef struct slic_dma_buf {
    ar7240_mbox_dma_desc *lastbuf;
    ar7240_mbox_dma_desc *db_desc;
    dma_addr_t db_desc_p;
    slic_buf_t db_buf[NUM_DESC];
    int tail;
} slic_dma_buf_t;

typedef struct ar7240_slic_softc {
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
} ar7240_slic_softc_t;

ar7240_slic_softc_t sc_buf_var;

