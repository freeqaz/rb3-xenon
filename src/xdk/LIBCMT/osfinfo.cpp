#include "errno.h"

// Internal CRT structures for file handle management
#define IOINFO_ARRAY_ELTS 32

struct ioinfo {
    long osfhnd;     // OS file handle (0x00)
    char osfile;     // flags (0x04)
    char pipech;     // one-char pipe buffer (0x05)
    char _padding[2]; // padding to 0x08
    int lockinitflag; // lock initialization flag (0x08)
    char _rest[60];  // rest of struct to make 0x48 bytes total
};

#define FOPEN 0x01  // file handle is open

extern "C" {
    extern int _nhandle;
    extern ioinfo* __pioinfo[64];  // Array of pointers to ioinfo arrays

    int* _errno();
    int* __doserrno();
}

extern "C" long long _free_osfhnd(unsigned int fh) {
    int array_idx;
    ioinfo* pio;

    if ((int)fh >= 0 && fh < (unsigned int)_nhandle) {
        array_idx = (int)fh >> 5;
        pio = &__pioinfo[array_idx][fh & 0x1f];
        if ((pio->osfile & FOPEN) != 0 && pio->osfhnd != -1) {
            *__doserrno() = -1;
            return 0;
        }
    }

    *_errno() = 9;
    *__doserrno() = 0;
    return -1;
}
