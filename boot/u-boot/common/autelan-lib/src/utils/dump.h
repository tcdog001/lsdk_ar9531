#ifndef __DUMP_H_202B4E0B91FFA0621EF08DACFCA3681E__
#define __DUMP_H_202B4E0B91FFA0621EF08DACFCA3681E__
/******************************************************************************/
#include "utils/printf.h"
/*
*   raw format(like UltraEdit)

      :                                     ;
 Line :       Hexadecimal Content           ; Raw Content
      : 0 1 2 3  4 5 6 7  8 9 A B  C D E F  ;
      :                                     ;
xxxxH : xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx ; cccccccccccccccc
xxxxH : xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx ; cccccccccccccccc
xxxxH : xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx ; cccccccccccccccc
xxxxH : xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx ; cccccccccccccccc
xxxxH : xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx ; cccccccccccccccc
xxxxH : xxxxxxxx xxxxxxxx xxxxxx            ; ccccccccccc
*/

#define __RAW_LINE_BYTES        16
#define __RAW_LINE_BLOCK_BYTES  4
#define __RAW_LINE_MAX          63 /* "xxxxH : xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx ; cccccccccccccccc" + "\n" */

#define __RAW_LINE_SEPARATOR \
"=============================================================="

#define __RAW_LINE_SEPARATOR_SUB \
"--------------------------------------------------------------"

#define __RAW_LINE_HEADER \
"      :                                     ;"                 __crlf \
" Line :       Hexadecimal Content           ; Raw Content"     __crlf \
"      : 0 1 2 3  4 5 6 7  8 9 A B  C D E F  ;"                 __crlf \
"      :                                     ;"                 __crlf

typedef void dump_line_f(char *line);

#ifdef OS_DUMP_PRINTF
#define os_dump_printf(_fmt, args...)   OS_DUMP_PRINTF(_fmt, ##args)
#else
#define os_dump_printf(_fmt, args...)   os_printf(_fmt, ##args)
#endif

static inline void
os_dump_line(int line, byte *raw, int len, dump_line_f *dump_line)
{
    int i, offset = 0;
    char buf[1 + __RAW_LINE_MAX] = {0};
    
    /*
    * line as 
    *
    * "xxxxH :"
    */
    offset += os_soprintf(buf, offset, "%.4XH :", __RAW_LINE_BYTES*line);

    /*
    * Hexadecimal Content as
    *
    * " xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx ; "
    */
    for (i=0; i<len; i++) {
        if (0 == (i%__RAW_LINE_BLOCK_BYTES)) {
            offset += os_soprintf(buf, offset, __space);
        }
        
        offset += os_soprintf(buf, offset, "%.2X", raw[i]);
    }
    
    for (; i<__RAW_LINE_BYTES; i++) {
        if (0 == (i%__RAW_LINE_BLOCK_BYTES)) {
            offset += os_soprintf(buf, offset, __space);
        }
        
        offset += os_soprintf(buf, offset, __space __space);
    }
    offset += os_soprintf(buf, offset, " ; ");
    
    /*
    * Raw Content as
    *
    * "cccccccccccccccc"
    */
    for (i=0; i<len; i++) {
        int c = (int)raw[i];
        
        offset += os_soprintf(buf, offset, "%c", isprint(c)?c:'.');
    }
    offset += os_soprintf(buf, offset, __crlf);

    if (dump_line) {
        (*dump_line)(buf);
    } else {
        os_dump_printf("%s", buf);
    }
}

static inline void
os_dump_buffer(void *buffer, int len, dump_line_f *dump_line)
{
    int i, line, tail;
    byte *raw = (byte *)buffer;
    
    /*
    * 行数，向上取整 
    */
    line = os_align(len, __RAW_LINE_BYTES)/__RAW_LINE_BYTES;
    /*
    * 尾行字节数
    *
    * 需要考虑特殊情况: 
    *   尾行字节数为0, 即 len 正好是 __RAW_LINE_BYTES 整数倍
    *   则尾行字节数重置为 __RAW_LINE_BYTES
    */
    tail = len%__RAW_LINE_BYTES;
    tail = tail?tail:__RAW_LINE_BYTES;
    
    /*
    * header
    */
    if (dump_line) {
        (*dump_line)(__RAW_LINE_HEADER);
    } else {
        os_dump_printf(__RAW_LINE_HEADER);
    }
    
    /*
    * body
    */
    for (i=0; i<(line-1); i++) {
        os_dump_line(i, raw + i * __RAW_LINE_BYTES, __RAW_LINE_BYTES, dump_line);
    }
    os_dump_line(line, raw + i * __RAW_LINE_BYTES, tail, dump_line);
}

/******************************************************************************/
#endif /* __DUMP_H_202B4E0B91FFA0621EF08DACFCA3681E__ */
