#pragma once
#include "utl/BinStream.h"
#include "math/StreamChecksum.h"

class BufStream : public BinStream {
public:
    BufStream(void *buffer, int size, bool littleEndian);
    virtual ~BufStream();
    virtual void Flush() {}
    virtual int Tell() { return mTell; }
    virtual EofType Eof() { return (EofType)((mTell == mSize) ? 1 : 0); }
    virtual bool Fail() { return mFail; }
    virtual const char *Name() const;
    // NOT virtual: BinStream declares no Size(), so `virtual` here APPENDS a
    // 12th vtable slot; retail's BufStream vtable has 11.  The sole subclass
    // (FixedSizeSaveableStream) does not override it, so devirtualizing
    // changes no dispatch.  See NetStream.h for the same defect.
    int Size();

    void DeleteChecksum();
    void StartChecksum(const char *);
    bool ValidateChecksum();
    void SetName(const char *);

private:
    char *mBuffer; // 0xc
    bool mFail; // 0x10
    int mTell; // 0x14
    int mSize; // 0x18
    StreamChecksumValidator *mChecksum; // 0x1c
    int mBytesChecksummed; // 0x20
    String mName; // 0x24

    virtual void ReadImpl(void *, int);
    virtual void WriteImpl(const void *, int);
    virtual void SeekImpl(int, SeekType);
};
