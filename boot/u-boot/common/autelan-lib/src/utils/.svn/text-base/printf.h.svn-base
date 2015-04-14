#ifndef __PRINTF_H_6115A6C5DFF707CC6D8400E2B7190543__
#define __PRINTF_H_6115A6C5DFF707CC6D8400E2B7190543__
/******************************************************************************/
#ifndef __KERNEL__
#define os_printf(_fmt, args...)                printf(_fmt, ##args)
#define os_vprintf(_fmt, args)                  vprintf(_fmt, args)
#else
#define os_printf(_fmt, args...)                printk(KERN_ALERT _fmt, ##args)
#define os_vprintf(_fmt, args)                  vprintk(KERN_ALERT _fmt, args)
#endif

#ifdef __APP__
#define os_fprintf(_stream, _fmt, args...)      fprintf(_stream, _fmt, ##args)
#define os_vfprintf(_stream, _fmt, args)        vfprintf(_stream, _fmt, args)
#else
#define os_fprintf(_stream, _fmt, args...)      0
#define os_vfprintf(_stream, _fmt, args)        0
#endif

#define os_sscanf(_buf, _fmt, args...)          sscanf(_buf, _fmt, ##args)
#define os_vsscanf(_buf, _fmt, args)            vsscanf(_buf, _fmt, args)

#define os_println(_fmt, args...)               os_printf(_fmt __crlf, ##args)
#define os_vprintln(_fmt, args)                 os_vprintf(_fmt __crlf, args)
#define os_printnewline()                       os_printf(__crlf)

#define os_fprintln(_stream, _fmt, args...)     os_fprintf(_stream, _fmt __crlf, ##args)
#define os_vfprintln(_stream, _fmt, args)       os_vfprintf(_stream, _fmt __crlf, args)

#define os_sprintf(_buf, _fmt, args...)         sprintf(_buf, _fmt, ##args)
#define os_vsprintf(_buf, _fmt, args)           vsprintf(_buf, _fmt, args)

#define os_snprintf_is_full(buffer_space, snprintf_return_value) \
        ((snprintf_return_value) >= (buffer_space))

#ifdef __BOOT__
#ifndef BOOT_SNPRINTF_BUFSIZE
#define BOOT_SNPRINTF_BUFSIZE   4096
#endif

#define os_snprintf(_buf, _size, _fmt, args...)  ({ \
    int nsize = BOOT_SNPRINTF_BUFSIZE;  \
    int bsize = (_size)?(_size)-1:0;    \
    int len;                            \
    char *p = (char *)os_zalloc(nsize); \
    if (p) {                            \
        len = os_sprintf(p, _fmt, ##args); \
        if (len <= bsize) {             \
            os_strcpy(_buf, p);         \
        } else {                        \
            os_memcpy(_buf, p, bsize);  \
        }                               \
        os_free(p);                     \
    } else {                            \
        len = 0;                        \
    }                                   \
                                        \
    len;                                \
})
#define os_vsnprintf(_buf, _size, _fmt, args)   BUILD_BUG_ON(true)

/*
* change from uclibc(vasprintf)
*/
#define os_asprintf(_pbuf, _fmt, args...)       ({ \
	int rv;                                         \
	char **pbuf = (char **)(_pbuf);                 \
                                                    \
 	rv = os_snprintf(NULL, 0, _fmt, ##args);        \
	*pbuf = NULL;                                   \
                                                    \
	if (rv >= 0) {                                  \
		if ((*pbuf = os_malloc(++rv)) != NULL) {    \
			if ((rv = os_snprintf(*pbuf, rv, _fmt, ##args)) < 0) { \
				os_free(*pbuf);                     \
			}                                       \
		}                                           \
	}                                               \
                                                    \
	os_assert(rv >= -1);                            \
                                                    \
	return rv;                                      \
})
#define os_vasprintf(_pbuf, _fmt, args)         BUILD_BUG_ON(true)

#else
#define os_snprintf(_buf, _size, _fmt, args...) snprintf(_buf, _size, _fmt, ##args)
#define os_vsnprintf(_buf, _size, _fmt, args)   vsnprintf(_buf, _size, _fmt, args)

#define os_asprintf(_pbuf, _fmt, args...)       asprintf(_buf, _size, _fmt, ##args)
#define os_vasprintf(_pbuf, _fmt, args)         vasprintf(_buf, _size, _fmt, args)
#endif


/*
* snprintf for array buffer
*/
#define os_saprintf(_buf, _fmt, args...)        os_snprintf(_buf, sizeof(_buf), _fmt, ##args)
#define os_vsaprintf(_buf, _fmt, args)          os_vsnprintf(_buf, sizeof(_buf), _fmt, args)
/*
* snprintf for array buffer + offset
*/
#define os_soprintf(_buf, _offset, _fmt, args...)  os_snprintf(_buf+(_offset), sizeof(_buf)-(_offset), _fmt, ##args)
#define os_voaprintf(_buf, _offset, _fmt, args)    os_vsnprintf(_buf+(_offset), sizeof(_buf)-(_offset), _fmt, args)

#define os_eprintf(_err, _fmt, args...)     (os_printf(_fmt, ##args), (_err))
#define os_eprintln(_err, _fmt, args...)    (os_println(_fmt, ##args), (_err))

#ifdef __APP__
#define os_v_xopen(type, func, mod, fmt, args...) ({  \
    char buf[1+OS_LINE_LEN] = {0};  \
    type tvar;                      \
                                    \
    os_saprintf(buf, fmt, ##args);  \
    tvar = func(buf, mod);          \
                                    \
    tvar;                           \
})

#define os_v_popen(mod, fmt, args...)   os_v_xopen(FILE*, popen, mod, fmt, ##args)
#define os_v_fopen(mod, fmt, args...)   os_v_xopen(FILE*, fopen, mod, fmt, ##args)
#define os_v_open(flag, fmt, args...)   os_v_xopen(int, open, flag, fmt, ##args)

#define os_system(_cmd)    ({  \
    int ret = 0, err = 0; \
                                \
    err = system(_cmd);         \
    if (-1==err) {/* fork/exec failed */ \
        ret = -ESYSTEM;         \
    } else if(WIFEXITED(err)) { /* 正常结束子进程 */ \
        ret = WEXITSTATUS(err); /* 子进程 exit()返回的结束代码 */ \
    } else if(WIFSIGNALED(err)) { /* 异常结束子进程 */ \
        ret = WTERMSIG(err); /* 子进程因信号而中止的信号代码 */ \
    } else if(WIFSTOPPED(err)) { /* 暂停子进程 */ \
        ret = WSTOPSIG(err); /* 引发子进程暂停的信号代码 */ \
	} else {                    \
        ret = errno;            \
	}                           \
                                \
    ret;                        \
})
#define __os_v_system(is_traced, fmt, args...) ({  \
    char cmd[1+OS_LINE_LEN] = {0}; \
    int len, ret = 0;           \
                                    \
    if (is_traced) {                \
        len = os_saprintf(cmd, fmt, ##args);   \
        os_println("%s", cmd);      \
    } else {                        \
        len = os_saprintf(cmd, fmt " > /dev/null 2>&1", ##args); \
    }                               \
    if (os_snprintf_is_full(sizeof(cmd), len)) {   \
        ret = -ENOSPACE;            \
    } else {                        \
        ret = os_system(cmd);       \
	}                               \
                                    \
	ret;                            \
})

#define os_v_system(fmt, args...)   \
        __os_v_system(__is_debug_init_trace, fmt, ##args)
#endif

/******************************************************************************/
#endif /* __PRINTF_H_6115A6C5DFF707CC6D8400E2B7190543__ */
