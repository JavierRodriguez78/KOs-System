#ifndef KOS_LIBC_STDDEF_H
#define KOS_LIBC_STDDEF_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
#if !defined(__SIZE_TYPE__) && !defined(_SIZE_T) && !defined(__size_t_defined) && !defined(__SIZE_T_DEFINED)
typedef unsigned int size_t;
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif // KOS_LIBC_STDDEF_H
