#include "utils.h"
#include "um.h"

struct blob_buf b;

static inline void
notify(char *event)
{
    ubus_notify(umc.ctx, &umc.obj.object, event, b.head, -1);
}

static void
pushuser(struct apuser *user, bool init, char *name)
{
    void *handle;
    
    if (NULL==user) {
        return;
    }

    if (init) {
        um_blob_buf_init();
    }
    
    handle = um_open_table(name);

	um_user_add_macstring(UM_USER_MAC, user->mac);
	um_user_add_macstring(UM_USER_AP, user->ap);
	um_user_add_macstring(UM_USER_VAP, user->vap);
	
	um_user_add_u32(UM_USER_WLANID, user->wlanid);
	um_user_add_u32(UM_USER_RADIOID, user->radioid);
	um_user_add_string(UM_USER_IP, os_ipstring(user->ip));
	//um_user_add_u32(UM_USER_IP, user->ip);
	um_user_add_u32(UM_USER_UPTIME, user->wifi.uptime);
	um_user_add_u32(UM_USER_LIVETIME, user->wifi.livetime);
	um_user_add_u32(UM_USER_SIGNAL, user->wifi.signal);
	um_user_add_u32(UM_USER_RX_RATE, user->wifi.rx.rate);
	um_user_add_u32(UM_USER_RX_WIFIRATE, user->wifi.rx.wifirate);
	um_user_add_u64(UM_USER_RX_BYTES, user->wifi.rx.bytes);
	um_user_add_u32(UM_USER_TX_RATE, user->wifi.tx.rate);
	um_user_add_u32(UM_USER_TX_WIFIRATE, user->wifi.tx.wifirate);
	um_user_add_u64(UM_USER_TX_BYTES, user->wifi.tx.bytes);
	
	um_user_add_bool(UM_USER_PORTAL_ENABLE, !!user->portal.uptime);
	um_user_add_string(UM_USER_PORTAL_TYPE, um_user_portal_type(user));
	um_user_add_string(UM_USER_PORTAL_STATE, um_user_portal_state(user));

    um_close_table(handle);
}

static multi_value_t
get_cb(struct apuser *user, void *data)
{
    char *name = (char *)data;
    
    pushuser(user, false, name);

    return mv2_OK;
}

static int
pushuserby(struct user_filter *filter)
{
    void *handle;
    int err;
    
    um_blob_buf_init();
    
    handle = um_open_array("users");
    err = um_user_getby(filter, get_cb, NULL);
    um_close_array(handle);
    
    return err;
}

static void 
insert_or_remove_cb(struct apuser *user, char *event)
{
    if (user->local) {        
        pushuser(user, true, "user");
    	notify(event);

    	debug_ubus_trace("%s user(%s)", event, um_macstring(user->mac));
    }
}

void
um_ubus_insert_cb(struct apuser *user)
{
    if (um_is_ev_enable(new)) {
        insert_or_remove_cb(user, "um.new");
    }
}

void
um_ubus_remove_cb(struct apuser *user)
{
    if (um_is_ev_enable(delete)) {
        insert_or_remove_cb(user, "um.delete");
    }
}

static void
update_cb(struct apuser *old, struct apuser *new)
{
    if (false==old->local && false==new->local) {
        return;
    }

    pushuser(old, true, "old");
    pushuser(new, false, "new");
	
	// user info
    notify("um.update");
    
	debug_ubus_trace("um.update user(%s)", um_macstring(old->mac));
}

void
um_ubus_update_cb(struct apuser *old, struct apuser *new)
{
    if (um_is_ev_enable(update)) {
        update_cb(old, new);
    }
}

static int
restart(void)
{
    os_println("do nothing, now!");
    
    return 0;
}

int
um_ubus_handle_restart(
    struct ubus_context *ctx, 
    struct ubus_object *obj,
    struct ubus_request_data *req, 
    const char *method,
    struct blob_attr *msg
)
{
	restart();
	return 0;
}

static int
reload(void)
{
    um_uci_load();
    
    return 0;
}

int
um_ubus_handle_reload(
    struct ubus_context *ctx, 
    struct ubus_object *obj,
    struct ubus_request_data *req, 
    const char *method,
    struct blob_attr *msg
)
{
	reload();
	return 0;
}

static int
report(void)
{
    struct user_filter f = USER_FILTER_INITER(true);
    int err;
    
    err = pushuserby(&f);
    if (err<0) {
        return err;
    }

    notify("um.report");

    return 0;
}

int
um_ubus_report(void)
{
    if (um_is_ev_enable(report)) {
        return report();
    } else {
        return 0;
    }
}

static bool
setfilter_ip(struct user_filter *filter, struct blob_attr *attr[])
{
    struct blob_attr *p     = attr[UM_GETUSER_IP];
    struct blob_attr *mask  = attr[UM_GETUSER_IPMASK];

    /* ip filter */
	if (p) {
	    struct blob_attr *onlyip[UM_GETUSER_END] = {
	        [UM_GETUSER_IP] = p,
        };
	    uint32_t ip = inet_addr(blobmsg_get_string(p));
	    
	    if (os_objeq(&onlyip, attr)) { // get user by only ip
            pushuser(um_user_getbyip(ip), true, NULL);
            
            return true;
	    } else { // get user by ip/ipmask
            filter->ip = ip;
            
            if (mask) {
        	    filter->ipmask = inet_addr(blobmsg_get_string(mask));
        	}
	    }
	}

	return false;
}

static bool
setfilter_mac(struct user_filter *filter, struct blob_attr *attr[])
{
    struct blob_attr *p     = attr[UM_GETUSER_MAC];
    struct blob_attr *mask  = attr[UM_GETUSER_MACMASK];

    /* mac filter */
	if (p) {
	    struct blob_attr *onlymac[UM_GETUSER_END] = {
	        [UM_GETUSER_MAC] = p,
        };
	    byte mac[OS_MACSIZE];
	    
	    os_getmac_bystring(mac, blobmsg_get_string(p));
	    if (os_objeq(&onlymac, attr)) { // get user by only mac
            pushuser(um_user_getbymac(mac), true, NULL);

            return true;
	    } else { // get user by mac/macmask
            os_maccpy(filter->mac, mac);
            if (mask) {
                os_getmac_bystring(filter->macmask, blobmsg_get_string(mask));
        	}
    	}
	}

	return false;
}

static void
setfilter_local(struct user_filter *filter, struct blob_attr *attr[])
{
    struct blob_attr *p = attr[UM_GETUSER_LOCAL];
    
    /* local filter */
	if (p) {
        filter->local = blobmsg_get_bool(p);
	} else {
        filter->local = false;
	}
}

static void
setfilter_ap(struct user_filter *filter, struct blob_attr *attr[])
{
    struct blob_attr *p     = attr[UM_GETUSER_AP];
    struct blob_attr *mask  = attr[UM_GETUSER_APMASK];
    
    /* ap filter */
	if (p) {
        os_getmac_bystring(filter->ap, blobmsg_get_string(p));
        if (mask) {
            os_getmac_bystring(filter->apmask, blobmsg_get_string(mask));
    	}
	}
}

static void
setfilter_radio(struct user_filter *filter, struct blob_attr *attr[])
{
    struct blob_attr *p = attr[UM_GETUSER_RADIO];

    /* radio filter */
	p = attr[UM_GETUSER_RADIO];
	if (p) {
        filter->radioid = (int)blobmsg_get_u32(p);
	}
}

static void
setfilter_wlan(struct user_filter *filter, struct blob_attr *attr[])
{
    struct blob_attr *p = attr[UM_GETUSER_WLAN];

    /* wlan filter */
	p = attr[UM_GETUSER_WLAN];
	if (p) {
        filter->wlanid = (int)blobmsg_get_u32(p);
	}
}

int
um_ubus_handle_getuser(
    struct ubus_context *ctx, 
    struct ubus_object *obj,
    struct ubus_request_data *req, 
    const char *method,
    struct blob_attr *msg
)
{
    struct user_filter f = USER_FILTER_INITER(false);
	struct blob_attr *attr[UM_GETUSER_END], *zero[UM_GETUSER_END] = {NULL};
	int err;

	blobmsg_parse(umc.policy.getuser, os_count_of(umc.policy.getuser), attr, blob_data(msg), blob_len(msg));
    if (os_objeq(&zero, attr)) {
        goto filter_ok; /* default get all user */
    }

    /*
    * set local filter first
    */
    setfilter_local(&f, attr);

    if (setfilter_ip(&f, attr) || setfilter_mac(&f, attr)) {
        goto push_ok;
    }
    
    setfilter_ap(&f, attr);
    setfilter_radio(&f, attr);
    setfilter_wlan(&f, attr);
    
filter_ok:
    err = pushuserby(&f);
    if (err<0) {
        return err;
    }

    /* down */
push_ok:
	um_ubus_send_reply(ctx, req);
	
	return 0;
}

static void
add_fd(void)
{
    ubus_add_uloop(umc.ctx);
    os_fd_set_cloexec(umc.ctx->sock.fd);
}

static void
reconnect_timer(struct uloop_timeout *timeout)
{
	static struct uloop_timeout retry = {
		.cb = reconnect_timer,
	};
	int t = 2;

	if (ubus_reconnect(umc.ctx, umc.path) != 0) {
		uloop_timeout_set(&retry, t * 1000);
		return;
	}

	add_fd();
}

static inline int 
add_object(struct ubus_object *obj)
{
	int err = 0;

	err = ubus_add_object(umc.ctx, obj);
	if (err) {
		debug_ubus_error("Failed to publish object '%s': %s\n", obj->name, ubus_strerror(err));
	}

    return 0;
}

static inline int 
add_subscriber(struct ubus_subscriber *subscriber)
{
	int err = 0;

	err = ubus_register_subscriber(umc.ctx, subscriber);
	if (err) {
		debug_ubus_error("Failed to register subscriber(%s)", ubus_strerror(err));
	}

    return 0;
}

static void
connection_lost(struct ubus_context *ctx)
{
	reconnect_timer(NULL);
}

static inline int
um_ubus_connect(char *path)
{
    umc.path = path;
	umc.ctx = ubus_connect(path);
	if (NULL==umc.ctx) {
	    debug_ubus_error("connect ubus failed");
	    
		return -EIO;
    }
	umc.ctx->connection_lost = connection_lost;

    add_fd();
    
    return 0;
}

int 
um_ubus_init(char *path)
{
	int err;

    uloop_init();
	err = um_ubus_connect(path);
	if (err<0) {
        return err;
	}
    
	add_object(&umc.obj.object);
    add_subscriber(&umc.obj.subscriber);
    
    debug_ubus_ok("ubus init");
    
	return 0;
}

void
um_ubus_fini(void)
{
    if (umc.ctx) {
	    ubus_free(umc.ctx);
	    uloop_done();
	}

	debug_ubus_ok("ubus fini");
}

/******************************************************************************/
