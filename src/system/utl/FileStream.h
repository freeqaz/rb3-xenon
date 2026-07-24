#pragma once
#include "os/File.h"
#include "utl/BinStream.h"
#include "utl/Str.h"
#include "math/StreamChecksum.h"
#include "utl/MemMgr.h"

class FileStream : public BinStream {
public:
    enum FileType {
        kRead = 0,
        kWrite = 1,
        kReadNoArk = 2,
        kAppend = 3,
    };

    FileStream(const char *, FileType, bool);
    FileStream(File *, bool);
    virtual ~FileStream();
    virtual void Flush();
    virtual int Tell();
    virtual EofType Eof();
    virtual bool Fail();
    virtual const char *Name() const { return mFilename.c_str(); }

    void StartChecksum();
    bool ValidateChecksum();

    int Size() { return (mFile) ? mFile->Size() : 0; }

    // Retail/match: FileStream's operator new is NOT noinline in the retail
    // XEX (unlike the general MEM_OVERLOAD policy) — `new FileStream(...)`
    // call sites show the fully-inlined 2-arg `MemAlloc(size, 0)` shape
    // (verified on HDCache::OpenHeader: target `li r4,0; li r3,0x28; bl
    // fn_827BCD38` vs our out-of-line `bl ??2FileStream@@SAPAXI@Z`). Define
    // the overload locally without __declspec(noinline) so /Ob2 inlines it.
#ifdef HX_NATIVE
    static void *operator new(size_t s) {
#else
    static void *operator new(unsigned int s) {
#endif
        return MemAlloc(s, __FILE__, 0x1A, "FileStream", 0);
    }
#ifdef HX_NATIVE
    static void *operator new(size_t s, void *place) { return place; }
#else
    static void *operator new(unsigned int s, void *place) { return place; }
#endif
    static void operator delete(void *v) { MemFree(v, __FILE__, 0x1A, "FileStream"); }

private:
    void DeleteChecksum();

    File *mFile;
    class String mFilename;
    bool mFail;
    StreamChecksumValidator *mChecksumValidator;
    int mBytesChecksummed;
    virtual void ReadImpl(void *, int);
    virtual void SeekImpl(int, SeekType);

protected:
    virtual void WriteImpl(const void *, int);
};
