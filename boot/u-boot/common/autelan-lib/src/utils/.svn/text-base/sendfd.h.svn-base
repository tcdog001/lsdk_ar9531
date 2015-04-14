#ifndef __SENDFD_H_A10E0BAC9AA4C142ABC15731138AB3C3__
#define __SENDFD_H_A10E0BAC9AA4C142ABC15731138AB3C3__
/******************************************************************************/
#include "utils.h"
#include "fd/fd.h"

#ifndef FDSENDER_UNIX_SOCKET
#define FDSENDER_UNIX_SOCKET    "/var/run/fdsender.sock"
#endif

static inline int //return fd
os_fdtransfer(char *path)
{
    int fd = INVALID_COMMON_ID;
    int err;
    struct sockaddr_un local = OS_SOCKADDR_UNIX(path?path:FDSENDER_UNIX_SOCKET);
    
    fd = fd_socket(PF_UNIX, SOCK_DGRAM, 0);
    if (fd<0) {
        return errno;
    }

    err = fd_bind(fd, (struct sockaddr*)&local, sizeof(local));
    if (err<0) {
        close(fd);

        return errno;
    }
    
    return fd;
}

static inline int
os_fdtransfer_close(int fd)
{
    return fd_close(fd);
}

#define __FDTRANSFER_BUFMIN             1024
#define __FDTRANSFER_CMSG_LEN           CMSG_SPACE(sizeof(int32_t))

#define __FDTRANSFER_EMPTY              "noNe"
#define __FDTRANSFER_EMPTY_SIZE         (sizeof(__FDTRANSFER_EMPTY) - 1)
#define __IS_FDTRANSFER_EMPTY(_buf)     \
        os_memeq(_buf, __FDTRANSFER_EMPTY, __FDTRANSFER_EMPTY_SIZE)
        
#define __FDTRANSFER(_msg, _cmsg, _buf, _len) \
    char ____fd_transfer_cmsg_buf[__FDTRANSFER_CMSG_LEN] = {0}; \
    struct iovec ____fd_transfer_iov = OS_IOVEC_INITER(_buf, _len); \
    struct msghdr _msg = \
        OS_MSGHDR_INITER(&____fd_transfer_iov, 1, NULL, 0, \
            ____fd_transfer_cmsg_buf, __FDTRANSFER_CMSG_LEN); \
    struct cmsghdr *_cmsg = CMSG_FIRSTHDR(&_msg)

static inline int
os_fdsend(int sender, int fd, void *buf, int len)
{
    int err = 0;
    
    if (false==is_good_fd(sender)) {
        return os_assert_value(-EINVAL1);
    }
    else if (false==is_good_fd(fd)) {
        return os_assert_value(-EINVAL2);
    }
    else if (NULL==buf) {
        return os_assert_value(-EINVAL3);
    }
    else if (len<__FDTRANSFER_BUFMIN)) {
        return os_assert_value(-EINVAL4);
    }

    __FDTRANSFER(msg, cmsg, buf, len);

    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = __FDTRANSFER_CMSG_LEN;
    *(int32_t *)CMSG_DATA(cmsg) = fd;

    err = fd_sendmsg(sender, &msg, 0);
    if (2!=err) {
        return errno;
    }

    return 0;  
}


static inline int
os_fdrecv(int recver, char *buf, int *len)
{
    int err = 0;

    if (false==is_good_fd(recver)) {
        return os_assert_value(-EINVAL1);
    }
    else if (NULL==buf) {
        return os_assert_value(-EINVAL2);
    }
    else if (NULL==len) {
        return os_assert_value(-EINVAL3);
    }
    else if (*len < __FDTRANSFER_BUFMIN) {
        return os_assert_value(-EINVAL4);
    }

    __FDTRANSFER(msg, cmsg, buf, *len);
    
    err = fd_recvmsg(recver, &msg, 0);
    if (err<__FDTRANSFER_BUFMIN) {
        return errno;
    } else if (__IS_FDTRANSFER_EMPTY(buf)) {
        
    } else {
        *len = err;
    }
    
    return *(int*)CMSG_DATA(cmsg);
}


/******************************************************************************/
#endif /* __SENDFD_H_A10E0BAC9AA4C142ABC15731138AB3C3__ */
