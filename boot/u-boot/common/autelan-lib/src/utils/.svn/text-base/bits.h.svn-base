#ifndef __BITS_H_92033DEDA810AF54224FF623B3115513__
#define __BITS_H_92033DEDA810AF54224FF623B3115513__
/******************************************************************************/
#ifndef __os_align
#define __os_align(x, align)        ((((x)+(align)-1)/(align))*(align))
#endif

#ifndef os_align
#define os_align(x, align)          (((x)+(align)-1) & ~((align)-1))
#endif

#ifndef os_align_down
#define os_align_down(x, align)     ((x) & ~((align)-1))
#endif

#ifndef os_setflag
#define os_setflag(value, flag)     do{(value) |= (flag);}while(0)
#endif

#ifndef os_clrflag
#define os_clrflag(value, flag)     do{(value) &= ~(flag);}while(0)
#endif

#ifndef os_hasflag
#define os_hasflag(value, flag)     ((flag) == ((value) & (flag)))
#endif

#ifndef os_bit
#define os_bit(bit)                 (1<<(bit))
#endif

#ifndef os_mask
#define os_mask(bit)                (os_bit(bit) - 1)
#endif

#ifndef os_maskmatch
#define os_maskmatch(a, b, mask)    (((a) & (mask))!=((b) & (mask)))
#endif

#ifndef os_setbit
#define os_setbit(value, bit)       os_setflag(value, os_bit(bit))
#endif

#ifndef os_clrbit
#define os_clrbit(value, bit)       os_clrflag(value, os_bit(bit))
#endif

#ifndef os_hasbit
#define os_hasbit(value, bit)       os_hasflag(value, os_bit(bit))
#endif

static inline bool
os_bufmaskmatch(byte *a, byte *b, byte *mask, int len)
{
    int i;

    for (i=0; i<len; i++) {
        if (false==os_maskmatch(a[i], b[i], mask[i])) {
            return false;
        }
    }

    return true;
}

/******************************************************************************/
#endif /* __BITS_H_92033DEDA810AF54224FF623B3115513__ */
