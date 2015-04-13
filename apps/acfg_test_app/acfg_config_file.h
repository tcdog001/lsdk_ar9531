#include "acfg_api_types.h"


#define OFFSET(a,b) ((long)&((a *) 0)->b)
#define OFFSET_RADIOPARAM(a,b) ((long)&((a *) 0)->radio_params.b)
#define OFFSET_TXPOWER(a,b) ((long)&((a *) 0)->txpow.b)
#define OFFSET_SECPARAM(a,b) ((long)&((a *) 0)->security_params.b)
#define OFFSET_EAPPARAM(a,b) ((long)&((a *) 0)->security_params.eap_param.b)
#define OFFSET_PRI_RADIUSPARAM(a,b) ((long)&((a *) 0)->security_params.pri_radius_param.b)
#define OFFSET_SEC1_RADIUSPARAM(a,b) ((long)&((a *) 0)->security_params.sec1_radius_param.b)
#define OFFSET_SEC2_RADIUSPARAM(a,b) ((long)&((a *) 0)->security_params.sec2_radius_param.b)
#define OFFSET_PRI_ACCTPARAM(a,b) ((long)&((a *) 0)->security_params.pri_acct_server_param.b)
#define OFFSET_SEC1_ACCTPARAM(a,b) ((long)&((a *) 0)->security_params.sec1_acct_server_param.b)
#define OFFSET_SEC2_ACCTPARAM(a,b) ((long)&((a *) 0)->security_params.sec2_acct_server_param.b)
#define OFFSET_HSPARAM(a,b) ((long)&((a *) 0)->security_params.hs_iw_param.b)
#define OFFSET_ACLPARAM(a,b) ((long)&((a *) 0)->node_params.b)
#define OFFSET_WDSPARAM(a,b) ((long)&((a *) 0)->wds_params.b)
#define OFFSET_RTSTHR(a,b) ((long)&((a *) 0)->rts_thresh.b)
#define OFFSET_FRAGTHR(a,b) ((long)&((a *) 0)->frag_thresh.b)
#define OFFSET_VENDORPARAM(a)  ((long)&((a *) 0)->vendor_param)

struct acfg_radio_params {
    a_uint8_t name[32];
    a_uint32_t offset;
    a_uint8_t type;
};

int acfg_read_file(char *filename, acfg_wlan_profile_t *profile);
