#pragma once
#include "os/NetworkSocket.h"
#include "utl/BinStream.h"

/**
 * @brief BinStream implementation that handles networking.
 * Useful for loading rawfiles from a network or similar.
 */
class NetStream : public BinStream {
public:
    NetStream();
    virtual ~NetStream();
    virtual int Tell() { return 0; }
    virtual void Flush() {}
    virtual EofType Eof();
    virtual bool Fail() { return mFail; }
    // NOT virtual: BinStream does not declare ReadAsync, so a `virtual` here
    // is a NEW virtual and MSVC appends it to the vtable.  Retail's
    // ??_7NetStream@@6B@ (0x8208da10) has exactly 11 slots -- the word after
    // slot 10 is 0x00000000 followed by the 0x19930522 FuncInfo EH magic, so
    // the table demonstrably ends there, while ours had 12 with ReadAsync at
    // [11].  Corroborating: retail's map carries ReadAsync only for
    // FileCacheFile / HDCache / CacheXbox, never NetStream.  Found by
    // tools/vtable_order_sweep.py slot-COUNT comparison (lane VTGRIND).
    int ReadAsync(void *, int);

    void ClientConnect(const NetAddress &);
    NetworkSocket *Socket() const { return mSocket; }

    static NetworkSocket *Create(bool);
    static unsigned int IPStringToInt(const String &);
    static String IPIntToString(unsigned int);
    static String GetHostName();
    static unsigned int ResolveHostName(String);

private:
    virtual void ReadImpl(void *, int);
    virtual void WriteImpl(const void *, int);
    virtual void SeekImpl(int, SeekType);

    NetworkSocket *mSocket; // 0xc
    bool mFail; // 0x10
    float mReadTimeoutMs; // 0x14
    int mBytesRead; // 0x18
    int mBytesWritten; // 0x1c
};
