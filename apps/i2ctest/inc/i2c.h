/*
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
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


#include <stdio.h>

/* IOCTL Commands */
#define I2C_MAGIC        121
#define I2C_SET_CLK      _IOW(I2C_MAGIC, 1, int)
#define I2C_SET_ADDRMODE _IOW(I2C_MAGIC, 2, int)

#define M24256_ADDRESS 0x50
#define MAX_DATA 4096

#define _I2C_CLK_100MHZ 0x10
#define _I2C_CLK_40MHZ  0x20

#define IC_HS_MODE_100MHZ 1
#define IC_FS_MODE_100MHZ 2
#define IC_SS_MODE_100MHZ 3
#define IC_HS_MODE_40MHZ 4
#define IC_FS_MODE_40MHZ 5
#define IC_SS_MODE_40MHZ 6

/* I2C param structure which contains the target
 * slave address, the number of bytes of offset, the offset,
 * number of bytes of data and data */
typedef struct ath_i2c_rw_t {
        unsigned short  slave;                                                         /* Slave-address */
        unsigned long   offset_size;
        unsigned char   *offset;
        unsigned long   data_size;                                                      /* Data size */
        unsigned char   *data;                                                          /* Data */
} i2c_rw_t;

