#include "utils.h"
#include "utils/cmd.h"

/* {"-load"} */
static int 
reload(int argc, char *argv[])
{
    appkey_reload();
    
    return 0;
}

int main(int argc, char *argv[])
{
    struct command_item commands[] = {
        {
            .list = {"-reload"},
            .func = reload,
            .help = "load appkey",
        },
    };
    struct command_ctrl ctrl = COMMAND_CTRL_INITER(commands);

    os_sigaction_default();
    
    return os_do_command(argc, argv, &ctrl);
}

/******************************************************************************/
AKID_DEBUGER; /* must last os_constructor */

