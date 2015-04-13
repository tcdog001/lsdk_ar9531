struct bridge;
struct port;
struct bridge {
    struct bridge *next;
    int ifindex;
    char ifname[IFNAMSIZ];
    struct port *firstport;
    struct port *ports[256];
};

struct port {
    struct port *next;
    int index;
    int ifindex;
    char ifname[IFNAMSIZ];
    struct bridge *parent;
};
a_status_t
acfg_get_br_name(a_uint8_t *ifname, char *brname);      

