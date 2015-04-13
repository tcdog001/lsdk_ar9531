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
 *Utility for playing and recording  wav file using
 *Atheros I2S device
 *Written by Jacob Philip
 * Rewritten by Varada ;)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
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

#define dp(...)	do { if (dbg) { fprintf(stderr, __VA_ARGS__); } } while(0)
#define ep(...)	do { fprintf(stderr, __VA_ARGS__); } while(0)

#if __BYTE_ORDER == __BIG_ENDIAN
#	if !defined(__NetBSD__)
#		include <byteswap.h>
#	else
#		include <sys/bswap.h>
#		define bswap_32 bswap32
#		define bswap_16 bswap16
#	endif
#endif

/* Globals */
int audio, bufsz, dbg, recorder = 0;
int valfix = -1;        /* Audio parameters */
/* IOCTL vairables */
char mic_in = 0, fine = 0;
int mclk_sel = 0, spdif_disable = 0;
unsigned int fine_freq;
/* Fill up record defaults */
int sample_freq = REC_DEF_FREQ_44, file_size = REC_DEF_FILE_SIZE;
short mode = REC_DEF_MODE_16, stereo_mono = REC_DEF_TYPE_STEREO;

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
					result = 0;
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

void signal_handler(sig)
        int sig;
{
   switch(sig) {
   case SIGHUP:
        break;
   case SIGTERM:
        exit(0);
        break;
   case SIGCHLD:
        exit(0);
        break;
    }
}


void pause_handler(int iSignal)
{
    signal(SIGUSR1, pause_handler);

    printf("Pausing.........\n");
    if (ioctl(audio, I2S_PAUSE, recorder) < 0) {
        perror("I2S_PAUSE");
    }
}


void resume_handler(int iSignal)
{
    signal(SIGUSR2, resume_handler);

    printf("Resuming.........\n");
    if (ioctl(audio, I2S_RESUME, recorder) < 0) {
        perror("I2S_RESUME");
    }
}


int
record (int fd)
{
    wavhead_t   hdr;
    pll_t audio_reg; 
    scdata_t    sc;
    int     ret=0, count=0, i=0, bufsz =0;
    char        *audiodata;
    u_char      *tmp;
    u_int byte_p_sec;
    u_short byte_p_spl;  
 
    if (fd < 0) {
        return EINVAL;
    }
    /*
     * Header needs to be formulated....
     * Refer WAV file format
     */

    byte_p_sec = (sample_freq * (mode / 8) * stereo_mono);
    byte_p_spl  = (stereo_mono * (mode /8));

    hdr.main_chunk = WAV_CHUNKID_RIFF;	// RIFF
    hdr.length = WAV_CHUNK_SIZE;
    hdr.chunk_type = WAV_FORMAT;	// WAVE
    hdr.sub_chunk = WAV_SUBCHUNK_ID;	// fmt
    hdr.sc_len = WAV_SUBCHUNK_SIZE;
    hdr.format = WAV_AUDIO_FORMAT;     // PCM
    hdr.modus = bswap_16(stereo_mono); //0x200; 
    hdr.sample_fq = bswap_32(sample_freq); //0x44ac0000;
    hdr.byte_p_sec = bswap_32(byte_p_sec); //0x10b10200;
    hdr.byte_p_spl = bswap_16(byte_p_spl); //0x400;
    hdr.bit_p_spl =  bswap_16(mode);       //0x1000;
    hdr.pad[0] = 0x0;
    hdr.pad[1] = 0x0;
    hdr.sc.data_chunk = WAV_DATA_CHUNKID;
    hdr.sc.data_length = bswap_32(file_size); //0xc0822700; // 0x00945228 to be tested for long hrs record

    write(fd, &hdr, sizeof (hdr));

#if __BYTE_ORDER == __BIG_ENDIAN
    hdr.length  = bswap_32(hdr.length);
    hdr.sc_len  = bswap_32(hdr.sc_len);
    hdr.format  = bswap_16(hdr.format);
    hdr.modus   = bswap_16(hdr.modus);
    hdr.sample_fq   = bswap_32(hdr.sample_fq);
    hdr.byte_p_sec  = bswap_32(hdr.byte_p_sec);
    hdr.byte_p_spl  = bswap_16(hdr.byte_p_spl);
    hdr.bit_p_spl   = bswap_16(hdr.bit_p_spl);
#endif

    /*
     * Refer to the comments in the declaration of the wavhead_t
     * structure.
     * Having a pointer and moving around would have been easier.
     * But, that results in unaligned reads for the 32bit integer
     * data resulting in core dump.  Hence...
     * -Varada
     */

    if (hdr.sc_len == WAV_SUBCHUNK_LEN_16) {
        tmp = &hdr.pad[0];
        lseek(fd, -2, SEEK_CUR);
    } else if (hdr.sc_len == WAV_SUBCHUNK_LEN_18) {
        tmp = &hdr.pad[2];
    } else {
        return EINVAL;
    }
    memcpy(&sc, tmp, sizeof(sc));

#if __BYTE_ORDER == __BIG_ENDIAN
    sc.data_chunk = bswap_32 (sc.data_chunk);
    sc.data_length = bswap_32 (sc.data_length);
#endif

    if (bufsz <= 0) {
        bufsz = BUFF_SIZE;
    }

    audiodata = (char *) malloc (bufsz * sizeof (char));
    if (audiodata == NULL) {
        return ENOMEM;
    }

    /* If 32 bit MIC IN is required */
    if (mic_in == 1) {
        ioctl(audio, I2S_MICIN, mic_in);
    }

    do {
        /*
         * Bug#:    26972
         * The byte stream after the `.wav' header could have
         * additional data (like author, album etc...) apart
         * from the actual `audio data'.  Hence, ensure that
         * extra stuff is not written to the device.  Stop at
         * wherever the audio data ends.
         *  +--------+----------------------+--------+
         *  | header | audio data . . . . . | extras |
         *  +--------+----------------------+--------+
         */
        count = bufsz;
        /* For the transmission of last buffer */
        if ((i + count) > sc.data_length) {
            count = (sc.data_length - i);
        }
eagain:
        /* Read count bytes from I2S interface */
        ret = read (audio, audiodata, count);
        if (ret == -ERESTART) {
            printf("record %d, error %d \n", __LINE__, ret);
            goto eagain;
        }

        /* Write the bytes read from I2S to a file */
        if ((write(fd, audiodata, ret)) < 0)  {
            printf("record %d, error %d \n", __LINE__, ret);
            perror("Read audio data");
            break;
        } 


        i += ret;
    } while (i < sc.data_length);
    free (audiodata);
    return 0;
}

/* Fine frequency adjustment. The PLL is first set to a known frequency, and
 * is then tuned to the target frequency specified for fine frequency
 * adjustment */
unsigned int i2s_fine_freq(unsigned int fine_freq, ushort num_bits, ushort num_chan) 
{
	pll_t my_new_pll;
	unsigned int target_frac;
	
	ath_i2s_set_freq(fine_freq, num_bits, num_chan, &my_new_pll);
	target_frac = ((my_new_pll.target_pll)>> AUDIO_PLL_MODULATION_TGT_DIV_FRAC_LSB);
	return target_frac;
} 

int
playwav (int fd)
{
	wavhead_t	hdr;
	scdata_t	sc;
	int		tmpcount, ret=0, count=0, i, stereo_mode = 0, bufsz = 0;
	char		*audiodata, *data;
	u_char		*tmp;
	pll_t            new_audio_pll;
	
	if (fd < 0) {
		return EINVAL;
	}

	read(fd, &hdr, sizeof (hdr));

#if __BYTE_ORDER == __BIG_ENDIAN
	hdr.length	= bswap_32(hdr.length);
	hdr.sc_len	= bswap_32(hdr.sc_len);
	hdr.format	= bswap_16(hdr.format);
	hdr.modus	= bswap_16(hdr.modus);
	hdr.sample_fq	= bswap_32(hdr.sample_fq);
	hdr.byte_p_sec	= bswap_32(hdr.byte_p_sec);
	hdr.byte_p_spl	= bswap_16(hdr.byte_p_spl);
	hdr.bit_p_spl	= bswap_16(hdr.bit_p_spl);
#endif

	/*
	 * Refer to the comments in the declaration of the wavhead_t
	 * structure.
	 * Having a pointer and moving around would have been easier.
	 * But, that results in unaligned reads for the 32bit integer
	 * data resulting in core dump.  Hence...
	 * -Varada
	 */
	if (hdr.sc_len == WAV_SUBCHUNK_LEN_16) {
		tmp = &hdr.pad[0];
		lseek(fd, -2, SEEK_CUR);
	} else if (hdr.sc_len == WAV_SUBCHUNK_LEN_18) {
		tmp = &hdr.pad[2];
	} else {
		return EINVAL;
	}
    memcpy(&sc, tmp, sizeof(sc));

#if __BYTE_ORDER == __BIG_ENDIAN
	sc.data_chunk = bswap_32 (sc.data_chunk);
	sc.data_length = bswap_32 (sc.data_length);
#endif

	if (bufsz <= 0) {
		bufsz = BUFF_SIZE;
	}

	/* Set Data Word Size and I2S Word Size */
	if (ioctl(audio, I2S_DSIZE, hdr.bit_p_spl) < 0) {
		perror("I2S_DSIZE");
	}

	/* To fill up mode in stereo config */
	if (hdr.modus == WAV_STEREO)
		stereo_mode = 0;
	
	else if (hdr.modus == WAV_MONO)
		stereo_mode = 1; /* Should be 2 for Channel 1 */

	if(ioctl(audio, I2S_MODE, stereo_mode) < 0) {
		perror("I2S_MODE");
	}

	/* Calculate PLL dividers and int frac values */
	ath_i2s_set_freq(hdr.sample_fq, hdr.bit_p_spl, hdr.modus, &new_audio_pll);
	
	if (ioctl(audio, I2S_FREQ, &new_audio_pll) < 0) {
		perror("I2S_FREQ");
	}

	/* MCLK : External or Internal */
        if (mclk_sel) {
		if (ioctl(audio, I2S_MCLK, mclk_sel) < 0) {
	    		perror("I2S_MCLK");
	    	}
	}

	/* Disable SPDIF if required. It is enabled by default */
	if (ioctl(audio, I2S_SPDIF_DISABLE, spdif_disable) < 0) {
		perror("I2S_SPDIF_DISABLE");
	}
	
	/* Is fine frequency adjustment required */
	if (fine) {
		unsigned int new_target_frac;
		new_target_frac = i2s_fine_freq(fine_freq, hdr.bit_p_spl, hdr.modus);
		printf("New target frac = %d \n", new_target_frac);
		if (ioctl(audio, I2S_FINE, new_target_frac) < 0) {
			perror("I2S_FINE");
		}
	}
   
	audiodata = (char *) malloc (bufsz * sizeof (char));
	if (audiodata == NULL) {
		return ENOMEM;
	}

	for (i = 0; i <= sc.data_length; i += bufsz) {
		/*
		 * Bug#:	26972
		 * The byte stream after the `.wav' header could have
		 * additional data (like author, album etc...) apart
		 * from the actual `audio data'.  Hence, ensure that
		 * extra stuff is not written to the device.  Stop at
		 * wherever the audio data ends.
		 *	+--------+----------------------+--------+
		 *	| header | audio data . . . . . | extras |
		 *	+--------+----------------------+--------+
		 */
		count = bufsz;
		if ((i + count) > sc.data_length) {
			count = sc.data_length - i;
		}

		/* Read count bytes from the Wave file */
		if ((count = read (fd, audiodata, count)) <= 0) {
			perror("Read audio data");
			break;
		}

#if 1
		tmpcount = count;
		data = audiodata;
		ret = 0;
		if (valfix != -1) {
			memset(data, valfix, tmpcount);
		}
erestart:
		/* Write the read bytes from the WAVE file to the 
		 * I2S interface */
		ret = write(audio, data, tmpcount);
		if (ret == -ERESTART) {
			goto erestart;
		}
		
#else
eagain:
		tmpcount = count;
		data = audiodata;
		ret = 0;
		do {
			ret = write(audio, data, tmpcount);
			if (ret < 0 && errno == EAGAIN) {
				dp("%s:%d %d %d\n", __func__, __LINE__, ret, errno);
				goto eagain;
			}
			tmpcount = tmpcount - ret;
			data += ret;

		} while(tmpcount);
#endif
		dp("i = %d\n", i);
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
        child;
        char option;

	bufsz = 0;
	fine=0;

	/* Device node */
	char *audev = "/dev/i2s";

	/* Get options from the user */
	while ((optc = getopt (argc, argv, "mrpv:t:d:f:F:M:S:B:s:")) != -1) {
		switch (optc) {
			case 'v': valfix = atoi (optarg); break;
			case 't': bufsz = atoi (optarg); break;
			case 'd': audev = optarg; break;
			case 'f':
				fine_freq = atoi(optarg);
				fine = 1;
				break;
			case 'p': /* Turn on prints */
				dbg = 1;
				break;
			case 'r':
				recorder = 1;
				break;
			case 'F':
				/* For record */
				sample_freq = atoi(optarg);
				break;
			case 'B':
				/* For record */
				mode = atoi(optarg);
				if (mode == 32) {
				/* 32 bit MIC IN I/F */
					mic_in = 1;
				}	
				break;
			case 'S':
				/* Size of the file to record */
				file_size = atoi(optarg);
				break;
			case 'M':
				/* For record - Stereo/Mono Mode */
				stereo_mono = atoi(optarg);
				break;				
			case 'm':
				 mclk_sel = 1;
				 break;
			case 's':
				spdif_disable = atoi(optarg);
				break;
			default: ep("Unknown option\n"); exit(-1);
		}
	}

	audio = open (audev, (recorder) ? O_RDONLY : O_WRONLY);

	if (audio < 0) {
		ep("Device %s opening failed\n", audev);
		exit (-1);
	}

	if (recorder) {
		if ((fd = open(argv[optind], O_CREAT | O_TRUNC | O_WRONLY)) == -1) {
			perror(argv[optind]);
			exit(-1);
		}
	} else {
		if ((fd = open (argv[optind], O_RDONLY)) == -1) {
			perror(argv[optind]);
			exit(-1);
		}
	}
	signal(SIGUSR1, pause_handler);
	signal(SIGUSR2, resume_handler);

	if(recorder) {
		record(fd);
	} else {
		playwav(fd);
	}
	close(fd);
rep:
	ret = close(audio);
	if (errno == EAGAIN) {
		dp("%s:%d %d %d\n", __func__, __LINE__, ret, errno);
		goto rep;
	}
	return 0;
}
