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

    MEM_OVERLOAD(FileStream, 0x1A);

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
