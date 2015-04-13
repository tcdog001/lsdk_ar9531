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


/* Utility to Write/to or read from an I2C device.
 * Supports burst mode operation. Required only for master 
 * since the master only initiates an I2C transaction */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "inc/i2c.h"

/* I2C File descriptor */
int fd;

/* i2c_write_single: Write a single byte of data in to the addr offset specified  
 *  Args:  
 *      slave     : Address of the slave to which write is to be performed 
 *      addr      : Offset address at which data is to be written
 *      addr_size : Number of bytes of offset 
 *      data      : Data to be written to the specified offset 
 *  Returns:
 *      0 on Success, -ve on failure */
int i2c_write_single(unsigned short slave, unsigned short addr, unsigned long addr_size, char data) 
{
	i2c_rw_t temp;
	int ret;

	temp.slave = slave;
	temp.offset_size = addr_size;
	temp.offset = (unsigned char *)&addr;
	temp.data_size = 1;
	temp.data = &data;

	ret = write(fd, &temp, sizeof(temp));
	return ret;
}

/* i2c_write_burst: Write burst data at the specified addr offset. The address
 *                  increment happens at the slave end
 *  Args:
 *      slave     : Address of the slave to which write is to be performed
 *      addr      : Offset address at which data is to be written
 *      addr_size : Number of bytes of offset
 *      *data     : Pointer to burst data
 *      len       : Number of bytes of data
 *  Returns:
 *      0 on Success, -ve on failure */
int i2c_write_burst (unsigned short slave, unsigned short addr, unsigned long addr_size, unsigned int *data, unsigned long len) 
{
        i2c_rw_t temp;
	int ret;
	unsigned char wr_data[MAX_DATA];

	int i;	
	
	for (i = 0; i < len; i++)
		wr_data[i] = ((*(data + i)) & 0xff);

	temp.slave = slave;
	temp.data_size = len;
	temp.data = wr_data;
	temp.offset_size = addr_size;
	temp.offset = (unsigned char *)&addr;
	ret = write(fd, &temp, sizeof(temp));
	return ret;
}

/* i2c_read_single: Read a single byte of data from the addr offset specified
 *  Args:
 *      slave     : Address of the slave from which read is to be performed
 *      addr      : Offset address from which data is to be read
 *      addr_size : Number of bytes of offset
 *      data      : Data to be read from the specified offset
 *  Returns:
 *      data on Success, -ve on failure */
char i2c_read_single(unsigned short slave, unsigned short addr, unsigned long addr_size) 
{
    i2c_rw_t temp;
	int ret;
	static char data;
	temp.slave = slave;
	temp.data_size = 1;
	temp.data = &data;
	temp.offset_size = addr_size;
	temp.offset = (unsigned char *)&addr;
	ret = read(fd, &temp, sizeof(temp));
	if (ret < 0 ) {
		return ret;
	}
	return data;
}

/* i2c_read_burst: Read burst data from the specified addr offset. The address
 *                  increment happens at the slave end
 *  Args:
 *      slave     : Address of the slave from which read is to be performed
 *      addr      : Offset address from which data is to be read
 *      addr_size : Number of bytes of offset
 *      *data     : Pointer to burst data
 *      len       : Number of bytes of data
 *  Returns:
 *      0 on Success, -ve on failure */
int i2c_read_burst (unsigned short slave, unsigned short addr, unsigned long addr_size, unsigned char *data, unsigned long len) 
{
	i2c_rw_t temp;
	int ret;
	int i;
	temp.slave = slave;
	temp.data_size = len;
	temp.data = data;
	temp.offset_size = addr_size;
	temp.offset = (unsigned char *)&addr;
	ret = read(fd, &temp, sizeof(temp));
	return ret;
}

int main(int argc, const char *argv[])
{
	int val = 0, cnt;
	unsigned int wr_data = 0;
	unsigned char rd_data;
	unsigned int wr_array[MAX_DATA];
	unsigned char rd_array[MAX_DATA];
	int option = 0;
	unsigned short slave_addr;
	int rw, clk_mode, rw_mode;
	int ret = 0;
	unsigned short addr_offset;
	unsigned long offset_size, count;
	int i;

	if (argc < 2) {
		printf("USAGE : %s <slave_addr> \n", argv[0]);
		ret = -EINVAL;
		return ret;
	}

	/* The slave address is the first argument passed
     * in the command line */	
	slave_addr = strtol(argv[1], NULL, 16);
	/* Slave could be 7bit or 10bit. Hence max possible
     * address is 0x3FF */
	if (slave_addr <= 0 || slave_addr > 0x3FF) {
		ret = -EINVAL;
		return ret;
	}
	
	/* Transaction type. Write/read */
	printf("0 --> Write \n 1 --> Read \n");
	scanf("%d", &rw);

	if (rw < 0 || rw > 1) {
		printf("Not an valid option. Enter 0 for Write and 1 for read \n");
		ret = -EINVAL;
		return ret;
	}
		
	/* Transaction mode. Single/Burst */
	printf("0 --> Single \n 1 --> Burst \n");
	scanf("%d", &rw_mode);

	if (rw_mode < 0 || rw_mode > 1) {
		printf("Not an valid option. Enter 0 for Single and 1 for burst \n");
		ret = -EINVAL;
		return ret;
	}	

	/* Clk and speed. Clock could be 100 MHz or  40 MHz.
     * Spped can be Slow, Fast or High */
	printf("Supported modes 1 to 6:\n");
	printf("1. I2C_HS_MODE_100MHZ \n");
	printf("2. I2C_FS_MODE_100MHZ \n");
	printf("3. I2C_SS_MODE_100MHZ \n");
	printf("4. I2C_HS_MODE_40MHZ  \n");
	printf("5. I2C_FS_MODE_40MHZ  \n");
	printf("6. I2C_SS_MODE_40MHZ  \n");
	
	scanf("%d", &clk_mode);

	if (clk_mode < 1 || clk_mode > 6) {
		printf("Not a valid option. Enter a value between 0 and 7 \n");
		ret = -EINVAL;
		return ret;
	}

	/* Open I2C device node and return a descriptor
     * to the I2C device */	
	fd = open("/dev/i2c", O_RDWR);
	if (fd == -1)
	{
		printf("Device open failed \n");
		ret = -ENODEV;
		return ret;
	}

	/* Configure I2C Clock. */	
	ret = ioctl(fd, I2C_SET_CLK, clk_mode);
	if (ret < 0) { 
		printf("Error setting clock \n");
		return ret;
	}	

	/* Get the required inputs from user. A typical 
     * transaction requires the following inputs.
     * Slave address: Entered through command line,
	 * offset size : number of bytes required to frame the
                     address offset ex: 2 for 16 bit
     * addr offset : The address offset to which write/read is
                     to be performed.
     * data size   : The number of data for read/write. 1 for a
				     single transaction and more than 1 for a burst
                     transaction
     * data        : Data to be written/read from the given addr 
                     offset */
     
	/* Number of bytes of addr offset */
	printf("Enter offset size \n");
	scanf("%d", &offset_size);

	/* Address offset from where data is to be 
     * written to or read from */
	printf("Enter addr offset \n");
	scanf("%hd", &addr_offset);
	
	/* Write transaction */
	if (rw == 0 ) {
		/* Single Write transaction */
		if (rw_mode == 0) {
			printf("Enter data to be written \n");
			scanf("%d", &wr_data);
			ret = i2c_write_single(slave_addr, addr_offset, offset_size, (wr_data & 0xff));
			if (ret < 0) {
				printf("Single Write error: Error code %d \n", ret);
			}
		}
		else {
		/* Burst Write */
			printf("Enter num of bytes to write \n");
			scanf("%ld", &count);
			printf("Enter %d data \n", count);
			for (i = 0; i < count; i++) {
				scanf("%d", &wr_array[i]);
			}
		    i2c_write_burst(slave_addr, addr_offset, offset_size, wr_array, count);
			if (ret < 0) {
                printf("Burst Write error: Error code %d \n", ret);
            }
		}
	}
	else {
		/* Single Read */
		if (rw_mode == 0) {
			ret = i2c_read_single(slave_addr, addr_offset, offset_size);
			if (ret < 0) {
                printf("Single Read error: Error code %d \n", ret);
            }
			else  {
				rd_data = ret;
				printf("Data read from offset %x = %d (0x%x) \n", addr_offset, rd_data, rd_data);
			}
		}
		/* Burst Read */
		else {
			printf("Enter num of data to read \n");
			scanf("%ld", &count);
			ret = i2c_read_burst(slave_addr, addr_offset, offset_size, rd_array, count);
			if (ret < 0) {
                printf("Burst Read error: Error code %d \n", ret);
            }
			else {
				for (i = 0; i < count; i++) 
	        		printf("Data %d = %d (0x%x)\n", i, rd_array[i], 
														rd_array[i]);
          	}
		}
	}
	if (ret < 0) 
		printf(" ***** I2C Failure ***** \n");
	else 
		printf(" ***** I2C Success ***** \n");
	
	return 0;
}


