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
    enum State {
        kHttpGet_Nil = -1,
        kHttpGet_Connecting = 0,
        kHttpGet_Sending = 1,
        kHttpGet_SendingBody = 2,
        kHttpGet_ReceivingHeaders = 3,
        kHttpGet_ReceivingBody = 4,
        kHttpGet_Downloaded = 5,
        kHttpGet_Failed = 6,
        kHttpGet_FailedSend = 7,
        kHttpGet_Pending = 8,
    };

    HttpGet(unsigned int ip, unsigned short port, const char *, const char *);
    HttpGet(
        unsigned int ip, unsigned short port, const char *, unsigned char, const char *
    );
    virtual ~HttpGet();
    virtual void SetContent(const char *content) {}
    virtual void SetContentLength(unsigned int len) {}

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
    virtual bool CanRetry();
    virtual void StartSending();
    virtual void Sending() {
        MILO_FAIL("HttpGet::Sending() - shouldn't be calling this");
    }

    void StartReceiving();
    void SafeDisconnect();
    void SafeShutdown();
    void StartConnection();
    bool HasTimedOut();
    void SetState(State);

    NetworkSocket *mSocket; // 0x8
    String mPath; // 0xc - URL path for GET/POST requests
    unsigned short mPort; // 0x14
    int mState; // 0x18
    bool mFlags;
    Timer mTimer; // 0x20
    float mTimeoutMs; // 0x50
    unsigned int mIP; // 0x54
    String mHeaders; // 0x58 - additional HTTP headers
    void *mRecvBuf; // 0x60 - receive buffer (allocated as 0x1000 bytes)
    int mRecvBufPos; // 0x64
    u32 mHttpStatus;
    char *mFileBuf; // 0x6c
    int mFileBufSize; // 0x70
    int mFileBufRecvPos; // 0x74
    int mRetryCount; // 0x78 - compared against kMaxRetries
    HttpGetFailType mFailType; // 0x7c
    State mPrevState; // 0x80
};

class HttpPost : public HttpGet {
public:
    HttpPost(unsigned int, unsigned short, const char *, unsigned char);
    virtual ~HttpPost();
    virtual void SetContent(const char *content) { mContent = content; }
    virtual void SetContentLength(unsigned int);

protected:
    virtual bool CanRetry();
    virtual void StartSending();
    virtual void Sending();

    const char *mContent; // 0x88
    unsigned int mContentLength; // 0x8c
    int mBytesRemaining; // 0x90
    String mRequestHeaders; // 0x94
    int mHeaderLength;
};
