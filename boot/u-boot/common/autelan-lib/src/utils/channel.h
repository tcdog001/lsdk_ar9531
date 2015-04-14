#ifndef __CHANNEL_H_18AEA12667A8CB993E7009393B52CF95__
#define __CHANNEL_H_18AEA12667A8CB993E7009393B52CF95__
/******************************************************************************/
#include "utils.h"

enum channel_type {
    CHANNEL_BUFFER,
    CHANNEL_U8,
    CHANNEL_U16,
    CHANNEL_U32,
    CHANNEL_U64,
    CHANNEL_POINTER,

    CHANNEL_END
};

typedef struct {
    int size;
    int count;
    int limit;
    int type;
    int reader;
    int writer;

    union {
        byte        *buffer;
        void        **pointer;
        uint8_t     *pv8;
        uint16_t    *pv16;
        uint32_t    *pv32;
        uint64_t    *pv64;
    } msg;
} channel_t;

static inline byte *
__channel_buffer(channel_t *ch, int idx)
{
    return ch->msg.buffer + (ch->size * idx);
}

static inline bool
__channel_empty(channel_t *ch)
{
    return 0==ch->count;
}

static inline bool
__channel_full(channel_t *ch)
{
    return ch->limit==ch->count;
}

static inline int
__channel_align(channel_t *ch, int number)
{
    if (number<0) {
        return __channel_align(ch, number + ch->limit);
    }
    else if(number<ch->limit) {
        return number;
    }
    else { /* number >= ch->limit */
        return __channel_align(ch, number - ch->limit);
    }
}

static inline channel_t *
os_ch_new(int size, int limit, int type)
{
    channel_t *ch = NULL;
    
    if (size<=0) {
        return os_assert_value(NULL);
    }
    else if (limit<=0) {
        return os_assert_value(NULL);
    }

    switch(type) {
        case CHANNEL_BUFFER:
            /* do nothing */
            break;
        case CHANNEL_POINTER:
            if (sizeof(void *)!=size) {
                return os_assert_value(NULL);
            }
            
            break;
        case CHANNEL_U8:
            if (sizeof(byte)!=size) {
                return os_assert_value(NULL);
            }
            
            break;
        case CHANNEL_U16:
            if (sizeof(uint16_t)!=size) {
                return os_assert_value(NULL);
            }
            
            break;
        case CHANNEL_U32:
            if (sizeof(uint32_t)!=size) {
                return os_assert_value(NULL);
            }
            
            break;
        case CHANNEL_U64:
            if (sizeof(uint64_t)!=size) {
                return os_assert_value(NULL);
            }
            
            break;
        default:
            return os_assert_value(NULL);
    }

    ch = (channel_t *)os_zalloc(sizeof(*ch) + limit * size);
    if (ch) {
        ch->size        = size;
        ch->limit       = limit;
        ch->type        = type;
        ch->msg.buffer  = (byte *)(ch + 1);
    }
    
    return ch;
}

static inline void
os_ch_free(channel_t *ch)
{
    os_free(ch);
}

static inline int
os_ch_read(channel_t *ch, void *msg)
{
    if (NULL==ch) {
        return os_assert_value(-ENOEXIST);
    }
    else if (NULL==msg) {
        return os_assert_value(-EINVAL1);
    }
    else if (__channel_empty(ch)) {
        return -EKEYBAD;
    }
    
    switch(ch->type) {
        case CHANNEL_BUFFER:
            os_memcpy(msg, __channel_buffer(ch, ch->reader), ch->size);
            
            break;
        case CHANNEL_POINTER:
            *(void **)msg = ch->msg.pointer[ch->reader];
            ch->msg.pointer[ch->reader] = NULL;

            os_assert(NULL != *(void **)msg);
            break;
        case CHANNEL_U8:
            *(uint8_t *)msg = ch->msg.pv8[ch->reader];
            
            break;
        case CHANNEL_U16:
            *(uint16_t *)msg = ch->msg.pv16[ch->reader];
            
            break;
        case CHANNEL_U32:
            *(uint32_t *)msg = ch->msg.pv32[ch->reader];
            
            break;
        case CHANNEL_U64:
            *(uint64_t *)msg = ch->msg.pv64[ch->reader];
            
            break;
        default:
            return -EINVAL9;
    }
    
    ch->reader = __channel_align(ch, ch->reader+1);
    if (__channel_full(ch)) {
        ch->writer = __channel_align(ch, ch->reader-1);
    }
    ch->count--;
    
    return 0;
}

static inline int
os_ch_write(channel_t *ch, void *msg)
{
    if (NULL==ch) {
        return os_assert_value(-EKEYBAD);
    }
    else if (NULL==msg) {
        return os_assert_value(-EINVAL1);
    }
    else if (__channel_full(ch)) {
        return -EKEYBAD;
    }
    
    switch(ch->type) {
        case CHANNEL_BUFFER:
            os_memcpy(__channel_buffer(ch, ch->writer), msg, ch->size);
            
            break;
        case CHANNEL_POINTER:
            ch->msg.pointer[ch->writer] = msg;
            
            break;
        case CHANNEL_U8:
            ch->msg.pv8[ch->writer] = *(uint8_t *)msg;
            
            break;
        case CHANNEL_U16:
            ch->msg.pv16[ch->writer] = *(uint16_t *)msg;
            
            break;
        case CHANNEL_U32:
            ch->msg.pv32[ch->writer] = *(uint32_t *)msg;
            
            break;
        case CHANNEL_U64:
            ch->msg.pv64[ch->writer] = *(uint64_t *)msg;
            
            break;
        default:
            return -EINVAL;
    }
    
    ch->writer = __channel_align(ch, ch->writer+1);
    if (__channel_empty(ch)) {
        ch->reader = __channel_align(ch, ch->writer-1);
    }
    ch->count++;
    
    return 0;
}

/******************************************************************************/
extern channel_t *
os_bch_new(int size, int limit)
{
    return os_ch_new(size, limit, CHANNEL_BUFFER);
}

extern void
os_bch_free(channel_t *ch)
{
    os_ch_free(ch);
}

static inline int
os_bch_read(channel_t *ch, void *buffer)
{
    return os_ch_read(ch, buffer);
}

static inline int
os_bch_write(channel_t *ch, void *buffer)
{
    return os_ch_write(ch, buffer);
}
/******************************************************************************/
static inline channel_t *
os_8ch_new(int limit)
{
    return os_ch_new(sizeof(byte), limit, CHANNEL_U8);
}

extern void
os_8ch_free(channel_t *ch)
{
    os_ch_free(ch);
}

static inline int
os_8ch_read(channel_t *ch, uint8_t *pv8)
{
    return os_ch_read(ch, pv8);
}

static inline int
os_8ch_write(channel_t *ch, uint8_t *pv8)
{
    return os_ch_write(ch, pv8);
}
/******************************************************************************/
static inline channel_t *
os_16ch_new(int limit)
{
    return os_ch_new(sizeof(uint16_t), limit, CHANNEL_U16);
}

extern void
os_16ch_free(channel_t *ch)
{
    os_ch_free(ch);
}

static inline int
os_16ch_read(channel_t *ch, uint16_t *pv16)
{
    return os_ch_read(ch, pv16);
}

static inline int
os_16ch_write(channel_t *ch, uint16_t *pv16)
{
    return os_ch_write(ch, pv16);
}
/******************************************************************************/
static inline channel_t *
os_32ch_new(int limit)
{
    return os_ch_new(sizeof(uint32_t), limit, CHANNEL_U32);
}

extern void
os_32ch_free(channel_t *ch)
{
    os_ch_free(ch);
}

static inline int
os_32ch_read(channel_t *ch, uint32_t *pv32)
{
    return os_ch_read(ch, pv32);
}

static inline int
os_32ch_write(channel_t *ch, uint32_t *pv32)
{
    return os_ch_write(ch, pv32);
}
/******************************************************************************/
static inline channel_t *
os_64ch_new(int limit)
{
    return os_ch_new(sizeof(uint64_t), limit, CHANNEL_U64);
}

extern void
os_64ch_free(channel_t *ch)
{
    os_ch_free(ch);
}

static inline int
os_64ch_read(channel_t *ch, uint64_t *pv64)
{
    return os_ch_read(ch, pv64);
}

static inline int
os_64ch_write(channel_t *ch, uint64_t *pv64)
{
    return os_ch_write(ch, pv64);
}
/******************************************************************************/
static inline channel_t *
os_pch_new(int limit)
{
    return os_ch_new(sizeof(void*), limit, CHANNEL_POINTER);
}

extern void
os_pch_free(channel_t *ch)
{
    os_ch_free(ch);
}

static inline int
os_pch_read(channel_t *ch, void **pointer)
{
    return os_ch_read(ch, (void *)pointer);
}

static inline int
os_pch_write(channel_t *ch, void *pointer)
{
    return os_ch_write(ch, pointer);
}
/******************************************************************************/
#endif /* __CHANNEL_H_18AEA12667A8CB993E7009393B52CF95__ */
