#pragma once
#include "os/AsyncFile.h"

class AsyncFileHolmes : public AsyncFile {
public:
    AsyncFileHolmes(const char *, int);
    virtual ~AsyncFileHolmes();

    MEM_OVERLOAD(AsyncFile, 0x14);

protected:
    virtual bool Truncate(int);
    virtual void _OpenAsync();
    virtual bool _OpenDone() { return true; }
    virtual void _WriteAsync(const void *, int);
    virtual bool _WriteDone() { return true; }
    virtual void _SeekToTell();
    virtual void _ReadAsync(void *, int);
    virtual bool _ReadDone();
    virtual void _Close();

    int mFd; // 0x34
};
