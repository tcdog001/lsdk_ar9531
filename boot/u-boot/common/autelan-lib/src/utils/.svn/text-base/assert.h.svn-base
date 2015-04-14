#ifndef __ASSERT_H_4F501D5C3D10778EA387560D3B8CB7C0__
#define __ASSERT_H_4F501D5C3D10778EA387560D3B8CB7C0__

#ifndef __KERNEL__
#define os_assert(x)                            assert(x)
#else
/*
*   return os_assert_value(x); 
*       ==> return ((void)0, x);
*
*   os_assert(0);
*       ==> (void)0;
*/
#define os_assert(x)                            (void)0
#endif

#define os_assert_value(x)                      (os_assert(0), x)
/******************************************************************************/
#endif /* __ASSERT_H_4F501D5C3D10778EA387560D3B8CB7C0__ */
