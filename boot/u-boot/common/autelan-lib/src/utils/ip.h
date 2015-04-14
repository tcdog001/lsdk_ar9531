#ifndef __IP_H_46F65504978D5AD68BA98D390A65EB49__
#define __IP_H_46F65504978D5AD68BA98D390A65EB49__
/******************************************************************************/
#include "number.h"

#ifndef OS_IPSTRINGLEN
#define OS_IPSTRINGLEN              15 /* strlen("255.255.255.255") */
#endif

typedef char os_ipstring_t[1+OS_IPSTRINGLEN];

#ifdef __APP__
/*
* just for single-thread, unsafe for multi-thread
*
* @ip: network sort
*/
static inline char *
os_ipstring(unsigned int ip)
{
    return (char *)inet_ntoa(*(struct in_addr *)&ip);
}
#endif

static inline bool
os_ipmatch(unsigned int ipa, unsigned int ipb, unsigned int mask)
{
    return (ipa & mask)==(ipb & mask);
}
/******************************************************************************/
#endif /* __IP_H_46F65504978D5AD68BA98D390A65EB49__ */
