#include "utils.h"
#include "um.h"


static void 
l3user_update(bool created, struct apuser *dst, struct apuser *src)
{
    time_t now = time(NULL);

    if (created) {
        /*
        * created 代表用户表项是新创建的
        * 对于l3user, created 应该一定为 false
        */
        return;
    }
    
    um_ubus_update_cb(dst, src);
    
    /*
    * dst is new
    */
    if (0==dst->portal.uptime) {
        dst->portal.uptime = now;
    }
    dst->portal.livetime = now - dst->portal.uptime;
    
    os_objdcpy(&dst->portal.rx, &src->portal.rx);
    os_objdcpy(&dst->portal.tx, &src->portal.tx);
    
    dst->portal.type = src->portal.type;
    dst->portal.state = src->portal.state;
}

static int 
l3user_timer(struct um_intf *intf)
{
    struct apuser info, *user;
    
    um_user_init(&info, true);

    /*
    * TODO: get l3user from wifidog
    */

    user = um_user_update(&info, l3user_update);
    if (NULL==user) {
        return -ENOMEM;
    }

    return 0;
}

int
um_l3user_timer(void)
{
#if 0
    struct um_intf *intf;
    int err = 0;
    
    /*
    * foreach intf in wlan cfg
    */
    list_for_each_entry(intf, &umc.uci.wlan.cfg, node) {
        err = l3user_timer(intf);
        if (err<0) {
            return err;
        }
    }
#endif

    return 0;
}

/******************************************************************************/
