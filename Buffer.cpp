#include "Buffer.h"

#include <sys/uio.h>                    // iovec、readv
#include <unistd.h>


ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char extrabuf[65536] = {0};

    struct iovec vec[2];
    const size_t writableSize = writableBytes();
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writableSize;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = writableSize > sizeof(extrabuf) ? 1 : 2;
    const ssize_t n = readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writableSize)
    {
        writerIndex_ += n;
    }
    else
    {
        writerIndex_ += writableSize;
        append(extrabuf, n - writableSize);
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}