#ifndef __PARTOOL_H_049D6308912C0CFE63093A469A2476FB__
#define __PARTOOL_H_049D6308912C0CFE63093A469A2476FB__
/******************************************************************************/
#include "utils/list.h"

#ifndef PART_BLOCK_CACHE_SIZE
#define PART_BLOCK_CACHE_SIZE   (64*1024)
#endif

struct part_var {
    int len;
    char *var;
};

struct part_cursor {
    struct part_var k;
    struct part_var v;
};

typedef struct part_block * part_block_t;


enum {
    /*
    * normal read(flash==>block)
    */
    PART_MODE_NORMAL,
    
    /*
    * erase flash
    */
    PART_MODE_EMPTY,
    
    /*
    * check crc
    */
    PART_MODE_CRC,
    
#ifndef __BOOT__
    /*
    * load file to block
    * and write to flash
    */
    PART_MODE_LOAD,
#endif

    PART_MODE_END
};

static inline bool
is_good_part_mode(int mode)
{
    return is_good_enum(mode, PART_MODE_END);
}

static inline char *
part_mode_string(int mode)
{
    char *string[PART_MODE_END] = {
        "normal",
        "empty",
        "crc",
#ifndef __BOOT__
        "load",
#endif
    };

    if (is_good_part_mode(mode)) {
        return string[mode];
    } else {
        return __unknow;
    }
}

/*
* create block
*   alloc cache memory
*   read from partition to cache
*   resolve cache to list
*
*   @block_size: same to partition size
*   @read: in this callback, you should do below
*       (1) partition==>tmp buf/file(you do it)
*       (2) tmp buf/file==>block cache(call part_block_write)
*       in os, use tmp file
*       in boot, use tmp buf
*   @write: in this callback, you should do below
*       (1) block cache==>tmp buf/file(call part_block_read)
*       (2) tmp buf/file==>partition(you do it)
*   @partition: partition name
*/
extern part_block_t 
part_block_create(
    int mode,
    unsigned int block_size, 
    int (*read)(part_block_t block, char *partition), /* partition ==> cache */
    int (*write)(part_block_t block, char *partition),/* cache ==> partition */
    char *partition,
    char *human, /* just for PART_MODE_LOAD */
    unsigned int xor
);

/*
* clean block
*   flush list to cache
*   flush cache to partition
*   free block memory
*/
extern void
__part_block_free(part_block_t block);

#define part_block_free(_block) do{ \
    __part_block_free(_block);      \
    (_block) = NULL;                \
}while(0)

/*
* copy data from block cache to buf
*/
extern int
part_block_read(part_block_t block, unsigned int offset, void *buf, unsigned int size);

/*
* copy data from buf to block cache
*/
extern int
part_block_write(part_block_t block, unsigned int offset, void *buf, unsigned int size);

extern int
part_block_crc_get(part_block_t block, unsigned int *crc_part, unsigned int *crc_calc);

extern bool
part_block_is_good(struct part_block *block);

/*
* find k/v from block list
*/
extern int
part_var_find(part_block_t block, char *name, struct part_cursor *c);

/*
* @c : kv cursor, readonly!!!
*/
typedef multi_value_t part_var_foreach_f(part_block_t block, struct part_cursor *c, void *data);

extern int
part_var_foreach(part_block_t block, part_var_foreach_f *foreach, void *data);

#ifndef __BOOT__
/*
* foreach k/v, prefix match k's name by @name
*/
extern int
part_var_foreach_byname_prefix(part_block_t block, char *name, part_var_foreach_f *foreach, void *data);
#endif

#define PART_VAR_CREATE_IF_NOT_EXIT     0x01
#define PART_VAR_UPDATE_IF_EXIT         0x02
extern int
__part_var_create(part_block_t block, char *name, char *value, int flag);

/*
* create k/v in block list
*   if not exist, create new
*   if exist, return -EEXIST
*/
static inline int
part_var_create(part_block_t block, char *name, char *value)
{
    return __part_var_create(block, name, value, PART_VAR_CREATE_IF_NOT_EXIT);
}

/*
* update k/v in block list
*   if not exist, return NULL
*   if exist, update it
*/
static inline int
part_var_update(part_block_t block, char *name, char *value)
{
    return __part_var_create(block, name, value, PART_VAR_UPDATE_IF_EXIT);
}

/*
* create k/v in block list
*   if not exist, create new
*   if exist, update it
*/
static inline int
part_var_new(part_block_t block, char *name, char *value)
{
    return __part_var_create(block, name, value, PART_VAR_CREATE_IF_NOT_EXIT | PART_VAR_UPDATE_IF_EXIT);
}

/*
* delete k/v in block list
*/
extern int
part_var_delete(part_block_t block, char *name);

#ifndef __BOOT__
extern int
part_var_delete_byname_prefix(part_block_t block, char *name);
#endif

/******************************************************************************/
#endif /* __PARTOOL_H_049D6308912C0CFE63093A469A2476FB__ */
