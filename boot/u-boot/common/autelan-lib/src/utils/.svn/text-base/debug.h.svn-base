#ifndef __DEBUG_H_E3D558ADAC195223DC447321A03A7DEC__
#define __DEBUG_H_E3D558ADAC195223DC447321A03A7DEC__
/******************************************************************************/
#ifdef __BOOT__
#define __THIS_NAME     "boot"
#endif

#ifdef __APP__
#define __syslog(_fmt, args...) syslog(LOG_DEBUG, _fmt, ##args)
#else
#define __syslog(_fmt, args...) 0
#define __THIS_FILE             0
#endif

#ifndef __THIS_NAME
#error "no defined __THIS_NAME in makefile!!!"
#endif

#ifndef __THIS_FILE
#error "no defined __THIS_FILE in makefile!!!"
#endif

#ifndef __AKID_DEBUG
#error "no defined __AKID_DEBUG in makefile!!!"
#endif

#define __debug_init_ok         (1<<0)  /* ³É¹¦ */
#define __debug_init_error      (1<<1)  /* ´íÎó */
#define __debug_init_trace      (1<<2)  /* ¸ú×Ù */
#define __debug_init_test       (1<<3)  /* ²âÊÔ */
#define __debug_init_consume    (1<<4)  /* ¼ÆËãcpuÏûºÄ */

#define __debug_init_all        0xffffffff
#define __debug_init_default    __debug_init_error

#if defined(__KERNEL__) || defined(__APPKEY__)
/*
* in boot/kernel/libappkey, should define __debug_init_pointer as below
*   int xxx;
*   static int *__debug_init_pointer = &xxx;
*/
#define __debug_init    (*__debug_init_pointer)
#elif defined(__BOOT__)
extern int __AKID_DEBUG;

#define __debug_init    __AKID_DEBUG
#else
/*
* for all app/lib(defined __THIS_NAME)
*/
extern appkey_t __AKID_DEBUG;

#define __debug_init    appkey_get(__AKID_DEBUG, __debug_init_default)
#endif

#define __is_debug_init(_mask)      (!!(__debug_init & (_mask)))
#define __is_debug_init_ok          __is_debug_init(__debug_init_ok)
#define __is_debug_init_error       __is_debug_init(__debug_init_error)
#define __is_debug_init_trace       __is_debug_init(__debug_init_trace)
#define __is_debug_init_test        __is_debug_init(__debug_init_test)
#define __is_debug_init_consume     __is_debug_init(__debug_init_consume)

#define __debug_prefix_fmt_and_args(_fmt)  \
        "[file:%.4d, line:%.5d, app:" __THIS_NAME "]" __crlf _fmt __crlf2, \
        __THIS_FILE, \
        __LINE__

#define __debug_with_prefix(_fmt, args...)      do{ \
    (void)os_printf(__debug_prefix_fmt_and_args(_fmt), ##args); \
     (void)__syslog(__debug_prefix_fmt_and_args(_fmt), ##args); \
}while(0)

#define __vdebug_with_prefix(_fmt, args)        do{ \
    (void)os_vprintf(__debug_prefix_fmt_and_args(_fmt), args); \
      (void)__syslog(__debug_prefix_fmt_and_args(_fmt), args); \
}while(0)

#define __debug_without_prefix(_fmt, args...)   do{ \
    (void)os_printf(_fmt __crlf2, ##args);          \
     (void)__syslog(_fmt __crlf2, ##args);          \
}while(0)

#define __vdebug_without_prefix(_fmt, args)     do{ \
    (void)os_vprintf(_fmt __crlf2, args);           \
      (void)__syslog(_fmt __crlf2, args);           \
}while(0)

#define os_error(_err, _fmt, args...)   (__debug_without_prefix(_fmt, ##args), (_err))
/*
* for cmd
*/
#define os_cerror(_err)             os_eprintln((_err), "error(%d)", (_err))
/*
* for daemon
*/
#define os_derror(_err)             os_error((_err), "error(%d)", (_err))

#define debug_ok(fmt, args...)      do{ if (__is_debug_init_ok) __debug_with_prefix(fmt, ##args);   }while(0)
#define vdebug_ok(fmt, args)        do{ if (__is_debug_init_ok) __vdebug_with_prefix(fmt, args);    }while(0)
#define __debug_ok(fmt, args...)    do{ if (__is_debug_init_ok) __debug_without_prefix(fmt, ##args);}while(0)
#define __vdebug_ok(fmt, args)      do{ if (__is_debug_init_ok) __vdebug_without_prefix(fmt, args); }while(0)

#define debug_error(fmt, args...)   do{ if (__is_debug_init_error) __debug_with_prefix(fmt, ##args);    }while(0)
#define vdebug_error(fmt, args)     do{ if (__is_debug_init_error) __vdebug_with_prefix(fmt, args);     }while(0)
#define __debug_error(fmt, args...) do{ if (__is_debug_init_error) __debug_without_prefix(fmt, ##args); }while(0)
#define __vdebug_error(fmt, args)   do{ if (__is_debug_init_error) __vdebug_without_prefix(fmt, args);  }while(0)

#define debug_trace(fmt, args...)   do{ if (__is_debug_init_trace) __debug_with_prefix(fmt, ##args);    }while(0)
#define vdebug_trace(fmt, args)     do{ if (__is_debug_init_trace) __vdebug_with_prefix(fmt, args);     }while(0)
#define __debug_trace(fmt, args...) do{ if (__is_debug_init_trace) __debug_without_prefix(fmt, ##args); }while(0)
#define __vdebug_trace(fmt, args)   do{ if (__is_debug_init_trace) __vdebug_without_prefix(fmt, args);  }while(0)

#define debug_test(fmt, args...)    do{ if (__is_debug_init_test) __debug_with_prefix(fmt, ##args);     }while(0)
#define vdebug_test(fmt, args)      do{ if (__is_debug_init_test) __vdebug_with_prefix(fmt, args);      }while(0)
#define __debug_test(fmt, args...)  do{ if (__is_debug_init_test) __debug_without_prefix(fmt, ##args);  }while(0)
#define __vdebug_test(fmt, args)    do{ if (__is_debug_init_test) __vdebug_without_prefix(fmt, args);   }while(0)

#ifdef __APP__
static inline int os_timeval_diff(struct timeval *old, struct timeval *new)
{
    int usec = 0;

    usec += ((int)new->tv_sec  - (int)old->tv_sec) * 1000 * 1000;
    usec += ((int)new->tv_usec - (int)old->tv_usec);
    
    return usec;
}

/*
    usage 1:
        function(...) 
        {
            function_consume_declare // must first

            ...

            function_consume_calculate // must befor return
        }
        
    usage 2:
        function(...) 
        {
            function_consume_declare // must first

            ...

            function_consume_calculate_begin
            ....
            function_consume_calculate_end

            
            function_consume_calculate_begin
            ....
            function_consume_calculate_end

            
            function_consume_calculate_begin
            ....
            function_consume_calculate_end

            ....
        }
*/

#define debug_consume(fmt, args...)     do{ if (__is_debug_init_consume) __debug_with_prefix(fmt, ##args);  }while(0)
#define vdebug_consume(fmt, args)       do{ if (__is_debug_init_consume) __vdebug_with_prefix(fmt, args);   }while(0)
#define __debug_consume(fmt, args...)   do{ if (__is_debug_init_consume) __debug_without_prefix(fmt, ##args);}while(0)
#define __vdebug_consume(fmt, args)     do{ if (__is_debug_init_consume) __vdebug_without_prefix(fmt, args);}while(0)

#define function_consume_declare    \
        struct timeval __function_consume_timeval_begin; \
        struct timeval __function_consume_timeval_end; \
        gettimeofday(&__function_consume_timeval_begin, NULL)
        
#define function_consume_calculate_begin  \
        gettimeofday(&__function_consume_timeval_begin, NULL)
        
#define function_consume_calculate_end  \
        gettimeofday(&__function_consume_timeval_end, NULL); \
        debug_consume("%s consume %dus", __func__, os_timeval_diff(&__function_consume_timeval_begin, &__function_consume_timeval_end))

#define function_consume_calculate      function_consume_calculate_end
#else
#define function_consume_declare            extern int __noused__function_consume_declare
#define function_consume_calculate_begin    os_do_nothing
#define function_consume_calculate_end      os_do_nothing
#define function_consume_calculate          os_do_nothing
#endif
/******************************************************************************/
#ifdef __APP__

#define BACKTRACE_by_none               0
#define BACKTRACE_by_libc_backtrace     1
#define BACKTRACE_by_libunwind          2

#ifndef BACKTRACE_TYPE
#define BACKTRACE_TYPE      BACKTRACE_by_libc_backtrace
#endif

#define BACKTRACE_DESTROY_STACK_FOREVER(_up) do{   \
    int var;            \
    int *p = &var;      \
                        \
    while(1) {          \
        os_println("write %p to zero", p); \
        if (_up) {      \
            *p++ = 0;   \
        } else {        \
            *p-- = 0;   \
        }               \
    }                   \
}while(0)

#define BACKTRACE_DESTROY_STACK_BYSIZE(_up, _size)  do{   \
    int var;                        \
    char *p = (char *)&var;         \
                                    \
    os_println("write stack %s %d's byte to zero", _up?"up":"down", _size); \
                                    \
    if (_up) {                      \
        os_memzero(p, _size);          \
    } else {                        \
        os_memzero(p - _size, _size);  \
    }                               \
}while(0)

#define BACKTRACE_WRITE_POINTER(_v)    do {   \
    int i;                  \
    int *p = _v;            \
                            \
    for (i=0; i<10; i++) {  \
        os_println("write pointer(%p) in function %s", p, __func__); \
    }                       \
                            \
    *p = 0;                 \
}while(0)

#define BACKTRACE_READ_POINTER(_v)    do {   \
    int i;                  \
    int *p = _v;            \
                            \
    for (i=0; i<10; i++) {  \
        os_println("read pointer(%p) in function %s", p, __func__); \
    }                       \
                            \
    i = *p;                 \
}while(0)

#ifndef BACKTRACE_PATH
#define BACKTRACE_PATH          "/usr/app/backtrace/" __THIS_NAME
#endif

#ifndef BACKTRACE_ASIZE
#define BACKTRACE_ASIZE         256
#endif

#if BACKTRACE_TYPE==BACKTRACE_by_libunwind
#include <libunwind.h>

static inline void 
os_sighandle_callstack(int sig)
{
    unw_cursor_t    cursor;
    unw_context_t   context;

    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    while (unw_step(&cursor) > 0) {
        unw_word_t  offset, pc;
        char        fname[128];

        unw_get_reg(&cursor, UNW_REG_IP, &pc);

        fname[0] = '\0';
        (void) unw_get_proc_name(&cursor, fname, sizeof(fname), &offset);

        printf ("%p : (%s+0x%x) [%p]\n", (void *)pc, fname, offset, (void *)pc);
    }

    exit(sig);
}

#elif BACKTRACE_TYPE==BACKTRACE_by_libc_backtrace
#include <execinfo.h>

static inline void 
os_sighandle_callstack(int sig)
{
    void *array[BACKTRACE_ASIZE] = {NULL};
    int i, count, fd;
    int pid = getpid();
    unsigned int now = (unsigned int)time(NULL);
    
    /*
    * copy app maps file
    */
    os_v_system("cp /proc/%d/maps " BACKTRACE_PATH "/%u.%d.maps", pid, now, sig);
    
    /*
    * create app trace file
    */
    fd = os_v_open(O_WRONLY | O_CREAT | O_TRUNC, BACKTRACE_PATH "/%u.%d.trace", now, sig);
    count = backtrace(array, os_count_of(array));
    backtrace_symbols_fd(array, count, fd);
    close(fd);
    
    os_println("backtrace array count(%d)", count);
    for (i=0; i<count; i++) {
        os_println(__tab "backtrace array[%d]=%p", i, array[i]);
    }
    os_println("backtrace array end");
    
    /*
    * locate bug file & line
    */
#if 0
    char lib[256];
    unsigned long base_address = 0;
    unsigned long dead_address = (unsigned long)array[3];
    
    stream = os_v_popen("r", "awk -F\"[- ]\" '{if($1<=\"%lx\" && $2>=\"%lx\") print$1, $NF}' ./%s.maps",
        dead_address,
        dead_address,
        appname);
    fscanf(stream, "%lx %s", &base_address, lib);
    
    if (NULL==strstr(lib, ".so.")) {
        /*
        * exe
        */
        os_v_system("addr2line -e ./%s 0x%lx > ./%s.locate", appname, dead_address, appname);
    } else {
        /*
        * lib
        */
        os_v_system("addr2line -e %s 0x%lx > ./%s.locate", lib, dead_address - base_address, appname);
    }
#endif

/*
    $(APP) is exe or lib's filename
    $(ADDRESS) is dead address, format as AABBCCDD
    
    readelf -s $(APP) | sort -k 2 | awk 'BEGIN{address="00000000";name=""}{if($2<="$(ADDRESS)"){address=$2;name=$8}}END{print address,name}'
*/
    exit(sig);
}
#else
static inline void os_sighandle_callstack(int sig){}
#endif /* BACKTRACE_TYPE */

static inline void 
os_sighandle_exit(int sig)
{
    exit(sig);
}

static inline void
__os_setup_sigaction(void (*handle)(int signo), int flag, int sigs[], int count)
{
    struct sigaction s = {
        .sa_flags   = flag,
        .sa_handler = handle,
    };
    int i;
    
	for (i=0; i<count; i++) {
        sigaction(sigs[i], &s, NULL);
	}
}
#define os_setup_sigaction(_handle, _flag, _sigs)  \
        __os_setup_sigaction(_handle, _flag, _sigs, os_count_of(_sigs))

static inline void
os_sigaction_exit(void)
{
    int sigs[] = {SIGINT};

    os_setup_sigaction(os_sighandle_exit, 0, sigs);
}

static inline void
os_sigaction_callstack(void)
{
    int sigs[] = {SIGBUS, SIGILL, SIGFPE, SIGSEGV};

    os_setup_sigaction(os_sighandle_callstack, 0, sigs);
}

static inline void
os_sigaction_default(void)
{
    os_sigaction_exit();
    os_sigaction_callstack();
}

#endif

/******************************************************************************/
#endif /* __DEBUG_H_E3D558ADAC195223DC447321A03A7DEC__ */
