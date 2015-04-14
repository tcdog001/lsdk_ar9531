#ifndef __SEM_H_EF321BBA18F9C8C9E4977949EC03D97E__
#define __SEM_H_EF321BBA18F9C8C9E4977949EC03D97E__
#ifdef __APP__
/******************************************************************************/
typedef struct {
    int id;
    unsigned int key;
    bool owner;
} os_sem_t;

#define OS_SEM_INIT(_key)  {   \
    .id     = INVALID_SEM_ID,   \
    .key    = _key,             \
    .owner  = false,            \
}

static inline void 
__os_do_sem(int semid, int op)
{
    struct {
        unsigned short sem_num;
        short sem_op;
        short sem_flag;
    } sem = {0, op, SEM_UNDO};
    
    semop(semid, (struct sembuf *)&sem, 1);
}

static inline bool
os_sem_is_inited(os_sem_t *sem)
{
    return false==(0==sem->id && 0==sem->key && false==sem->owner);
}

static inline void 
os_sem_lock (os_sem_t *sem)
{
    __os_do_sem(sem->id, -1);
}

static inline void 
os_sem_unlock(os_sem_t *sem)
{
    __os_do_sem(sem->id, 1);
}

static inline void 
os_sem_destroy(os_sem_t *sem)
{
    if (sem && is_good_semid(sem->id) && true == sem->owner) {
        semctl(sem->id, 0, IPC_RMID, NULL);
        
        debug_trace("sem(%d) destroy", sem->id);
    }
}

static inline int 
os_sem_create(os_sem_t *sem, int key)
{
    int flags = IPC_CREAT | IPC_EXCL | 0x1FF;

    if (NULL==sem) {
        return INVALID_SEM_ID;
    }

    sem->key = key;
    sem->id = semget(sem->key, 1, flags);
    if (false==is_good_semid(sem->id)){
        if (EEXIST == errno) {
            flags = IPC_CREAT | 0x1FF;
            sem->id = semget(sem->key, 1, flags);
            if (false==is_good_semid(sem->id)){ 
                debug_error(
                    "sem get(key:%#x, flags:%#x) error:%d", 
                    sem->key,
                    flags,
                    errno);
                
                return INVALID_SEM_ID;
            }
        } else {
            debug_error(
                "sem create (key:%#x, flags:%#x) error:%d.", 
                sem->key,
                flags,
                errno);
            
            return INVALID_SEM_ID;
        }
    }
    else {
        sem->owner = true;
    }
    
    if (true == sem->owner) {
        int ret = semctl(sem->id, 0, SETVAL, 1);
        
        if (ret<0) {
            os_sem_destroy(sem);
            
            debug_error(
                "sem(%d) control(SETVAL, 1) error:%d", 
                sem->id,
                ret);
            
            return INVALID_SEM_ID;
        }
    }
    
    debug_trace("sem(%d) %s OK!", 
        sem->id, 
        (true == sem->owner)?"create":"get");
    
    return sem->id;
}
/******************************************************************************/
#endif /* __APP__ */
#endif /* __SEM_H_EF321BBA18F9C8C9E4977949EC03D97E__ */

