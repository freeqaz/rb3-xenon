// DC3 Native Port - Async File Implementation
// Replaces AsyncFile_Win.cpp - uses synchronous POSIX I/O under the hood

#include "os/AsyncFile.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/MemMgr.h"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#ifdef __EMSCRIPTEN__
#include "platform/WebAssets.h"
#endif

// Native async file - actually synchronous (good enough for initial port)
class AsyncFileNative : public AsyncFile {
public:
    AsyncFileNative(const char *filename, int mode)
        : AsyncFile(filename, mode), mFp(nullptr) {}

    virtual ~AsyncFileNative() { Terminate(); }

    MEM_OVERLOAD(AsyncFile, 0x17);

protected:
    virtual void _OpenAsync() {
        const char *fmode = "rb";
        if (mMode & 1) fmode = "r+b";
        if (mMode & 0x200) fmode = "wb";
        if (mMode & 0x100) fmode = "w+b";

        // mFilename is already qualified by AsyncFile::Init()
        mFp = fopen(mFilename.c_str(), fmode);
#ifdef __EMSCRIPTEN__
        // On-demand fetch: if file isn't in MEMFS, try fetching from server
        if (!mFp && !(mMode & 0x300)) {  // read-only modes only
            if (WebAssetsFetchSync(mFilename.c_str())) {
                mFp = fopen(mFilename.c_str(), fmode);
            }
        }
#endif
        if (!mFp) {
            MILO_LOG("AsyncFile: failed to open '%s'\n", mFilename.c_str());
            mFail = true;
            return;
        }
        // Get file size
        fseek(mFp, 0, SEEK_END);
        mSize = ftell(mFp);
        if (!(mMode & 8)) {
            fseek(mFp, 0, SEEK_SET);
        }
    }

    virtual bool _OpenDone() { return true; }

    virtual void _WriteAsync(const void *data, int size) {
        if (mFp) fwrite(data, 1, size, mFp);
    }

    virtual bool _WriteDone() { return true; }

    virtual void _SeekToTell() {
        if (mFp) fseek(mFp, mTell, SEEK_SET);
    }

    virtual void _ReadAsync(void *buf, int size) {
        if (mFp) {
            size_t got = fread(buf, 1, size, mFp);
            (void)got; // Base class tracks mBytesRead; we must NOT overwrite it
        }
    }

    virtual bool _ReadDone() { return true; }

    virtual void _Close() {
        if (mFp) {
            fclose(mFp);
            mFp = nullptr;
        }
    }

private:
    FILE *mFp;
};

void ReadError(const char *cc) {
    MILO_LOG("ReadError: %s\n", cc);
}

AsyncFile *AsyncFile::New(const char *filename, int mode) {
    AsyncFile *result = new AsyncFileNative(filename, mode);
    result->Init();
    return result;
}
