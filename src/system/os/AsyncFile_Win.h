#pragma once
#include "os/AsyncFile.h"
#include "utl/MemMgr.h"
#include "xdk/XAPILIB.h"

class AsyncFileWin : public AsyncFile {
public:
    AsyncFileWin(const char *, int);
    virtual ~AsyncFileWin();
    virtual bool GetFileHandle(void *&) { return false; }

#ifdef HX_NATIVE
    MEM_OVERLOAD(AsyncFile, 0x17);
#else
    // RB3-360 retail inlines both: new -> _MemAllocTemp(s, 0) at the
    // AsyncFile::New call site, delete -> MemFree(v) in ??_GAsyncFileWin.
    static void *operator new(unsigned int s) {
        return _MemAllocTemp(s, __FILE__, 0x17, "AsyncFile", 0);
    }
    static void *operator new(unsigned int s, void *place) { return place; }
    static void operator delete(void *v) { MemFree(v, __FILE__, 0x17, "AsyncFile"); }
#endif

protected:
    virtual bool Truncate(int);
    virtual void _OpenAsync();
    virtual bool _OpenDone() { return true; }
    virtual void _WriteAsync(const void *, int);
    virtual bool _WriteDone();
    virtual void _SeekToTell();
    virtual void _ReadAsync(void *, int);
    virtual bool _ReadDone();
    virtual void _Close();

    int mSectorBytes; // 0x38
    HANDLE mFile; // 0x3c
    int mFd; // 0x40
    bool mReadInProgress; // 0x44
    bool mWriteInProgress; // 0x45
    OVERLAPPED mOverlapped; // 0x48
    bool unk58;
    void *unk5c;
    void *unk60;
    int unk64;
    int unk68;
};
