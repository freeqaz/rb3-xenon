#pragma once

int _open(const char *filename, int oflag, int pmode);

#ifdef __cplusplus
extern "C" {
#endif

int _close(int fd);
int _write(int fd, const void *buffer, unsigned int count);
long _lseek(int fd, long offset, int origin);
long long _lseeki64(int fd, long long offset, int origin);

#ifdef __cplusplus
}
#endif
