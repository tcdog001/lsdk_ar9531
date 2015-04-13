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

/* athplay related defines */
/* SRAM */
#define USE_SRAM
/* Uncomment for SRAM access. Use only
 * for Scorpion */
#undef USE_SRAM

#ifndef USE_SRAM
#define NUM_DESC         128
#define I2S_BUF_SIZE     768
#else
/* For SRAM */
#define NUM_DESC         4
#define I2S_BUF_SIZE     128
#endif

#define BUFF_SIZE        (NUM_DESC * I2S_BUF_SIZE)

#define MHZ              1000000

#define AUDIO_PLL_CONFIG_EXT_DIV_LSB                                 12
#define AUDIO_PLL_CONFIG_POSTPLLDIV_LSB                              7
#define AUDIO_PLL_CONFIG_REFDIV_LSB                                  0
#define AUDIO_PLL_MODULATION_TGT_DIV_FRAC_LSB                        11
#define AUDIO_PLL_MODULATION_TGT_DIV_INT_LSB                         1


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

/* WAV specific defines */
#define WAV_CHUNKID_RIFF      0x52494646          /* 'RIFF' in ASCII */
#define WAV_CHUNK_SIZE        0xe6822700
#define WAV_FORMAT            0x57415645          /* 'WAVE' in ASCII */
#define WAV_SUBCHUNK_ID       0x666d7420          /* Contains the letters 'fmt' */
#define WAV_SUBCHUNK_SIZE     0x12000000          /* Subchunk size */
#define WAV_AUDIO_FORMAT      0x100               /* PCM */
#define WAV_DATA_CHUNKID      0x64617461          /* Contains 'data' */
#define WAV_SUBCHUNK_LEN_16   16
#define WAV_SUBCHUNK_LEN_18   18
#define WAV_MONO              1
#define WAV_STEREO            2

/* Record Defaults */
#define REC_DEF_FREQ_44       0x0000ac44
#define REC_DEF_FILE_SIZE     0x002782c0
#define REC_DEF_MODE_16       0x10
#define REC_DEF_TYPE_STEREO   0x2

/* PLL frequency calculation related macros . Refer I2S Document */
/* Specific to Scorpion */
#define VCO_ULIMIT            800
#define VCO_LLIMIT            500
#define REF_CLK               40      /* MHZ */
#define REF_DIV               2
#define EXT_DIV_MIN           2
#define EXT_DIV_MAX           6
#define POST_PLLDIV_MAX       32
#define DWORD_SIZE_24         24
#define DWORD_SIZE_32         32
#define I2S_WORD_SIZE_16      16
#define I2S_WORD_SIZE_32      32
#define SCLK_DIV_2            2
#define SCLK_DIV_4            4


/* Codec Specific defines. Refer Codec datasheet */
#define MAX_SPEEDS            3
#define MAX_MDIVS             4
/* Different Speed modes based on Sampling frequency */
#define SINGLE_LLIMIT         4000  /* KHZ */
#define SINGLE_ULIMIT         50000
#define DOUBLE_ULIMIT         100000
#define QUAD_ULIMIT           216000
#define SINGLE                0
#define DOUBLE                1
#define QUAD                  2

typedef struct {
        u_int   data_chunk;     /* 'data' */
        u_int   data_length;    /* samplecount (lenth of rest of block?) */
} scdata_t;

typedef struct {
        u_int           main_chunk;     /* 'RIFF' */
        u_int           length;         /* Length of rest of file */
        u_int           chunk_type;     /* 'WAVE' */
        u_int           sub_chunk;      /* 'fmt ' */
        u_int           sc_len;         /* length of sub_chunk */
        u_short         format;         /* should be 1 for PCM-code */
        u_short         modus;          /* 1 Mono, 2 Stereo */
        u_int           sample_fq;      /* frequence of sample */
        u_int           byte_p_sec;
        u_short         byte_p_spl;     /* samplesize; 1 or 2 bytes */
        u_short         bit_p_spl;      /* 8, 12 or 16 bit */
        /*
         * FIXME:
         * Apparently, two different formats exist.
         * One with a sub chunk length of 16 and another of length 18.
         * For the one with 18, there are two bytes here.  Don't know
         * what they mean.  For the other type (i.e. length 16) this
         * does not exist.
         *
         * To handle the above issue, some jugglery is done after we
         * read the header
         *              -Varada (Wed Apr 25 14:53:02 PDT 2007)
         */
        u_char          pad[2];
        scdata_t        sc;
} __attribute__((packed)) wavhead_t;

typedef struct pll {
        unsigned long audio_pll;
        unsigned long target_pll;
        unsigned int psedge;
} pll_t;

/* SPDIF VUC Read/Write */
typedef struct spdif_vuc_t {
        unsigned int write;
        unsigned int va[6];
        unsigned int ua[6];
        unsigned int ca[6];
        unsigned int vb[6];
        unsigned int ub[6];
        unsigned int cb[6];
}spdif_vuc;

spdif_vuc vuc_data;
