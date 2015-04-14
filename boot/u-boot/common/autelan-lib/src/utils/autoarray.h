#ifndef __AUTOARRAY_H_D8422899B40DF42D4E9371468AD4BE8A__
#define __AUTOARRAY_H_D8422899B40DF42D4E9371468AD4BE8A__
/******************************************************************************/
#include "utils.h"

struct atuoarray {
    byte *base; /* array memory */
    int size;   /* item size */
    int count;  /* item total count */
    int limit;  /* item total limit */
    int grow;   /* once grow count */
    
    void (*init)(void *item);
    void (*clean)(void *item);
};

static inline void *
__os_aa_item(struct atuoarray *aa, int idx)
{
    if (idx < aa->count) {
        return (void *)(aa->base + idx * aa->size);
    } else {
        return NULL;
    }
}

static inline int
__os_aa_grow_to(struct atuoarray *aa, int count)
{
    int old  = aa->count;
    int grow = count - old;
    
    if (grow <= 0) {
        return 0; //needn't grow
    }
    
    if (aa->limit && count > aa->limit) {
        return -ESPIPE;
    }
    
    aa->base = (byte *)os_realloc(aa->base, aa->size * count);
    if (NULL==aa->base) {
        return -ENOMEM;
    }
    aa->count = count;
    
    if (aa->init) {
        int i;
        
        for (i=old; i<count; i++) {
            (*aa->init)(__os_aa_item(aa, i));
        }
    } else {
        os_memzero(__os_aa_item(aa, old), aa->size * grow);
    }
    
    return 0;
}

static inline int
__os_aa_grow(struct atuoarray *aa)
{
    int count;
    
    if (NULL==aa) {
        return os_assert_value(-EKEYNULL);
    }

    count = aa->count + aa->grow;
    if (aa->limit) {
        if (aa->count==aa->limit) {
            return -ESPIPE;
        }

        if (count > aa->limit) {
            count = aa->limit;
        }
    }

    return __os_aa_grow_to(aa, count);
}

static inline void
os_aa_clean(struct atuoarray *aa)
{
    if (aa) {
        if (aa->clean) {
            int i;
            
            for (i=0; i<aa->count; i++) {
                (*aa->clean)(__os_aa_item(aa, i));
            }
        }

        if (aa->base) {
            os_free(aa->base);
        }
    }
}

static inline int
os_aa_init(
    struct atuoarray *aa, 
    int size, 
    int count,
    int limit, 
    int grow, 
    void (*init)(void *item),
    void (*clean)(void *item)
)
{
    if (NULL==aa) {
        return os_assert_value(-EKEYNULL);
    }
    else if (size<=0) {
        return os_assert_value(-EINVAL1);
    }
    else if (count<=0) {
        return os_assert_value(-EINVAL2);
    }
    else if (limit<0) {
        return os_assert_value(-EINVAL3);
    }
    else if (limit && count > limit) {
        return os_assert_value(-EINVAL4);
    }
    else if (grow<0) {
        return os_assert_value(-EINVAL5);
    }
    
    aa->size    = size;
    aa->count   = count;
    aa->limit   = limit;
    aa->grow    = grow;
    aa->init    = init;
    aa->clean   = clean;
    
    aa->base = (byte *)os_calloc(aa->count, aa->size);
    if (NULL==aa->base) {
        return -ENOMEM;
    }
    
    if (aa->init) {
        int i;
        
        for (i=0; i<aa->count; i++) {
            (*aa->init)(__os_aa_item(aa, i));
        }
    }
    
    return 0;
}

static inline void *
os_aa_get(struct atuoarray *aa, int idx, bool grow)
{
    void *value;
    
    if (NULL==aa) {
        return os_assert_value(NULL);
    }
    
    if (idx<0) {
        return os_assert_value(NULL);
    }
    
    value = __os_aa_item(aa, idx);
    if (NULL==value /* no space */ 
        && grow     /* try grow */ 
        && aa->grow /* can grow */ 
        && 0==__os_aa_grow_to(aa, idx) /* grow ok */
        ) {
        return __os_aa_item(aa, idx);
    }
    
    return value;
}

static inline int
os_aa_count(struct atuoarray *aa)
{
    if (aa) {
        return aa->count;
    } else {
        return os_assert_value(0);
    }
}

/******************************************************************************/
#endif /* __AUTOARRAY_H_D8422899B40DF42D4E9371468AD4BE8A__ */
