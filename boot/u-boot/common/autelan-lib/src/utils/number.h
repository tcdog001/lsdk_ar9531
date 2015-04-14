#ifndef __NUMBER_H_D1912A84AB1249D4A3313CDD1D2C0B44__
#define __NUMBER_H_D1912A84AB1249D4A3313CDD1D2C0B44__
/******************************************************************************/

static inline int
os_digitchar2int(int ch)
{
    /*
    * "0123456789"
    */
    if (ch>='0' && ch<='9') {
        return ch - '0';
    }
    /*
    * "abcdef"
    */
    else if (ch>='a' && ch<='f') {
        return ch - 'a' + 10;
    }
    /*
    * "ABCDEF"
    */
    else if (ch>='A' && ch<='F') {
        return ch - 'A' + 10;
    }
    else {
        return os_assert_value(ch - '0');
    }
}

#define os_digitstring2number(digitstring, len, base, type) ({ \
    type n = 0;                                 \
    char *str = (char *)digitstring;            \
    int i;                                      \
                                                \
    for (i=0; i<len; i++)  {                    \
        n *= (type)base;                        \
        n += (type)os_digitchar2int(str[i]);    \
    }                                           \
                                                \
    n;                                          \
})

/******************************************************************************/
#endif /* __NUMBER_H_D1912A84AB1249D4A3313CDD1D2C0B44__ */
