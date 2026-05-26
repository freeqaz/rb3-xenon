#pragma once
#include "os/File.h"

class AsyncFile : public File {
public:
    AsyncFile(const char *filename, int mode);
    virtual ~AsyncFile();
    virtual class String Filename() const { return mFilename; }
    virtual int Read(void *, int);
    virtual bool ReadAsync(void *, int);
    virtual int Write(const void *, int);
    virtual bool WriteAsync(const void *, int);
    virtual int Seek(int, int);
    virtual int Tell() { return mTell; }
    virtual void Flush();
    virtual bool Eof();
    virtual bool Fail() { return mFail; }
    virtual int Size() { return mSize; }
    virtual int UncompressedSize() { return mUCSize; }
    virtual bool ReadDone(int &);
    virtual bool WriteDone(int &i);
    virtual bool GetFileHandle(void *&) { return false; }

    void Init();
    static AsyncFile *New(const char *, int);

protected:
    virtual void _OpenAsync() = 0;
    virtual bool _OpenDone() = 0;
    virtual void _WriteAsync(const void *, int) = 0;
    virtual bool _WriteDone() = 0;
    virtual void _SeekToTell() = 0;
    virtual void _ReadAsync(void *, int) = 0;
    virtual bool _ReadDone() = 0;
    virtual void _Close() = 0;

    void Terminate();
    void FillBuffer();

    int mMode; // 0x4
    bool mFail; // 0x8
    String mFilename; // 0xc
    unsigned int mTell; // 0x14
    int mOffset; // 0x18
    unsigned int mSize; // 0x1c
    unsigned int mUCSize; // 0x20
    char *mBuffer; // 0x24
    char *mData; // 0x28
    int mBytesLeft; // 0x2c
    int mBytesRead; // 0x30
};
