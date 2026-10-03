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
    // AsyncFile does NOT override File::GetFileHandle in retail: its primary
    // vtable slot 15 is _purecall (0x828299B8) and AsyncFileWin supplies the
    // body (lane W16-OR; DC3 reached the same answer). The native engine's
    // AsyncFileNative declares none, so the native build keeps this one.
#ifdef HX_NATIVE
    virtual bool GetFileHandle(void *&) { return false; }
#endif

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
    unsigned int mTell; // 0x18
    int mOffset; // 0x1c
    unsigned int mSize; // 0x20
    unsigned int mUCSize; // 0x24
    char *mBuffer; // 0x28
    char *mData; // 0x2c
    int mBytesLeft; // 0x30
    int mBytesRead; // 0x34
};
