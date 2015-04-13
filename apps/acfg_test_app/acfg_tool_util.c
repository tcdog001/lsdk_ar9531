
#include<stdlib.h>

#include<acfg_tool.h>
#include<a_base_types.h>

int get_uint32(char *str, a_uint32_t *val)
{
    unsigned long int ret ;

    ret = strtoul(str , NULL , 10);
    *val = (a_uint32_t) ret ;
    return 0;
}

int get_hex(char *str, a_uint32_t *val)
{
    unsigned long int ret ;

    ret = strtoul(str , NULL , 16);
    *val = (a_uint32_t) ret ;
    return 0;
}

