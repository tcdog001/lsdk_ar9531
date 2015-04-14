#ifndef __MAC_H_F836BDB5B61CB538446562E3E6DD918C__
#define __MAC_H_F836BDB5B61CB538446562E3E6DD918C__
/******************************************************************************/
#include "number.h"

#ifndef OS_MACSIZE
#define OS_MACSIZE                  6
#endif

#ifndef OS_OUISIZE
#define OS_OUISIZE                  3
#endif

/*
* macstring is "XX:XX:XX:XX:XX:XX" / "XX-XX-XX-XX-XX-XX"
*/
#ifndef MACSTRINGLEN_L
#define MACSTRINGLEN_L              (3*OS_MACSIZE-1)
#endif

/*
* macstring is "XXXXXXXXXXXX"
*/
#ifndef MACSTRINGLEN_S
#define MACSTRINGLEN_S              (2*OS_MACSIZE)
#endif

#define OS_ZEROMAC                  ((byte *)"\x00\x00\x00\x00\x00\x00")
#define OS_FULLMAC                  ((byte *)"\xff\xff\xff\xff\xff\xff")

static inline byte *
os_maccpy(byte *mac_dst, byte *mac_src)
{
    return os_memcpy(mac_dst, mac_src, OS_MACSIZE);
}

static inline byte *
os_maczero(byte *mac)
{
    return os_memzero(mac, OS_MACSIZE);
}

static inline byte *
os_macfull(byte *mac)
{
    return os_maccpy(mac, OS_FULLMAC);
}

static inline int
os_maccmp(byte *mac_a, byte *mac_b)
{
    return os_memcmp(mac_a, mac_b, OS_MACSIZE);
}

static inline bool
os_maceq(byte *mac_a, byte *mac_b)
{
    return 0==os_maccmp(mac_a, mac_b);
}

static inline bool
os_macmaskmach(byte *mac_a, byte *mac_b, byte *mac_mask)
{
    return os_bufmaskmatch(mac_a, mac_b, mac_mask, OS_MACSIZE);
}

static inline bool
is_zero_mac(byte *mac)
{
    return os_maceq(mac, OS_ZEROMAC);
}

static inline bool
is_full_mac(byte *mac)
{
    return os_maceq(mac, OS_FULLMAC);
}

static inline bool
is_good_mac(byte *mac)
{
    return false==is_zero_mac(mac) 
        && false==is_full_mac(mac);
}

static inline byte *
os_getmac_bystring(byte *mac, char *macstring)
{
    int len = os_strlen(macstring);
    int width = 3;
    int i;

    /*
    * macstring is "XX:XX:XX:XX:XX:XX" / "XX-XX-XX-XX-XX-XX"
    */
    if (MACSTRINGLEN_L==len) {
        width = 3;
    }
    /*
    * macstring is "XXXXXXXXXXXX"
    */
    else if (MACSTRINGLEN_S==len) {
        width = 2;
    }
    else {
        os_assert(0);
    }
    
    for (i=0; i<OS_MACSIZE; i++) {
        mac[i] = os_digitstring2number(macstring + width*i, 2, 16, int);
    }

    return mac;
}

static inline int
os_macsnprintf(byte *mac, char *macstring, int len, int sep)
{
    if (0==sep) {
        return os_snprintf(macstring, len,
            "%.2x"
            "%.2x"
            "%.2x"
            "%.2x"
            "%.2x"
            "%.2x",
            mac[0], 
            mac[1], 
            mac[2], 
            mac[3], 
            mac[4], 
            mac[5]);
    } else {
        return os_snprintf(macstring, len,
            "%.2x"  "%c"
            "%.2x"  "%c"
            "%.2x"  "%c"
            "%.2x"  "%c"
            "%.2x"  "%c"
            "%.2x",
            mac[0], sep,
            mac[1], sep,
            mac[2], sep,
            mac[3], sep,
            mac[4], sep,
            mac[5]);
    }
}

#define os_macsaprintf(mac, macstring, sep) \
        os_macsnprintf(mac, macstring, sizeof(macstring), sep)

/*
*  multi-thread unsafe
*/
static inline char *
os_getmacstring(byte mac[], int sep)
{
    static char macstring[1+MACSTRINGLEN_L] = {0};

    os_macsaprintf(mac, macstring, sep);

    return macstring;
}

/******************************************************************************/
#endif /* __MAC_H_F836BDB5B61CB538446562E3E6DD918C__ */
