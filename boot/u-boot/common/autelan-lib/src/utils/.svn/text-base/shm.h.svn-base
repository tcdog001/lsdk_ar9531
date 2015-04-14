#ifndef __SHM_H_B3E2577A5EA2EF53FBFCE139C3E3B2FD__
#define __SHM_H_B3E2577A5EA2EF53FBFCE139C3E3B2FD__
#ifdef __APP__
/******************************************************************************/
#include "utils/base.h"

#define INVALID_SHM_ID      INVALID_COMMON_ID
#define INVALID_SHM_ADDR    ((void *)(-1))
#define INVALID_SHM_SIZE    0

enum {
    OS_BASE_SHM_ID      = 0xf00d0000,
    OS_APPKEY_SHM_ID    = OS_BASE_SHM_ID + 1,
    
    OS_SHM_ID_END
};

typedef struct {
    int id;
    int size;
    unsigned int key;
    void *address;
} os_shm_t;

#define OS_SHM_INIT(_size, _key) { \
    .id     = INVALID_SHM_ID,       \
    .size   = _size,                \
    .key    = _key,                 \
    .address= INVALID_SHM_ADDR,     \
}

static inline void 
os_shm_init(os_shm_t *shm, int size, int key)
{
    shm->id = INVALID_SHM_ID;
    shm->size = size;
    shm->key = key;
    shm->address = INVALID_SHM_ADDR;
}

static inline void 
os_shm_destroy(os_shm_t *shm)
{
    if (shm && INVALID_SHM_ADDR != shm->address) {
        shmdt(shm->address);
        shm->address = INVALID_SHM_ADDR;
        
        debug_trace("shm destroy(key:%#x, size:%#x, id:%d, address:%p)", 
            shm->key,
            shm->size,
            shm->id,
            shm->address);
    }
}

static inline int 
os_shm_create(os_shm_t *shm, bool readonly)
{
    int flags = IPC_CREAT | (readonly?SHM_RDONLY:0) | 0x1FF;
    int pagesize;
    
    if (NULL==shm) {
        return -EINVAL1;
    }
    
    if (INVALID_SHM_ADDR != shm->address) {
        return 0;
    }

    pagesize = getpagesize();
    debug_trace("pagesize=%d", pagesize);
            
    shm->size = os_align(shm->size, pagesize);
    
    shm->id = shmget(shm->key, shm->size, flags);
    if (false==is_good_shmid(shm->id)) {
        debug_error(
            "shm create(key:%#x, size:%#x, flags:%#x) error:%d", 
            shm->key,
            shm->size,
            flags,
            errno);
        
        return errno;
    } else {
        debug_trace(
            "shm create(key:%#x, size:%#x, flags:%#x) id:%d", 
            shm->key,
            shm->size,
            flags,
            shm->id);
    }
    
    shm->address = shmat(shm->id, NULL, 0);
    if (INVALID_SHM_ADDR == shm->address) {
        debug_error(
            "shm map(key:%#x, size:%#x, flags:%#x, id:%d) error:%d", 
            shm->key,
            shm->size,
            flags,
            shm->id,
            errno);
        
        return errno;
    } else {
        debug_trace(
            "shm map(key:%#x, size:%#x, flags:%#x, id:%d) address:%p", 
            shm->key,
            shm->size,
            flags,
            shm->id,
            shm->address);
    }
    
    return shm->id;
}
/******************************************************************************/
#endif /* __KERNEL__ */
#endif /* __SHM_H_B3E2577A5EA2EF53FBFCE139C3E3B2FD__ */

