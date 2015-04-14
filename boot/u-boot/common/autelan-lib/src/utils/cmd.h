#ifndef __CMD_H_F3687584F159827DAA20B322924194D1__
#define __CMD_H_F3687584F159827DAA20B322924194D1__
/******************************************************************************/

#ifndef __COMMAND_COUNT
#define __COMMAND_COUNT         (2*32)
#endif

#define __COMMAND_F_HIDE        0x01

/*
* 两种模式
*  (1): 参数之间没有关联关系，可以任意组合
*       os_do_command 需要循环处理所有匹配的命令
*       目前不支持
*
*  (2): 参数之间有关联关系，不能任意组合，需要列出全部有效参数组合
*       os_do_command 只处理首次匹配到的命令
*       目前支持的模式
*/

struct command_item {
    /*
    * list数组元素实际个数
    * 无需初始化，do_command 内部会扫描list自动计算
    */
    int count;
    
    /*
    * 命令列表，用于描述命令格式
    *   比如命令 xxx -a address -v value1 value2, 则
    *       list[0] 初始化为 "-a"，     命令关键字
    *       list[1] 初始化为 "address"，"-a"的参数
    *       list[2] 初始化为 "-v"，     命令关键字
    *       list[3] 初始化为 "value1"， "-v"的第一个参数
    *       list[4] 初始化为 "value2"， "-v"的第二个参数
    *       list剩余为NULL
    *
    * os_do_command 会根据 list 自动生成帮助信息，要求
    *   (1)程序至少有一个参数(main's argc>1)，如果无参数会自动生成并打印帮助
    *   (2)执行 "xxx -h" 会自动生成并打印帮助，但不会显示隐藏命令
    *   (3)执行 "xxx --" 会自动生成并打印帮助，包括隐藏命令
    */
    char *list[__COMMAND_COUNT+1];
    
    /*
    * 对整条命令的帮助信息, 可以为 NULL
    */
    char *help;
    
    /*
    * 命令执行函数
    *   argc, argv 是从 main 函数传入的, argv[0] 是程序名
    */
    int (*func)(int argc, char *argv[]);
    
    /*
    * 命令标识
    *   __COMMAND_F_HIDE 置位表示命令为隐藏命令
    *       执行 "xxx" 或 "xxx -h" 时不显示帮助信息，但实际可执行
    *       执行 "xxx --" 会显示帮助信息, "xxx --"自身不在help信息中
    */
    unsigned int flag;

    /*
    * 命令有效性控制，与 command_ctrl is_use_mask/bit 配合使用
    *
    * 先使用32位，后续如不够用改为64位
    */
    unsigned int mask;
};

struct command_ctrl {
    struct command_item *cmd;   /* array of (struct command_item) */
    int count;                  /* count of cmd         */
    /*
    * 如果 is_use_mask 为假，则 bit 及 command的mask 均无意义
    * 如果 is_use_mask 为真，则 对每条命令执行掩码测试 ((1<<ctrl's bit) & cmd's mask)
    *   测试结果为真，则此条命令有效
    *   否则，此条命令无效(不生成帮助，不能执行)
    * 目前主要用于过滤特定产品相关命令
    */
    bool is_use_mask;
    int bit;
};

#define __COMMAND_ARG_HELPS     "-h"
#define __COMMAND_ARG_HELPL     "-help"
#define __COMMAND_ARG_HIDE      "--"

#define __COMMAND_MASK_ALL      0xffffffff

#define __COMMAND_CTRL_INITER(_cmd, _is_use_mask, _bit) { \
    .cmd        = _cmd,             \
    .count      = os_count_of(_cmd),\
    .is_use_mask= _is_use_mask,     \
    .bit        = _bit,             \
}
#define COMMAND_CTRL_INITER(_cmd) \
        __COMMAND_CTRL_INITER(_cmd, false, 0)

static inline struct command_item *
__command_getbyindex(struct command_ctrl *ctrl, int idx)
{
    return &ctrl->cmd[idx];
}


static inline int 
__command_list_count(struct command_item *cmd)
{
    int i;

    for (i=0; i<__COMMAND_COUNT && cmd->list[i]; i++) {
        ;
    }

    return i;
}

static inline void 
__command_init_one(struct command_ctrl *ctrl, int idx)
{
    struct command_item *cmd = __command_getbyindex(ctrl, idx);
    
    /*
    * step:1
    *
    * get command count
    */
    cmd->count = __command_list_count(cmd);

    /*
    * step:2
    *
    * rewrite command mask
    */
    if (ctrl->is_use_mask && 0 == cmd->mask) {
        cmd->mask = __COMMAND_MASK_ALL;
    }
}


static inline void 
__command_init(struct command_ctrl *ctrl)
{
    int i;
    
    for (i=0; i<ctrl->count; i++) {
        __command_init_one(ctrl, i);
    }
}

static inline bool 
__is_command_used(struct command_ctrl *ctrl, int idx)
{
    struct command_item *cmd = __command_getbyindex(ctrl, idx);
    
    return (false == ctrl->is_use_mask || os_hasbit(cmd->mask, ctrl->bit));
}

static inline bool 
__is_command_hide(struct command_item *cmd)
{
    return os_hasflag(cmd->flag, __COMMAND_F_HIDE);
}


static inline void 
__commond_do_help_one(struct command_item *cmd)
{
    int i;

    /* 
    * __tab
    */
    os_printf(__tab);
    
    for (i=0; i<cmd->count && cmd->list[i]; i++) {
        os_printf( "%s ", cmd->list[i]);
    }
    
    /* 
    * help string
    */
    os_printf( __crlf __tab2 "%s" __crlf, cmd->help?cmd->help:"......");
}


static inline void 
__commond_help_one(struct command_ctrl *ctrl, int idx, bool show_hide)
{
    struct command_item *cmd = __command_getbyindex(ctrl, idx);
    
    if (false == __is_command_used(ctrl, idx)) {
        return;
    }
    
    /*
    * is hide command, and not show_hide
    */
    if (__is_command_hide(cmd) && false==show_hide) {
        return;
    }
    
    /*
    * then, make help
    */
    __commond_do_help_one(cmd);
}


static inline void 
__commond_help(struct command_ctrl *ctrl, char *name, bool show_hide)
{
    int i;
    
    /* 
    * help head
    */
    os_println("%s:", name);
    
    for (i=0; i<ctrl->count; i++) {
        __commond_help_one(ctrl, i, show_hide);
    }
}

static inline bool 
__command_match(int argc, char *argv[], struct command_ctrl *ctrl, int idx)
{
    int i;
    struct command_item *cmd = __command_getbyindex(ctrl, idx);

    if ((argc-1)!=cmd->count) {
        return false;
    }
    /*
    * not match current
    */
    else if (false==__is_command_used(ctrl, idx)) {
        return false;
    }
    
    for (i=0; i<cmd->count && cmd->list[i]; i++) {
        if ('-' != cmd->list[i][0]) {
            /* 
            * not begin with '-', need not compare 
            */
            continue;
        }
        
        if (0 != os_strcmp(argv[i+1], cmd->list[i])) {
            /* 
            * main's argv != cmd's arg
            */
            return false;
        }
    }
    
    return (i == cmd->count);
}

/* os_do_command 举例

struct command_item commands[] = {
    {
        .list = {"-r", "-a", "address"},    // xxx -r -a address
        .func = reg_read,                   // command function
        .help = "read from the address.",   // command help
    },
    {
        .list = {"-w", "-a", "address", "-v", "value"}, // xxx -w -a address -v value
        .func = reg_write,                      // command function
        .help = "write value to the address.",  // command help
        .flag = __COMMAND_F_HIDE,                 // hide command
    },
};

struct command_ctrl ctrl = __COMMAND_CTRL_INITER(commands, true, 0xffffffff);

os_do_command(argc, argv, &ctrl);

*/
static inline int 
os_do_command(int argc, char *argv[], struct command_ctrl *ctrl)
{
    int i;
    bool show_hide = false;
    
    __command_init(ctrl);

    if (1==argc) {
        goto show_help;
    }
    else if (2 == argc) {
        char *help = argv[1];

        /*
        * "xxx -h"
        * "xxx -help"
        * "xxx --"
        */
        if (0 == os_strcmp(help, __COMMAND_ARG_HELPS)   ||
            0 == os_strcmp(help, __COMMAND_ARG_HELPL)) {
            goto show_help;
        }
        else if (0 == os_strcmp(help, __COMMAND_ARG_HIDE)) {
            show_hide = true;
            goto show_help;
        }
    }

    for (i=0; i<ctrl->count; i++) {
        if (__command_match(argc, argv, ctrl, i)) {
            struct command_item *cmd = __command_getbyindex(ctrl, i);
            
            return (*cmd->func)(argc, argv);
        }
    }
    
show_help:
    __commond_help(ctrl, argv[0], show_hide);
    
    return -EFORMAT;
}

/******************************************************************************/
#endif /* __CMD_H_F3687584F159827DAA20B322924194D1__ */
