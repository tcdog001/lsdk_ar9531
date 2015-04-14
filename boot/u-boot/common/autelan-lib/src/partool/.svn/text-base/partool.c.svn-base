/******************************************************************************/
#include "utils.h"
#include "partool.h"
#ifdef __BOOT__
int __AKID_DEBUG = 0;
#endif

#ifndef PART_XOR
#define PART_XOR            0x94f00d1e
#endif

#ifndef PART_MAX_LINE 
#define PART_MAX_LINE       OS_LINE_LEN
#endif

#ifndef PART_COMMENT
#define PART_COMMENT        '#'
#endif

#ifndef PART_SEPERATOR
#define PART_SEPERATOR      '\t'
#endif

#ifndef PART_CRLF
#define PART_CRLF           '\n'
#endif

#ifndef part_seperators
#define part_seperators     "\t "
#endif

#ifndef PART_BLOCK_DUMP_LEN
#define PART_BLOCK_DUMP_LEN 256
#endif

#define PART_VAR_CREATE     (PART_VAR_CREATE_IF_NOT_EXIT | PART_VAR_UPDATE_IF_EXIT)
#define PART_CACHED         0x10
#define PART_DIRTY          0x20

struct part_item {
    struct list_head node;
    
    struct part_cursor c;
};

struct part_block {
    /*
    * block cache, a memory buffer
    *   size==flash partition size
    */
    unsigned int size;
    char *cache;

    /*
    * store k/v
    */
    unsigned int count;
    struct list_head list;

    /*
    * flash partition name
    *   as /dev/XXX
    */
    char *partition;

    /*
    * flash==>cache
    */
    int (*read)(part_block_t block, char *partition);

    /*
    * cache==>flash
    */
    int (*write)(part_block_t block, char *partition);
    
    unsigned int flag;
    unsigned int xor;
    int mode;
    int lockfd;
};

#define PART_BLOCK_SIZE(_size)  (sizeof(struct part_block) + (_size))

#ifdef __BOOT__
static byte boot_block[PART_BLOCK_SIZE(PART_BLOCK_CACHE_SIZE)];

static inline struct part_block *
part_zalloc(int block_size)
{
    if (block_size > PART_BLOCK_CACHE_SIZE) {
        return os_assert_value(NULL);
    } else {
        os_objzero(&boot_block); /* os_objzero(&array) */
        return (struct part_block *)boot_block;
    }
}
#define part_free(_blk)         os_do_nothing

#define part_lock(_block)       0
#define part_unlock(_block)     os_do_nothing

#else /* !__BOOT__ */
#define part_zalloc(_size)      (struct part_block *)os_zalloc(PART_BLOCK_SIZE(_size))
#define part_free(_blk)         os_free(_blk)

#ifndef PART_LOCKFILE
#define PART_LOCKFILE       "/var/run/part.lock"
#endif

static inline int 
part_lock(struct part_block *block)
{
    int fd = INVALID_FD;
    int err = 0;
    
    fd = open(PART_LOCKFILE, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
	if (fd < 0) {
		err = errno;

		goto error;
	}

	if (flock(fd, LOCK_EX)) {
		err = errno;

		goto error;
	}
    
    block->lockfd = fd;
    
	return 0;
error:
    if (fd>=0) {
        close(fd);
    }

    return err;
}

static inline void 
part_unlock(struct part_block *block)
{
    if (block->lockfd >= 0) {
    	if (flock(block->lockfd, LOCK_UN)) {
    		/* log */
        }
        
    	close(block->lockfd);
    	unlink(PART_LOCKFILE);
        
    	block->lockfd = -1;
	}
}
#endif /* __BOOT__ */

bool
part_block_is_good(struct part_block *block)
{
    if (NULL==block) {
        return false;
    }
    else if (NULL==block->cache) {
        return false;
    }
    else if (NULL==block->read) {
        return false;
    }
    else if (NULL==block->write) {
        return false;
    }
    else if (NULL==block->partition) {
        return false;
    }
    else if (0==block->size) {
        return false;
    }
    else if (false==is_good_part_mode(block->mode)) {
        return false;
    }
    else {
        return true;
    }
}

static inline bool 
is_cached(struct part_block *block)
{
    return os_hasflag(block->flag, PART_CACHED);
}

static inline bool 
is_dirty(struct part_block *block)
{
    return os_hasflag(block->flag, PART_DIRTY);
}

/*
* skip crc
*/
static int
part_block_size(struct part_block *block)
{
    return block->size - (1+CRC32_STRINGLEN) ;
}

/*
* skip crc
*/
static char *
part_block_begin(struct part_block *block)
{
    return block->cache + (1+CRC32_STRINGLEN);
}

static inline char *
part_block_end(struct part_block *block)
{
    return block->cache + block->size;
}

static inline unsigned int
part_block_crc(struct part_block *block)
{
    void *begin = part_block_begin(block);
    unsigned int size= part_block_size(block);
    unsigned int crc = os_crc32(begin, size);
    
    debug_trace("calc crc:%x with begin:%p size:%d)", crc, begin, size);
    
    return crc;
}

static unsigned int
part_block_crc_read(struct part_block *block)
{
    char crcstring[1+CRC32_STRINGLEN] = {0};
    unsigned long long crc = 0;
    os_memcpy(crcstring, block->cache, CRC32_STRINGLEN);

    /*
    * uboot not support sscanf, so can not as below
    *   os_sscanf(crcstring, "%x", &crc);
    */
    /*crc = os_digitstring2number(crcstring, CRC32_STRINGLEN, 16, typeof(crc));*/
    crc = os_digitstring2number(crcstring, strlen(crcstring), 16, typeof(crc));


    debug_trace("read crc(string:%s, number:%llx)", crcstring, crc);
    
    return (unsigned int)crc;
}

static void
part_block_crc_write(struct part_block *block)
{
    char crcstring[1+CRC32_STRINGLEN] = {0};
    unsigned int crc = part_block_crc(block);

    os_saprintf(crcstring, "%x", crc);
    os_memcpy(block->cache, crcstring, CRC32_STRINGLEN);
    block->cache[CRC32_STRINGLEN] = '\n';
}

static inline void
part_block_dump(struct part_block *block, char *header)
{
    if (__is_debug_init_trace) {
        os_println("block cache(%s)", header);
        os_dump_buffer(block->cache, PART_BLOCK_DUMP_LEN, NULL);
        os_println(__crlf);
    }
}

static char *
part_get_value_from_line(char *line)
{
    char *p;
    char *end = line + os_strlen(line);

    /*
    * find seperator
    */
    for (p=line; p<end; p++) {
        if (strchr(part_seperators, *p)) {
            break;
        }
    }

    if (p==end) {
        return NULL;
    } else {
        *p = 0;

        return (p+1);
    }
}

/*
*   [begin, end) °ë¿ªÇø¼ä
*/
static char *
part_read_line(char *begin, char *end, char *line, int line_size)
{
    char *p, *newline;
    int len;
    
    /* first reset line */
    line[0] = 0;
    
    if (NULL==begin || NULL==end || begin >= end) {
        return NULL;
    }
    
    /*
    * find '\n'
    */
    for (p=begin; p<end; p++) {
        if (PART_CRLF == *p) {
            break;
        }
    }

    /*
    * found '\n', p-->'\n'
    */
    if (PART_CRLF == *p) {
        newline = p + 1;

        if (newline >= end) {
            newline = NULL; /* p is the block last */
        }

        len = (p - begin + 1) - 1/* drop '/n' */;
    } else {
        /*
        * not found '\n', p-->end
        */
        newline = NULL;
        len = end - begin + 1;
    }
    
    if (len >= line_size) {
        return NULL;
    }
    
    os_memcpy(line, begin, len);
    line[len] = 0;

    return newline;
}

static char *
part_write_line(char *begin, char *end, struct part_cursor *c)
{
    int len, left;
    char *name, *separator, *value, *crlf;
    
    if (NULL==begin || NULL==end || NULL==c) {
        return NULL;
    }

    len = c->k.len + 1/* '\t' */ + c->v.len + 1/* '\n' */;
    left = end - begin;
    if (left < (len + 1/* '\0' */)) {
        return NULL;
    }

    name = begin;
    separator = name + c->k.len;
    value = separator + 1;
    crlf = value + c->v.len;
    
    os_memcpy(name, c->k.var, c->k.len);
    *separator = PART_SEPERATOR;
    os_memcpy(value, c->v.var, c->v.len);
    *crlf = PART_CRLF;
    
    return crlf + 1;
}

static void
part_cursor_free(struct part_item *item)
{
    struct part_cursor *c = &item->c;
    
    if (c->k.var) {
        os_free(c->k.var);
    }
    if (c->v.var) {
        os_free(c->v.var);
    }
}

static int
part_cursor_init(struct part_item *item, char *name, char *value)
{
    struct part_cursor *c = &item->c;
    
    part_cursor_free(item);
    
    c->k.len = os_strlen(name);
    c->k.var = (char *)os_zalloc(1+c->k.len);
    if (NULL==c->k.var) {
        goto error;
    }
    os_memcpy(c->k.var, name, c->k.len);
    
    c->v.len = os_strlen(value);
    c->v.var = (char *)os_zalloc(1+c->v.len);
    if (NULL==c->v.var) {
        goto error;
    }
    os_memcpy(c->v.var, value, c->v.len);

    return 0;
error:
    part_cursor_free(item);

    return -ENOMEM;
}

static void
part_item_free(struct part_item *item)
{
    if (item) {
        part_cursor_free(item);

        os_free(item);
    }
}

static int
part_item_init(struct part_item *item, char *name, char *value)
{
    INIT_LIST_HEAD(&item->node);
    
    return part_cursor_init(item, name, value);
}

static struct part_item *
part_item_new(char *name, char *value)
{
    struct part_item *item = NULL;

    item = (struct part_item *)os_zalloc(sizeof(*item));
    if (NULL==item) {
        goto error;
    }

    if (0!=part_item_init(item, name, value)) {
        goto error;
    }

    return item;
error:
    part_item_free(item);

    return NULL;
}

static int
part_item_insert(struct part_block *block, struct part_item *item)
{
    if (item) {
        list_add_tail(&item->node, &block->list);
        block->count++;

        return 0;
    } else {
        return -ENOMEM;
    }
}

static void
part_item_remove(struct part_block *block, struct part_item *item)
{
    if (block && item) {
        list_del(&item->node);
        block->count--;
    }
}

static inline int
kv_item_destroy(struct part_block *block, struct part_item *item)
{
    if (item) {
        part_item_remove(block, item);
        part_item_free(item);

        block->flag |= PART_DIRTY;
        
        return 0;
    } else {
        return -ENOEXIST;
    }
}

static inline void
part_item_clean(struct part_block *block)
{
    struct part_item *item, *n;

    list_for_each_entry_safe(item, n, &block->list, node) {
        kv_item_destroy(block, item);
    }
}

static inline struct part_item *
part_item_getbycursor(struct part_cursor *c)
{
    return container_of(c, struct part_item, c);
}

static bool 
is_separator(int ch)
{
    return NULL!=strchr(part_seperators, ch);
}

/* 
* partition ==> cache 
*/
static inline void
part_xor(struct part_block *block)
{
    if (block->xor) {
        int *p;
        int *end = (int *)part_block_end(block);
        
        for (p=(int *)block->cache; p<end; p++) {
            *p ^= block->xor;
        }

        debug_trace("part xor OK.");

        part_block_dump(block, "after xor");
    }
}

#ifndef __BOOT__
/* 
* partition ==> cache 
*/
static inline int
part_human_read(struct part_block *block, char *human)
{
    FILE *f = NULL;
    int err = 0, size;
    struct stat st;

    stat(human, &st);
    size = st.st_size;
    if (size > part_block_size(block)) {
        size = part_block_size(block);
    }
    
    f = fopen(human, "r");
    if (NULL==f) {
        err = errno;

        debug_error("part load read error(%d)", err);
        
        goto error;
    }

    err = fread(part_block_begin(block), size, 1, f);
    if (1!=err) {
        err = errno;

        debug_error("part load read error(%d)", err);
        
        goto error;
    }
    
    err = 0;
    debug_trace("part load read OK.");
error:
    if (f) {
        fclose(f);
    }

    return err;
}
#endif

/* 
* partition ==> cache 
*/
static inline int
part_low_read(struct part_block *block)
{
    int err = 0;
    
    err = (*block->read)(block, block->partition);
    if (err<0) {
        debug_error("part low read error(%d)", err);
    } else {
        debug_trace("part low read OK.");
    }
    
    part_xor(block);
    
    return err;
}

/* 
* cache ==> partition 
*/
static inline int
part_low_wirte(struct part_block *block)
{
    int err = 0;

    err = (*block->write)(block, block->partition);
    if (err<0) {
        debug_error("part low write error(%d)", err);
    } else {
        debug_trace("part low write OK.");
    }

    return err;
}

/* 
* cache ==> list
*/
static int
part_line_read(struct part_block *block)
{
    char line[1+PART_MAX_LINE];
    char *begin, *end, *value;
    int size, err;
    
    begin = part_block_begin(block);
    end   = part_block_end(block);

    while(NULL!=(begin = part_read_line(begin, end, line, PART_MAX_LINE)) || line[0]) {
        /*
        * cut left and right blanks("\t \r\n")
        */
        __string_strim_both_ends(line, NULL);
        
        if (PART_COMMENT==line[0]) {
            continue; /* comment line */
        }
        else if (0==line[0]) {
            continue; /* blank line */
        }
        
        /* reduce k/v separator */
        __string_reduce(line, is_separator);

        value = part_get_value_from_line(line);
        if (NULL==value) {
            /*
            * no found seperators, bad line
            *
            * ignore this line and try next line
            */
            continue;
        }

        debug_trace("insert k(%s)/v(%s)", line, value);
        err = part_item_insert(block, part_item_new(line, value));
        if (err<0) {
            /* 
            * insert error
            *
            * ignore this line and try next line
            */
            continue;
        }
    }
    
    block->flag |= PART_CACHED;
    block->flag &= ~PART_DIRTY;
    
    return 0;
}

/* 
* list ==> cache
*/
static int
part_line_wirte(struct part_block *block)
{
    struct part_item *item;
    char *begin, *end;

    os_memzero(block->cache, block->size);

    begin = part_block_begin(block);
    end   = part_block_end(block);
    list_for_each_entry(item, &block->list, node) {
        begin = part_write_line(begin, end, &item->c);
        if (NULL==begin) {
            /* log */
        }
    }
    part_block_crc_write(block);
    
    block->flag &= ~PART_DIRTY;
    
    return 0;
}

/* 
* cache ==> list
*/
static int
part_high_read(struct part_block *block)
{
    unsigned int crc_old, crc_new;
    int err;
    
    if (block->count) {
        return -ENOEMPTY;
    }
    
    /*
    * check crc
    */
    crc_old = part_block_crc_read(block);
    crc_new = part_block_crc(block);
    if (crc_old!=crc_new) {
        debug_error("bad crc(old=0x%x, new=0x%x)", crc_old, crc_new);
        
        return -EBADCRC;
    }
    
    err = part_line_read(block);
    if (err<0) {
        debug_error("part high read error(%d)", err);
    } else {
        debug_trace("part high read OK.");
    }
    
    part_block_dump(block, "after high read");

    return err;
}

/* 
* list ==> cache
*/
static int
part_high_wirte(struct part_block *block)
{
    int err = 0;

    /*
    * cannot check block->count
    *   when do empty, the block->count is 0
    */
    err = part_line_wirte(block);
    if (err<0) {
        debug_error("part high write error(%d)", err);
    } else {
        debug_trace("part high write OK.");
    }
    
    part_block_dump(block, "after high write");
    
    return err;
}

static int
part_block_init_bynormal(part_block_t block)
{
    int err;
    
    err = part_low_read(block);
    if (err<0) {
        return err;
    }
    
    err = part_high_read(block);
    if (err<0) {
        return err;
    }
    
    return 0;
}

static int
part_block_init_byempty(part_block_t block)
{
    block->flag |= PART_DIRTY;
    
    debug_trace("part create empty OK");
    
    return 0;
}

#ifndef __BOOT__
static int
part_block_init_byload(part_block_t block, char *human)
{
    int err;
    
    err = part_human_read(block, human);
    if (err<0) {
        return err;
    }

    block->flag |= PART_DIRTY;
    
    return 0;
}
#endif

static int
part_block_init_bycrc(part_block_t block)
{
    int err;
    
    err = part_low_read(block);
    if (err<0) {
        return err;
    }
    
    return 0;
}

static int
part_block_init(part_block_t block, char *human)
{
    int err = 0;
    
    switch(block->mode) {
        case PART_MODE_NORMAL:
            err = part_block_init_bynormal(block);
            break;
        case PART_MODE_EMPTY:
            err = part_block_init_byempty(block);
            break;
#ifndef __BOOT__
        case PART_MODE_LOAD:
            err = part_block_init_byload(block, human);
            break;
#endif
        case PART_MODE_CRC:
            err = part_block_init_bycrc(block);
            break;
        default:
            err = -EKEYBAD;
            break;
    }
    
    if (err<0) {
        debug_error("%s init failed(%d)", part_mode_string(block->mode), err);
    }
    
    return err;
}

static int
part_block_flush(part_block_t block)
{
    int err;
    
    if (is_dirty(block)) {
        err = part_high_wirte(block);
        if (err<0) {
            return err;
        }
        
        part_xor(block);
        
        err = part_low_wirte(block);
        if (err<0) {
            return err;
        }
    }
    
    return 0;
}

static void
part_block_free_only(part_block_t block)
{
    if (block) {
        part_item_clean(block);
        part_unlock(block);
        part_free(block);
    }
}

void
__part_block_free(part_block_t block)
{
    if (block) {
        part_block_flush(block);
        part_block_free_only(block);
    }
}

static part_block_t 
__part_block_new(
    int mode,
    unsigned int block_size, 
    int (*read)(part_block_t block, char *partition), 
    int (*write)(part_block_t block, char *partition),
    char *partition,
    unsigned int xor
)
{
    part_block_t block = NULL;
    int err;
    
    block = part_zalloc(block_size);
    if (NULL==block) {
        return os_assert_value(NULL);
    }
    block->cache= (char *)(block + 1);
    block->size = block_size;
    block->partition = partition;
    block->xor  = xor?xor:PART_XOR;
    block->mode = mode;
    block->read = read;
    block->write= write;
    INIT_LIST_HEAD(&block->list);
    
    debug_trace("new block with"    __crlf
                __tab "size:%d"     __crlf
                __tab "xor:0x%x"    __crlf
                __tab "mode:%s"     __crlf
                __tab "partition:%s", 
                block->size, 
                block->xor,
                part_mode_string(block->mode),
                os_safestring(partition));

    err = part_lock(block);
    if (err<0) {
        goto error;
    }
    
    return block;
error:
    part_free(block);

    return NULL;
}

part_block_t 
part_block_create(
    int mode,
    unsigned int block_size, 
    int (*read)(part_block_t block, char *partition), 
    int (*write)(part_block_t block, char *partition),
    char *partition,
    char *human,
    unsigned int xor
)
{
    part_block_t block = NULL;
    int err;
    
    if (false==is_good_part_mode(mode)) {
        return os_assert_value(NULL);
    }
    else if (0==block_size) {
        return os_assert_value(NULL);
    }
    else if (NULL==read) {
        return os_assert_value(NULL);
    }
    else if (NULL==write) {
        return os_assert_value(NULL);
    }
    else if (NULL==partition) {
        return os_assert_value(NULL);
    }
    
    block = __part_block_new(mode, block_size, read, write, partition, xor);
    if (NULL==block) {
        return NULL;
    }

    err = part_block_init(block, human);
    if (err<0) {
        goto error;
    }

    return block;
error:
    part_block_free_only(block);
    
    return NULL;
}


static int
part_var_rw_check(struct part_block *block, int offset, void *buf, unsigned int size)
{
    if (false==part_block_is_good(block)) {
        return os_assert_value(-EKEYBAD);
    }
    else if (offset >= block->size) {
        return os_assert_value(-EINVAL1);
    }
    else if (NULL==buf) {
        return os_assert_value(-EINVAL2);
    }
    else if (size < 0) {
        return os_assert_value(-EINVAL3);
    }
    else if ((offset + size) > block->size) {
        return os_assert_value(-EINVAL4);
    }

    return 0;
}

int
part_block_read(part_block_t block, unsigned int offset, void *buf, unsigned int size)
{
    int err;

    err = part_var_rw_check(block, offset, buf, size);
    if (err) {
        return err;
    }
    
    os_memcpy(buf, block->cache + offset, size);

    return size;
}

int
part_block_write(part_block_t block, unsigned int offset, void *buf, unsigned int size)
{
    int err;

    err = part_var_rw_check(block, offset, buf, size);
    if (err) {
        return err;
    }

    os_memcpy(block->cache + offset, buf, size);
    
    return size;
}

int
part_block_crc_get(part_block_t block, unsigned int *crc_part, unsigned int *crc_calc)
{
    if (false==part_block_is_good(block)) {
        return os_assert_value(-EKEYBAD);
    }
    else if (NULL==crc_part) {
        return os_assert_value(-EINVAL1);
    }
    else if (NULL==crc_calc) {
        return os_assert_value(-EINVAL2);
    }

    *crc_part = part_block_crc_read(block);
    *crc_calc = part_block_crc(block);
    
    return 0;
}

int
part_var_foreach(part_block_t block, part_var_foreach_f *foreach, void *data)
{
    struct part_item *item, *n;
    multi_value_u mv = MV_INITER;
    
    if (false==part_block_is_good(block)) {
        return os_assert_value(-EKEYBAD);
    }
    else if (NULL==foreach) {
        return os_assert_value(-EINVAL1);
    }

    list_for_each_entry_safe(item, n, &block->list, node) {
        mv.value = (*foreach)(block, &item->c, data);
        if (mv2_is_break(mv)) {
            return mv2_result(mv);
        }
    }

    return mv2_result(mv);
}

#ifndef __BOOT__
static multi_value_t 
kv_foreach_byname_prefix_cb(part_block_t block, struct part_cursor *c, void *data)
{
    void **param = (void **)data;
    char *DATA = param[0];
    char *name  = param[1];
    part_var_foreach_f *foreach = param[2];
    int len = os_strlen(name);
    
    if (os_strlen(c->k.var) >= len && os_memeq(c->k.var, name, len)) {
        return (*foreach)(block, c, DATA);
    } else {
        return mv2_OK;
    }
}

int
part_var_foreach_byname_prefix(part_block_t block, char *name, part_var_foreach_f *foreach, void *data)
{
    void *param[4] = {
        [0] = data,
        [1] = name,
        [2] = foreach,
    };
    
    if (NULL==name) {
        return os_assert_value(-EINVAL9);
    }

    return part_var_foreach(block, kv_foreach_byname_prefix_cb, param);
}
#endif

static struct part_item *
__part_var_find(struct part_block *block, char *name)
{
    struct part_item *item;

    list_for_each_entry(item, &block->list, node) {
        if (0==os_strcmp(name, item->c.k.var)) {
            return item;
        }
    }
    
    return NULL;
}

int
part_var_find(part_block_t block, char *name, struct part_cursor *c)
{
    struct part_item *item;
    
    if (false==part_block_is_good(block)) {
        return os_assert_value(-EKEYBAD);
    }
    else if (NULL==name) {
        return os_assert_value(-EINVAL1);
    }
    else if (NULL==c) {
        return os_assert_value(-EINVAL2);
    }

    item = __part_var_find(block, name);
    if (NULL==item) {
        return -ENOEXIST;
    }

    os_objdcpy(c, &item->c);
    
    return 0;
}

int
__part_var_create(part_block_t block, char *name, char *value, int flag)
{
    struct part_item *item = NULL;
    
    if (false==part_block_is_good(block)) {
        return os_assert_value(-EKEYBAD);
    }
    else if (NULL==name) {
        return os_assert_value(-EINVAL1);
    }
    else if (NULL==value) {
        return os_assert_value(-EINVAL2);
    }
    else if (0==(PART_VAR_CREATE & flag)) {
        return os_assert_value(-EINVAL3);
    }

    item = __part_var_find(block, name);
    if (item) {
        if (os_hasflag(flag, PART_VAR_UPDATE_IF_EXIT)) {
            /* update it */
            block->flag |= PART_DIRTY;

            return part_cursor_init(item, name, value);
        }
        else if (os_hasflag(flag, PART_VAR_CREATE_IF_NOT_EXIT)) {
            return -EEXIST;
        }
    } else {
        if (os_hasflag(flag, PART_VAR_CREATE_IF_NOT_EXIT)) {
            /* create new */
            block->flag |= PART_DIRTY;

            return part_item_insert(block, part_item_new(name, value));
        }
        else if (os_hasflag(flag, PART_VAR_UPDATE_IF_EXIT)) {
            return -ENOEXIST;
        }
    }

    return os_assert_value(-EINVAL5);
}

int
part_var_delete(part_block_t block, char *name)
{
    if (false==part_block_is_good(block)) {
        return os_assert_value(-EKEYBAD);
    }
    else if (NULL==name) {
        return os_assert_value(-EINVAL1);
    }

    return kv_item_destroy(block, __part_var_find(block, name));
}

#ifndef __BOOT__
static multi_value_t 
part_var_delete_byname_prefix_cb(part_block_t block, struct part_cursor *c, void *data)
{
    int err;
    
    err = kv_item_destroy(block, part_item_getbycursor(c));

    return mv2_GO(err);
}

int
part_var_delete_byname_prefix(part_block_t block, char *name)
{
    return part_var_foreach_byname_prefix(block, name, part_var_delete_byname_prefix_cb, NULL);
}
#endif

/******************************************************************************/
AKID_DEBUGER; /* must last os_constructor */

