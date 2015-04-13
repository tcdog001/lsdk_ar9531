#ifndef __ACFG_TOOL_H__
#define __ACFG_TOOL_H__

#include<stdio.h>
#include<stdlib.h>

#include<a_base_types.h>


/*
 * Debugging
 */
#ifdef ACFG_TRACE
#define dbglog(args...) do{ extern char *appname ;              \
    fprintf(stdout, "%s: ",appname);    \
    fprintf(stdout, args);              \
    fprintf(stdout,"\n"); }while(0)

#define dbg_print_params(fmt , args...)         \
    do{                                         \
        char *str;                          \
        \
        if(asprintf(&str, fmt, args))       \
        {                                   \
            dbglog(str);                    \
            free(str);                      \
        }                                   \
    }while(0)

#else
#define dbglog(args...)
#define dbg_print_params(fmt , args...)
#endif //ACFG_TRACE




/* -----------------------
 *  Type Declarations
 * -----------------------
 */

/**
 * @brief Type of wrapper function.
 *        We define one wrapper for each acfg lib api
 *        that we intend to test. The wrapper converts command line
 *        parameters from their string representation to the data types
 *        accepted by the acfg lib api and executes a call to the api.
 *
 * @param
 *
 * @return
 */
typedef int (*api_wrapper_t)(char *params[]) ;



/**
 * @brief Type of parameter
 */
typedef enum param_type {
    PARAM_UINT8 = 0,
    PARAM_UINT16,
    PARAM_UINT32,
    PARAM_SINT8,
    PARAM_SINT16,
    PARAM_SINT32,
    PARAM_STRING,
} param_type_t ;



/**
 * @brief Details about one parameter
 */
typedef struct param_info {
    char *name;
    param_type_t type ;
    char *desc ;
} param_info_t ;


/**
 * @brief Table of function pointers. This helps us to execute the correct
 *        wrapper for each acfg lib api. The 'num_param' field determines the
 *        number of commnd line arguments needed for each api.
 */
typedef struct fntbl{
    char *apiname ;
    api_wrapper_t wrapper ;
    int num_param ;
    param_info_t *param_info ;
} fntbl_t ;





/* ---------------------------
 *      Utility Functions
 * ---------------------------
 */


#define msg(args...)   do{                              \
    extern char *appname ;      \
    fprintf(stdout, "%s: ",appname); \
    fprintf(stdout, args);      \
    fprintf(stdout, "\n");      \
}while(0)

#define msgstatus(status)   msg("acfg lib status - %d",status)

/**
 * @brief   Convert a string representation of a
 *          number to unsigned 32 bit 
 *
 * @param str
 * @param val
 *
 * @return
 */
int get_uint32(char *str, a_uint32_t *val) ;

/**
 * @brief   Convert a hex string representation of a 
 *          number to unsigned 32 bit 
 *
 * @param str
 * @param val
 *
 * @return
 */
int get_hex(char *str, a_uint32_t *val) ;

/**
 * @brief Convert acfg lib status to OS specific
 *        status.
 *
 * @param status
 *
 * @return
 */
static
inline int acfg_to_os_status(a_status_t status)
{
    if(status != A_STATUS_OK)
        return -1;
    else
        return 0;
}
#define ACFG_EVENT_LOG_FILE "/etc/acfg_event_log"
#endif //__ACFG_TOOL_H__

