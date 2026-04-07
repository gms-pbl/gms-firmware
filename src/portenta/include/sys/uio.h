#pragma once

#ifndef _SYS_UIO_H_
#define _SYS_UIO_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _STRUCT_IOVEC
#define _STRUCT_IOVEC
struct iovec {
    void  *iov_base;
    size_t iov_len;
};
#endif

#ifdef __cplusplus
}
#endif

#endif // _SYS_UIO_H_