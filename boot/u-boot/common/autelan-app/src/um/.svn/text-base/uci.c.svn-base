#include "utils.h"
#include "um.h"

static void
deluser_byif(struct um_intf *intf)
{
    struct user_filter f = USER_FILTER_INITER(true);

    switch(intf->type) {
        case UM_INTF_RADIO:
            f.radioid = intf->radioid;
            break;
        case UM_INTF_WLAN:
            f.radioid = intf->radioid;
            f.wlanid = intf->wlanid;
            break;
        default:
            return;
    }
    
    um_user_delby(&f);
}

static struct um_intf *
intf_create(char *ifname)
{
    struct um_intf *intf = NULL;

    intf = (struct um_intf *)os_zalloc(sizeof(*intf));
    if (NULL==intf) {
        return NULL;
    }
    os_strdcpy(intf->ifname, ifname);

    debug_uci_ok("load intf(%s)", ifname);
    
    return intf;
}

static void
intf_destroy(struct um_intf *intf)
{
    debug_uci_ok("unload intf(%s)", intf->ifname);
    
    os_free(intf);
}

static void
intf_insert(struct um_intf *intf, struct list_head *head)
{
    list_add(&intf->node, head);
}

static void
intf_remove(struct um_intf *intf, bool delete_user)
{
    if (delete_user) {
        deluser_byif(intf);
    }
    
    list_del(&intf->node);
}

static void 
uci_init(void)
{
    umc.uci.ctx = uci_alloc_context();
    if (NULL==umc.uci.ctx) {
        debug_uci_error("open uci context failed");

        return;
    } else {
        debug_uci_ok("open uci context");
    }

#if 0
    uci_set_confdir(umc.uci.ctx, "/etc/config");
    debug_uci_trace("set uci config path");
#endif
}

static void 
uci_fini(void)
{
    if (umc.uci.ctx) {
        uci_free_context(umc.uci.ctx);

        debug_uci_ok("close uci context");
    }
}

static void
package_close(struct uci_package *p)
{
    if (umc.uci.ctx && p) {
        uci_unload(umc.uci.ctx, p);

        debug_uci_ok("close uci package(%p)", p);
    }
}

static struct uci_package *
package_open(char *name)
{
    struct uci_package *package = NULL;
    int err = 0;
    
    err = uci_load(umc.uci.ctx, name, &package);
    if (err) {
        debug_uci_error("open uci package(%s) failed", name);
    } else {
        debug_uci_ok("open uci package(%s)", name);
    }

    return package;
}


static void
section_to_blob
(
    struct uci_section *s, 
    struct uci_blob_param_list *param, 
    struct blob_attr *tb[],
    int count
)
{
    if (param->n_params == count) {
        um_blob_buf_init();
        uci_to_blob(&b, s, param);
        blobmsg_parse(param->params, param->n_params, tb, blob_data(b.head), blob_len(b.head));
    } else {
        os_assert(0);
    }
}

static struct um_intf *
__load_intf(char *ifname, struct blob_attr *tb[], int count, struct list_head *head)
{
    struct blob_attr *p;
    struct um_intf *intf = NULL;
    bool disable = false;
    
    p = tb[UM_INTFPOLICY_DISABLE];
    if (p) {
        disable = blobmsg_get_bool(p);
    }
    
    if (disable) {
        debug_uci_trace("no load disabled intf(%s)", ifname);
        
        return NULL;
    }
    
    intf = intf_create(ifname);
    if (NULL==intf) {
        debug_uci_error("create intf(%s) failed", ifname);
        
        return NULL;
    }
    intf_insert(intf, head);

    debug_uci_trace("load intf(%s) to tmp", ifname);
    
    return intf;
}

static int
load_radio(struct uci_section *s, struct blob_attr *tb[], int count, struct list_head *head)
{
    struct um_intf *intf;
    char *ifname = s->e.name;
    int radioid = 0;

    intf = __load_intf(ifname, tb, count, head);
    if (NULL==intf) {
        return 0;
    }
    intf->type = UM_INTF_RADIO;
    
    if (1!=os_sscanf(ifname, "radio%d", &radioid)) {
        debug_uci_error("load radio failed(bad name:%s)", ifname);
        
        return -EFORMAT;
    }
    intf->radioid = radioid;
    
    debug_uci_trace("load %s to tmp", ifname);
    
    return 0;
}

static int
load_wlan(struct uci_section *s, struct blob_attr *tb[], int count, struct list_head *head)
{
    struct blob_attr *p;
    struct um_intf *intf;
    char *ifname = NULL;
    int radioid = 0;
    int wlanid  = 0;

    p = tb[UM_WLANPOLICY_IFNAME];
    if (p) {
        ifname = blobmsg_get_string(p);
    } else {
        debug_uci_trace("load wlan without ifname");

        return -ENOEXIST;
    }

    intf = __load_intf(ifname, tb, count, head);
    if (NULL==intf) {
        return -ENOMEM;
    }
    os_sscanf(ifname, "wlan%d-%d", &radioid, &wlanid);
    intf->type      = UM_INTF_WLAN;
    intf->radioid   = radioid;
    intf->wlanid    = wlanid;
    
    debug_uci_trace("load %s", ifname);
    
    return 0;
}

static void
load_intf(
    struct uci_package *wireless,
    struct um_uci *uci,
    int (*load)(struct uci_section *s, struct blob_attr *tb[], int count, struct list_head *head)
)
{
    struct uci_element *e = NULL;
    int count = uci->param.n_params;
    struct blob_attr **tb = (struct blob_attr **)os_alloca(count * sizeof(struct blob_attr *));
    
    if (NULL==tb) {
        return;
    }
    
	uci_foreach_element(&wireless->sections, e) {
		struct uci_section *s = uci_to_section(e);
		
		if (0==os_strcmp(s->type, uci->uci_type)) {
    		section_to_blob(s, &uci->param, tb, count);
    		(*load)(s, tb, count, &uci->tmp);
		}
	}
}

static int
intf_compare(int type)
{
    struct um_intf *intf_cfg, *intf_tmp, *a, *b;
    struct list_head *cfg;
    struct list_head *tmp;

    switch(type) {
        case UM_INTF_RADIO:
            cfg = &umc.uci.radio.cfg;
            tmp = &umc.uci.radio.tmp;
            break;
        case UM_INTF_WLAN:
            cfg = &umc.uci.wlan.cfg;
            tmp = &umc.uci.wlan.tmp;
            break;
        default:
            return -EINVAL0;
    }
    
    /*
    * delete the intf(in cfg, NOT in tmp)
    */
    list_for_each_entry_safe(intf_cfg, a, cfg, node) {
        bool matched = false;
        
        list_for_each_entry_safe(intf_tmp, b, tmp, node) {
            if (0==os_stracmp(intf_cfg->ifname, intf_tmp->ifname)) {
                matched = true;
                
                break;
            }
        }
        
        if (false==matched) {
            intf_remove(intf_cfg, true);
            intf_destroy(intf_cfg);
            
            debug_uci_trace("delete %s from cfg", intf_cfg->ifname);
        }
    }

    /*
    * move the intf(NOT in cfg, in tmp), tmp==>cfg
    */
    list_for_each_entry_safe(intf_tmp, a, tmp, node) {
        bool matched = false;
        
        list_for_each_entry_safe(intf_cfg, b, cfg, node) {
            if (0==os_stracmp(intf_cfg->ifname, intf_tmp->ifname)) {
                matched = true;
                
                break;
            }
        }
        
        /*
        * the intf in tmp, but NOT in cfg, move it(tmp==>cfg)
        */
        if (false==matched) {
            intf_remove(intf_tmp, false);
            intf_insert(intf_tmp, cfg);

            debug_uci_trace("move %s from tmp to cfg", intf_tmp->ifname);
        }
        /*
        * the intf in both tmp and cfg
        */
        else {
            debug_uci_trace("keep %s in cfg", intf_tmp->ifname);
        }
    }

    return 0;
}

static int
load_wireless(struct uci_package *wireless)
{
	load_intf(wireless, &umc.uci.radio, load_radio);
	debug_uci_trace("load radio to tmp");
	
	load_intf(wireless, &umc.uci.wlan, load_wlan);
    debug_uci_trace("load wlan to tmp");
    
	intf_compare(UM_INTF_RADIO);
	debug_uci_trace("radio compare(tmp==>cfg)");
	
	intf_compare(UM_INTF_WLAN);
	debug_uci_trace("wlan compare(tmp==>cfg)");
    
	return 0;
}

int
um_uci_load(void)
{
    struct uci_package *wireless = NULL;
    int err = 0;
    
    uci_init();
    
    wireless = package_open("wireless");
    if (NULL==wireless) {
        err = -EINVAL1;
        debug_uci_error("wireless open failed(%d)", err);
        
        goto error;
    }

    err = load_wireless(wireless);
    if (err<0) {
        debug_uci_error("wireless load failed(%d)", err);
        
        goto error;
    }

    debug_uci_ok("uci load");

    /* go down */
error:
    package_close(wireless);
    uci_fini();
    
    return err;
}

/******************************************************************************/
