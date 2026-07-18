#pragma once
#include "os/NetworkSocket.h"
#include "os/Timer.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"

enum HttpGetFailType {
    kHttpFail_None = 0,
    kHttpFail_Send = 1,
    kHttpFail_Timeout = 2,
    kHttpFail_ClientError = 3,
    kHttpFail_ServerError = 4,
};

class HttpGet {
public:
    // Retail ordering, reconstructed from disasm ground truth:
    //   SetState enter-switch (827DC588): -1->SafeShutdown, 0->StartConnection,
    //   1->StartSending, 2->StartReceiving, 4->return, 5->retry(FailedSend).
    //   StartConnection li r4,0x4 -> SetState(Failed); DetachBuffer/IsDownloaded
    //   cmpwi 3 -> Downloaded; HasFailed cmpwi 4 -> Failed; StartSending li r4,0x5
    //   -> SetState(FailedSend); Poll (827DCAB0) cmpwi 6 -> Pending.
    // Retail collapsed SendingBody/ReceivingHeaders into the Sending(1) state;
    // they remain here only for HttpPost/source callers (unmeasured), parked at
    // 7/8 so they don't collide with the real states above.
    enum State {
        kHttpGet_Nil = -1,
        kHttpGet_Connecting = 0,
        kHttpGet_Sending = 1,
        kHttpGet_ReceivingBody = 2,
        kHttpGet_Downloaded = 3,
        kHttpGet_Failed = 4,
        kHttpGet_FailedSend = 5,
        kHttpGet_Pending = 6,
        kHttpGet_SendingBody = 7,
        kHttpGet_ReceivingHeaders = 8,
    };

    HttpGet(unsigned int ip, unsigned short port, const char *, const char *);
    HttpGet(
        unsigned int ip, unsigned short port, const char *, unsigned char, const char *
    );
    // Cross-TU ODR divergence in retail RB3: HttpGet.cpp itself compiled HttpGet
    // as NON-polymorphic (its dtor/ctor store no vptr, mTimer at this+0x0 — proven
    // from disasm), but callers such as NetLoader_Xbox.cpp were compiled against a
    // header where ~HttpGet is virtual, so their `delete mHttpGet` emits a virtual
    // scalar-deleting-destructor call (vtable slot 0, r4=1). To reproduce both, the
    // dtor is virtual only for TUs that opt in via RB3_HTTPGET_VIRTUAL_DTOR.
#ifdef RB3_HTTPGET_VIRTUAL_DTOR
    virtual ~HttpGet();
#else
    ~HttpGet();
#endif
    void SetContent(const char *content) {}
    void SetContentLength(unsigned int len) {}

    bool IsDownloaded();
    bool HasFailed();
    char *DetachBuffer();
    void Send();
    void Poll();
    unsigned int GetBufferSize();
    void SetTimeout(float);
    HttpGetFailType FailType() const { return mFailType; }
    State PrevState() const { return mPrevState; }

    MEM_OVERLOAD(HttpGet, 0x1C);

private:
    void AddRequiredHeaders();

    static const float kDefaultTimeoutMs;
    static const int kMaxRetries;
    static const int kRecvBufSize;

protected:
    bool CanRetry();
    void StartSending();
    void Sending() {
        MILO_FAIL("HttpGet::Sending() - shouldn't be calling this");
    }

    void StartReceiving();
    void SafeDisconnect();
    void SafeShutdown();
    void StartConnection();
    bool HasTimedOut();
    void SetState(State);

    // Retail layout (reconstructed from disasm): HttpGet is non-polymorphic
    // (no vfptr at 0x0 — the ctor/dtor store no vtable and mTimer.mStart is at
    // this+0x0). Members reordered to match the retail offsets exactly.
    Timer mTimer; // 0x00
    float mTimeoutMs; // 0x30
    int mState; // 0x34
    const char *mPath; // 0x38 - URL path (read-only const char*, not a String)
    unsigned int mIP; // 0x3c
    String mHeaders; // 0x40 - additional HTTP headers (dtor destructs this)
    NetworkSocket *mSocket; // 0x4c
    void *mRecvBuf; // 0x50 - receive buffer (allocated as 0x1000 bytes)
    int mRecvBufPos; // 0x54
    char *mFileBuf; // 0x58
    int mFileBufSize; // 0x5c
    int mFileBufRecvPos; // 0x60
    int mRetryCount; // 0x64 - compared against kMaxRetries
    u32 mHttpStatus; // 0x68
    HttpGetFailType mFailType; // 0x6c
    State mPrevState; // 0x70
    unsigned short mPort; // 0x74
    bool mFlags; // 0x76
};

class HttpPost : public HttpGet {
public:
    HttpPost(unsigned int, unsigned short, const char *, unsigned char);
    ~HttpPost();
    void SetContent(const char *content) { mContent = content; }
    void SetContentLength(unsigned int);

protected:
    bool CanRetry();
    void StartSending();
    void Sending();

    const char *mContent; // 0x88
    unsigned int mContentLength; // 0x8c
    int mBytesRemaining; // 0x90
    String mRequestHeaders; // 0x94
    int mHeaderLength;
};
