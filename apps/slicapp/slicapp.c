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

/*
 * Utility for transmitting and receiving data using
 * QCA SLIC device
 * Written by Mughilan
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>

/*For SRAM access */
#define USE_SRAM
/* Uncomment this for SRAM. Strictly use
 * only for Scorpion */
#undef USE_SRAM

#ifndef USE_SRAM
#define NUM_DESC          384
#define SLIC_BUF_SIZE     2048

#else
/* For SRAM */
#define NUM_DESC                4
#define SLIC_BUF_SIZE           128
#endif

#define SLIC_BITSWAP     _IOW('N', 0x23, int)
#define SLIC_BYTESWAP    _IOW('N', 0x24, int)

char *audev = "/dev/slic";
int audio, fine, dbg, receiver = 0; 
int valfix = -1, master = 0, incr = 0, loop = 0; /* Audio parameters */
int max_slots = 0xff, slots_en = 8;
int buff_size = 0;


/* Receive function to read the data from SLIC and compare it 
 * with a pre-deteremined pattern */
int
receive (int fd)
{
	int     loop_cnt=0,check_zero ;
	unsigned int  tmpval=0,ret=0,count=0,tmpcount,zero_count,zero_boundary,pass_count,slot_error,i;
	char        *audiodata, *data , *paudiodata;
	u_char      *tmp;
	int bufsz = 0;
	
	if (fd < 0) {
		return EINVAL;
	}

	bufsz = buff_size;
	audiodata = (char *) malloc (bufsz * sizeof (char));
	if (audiodata == NULL) {
		return ENOMEM;
	}
	paudiodata = audiodata ;

	do {
		count = bufsz;
		i = 0 ;
eagain:
		tmpcount = count;
		do {
			/* Read tmpcount bytes from SLIC */
			ret = read (audio, audiodata, tmpcount);

			if (ret < 0 && errno == EAGAIN) {
				printf("receive %d, error %d \n", __LINE__, ret);
				goto eagain;
			}

			tmpcount = tmpcount - ret;
			i += ret;
			audiodata += ret ;
		} while (tmpcount) ;

		audiodata = paudiodata ;

		check_zero = 1 ;
		zero_count = zero_boundary = pass_count = slot_error = 0;

		/* Pattern Comparison Logic */
		while (tmpcount < i) {
			tmpval = ((((tmpcount%slots_en)+1) | (((tmpcount%slots_en)+1) << 4)) & 0xff);
			if ((audiodata[tmpcount] & 0xff) == tmpval) {
				pass_count++;
			} else {
				if (!(audiodata[tmpcount])) {
					if (!zero_boundary) {
						zero_boundary = tmpcount;
					}
					zero_count++;
					if (zero_count == 1) {
						printf("Zero: RxData[%d] %x, ExpData %x\n", tmpcount, (audiodata[tmpcount] & 0xff), tmpval);
					}
				} else {
					slot_error++;
					if (slot_error <= 5) {
						printf("Slot: RxData[%d] %x, ExpData %x\n", tmpcount, (audiodata[tmpcount] & 0xff), tmpval);
                    }
				}
			}
			tmpcount++;
		}
		if (master) {
		    printf("MASTER :Slot Error %d, Zero Count %d, Zero Boundary %d, Pass Count %d, Loop Count %d\n", 
				slot_error, zero_count, zero_boundary, pass_count, loop_cnt);
		} else {
		    printf("SLAVE :Slot Error %d, Zero Count %d, Zero Boundary %d, Pass Count %d, Loop Count %d\n", 
				slot_error, zero_count, zero_boundary, pass_count, loop_cnt);
		}


#ifdef DEBUG_INTEGRITY_MODE2
		while ( tmpcount < (i-1) ) {
			if ( check_zero && !(audiodata[tmpcount] & 0x7 ) && !(audiodata[tmpcount+1] & 0x7 )) {
				if (!zero_boundary) {
					zero_boundary = tmpcount;
				}
				tmpcount++ ;
				zero_count++;
			} else {
				if ( ((audiodata[tmpcount]  + 1) & 0x7 )  == ((audiodata[tmpcount + 1]) & 0x7)) {
					check_zero = 0;
				} else {
					if ( !(audiodata[tmpcount] & 0x7) && !(audiodata[tmpcount+1] & 0x7)) {
						check_zero=1;
						zero_count++;
					} else {
						printf( "Data  change . Is FIFO dat ??  audiodata : %0x audiodata[%d]: %0x %0x  \n",
							audiodata[tmpcount-1] & 0x7 , tmpcount,audiodata[tmpcount] & 0x7 , audiodata[tmpcount+1] & 0x7  ) ;
					}
				}
				tmpcount++;
			}
		}
		printf( "loop_cnt : %d zero_count %d, boundary %d\n", loop_cnt, zero_count, zero_boundary) ;
#endif
		audiodata = paudiodata ;
		loop_cnt++;
	} while(1);

	free(audiodata);

	return 0;
}

/* Send data to the SLIC. The pattern to be sent is filled up by
 * the driver */
int
transmit (int fd)
{
	int		tmpcount, ret=0, count=0, i = 0, j=0, loop_cnt=0;
	char		*audiodata, *data;
	u_char		*tmp;
	int bufsz = 0;	

	if (fd < 0) {
		return EINVAL;
	}

	bufsz = buff_size;

	audiodata = (char *) malloc (bufsz * sizeof (char));
	if (audiodata == NULL) {
		return ENOMEM;
	}

	if (incr) {
		do {
			*(audiodata+i) = j++;
			if (j >= buff_size) {
				j = 0;
			}
			i++;
		} while (i < bufsz);
	}

	while (1) {
		count = bufsz;

eagain:
		tmpcount = count;
		data = audiodata;
		ret = 0;
		do {
			if (valfix != -1) {
				memset(data, valfix, tmpcount);
			}
			/* write count bytes to tmpcount */
			ret = write(audio, data, tmpcount);
			if (ret < 0 && errno == EAGAIN) {
				printf("%s:%d %d %d\n", __func__, __LINE__, ret, errno);
				goto eagain;
			}

			tmpcount = tmpcount - ret;
			data += ret;
		} while(tmpcount);

		loop_cnt++;

		if (loop && (loop_cnt >= loop)) {
			break;
		}
	}

	free (audiodata);

	return 0;
}

int
main (int argc, char *argv[])
{

	int	fd,		/* The file descriptor */
		optc,		/* For getopt */
		counter,
		i,
		ret,
		bit_swap = 0,
		bit_swap_mode,
		byte_swap = 0,
		byte_swap_mode,
		child;
	char option;
	struct stat buf;
	int ath_slic_buf_size;
	
	fine=-2;

	while ((optc = getopt (argc, argv, "rpm:i:v:l:s:S:b:B:")) != -1) {
		switch (optc) {
			case 'm': master = atoi (optarg); break;
			case 'i': incr = atoi (optarg); break;
			case 'v': valfix = atoi (optarg); break;
			case 'l': loop = atoi (optarg); break;
			case 'p': /* Turn on prints */
				  dbg = 1;
				  break;
			case 'r':
				  receiver = 1;
				  break;
			case 'S': max_slots = atoi(optarg);break;
			case 's': slots_en = atoi(optarg);break;
			case 'b':
				 bit_swap = 1;
				 bit_swap_mode = atoi(optarg)?1:0;
				 break;
			case 'B':
				byte_swap = 1;
				byte_swap_mode = atoi(optarg)?1:0;
				break;
			default: printf("Unknown option\n"); exit(-1);
		}
	}
	/* Added since there is a constraint that the total bytes
	 * has to be a integral multiple of num of slots and 32 */
	ath_slic_buf_size = (((SLIC_BUF_SIZE)/(32 * slots_en)) * (32 * slots_en));	
	buff_size = (NUM_DESC * ath_slic_buf_size);

	audio = open (audev, (receiver) ? O_RDONLY : O_WRONLY);

	if (audio < 0) {
		printf("Device %s opening failed\n", audev);
		exit (-1);
	}
	
	if (byte_swap) {
		if (ioctl(audio, SLIC_BYTESWAP, byte_swap_mode) < 0) {
			perror("SLIC_BYTESWAP");
		}
	}
	
	if (bit_swap) {
		if (ioctl(audio, SLIC_BITSWAP, bit_swap_mode) < 0) {
			perror("SLIC_BITSWAP");
		}
	}

	if (receiver) {
		if ((fd = open(
						argv[optind], O_CREAT | O_TRUNC | O_WRONLY
			      )) == -1) {
			perror(argv[optind]);
			exit(-1);
		}
	} else {
		if ((fd = open (argv[optind], O_RDONLY)) == -1) {
			perror(argv[optind]);
			exit(-1);
		}
	}

	if(receiver) {
		receive(fd);
	} else {
		transmit(fd);
	}
	close(fd);
rep:
	ret = close(audio);
	if (errno == EAGAIN) {
		printf("%s:%d %d %d\n", __func__, __LINE__, ret, errno);
		goto rep;
	}
	return 0;
}
