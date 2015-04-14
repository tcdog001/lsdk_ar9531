#ifndef __MEMORY_H_449CCD8BC1CB461480DB11AD8A1CEB17__
#define __MEMORY_H_449CCD8BC1CB461480DB11AD8A1CEB17__
/******************************************************************************/
#ifdef __BOOT__
#define size_t int
#endif

static inline void *
os_memcpy(void *dst, const void *src, size_t n)
{
    if (dst && src) {
        return memcpy(dst, src, n);
    } else {
        return os_assert_value(dst);
    }
}

static inline void *
os_memset(void *s, int ch, size_t n)
{
    if (s) {
        return memset(s, ch, n);
    } else {
        return os_assert_value(s);
    }
}

#ifndef os_memzero
#define os_memzero(_ptr, _size)         os_memset(_ptr, 0, _size)
#endif

static inline int
os_memcmp(void *a, const void *b, size_t n)
{
    if (a) {
        if (b) {
            return n?memcmp(a, b, n):0;
        } else {
            /*
            * a is good
            * b is zero
            * so, a > b
            */
            return os_assert_value(1);
        }
    } else {
        if (b) {
            /*
            * a is zero
            * b is good
            * so, a < b
            */
            return os_assert_value(-1);
        } else {
            /*
            * a is zero
            * b is zero
            * so, a = b
            */
            return os_assert_value(0);
        }
    }
}

#ifndef os_memeq
#define os_memeq(_a, _b, _size)         (0==os_memcmp(_a, _b, _size))
#endif

/*
* use _array's size
*   _array is array name
*/
#ifndef os_arrayzero
#define os_arrayzero(_array)        os_memzero(_array, sizeof(_array))
#endif

/*
* use _dst's size
*   _dst and _src is array name
*/
#ifndef os_arraydcpy
#define os_arraydcpy(_dst, _src)    os_memcpy(_dst, _src, sizeof(_dst))
#endif

/*
* use _src's size
*   _dst and _src is array name
*/
#ifndef os_arrayscpy
#define os_arrayscpy(_dst, _src)    os_memcpy(_dst, _src, sizeof(_src))
#endif

/*
* use _a's size
*   _a and _b is array name
*/
#ifndef os_arraycmp
#define os_arraycmp(_a, _b)         os_memcmp(_a, _b, sizeof(_a))
#endif

/*
* use _a's size
*   _a and _b is array name
*/
#ifndef os_arrayeq
#define os_arrayeq(_a, _b)          (0==os_arraycmp(_a, _b))
#endif

/*
* use (*_pobj)'s size
*
* obj is a object(struct/array/native type)
*   pobj is the obj's address
*
* if obj is array
*   (1)the array MUST be real array(not function param)
*   (2)the _pobj MUST be the address of array(use as &array)
*
* array sizeof test
*   sizeof(array)==sizeof(*&array)
*   sizeof(*array)==sizeof(array[0])
*   sizeof(&array)==sizeof(void *)
*/
#ifndef os_objzero
#define os_objzero(_pobj)           os_memzero(_pobj, sizeof(*(_pobj)))
#endif

/*
* use (*_dst)'s size
*/
#ifndef os_objdcpy
#define os_objdcpy(_dst, _src)      os_memcpy(_dst, _src, sizeof(*(_dst)))
#endif

/*
* use (*_src)'s size
*/
#ifndef os_objscpy
#define os_objscpy(_dst, _src)      os_memcpy(_dst, _src, sizeof(*(_src)))
#endif

/*
* use (*_a)'s size
*/
#ifndef os_objcmp
#define os_objcmp(_a, _b)           os_memcmp(_a, _b, sizeof(*(_a)))
#endif

/*
* use (*_a)'s size
*/
#ifndef os_objeq
#define os_objeq(_a, _b)            (0==os_objcmp(_a, _b))
#endif


#ifndef __KERNEL__
#define os_malloc(_size)            malloc(_size)
#define os_calloc(_count, _size)    calloc(_count, _size)
#define os_alloca(_size)            alloca(_size)
#define os_realloc(_ptr, _size)     realloc(_ptr, _size)
#define os_free(_ptr) \
        do{ if (_ptr) { free(_ptr); (_ptr) = NULL; } }while(0)
#else
#define os_malloc(_size)            kmalloc(_size, GFP_KERNEL)
#define os_calloc(_count, _size)            ({  \
        void *p = os_malloc((_count)*(_size));  \
        if (p) {                                \
            os_memzero(p, (_count)*(_size));    \
        }                                       \
        p;                                      \
    })
#define os_realloc(_ptr, _size)     krealloc(_ptr, _size, GFP_KERNEL)
#define os_free(_ptr) \
        do{ if (_ptr) { kfree(_ptr); (_ptr) = NULL; } }while(0)
#endif
#define os_zalloc(_size)            os_calloc(1, _size)

/******************************************************************************/
#endif /* __MEMORY_H_449CCD8BC1CB461480DB11AD8A1CEB17__ */
