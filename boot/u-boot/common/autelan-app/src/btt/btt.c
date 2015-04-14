#include "utils.h"
#include "utils/cmd.h"

enum {
    BT_DIV_0,
    BT_DESTROY_STACK_FOREVER,
    BT_WRITE_POINTER,
    BT_READ_POINTER,

    BT_END
};

static int
bt_div_0(void)
{
    int a = 1024;
    int b = 0;
    int n;

    os_println("before div 0");
    n = a/b;
    os_println("after div 0");
    
    return 0;
}

static int
bt_destroy_stack_forever(bool up)
{
    os_println("before write stack %s", up?"up":"down");
    BACKTRACE_DESTROY_STACK_FOREVER(up);
    os_println("after write stack %s", up?"up":"down");
    
    return 0;
}

static int
bt_write_pointer(int *p)
{
    os_println("before write pointer(%p)", p);
    *p = 0;
    os_println("after write pointer(%p)", p);

    return 0;
}

static int
bt_read_pointer(int *p)
{
    int a;

    os_println("before read pointer(%p)", p);
    a = *p;
    os_println("after read pointer(%p)=0x%x", p, a);

    return 0;
}

static bool up = true;
static int *pointer;

static int 
callstack_21(int type)
{
    switch(type) {
        case BT_DIV_0:
            return bt_div_0();
        case BT_DESTROY_STACK_FOREVER:
            return bt_destroy_stack_forever(up);
        case BT_WRITE_POINTER:
            return bt_write_pointer(pointer);
        case BT_READ_POINTER:
            return bt_read_pointer(pointer);
        default:
            return 0;
    }
}

#define BT_FUNC_LIST(_) \
        _(20,21) \
        _(19,20) \
        _(18,19) \
        _(17,18) \
        _(16,17) \
        _(15,16) \
        _(14,15) \
        _(13,14) \
        _(12,13) \
        _(11,12) \
        _(10,11) \
        _(09,10) \
        _(08,09) \
        _(07,08) \
        _(06,07) \
        _(05,06) \
        _(04,05) \
        _(03,04) \
        _(02,03) \
        _(01,02) \
        _(00,01) \
        extern int __noused /* just for sourceinsight */

#define FUNC(a, b) \
static int \
callstack_##a(int type) \
{ \
    int err; \
    os_println("enter %s", __func__); \
    err = callstack_##b(type); \
    os_println("leave %s", __func__); \
    return err; \
}

BT_FUNC_LIST(FUNC);

static int
bt_callstack(int type)
{
    return callstack_00(type);
}

/* {"-div0"} */
static int 
cmd_div0(int argc, char *argv[])
{
    return bt_callstack(BT_DIV_0);
}

/* {"-destroystack", "up|down"} */
static int
cmd_destroy_stack(int argc, char *argv[])
{
    char *direction = argv[2];

    if (0==os_strcmp(direction, "up")) {
        up = true;
    } 
    else if (0==os_strcmp(direction, "down")) {
        up = false;
    }
    else {
        os_println("bad direction(%s)", direction);
        
        return 0;
    }
    
    return bt_callstack(BT_DESTROY_STACK_FOREVER);
}

static int *
getpointer(char *address)
{
    int *p = NULL;
    
    if (1!=os_sscanf(address, "%p", &p)) {
        os_println("bad pointer address(%s)", address);
        
        return NULL;
    } else {
        return p;
    }
}

/* {"-writepointer", "pointer"} */
static int
cmd_write_pointer(int argc, char *argv[])
{
    pointer = getpointer(argv[2]);
    if (NULL==pointer) {
        return -EINVAL;
    }

    return bt_callstack(BT_WRITE_POINTER);
}

/* {"-readpointer", "pointer"} */
static int
cmd_read_pointer(int argc, char *argv[])
{
    pointer = getpointer(argv[2]);
    if (NULL==pointer) {
        return -EINVAL;
    }
    
    return bt_callstack(BT_READ_POINTER);
}

int main(int argc, char *argv[])
{
    struct command_item commands[] = {
        {
            .list = {"-div0"},
            .func = cmd_div0,
            .help = "test div 0",
        },
        {
            .list = {"-stack", "up|down"},
            .func = cmd_destroy_stack,
            .help = "test destroy stack",
        },
        {
            .list = {"-write", "pointer(0xXXXXXXXX)"},
            .func = cmd_write_pointer,
            .help = "test write pointer",
        },
        {
            .list = {"-read", "pointer(0xXXXXXXXX)"},
            .func = cmd_read_pointer,
            .help = "test read pointer",
        },
    };
    
    struct command_ctrl ctrl = COMMAND_CTRL_INITER(commands);

    os_sigaction_default();
    
    return os_do_command(argc, argv, &ctrl);
}

AKID_DEBUGER; /* must last os_constructor */
/******************************************************************************/
