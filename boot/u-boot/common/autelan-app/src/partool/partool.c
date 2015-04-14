/*******************************************************************************
Copyright (c) 2012-2015, Autelan Networks. All rights reserved.
*******************************************************************************/
#ifdef __BOOT__
#include <common.h>
#include <command.h>
#endif
#include "utils.h"
#include "utils/cmd.h"
#include "partool/partool.h"
#ifdef __BOOT__
#include "partoolapi.h"
#endif

static int  partool_size;
static part_block_t blk;

/*
* 如果新产品的定义与本文件默认定义不一致
* 则应该在makefile中重定义以下信息
*/
#ifndef NAME_OSENV
#ifdef __BOOT__
#define NAME_OSENV      "osenv"
#else
#define NAME_OSENV      "mtd2"
#endif
#endif

#ifndef NAME_PRODUCT
#ifdef __BOOT__
#define NAME_PRODUCT    "product"
#else
#define NAME_PRODUCT    "mtd8"
#endif
#endif

#ifndef ADDR_OSENV
#define ADDR_OSENV      0x9f050000
#endif

#ifndef ADDR_PRODUCT
#define ADDR_PRODUCT    0x9ffe0000
#endif

#ifndef SIZE_OSENV
#define SIZE_OSENV      PART_BLOCK_CACHE_SIZE
#elif SIZE_OSENV > PART_BLOCK_CACHE_SIZE
#error "SIZE_OSENV > PART_BLOCK_CACHE_SIZE"
#endif

#ifndef SIZE_PRODUCT
#define SIZE_PRODUCT    PART_BLOCK_CACHE_SIZE
#elif SIZE_PRODUCT > PART_BLOCK_CACHE_SIZE
#error "SIZE_PRODUCT > PART_BLOCK_CACHE_SIZE"
#endif

#define ADDR_BAD        0xffffffff
#define NAME_GOOD       NAME_OSENV "|" NAME_PRODUCT
#define NAME_HELP       "partname must be " NAME_GOOD

static struct {
    char *partition;
    unsigned long addr;
    int size;
} part_info[] = {
    {
        .partition  = NAME_OSENV,
        .addr       = ADDR_OSENV,
        .size       = SIZE_OSENV,
    },
    {
        .partition  = NAME_PRODUCT,
        .addr       = ADDR_PRODUCT,
        .size       = SIZE_PRODUCT,
    }
};

static inline unsigned long 
part_begin(char *partition)
{
    int i;

    for (i=0; i<os_count_of(part_info); i++) {
        if (0==os_strcmp(partition, part_info[i].partition)) {
            partool_size = part_info[i].size;
            
            return part_info[i].addr;
        }
    }
    
    return ADDR_BAD;
}

#ifdef __BOOT__

static byte part_tmp[PART_BLOCK_CACHE_SIZE];

static inline void
part_clean(void)
{
    os_do_nothing;
}

static inline int
part_init(char *partition)
{
    if (ADDR_BAD==part_begin(partition)) {
        os_println(NAME_HELP);
        
        return -EINVAL9;
    } else {
        return 0;
    }
}

static inline int
part_read(char *partition)
{
    unsigned long begin = part_begin(partition);
    byte *tmp;
    int i;

    if (ADDR_BAD==begin) {
        return -EINVAL7;
    }
    
    for(i=0, tmp=(byte *)begin; i<partool_size; i++, tmp++) {
	    part_tmp[i] = *tmp;
	}

	return 0;
}

static inline int
part_write(char *partition)
{
    unsigned long begin = part_begin(partition);
    unsigned long end   = begin + partool_size - 1;
    byte *tmp;
    int err;
    
    if (ADDR_BAD==begin) {
        return -EINVAL8;
    }
    
    err = flash_sect_erase(begin, end);
    if (err) {
        debug_error("flash erase(begin:0x%x end:0x%x size:0x%x) failed(%d)", 
            begin, end, partool_size, err);
        return err;
    }

    err = flash_write(part_tmp, begin, partool_size);
    if (err) {
        debug_error("flash write(begin:0x%x end:0x%x size:0x%x) failed(%d)", 
            begin, end, partool_size, err);
        return err;
    }

	return 0;
}

#else

#ifndef PART_BASE_PATH
#ifdef  __TEST__
#define PART_BASE_PATH "."
#else
#define PART_BASE_PATH "/dev"
#endif
#endif

static char partool_tmpfile[1+OS_FILENAME_LEN];
static char partool_mtdfile[1+OS_FILENAME_LEN];
static char *partool_humfile;
static byte *part_tmp;

static inline void
init_tmp_filename(char *mtd)
{
    int r, pid, t;
    
    pid = getpid();
    t = time(NULL);
    srand(t);
    r = rand();

    os_saprintf(partool_tmpfile, "/tmp/%s-%u.%u.%u", mtd, pid, t, r);
}

static inline void
init_mtd_filename(char *mtd)
{
    os_saprintf(partool_mtdfile, PART_BASE_PATH "/%s", mtd);
}

static inline void
rm_fille(char *filename)
{
    /*
    * in debugging trace
    *   keep tmp file
    */
    if (false==__is_debug_init_trace && filename[0]) {
        os_v_system("rm -fr %s", filename);

        filename[0] = 0;
    }
}

static inline void
tmp_2_mtd(void)
{
#if defined(PART_RW_MTD)
#   define part_write_format    "mtd -q write %s %s"
#elif defined(PART_RW_DD)
#   define part_write_format    "dd if=%s of=%s"
#elif defined(PART_RW_CP)
#   define part_write_format    "cp -f %s %s"
#else
#   error "must define PART_RW_MTD|PART_RW_DD|PART_RW_CP in makefile"
#endif

    os_v_system(part_write_format, partool_tmpfile, partool_mtdfile);

    os_free(part_tmp);
}

static inline int
mtd_2_tmp(char *mtd)
{
    int size;
    
#if defined(PART_RW_MTD) || defined(PART_RW_CP)
#   define part_read_format     "cp -f %s %s"
#elif defined(PART_RW_DD)
#   define part_read_format     "dd if=%s of=%s"
#else
#   error "must define PART_RW_MTD|PART_RW_DD|PART_RW_CP in makefile"
#endif
    init_mtd_filename(mtd);
    init_tmp_filename(mtd);
    
    os_v_system(part_read_format, partool_mtdfile, partool_tmpfile);

    size = os_sfsize(partool_tmpfile);
    if (size != partool_size) {
        return -EINVAL;
    }

    part_tmp = (byte *)os_zalloc(partool_size);
    if (NULL==part_tmp) {
        return -ENOMEM;
    }
    
    return 0;
}

static inline void
part_clean(void)
{
    rm_fille(partool_tmpfile);
}

static inline int
part_init(char *partition)
{
    if (ADDR_BAD==part_begin(partition)) {
        os_println(NAME_HELP);
        
        return -EFORMAT;
    } else {
        return mtd_2_tmp(partition);
    }
}

static inline int 
part_read(char *partition)
{
    FILE *f = NULL;
    int err;
    
    f = fopen(partition, "r");
    if (NULL==f) {
        err = errno;

        goto error;
    }

    err = fread(part_tmp, partool_size, 1, f);
    if (1!=err) {
        err = errno;

        goto error;
    }

    err = 0;
error:
    if (f) {
        fclose(f);
    }
    
    return err;
}

static inline int 
part_write(char *partition)
{
    FILE *f = NULL;
    int err;
    
    f = fopen(partition, "r+");
    if (NULL==f) {
        err = errno;

        goto error;
    }
    
    err = fwrite(part_tmp, partool_size, 1, f);
    if (1!=err) {
        err = errno;

        goto error;
    }
    
    err = 0;
error:
    if (f) {
        fclose(f);
    }
    if (0==err) {
        tmp_2_mtd();
    }
    
    return err;
}

#endif


static int 
partool_read(part_block_t block, char *partition)
{
    int err;

    err = part_read(partition);
    if (err<0) {
        goto error;
    }

    err = part_block_write(block, 0, part_tmp, partool_size);
    if (err<0) {
        goto error;
    }

    err = 0;
error:
    return err;
}

static int 
partool_write(part_block_t block, char *partition)
{
    int err;
    
    err = part_block_read(block, 0, part_tmp, partool_size);
    if (err<0) {
        goto error;
    }

    err = part_write(partition);
    if (err<0) {
        goto error;
    }
    
    err = 0;
error:
    return err;
}


static int
partool_init(int mode, int argc, char *argv[])
{
    char *partition = argv[2];
    int err;
    
    err = part_init(partition);
    if (err<0) {
        return err;
    }
    
    blk = part_block_create(
            mode, 
            partool_size, 
            partool_read, 
            partool_write,
#ifdef __BOOT__
            partition,
            NULL,
#else
            partool_tmpfile, 
            partool_humfile, 
#endif
            0);
    if (NULL==blk) {
        return -EINVAL;
    }

    return 0;
}


void
partool_clean(void)
{
    part_block_free(blk);
    
    part_clean();
}

static inline void
show_cursor(struct part_cursor *c, bool simple)
{
    if (simple) {
        os_println("%s", c->v.var);
    } else {
        os_println("%s=%s", c->k.var, c->v.var);
    }
}

static multi_value_t 
partool_show_cb(part_block_t block, struct part_cursor *c, void *data)
{
    show_cursor(c, false);
    
    (void)block;
    (void)data;
    
    return mv2_OK;
}

static int
part_error(int err, char *name)
{
    switch(err) {
        case -ENOEXIST:
            os_println("%s not exist", os_safestring(name));
            
            break;
        case -EEXIST:
            os_println("%s exist", os_safestring(name));
            
            break;
        default:
            os_println("error(%d)", err);
            
            break;
    }

    return err;
}

/* {"-part", "partname", "-crc"} */
static int
partool_crc(int argc, char *argv[])
{
    int err = 0;
    unsigned int crc_part = 0;
    unsigned int crc_calc = 0;
    
    err = partool_init(PART_MODE_CRC, argc, argv);
    if (err<0) {
        debug_error("partool init error(%d)", err);
        
        /* 
        * if block is good, NO return
        */
        if (false==part_block_is_good(blk)) {
            return err;
        }
    }

    part_block_crc_get(blk, &crc_part, &crc_calc);
    if (err<0) {
        return part_error(err, NULL);
    }

    os_println("0x%x 0x%x", crc_part, crc_calc);
    
    return 0;
}

/* {"-part", "partname", "-empty"} */
static int
partool_empty(int argc, char *argv[])
{
    int err = 0;
    
    err = partool_init(PART_MODE_EMPTY, argc, argv);
    if (err<0) {
        return part_error(err, NULL);
    }
    
    return 0;
}

/* {"-part", "partname", "-show"} */
static int
partool_show_all(int argc, char *argv[])
{
    int err;
    
    err = partool_init(PART_MODE_NORMAL, argc, argv);
    if (err<0) {
        return part_error(err, NULL);
    }
    
    err = part_var_foreach(blk, partool_show_cb, NULL);
    if (err<0) {
        return part_error(err, NULL);
    }
    
    return 0;
}

/* {"-part", "partname", "-show", "name"} */
static int 
partool_show_byname(int argc, char *argv[])
{
    struct part_cursor c;
    char *name = argv[4];
    int err = 0;
    
    err = partool_init(PART_MODE_NORMAL, argc, argv);
    if (err<0) {
        return part_error(err, name);
    }
    
    err = part_var_find(blk, name, &c);
    if (err<0) {
        return part_error(err, name);
    }

    show_cursor(&c, true);
    
    return 0;
}

#ifdef __BOOT__
int 
partool_show_byname_api(int argc, char *argv[],void *buf)
{
    struct part_cursor c;
    char *name = argv[4];
    int err = 0;
    int len;
    
    err = partool_init(PART_MODE_NORMAL, argc, argv);
    if (err<0) {
        return part_error(err, name);
    }
    
    err = part_var_find(blk, name, &c);
    if (err<0) {
        return part_error(err, name);
    }
    len=strlen(c.v.len);
    strncpy(buf, c.v.var,len);

    show_cursor(&c, true);
    
    return 0;
}
#endif

/* {"-part", "partname", "-new", "name", "value"} */
int 
partool_new(int argc, char *argv[])
{
    char *name = argv[4];
    char *value = argv[5];
    int err = 0;
    
    err = partool_init(PART_MODE_NORMAL, argc, argv);
    if (err<0) {
        return part_error(err, name);
    }

    err = part_var_new(blk, name, value);
    if (err<0) {
        return part_error(err, name);
    }
    
    return 0;
}

/* {"-part", "partname", "-delete", "name"} */
static int 
partool_delete(int argc, char *argv[])
{
    char *name = argv[4];
    int err;
    
    err = partool_init(PART_MODE_NORMAL, argc, argv);
    if (err<0) {
        return part_error(err, name);
    }

    err = part_var_delete(blk, name);
    if (err<0) {
        return part_error(err, name);
    }
    
    return 0;
}

#ifndef __BOOT__
/* {"-part", "partname", "-load", "file"} */
static int
partool_load(int argc, char *argv[])
{
    int err = 0;

    partool_humfile = argv[4];
    
    err = partool_init(PART_MODE_LOAD, argc, argv);
    if (err<0) {
        return part_error(err, NULL);
    }
    
    return 0;
}

/* {"-part", "partname", "-show", "-prefix", "name"} */
static int
partool_show_byprefix(int argc, char *argv[])
{
    char *name = argv[5];
    int err;
    
    err = partool_init(PART_MODE_NORMAL, argc, argv);
    if (err<0) {
        return part_error(err, name);
    }
    
    err = part_var_foreach_byname_prefix(blk, name, partool_show_cb, NULL);
    if (err<0) {
        return part_error(err, name);
    }

    return 0;
}

/* {"-part", "partname", "-delete", "-prefix", "name"} */
static int 
partool_delete_byprefix(int argc, char *argv[])
{
    char *name = argv[5];
    int err;
    
    err = partool_init(PART_MODE_NORMAL, argc, argv);
    if (err<0) {
        return part_error(err, name);
    }

    err = part_var_delete_byname_prefix(blk, name);
    if (err<0) {
        return part_error(err, name);
    }
    
    return 0;
}

/* {"-part", "partname", "-create", "name", "value"} */
static int 
partool_create(int argc, char *argv[])
{
    char *name = argv[4];
    char *value = argv[5];
    int err = 0;
    
    err = partool_init(PART_MODE_NORMAL, argc, argv);
    if (err<0) {
        return part_error(err, name);
    }

    err = part_var_create(blk, name, value);
    if (err<0) {
        return part_error(err, name);
    }
    
    return 0;
}

/* {"-part", "partname", "-update", "name", "value"} */
static int 
partool_update(int argc, char *argv[])
{
    char *name = argv[4];
    char *value = argv[5];
    int err;
    
    err = partool_init(PART_MODE_NORMAL, argc, argv);
    if (err<0) {
        return part_error(err, name);
    }

    err = part_var_update(blk, name, value);
    if (err<0) {
        return part_error(err, name);
    }
    
    return 0;
}
#endif

#define PARTNAME_HELP   "partname(" NAME_GOOD ")"
int 
partool_main
(
#ifdef __BOOT__
	cmd_tbl_t *cmdtp,
	int flag,
#endif
	int argc,
 	char *argv[]
)
{
    struct command_item commands[] = {
        {
            .list = {"-part", PARTNAME_HELP, "-crc"},
            .func = partool_crc,
            .help = "calc crc",
        },
        {
            .list = {"-part", PARTNAME_HELP, "-empty"},
            .func = partool_empty,
            .help = "flush empty to mtd",
        },
        {
            .list = {"-part", PARTNAME_HELP, "-show"},
            .func = partool_show_all,
            .help = "show all",
        },
        {
            .list = {"-part", PARTNAME_HELP, "-show", "name"},
            .func = partool_show_byname,
            .help = "show by name",
        },
        {
            .list = {"-part", PARTNAME_HELP, "-new", "name", "value"},
            .func = partool_new,
            .help = "new(create or update)",
        },
        {
            .list = {"-part", PARTNAME_HELP, "-delete", "name"},
            .func = partool_delete,
            .help = "delete by name, the name must must exist",
        },
#ifndef __BOOT__
        {
            .list = {"-part", PARTNAME_HELP, "-load", "file"},
            .func = partool_load,
            .help = "load file(human readable) to mtd",
        },
        {
            .list = {"-part", PARTNAME_HELP, "-show", "-prefix", "name"},
            .func = partool_show_byprefix,
            .help = "show by name prefix",
        },
        {
            .list = {"-part", PARTNAME_HELP, "-delete", "-prefix", "name"},
            .func = partool_delete_byprefix,
            .help = "delete by name prefix",
        },
        {
            .list = {"-part", PARTNAME_HELP, "-create", "name", "value"},
            .func = partool_create,
            .help = "create name/value, the name cannot exist",
        },
        {
            .list = {"-part", PARTNAME_HELP, "-update", "name", "value"},
            .func = partool_update,
            .help = "update name/value, the name must exist",
        },
#endif
    };
    
    struct command_ctrl ctrl = COMMAND_CTRL_INITER(commands);
    int err = 0;
    
    err = os_do_command(argc, argv, &ctrl);
    
    partool_clean();
    
#ifdef __BOOT__
    return 0;
#else
    return err;
#endif
}

#ifdef __BOOT__
U_BOOT_CMD(
	partool,	6,	1,	partool_main,
	"partool     - Partool utility commands\n",

	"partool -part  <partname>  -crc       - show crc\n"
	"partool -part  <partname>  -empty       - clean the mtd\n"
	"partool -part  <partname>  -show       - display <partname> info \n"
	"partool -part  <partname>  -show   <name>           - display <partname> info about <name>\n"
	"partool -part  <partname>  -new   <name>  <value>- write <partname> info about <name> \n"
	"partool -part  <partname>  -delete   <name>  - delete <partname> info about <name> \n"

);
#else
int main(int argc, char *argv[])
{
    return partool_main(argc, argv);
}

static os_destructor void
__fini(void)
{
    partool_clean();
}
#endif
/******************************************************************************/
AKID_DEBUGER; /* must last os_constructor */

