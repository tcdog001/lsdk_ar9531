#ifndef __APPKEY_H_4DCB6A57D69734A56298406D6DF398AA__
#define __APPKEY_H_4DCB6A57D69734A56298406D6DF398AA__
/******************************************************************************/
typedef unsigned int appkey_t;

#ifdef __APP__
extern int 
appkey_reload(void);

extern appkey_t 
__appkey_getbyname(char *app, char *key);

#define appkey_getbyname(key)   __appkey_getbyname(__THIS_NAME, key)

extern int 
__appkey_get(appkey_t akid, unsigned int *pvalue);

static inline unsigned int
appkey_get(appkey_t akid, unsigned int deft)
{
    unsigned int value = deft;

    __appkey_get(akid, &value);

    return value;
}

extern int 
appkey_set(appkey_t akid, unsigned int value);

#define AKID_DEBUG_NAME     "debug"

#define AKID_DEBUGER \
        static os_constructor void ____akid_debug_initer(void) \
        { \
            __AKID_DEBUG = appkey_getbyname(AKID_DEBUG_NAME); \
            openlog(__THIS_NAME, LOG_PID | LOG_CONS, LOG_DAEMON); \
        } \
        static os_destructor  void ____akid_debug_finier(void) \
        { \
            closelog(); \
        } \
        appkey_t __AKID_DEBUG
#else
#define AKID_DEBUGER   extern int __AKID_DEBUG
#endif /* __APP__ */
/******************************************************************************/
#endif /* __APPKEY_H_4DCB6A57D69734A56298406D6DF398AA__ */
