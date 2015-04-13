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
 * =====================================================================================
 *
 *       Filename:  i2sconf.c
 *
 *    Description:  Application to control i2s device
 *
 *        Version:  1.0
 *        Created:  Tuesday 23 March 2010 04:20:46  IST
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *        Company:  
 *
 * =====================================================================================
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
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>

#include "i2sio.h"

char *audev = "/dev/i2s";


void usage(void)
{
	printf("i2sconf -hmM\n");
	printf("-h : display help message\n");
	printf("-m <1/0> : Enable/Disable external master clock\n");
	printf("-f <freq> : Set the PLL config and posedge \n");
}

/* Set frequency function to calcualte the PLL divider values and the int and frac.
 * For MCLK derivation, codec is taken as the reference */
int ath_i2s_set_freq(unsigned int data, ushort i2s_ws, ushort num_chan, pll_t *my_audio_pll)
{
	float mclk, VCO, int_frac, frac, temp_float;
	unsigned int fs, mf, post_pll, post_pll_count = 0, in, i, temp_int, ext_div;
	unsigned int frac_new, temp_mult = 1, result = 0;
	unsigned int target_pll_int = 0, target_pll_frac = 0;
	unsigned int ulimit = VCO_ULIMIT, llimit = VCO_LLIMIT, ref_clk = REF_CLK, ref_div = REF_DIV;
	unsigned int vco_found = 0;
	unsigned long audio_pll = 0, target_pll = 0;
	float i2s_sck, div;
	int int_div, psedge, ps_div, ws;

	/* Codec specific table CS 4270 */
	unsigned int mdiv_count = 0;
	int mult_table[MAX_SPEEDS][MAX_MDIVS] = {{256, 384, 512, 1024},
                                {128, 192, 256, 512},
                                {64, 96, 128, 256}};
	int speed;

	fs = data;
	/* This part is based on the codec CS4270. Refer to
	codec's datasheet Table 2.*/
	/* Assume mdiv1 = 0 mdiv 1 = 0 */
	if (fs >= SINGLE_LLIMIT && fs < SINGLE_ULIMIT)
		speed = SINGLE;
	else if (fs >= SINGLE_ULIMIT && fs < DOUBLE_ULIMIT)
		speed = DOUBLE;
	else if (fs >= DOUBLE_ULIMIT && fs < QUAD_ULIMIT)
		speed = QUAD;
	else {
		printf("Freq %d not supported \n",
               data);
		exit(-1);
	}
psedge_recalc:
	while (mdiv_count < MAX_MDIVS) {
		mf = mult_table[speed][mdiv_count];
		/* Starting point of the calcualtion. Finding the MCLK. This MCLK derivation is based
		* on the codec, since the multiplication factor for different sampling frequencies
		* are obtained from the codecs datasheet */
		mclk = ((float)(mf * fs)/ (float) (MHZ));
		/* Finding the Dividers. ext_div is a 3 bit number which has to be even, hence, 2, 4 & 6.
		* The constraint set by design */
		for (ext_div = EXT_DIV_MIN; ext_div <= EXT_DIV_MAX; ext_div += 2) {
			post_pll = 1;
			post_pll_count = 0;
			/* post_pll is a 3 bit number that has to be less than 5, hence, 1, 2, 4, 8 & 16. Post
			PLL divider is (2 ^ post pll). Hence a max divider value of 32 is supported */
			while (post_pll < POST_PLLDIV_MAX) {
				/* Refer Datasheet for the formulae */
				VCO = mclk * post_pll * ext_div;
				/* Calculate int and frac only if VCO freq falls within the Valid range for
				* an ext_div and post_pll_div combination. If not move to the next divider
				* values */
				if (VCO > llimit && VCO < ulimit) {
					int_frac=(VCO*ref_div)/ref_clk;
					in=(int)int_frac;
					frac=int_frac - in;
					if(in<64) {
						/* Convert FRAC to 18 bit precision */
						vco_found = 1;
						temp_float=frac;
						for(i=1;i<=18;i++) {
							temp_float=2*temp_float;
							temp_int=(int)temp_float;
							temp_float=temp_float-temp_int;
							result = result | temp_int;
							result = result << 1;
						}
						result = result >> 1;
						target_pll_int = (in << AUDIO_PLL_MODULATION_TGT_DIV_INT_LSB);
						target_pll_frac = (result << AUDIO_PLL_MODULATION_TGT_DIV_FRAC_LSB);
						target_pll = (target_pll_int | target_pll_frac);
					}
				}
				/* Break if VCO falls within limits */
				if (vco_found == 1)
					break;
				else {
					post_pll = post_pll << 1;
					post_pll_count++;
				}
			}

			if (vco_found == 1)
				break;
		}
	
		/* VCO frequncy does not fall within limits for any ext_div and
		* post_pll_div combination with the mentioned constraints.
		* Hence Change MCLK by changing the mdiv values and recalculate
		* VCO, int, frac and divider values */
		if (vco_found == 0) {
			printf("Default MDIV does not match VCO criteria \n");
			printf("Change mdiv values to index %d \n", (mdiv_count + 1));
			mdiv_count++;
		}
		else
			break;
	}

	/* No values of mdiv and dividers match VCO constraints */
	if (vco_found == 0)
		printf("VCO does not fall within limits \n");
	audio_pll = ((ext_div << AUDIO_PLL_CONFIG_EXT_DIV_LSB) |
                     (post_pll_count << AUDIO_PLL_CONFIG_POSTPLLDIV_LSB) |
                     (ref_div << AUDIO_PLL_CONFIG_REFDIV_LSB));

	my_audio_pll->audio_pll = audio_pll;
	my_audio_pll->target_pll = target_pll;

	/* I2S SCLK and Posedge calculation */
	if (i2s_ws == DWORD_SIZE_32 || i2s_ws == DWORD_SIZE_24) {
		ws = I2S_WORD_SIZE_32;
		ps_div = SCLK_DIV_2;
	}
	else {
		ws = I2S_WORD_SIZE_16;
		ps_div = SCLK_DIV_4;
	}

	i2s_sck = ((float)(data * ws * num_chan)/(float) (MHZ));
	div = (mclk/i2s_sck);
	int_div = (int)div;
	psedge = (int_div/ps_div);
	/* Posedge cannot be Zero. Repeat calc with new mdivs */
	if ( psedge == 0 ) {
		printf("Posege is zero. Change mdiv to next index \n");
		mdiv_count++;
		vco_found = 0;
		goto psedge_recalc;
	}
	my_audio_pll->psedge=psedge;

	return 0;
}

                                                                
/* Change values here and pass to driver. Hardcoded */
void set_vuc_data(int write)
{
	int i = 0;
	if (write)
		vuc_data.write = 1;
	else
		vuc_data.write = 0;

	if (write) {
		for (i= 0 ; i < 6; i++) {
			vuc_data.va[i] = 0xaaaa;
			vuc_data.ua[i] = 0xbbbb;
			vuc_data.ca[i] = 0xcccc;
			vuc_data.vb[i] = 0xdddd;
			vuc_data.ub[i] = 0xeeee;
			vuc_data.cb[i] = 0xffff;
		}
	}
	

}

int main (int argc, char *argv[])
{
	int audio;  
	int mclk_sel = 0;
	int freq = 0, i, bit_swap, byte_swap, volume, write, num_bits = REC_DEF_MODE_16;

	int fd;         /* The file descriptor */
	int optc;       /* For getopt */
	int ret;
	unsigned int addr;
	pll_t            new_audio_pll;


	audio = open(audev, O_WRONLY);

	if (audio < 0) {
		exit (-1);
	}

	while ((optc = getopt(argc, argv, "hHm:n:f:b:B:v:S:")) != -1) {
		switch (optc) {
			case 'm':  
				mclk_sel = atoi(optarg)?1:0;
				if (ioctl(audio, I2S_MCLK, mclk_sel) < 0) {
					perror("I2S_MCLK");
				}                    
				break;
			case 'n':
				num_bits = atoi(optarg);
				break;
			case 'f':
				freq = atoi(optarg);
				 /* Calculate PLL dividers and int frac values */
			        ath_i2s_set_freq(freq, num_bits, WAV_STEREO, &new_audio_pll);
				if (ioctl(audio, I2S_FREQ, &new_audio_pll) < 0) {
        			        perror("I2S_FREQ");
			     	}
				break;
			case 'b':
				bit_swap = atoi(optarg)?1:0;
				if (ioctl(audio, I2S_BITSWAP, bit_swap) < 0) {
					perror("I2S_BITSWAP");
				}
				break;
			case 'B':
				byte_swap = atoi(optarg)?1:0;
				if (ioctl(audio, I2S_BYTESWAP, byte_swap) < 0) {
					perror("I2S_BYTESWAP");
				}
				break;
			case 'v' :
				volume = atoi(optarg);
				if (ioctl(audio, I2S_VOLUME, volume) < 0) {
						perror("I2S_VOLUME");
				}
				break;
			case 'S':
				write = atoi(optarg);
				set_vuc_data(write);
				if (ioctl(audio, I2S_SPDIF_VUC, &vuc_data) < 0) {
					perror("I2S_VUC");
				}
				if(!write) {
					/* Read vuc regs */
					for (i= 0 ; i < 6; i++) {
						printf("vuc_data.va[i] = %x \n", vuc_data.va[i]);
						printf("vuc_data.ua[i] = %x \n", vuc_data.ua[i]);
						printf("vuc_data.ca[i] = %x \n", vuc_data.ca[i]);
						printf("vuc_data.vb[i] = %x \n", vuc_data.vb[i]);
						printf("vuc_data.ub[i] = %x \n", vuc_data.ub[i]);
						printf("vuc_data.cb[i] = %x \n", vuc_data.cb[i]);
					}

				}
				break;

			case 'h':
			case 'H':
				usage();
				break;
			default:   
				break;
		}
	}

rep:
	ret = close(audio);
	if (errno == EAGAIN) {
		goto rep;
	}
	return 0;
}
