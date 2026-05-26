#include "net/HttpGet.h"
#include "os/Debug.h"
#include "os/NetworkSocket.h"
#include "stl/_vector.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"
#include <cctype>

const float HttpGet::kDefaultTimeoutMs = 5000.0f;
const int HttpGet::kMaxRetries = 3;
const int HttpGet::kRecvBufSize = 0x1000;

namespace {
    // Validates HTTP header by searching for double newline (end of headers)
    // Returns true when "\n\n" is found (detects both CRLF and LF line endings)
    bool ValidateHeader(char *buf, int len, int *outPos, int *outLines) {
        unsigned char sawNewline = 0;
        int lineCount = 0;
        char *start = buf;

        if (len > 0) {
            do {
                signed char c = *buf;
                if (c == '\n') {
                    if (sawNewline) {
                        // Found double newline - end of headers
                        if (outPos != 0) {
                            *outPos = buf - start;
                        }
                        if (outLines != 0) {
                            *outLines = lineCount;
                        }
                        return true;
                    }
                    lineCount++;
                    sawNewline = 1;
                } else {
                    // Keep sawNewline set only if we just saw '\r' (CRLF handling)
                    // Bitwise AND ensures sawNewline stays set through '\r' but clears on other chars
                    if (c != '\r') sawNewline = 0;
                }
                len--;
                buf++;
            } while (len > 0);
        }
        return false;
    }
    char *GetNextLine(char *buf, int *remaining) {
        if (!buf || !remaining) return 0;

        int len = *remaining;
        while (len > 0) {
            if (*buf == '\r' || *buf == '\n') break;
            len--;
            buf++;
        }
        if (len > 0 && *buf == '\r') { buf++; len--; }
        if (len > 0 && *buf == '\n') { buf++; len--; }
        *remaining = len;
        return len > 0 ? buf : 0;
    }

    int LineLength(char *buf, int len) {
        MILO_ASSERT(buf, 0x54);
        char *start = buf;
        if (len > 0) {
            do {
                if (*buf == '\r' || *buf == '\n') break;
                len--;
                buf++;
            } while (len > 0);
        }
        return buf - start;
    }

    bool StrIStartsWith(String const &str, const char *prefix) {
        const char *s = str.c_str();
        unsigned int prefixLen = strlen(prefix);
        unsigned int strLen = strlen(s);
        if (prefixLen > strLen) return false;
        if (prefixLen != 0) {
            int offset = s - prefix;
            do {
                if (tolower(prefix[offset]) != tolower(*prefix)) break;
                prefixLen--;
                prefix++;
            } while (prefixLen != 0);
        }
        return prefixLen == 0;
    }
    char *ParseHeader(char *p, int lineLen, std::vector<String> *pHeader) {
        MILO_ASSERT(pHeader, 0x83);
        int count = (((int **)pHeader)[1] - ((int **)pHeader)[0]) >> 3;
        if (count > 0) {
            int idx = 0;
            while (count != 0) {
                int len = LineLength(p, lineLen);
                MILO_ASSERT(len > 0, 0x8C);
                (*pHeader)[idx].resize(len + 1);
                strncpy((char *)(*pHeader)[idx].c_str(), p, len);
                (*pHeader)[idx].erase(len);
                count = count - 1;
                p = GetNextLine(p, &lineLen);
                idx = idx + 1;
            }
        }
        return p;
    }

    unsigned int ParseStatusCode(std::vector<String> const &lines) {
        String status;

        if (StrIStartsWith(lines[0], "HTTP/1.0") == 0) {
            if (StrIStartsWith(lines[0], "HTTP/1.1") == 0) {
                goto done;
            }
        }

        {
            const char *ptr = lines[0].c_str() + 8;

            char c = *ptr;
            while (((c < '0') || (c > '9')) && (c != '\0') && (c != '\n')) {
                ptr++;
                c = *ptr;
            }

            if (c >= '0') {
                do {
                    status += c;
                    ptr++;
                    c = *ptr;
                } while ((c >= '0') && (c <= '9'));
            }

            if (status.c_str()[0] != '\0') {
                return atoi(status.c_str());
            }
        }

    done:
        return 0;
    }

    int GetContentLength(std::vector<String> const &lines) {
        int count = (int)lines.size();
        for (int i = 1; i < count; i++) {
            if (StrIStartsWith(lines[i], "Content-Length")) {
                const char *ptr = lines[i].c_str() + 14;
                while ((*ptr < '0' || *ptr > '9') && *ptr != '\0' && *ptr != '\n') {
                    ptr++;
                }
                return atoi(ptr);
            }
        }
        return -1;
    }
};

HttpGet::HttpGet(unsigned int ip, unsigned short port, const char *c1, const char *c2)
    : mSocket(nullptr), mPath(c1), mPort(port), mState(-1), mFlags(false),
      mTimeoutMs(kDefaultTimeoutMs), mIP(ip), mHeaders(c2), mRecvBuf(nullptr), mRecvBufPos(0),
      mFileBuf(nullptr), mFileBufSize(0), mFileBufRecvPos(0), mRetryCount(0), mFailType(),
      mPrevState(kHttpGet_Nil) {
    SetState(kHttpGet_Connecting);
    AddRequiredHeaders();
}

HttpGet::HttpGet(
    unsigned int ip, unsigned short port, const char *c1, unsigned char uc, const char *c2
)
    : mSocket(nullptr), mPath(c1), mPort(port), mState(-1), mFlags(uc & 3),
      mTimeoutMs(kDefaultTimeoutMs), mIP(ip), mHeaders(c2), mRecvBuf(nullptr), mRecvBufPos(0),
      mFileBuf(nullptr), mFileBufSize(0), mFileBufRecvPos(0), mRetryCount(0), mFailType() {
    SetState((uc & 4) == 0 ? kHttpGet_Pending : kHttpGet_Connecting);
    AddRequiredHeaders();
}

HttpGet::~HttpGet() { SafeShutdown(); }

void HttpGet::StartSending() {
    MILO_ASSERT(mSocket, 0x311);
    if (!mSocket->CanSend()) {
        mFailType = kHttpFail_Send;
        SetState(kHttpGet_FailedSend);
        return;
    }
    String str = "GET ";
    str += mPath;
    str += " ";
    str += "HTTP/1.1";
    if (!mHeaders.empty()) {
        str += "\r\n";
        str += mHeaders;
    }
    str += "\r\n\r\n";
    int len = (int)str.length();
    if (mSocket->Send(str.c_str(), len) != len) {
        mFailType = kHttpFail_Send;
        SetState(kHttpGet_FailedSend);
    } else {
        SetState(kHttpGet_ReceivingHeaders);
    }
}

// Cleanup and free resources. Match: 99.2% (limited by __FILE__ path difference)
void HttpGet::SafeShutdown() {
    SafeDisconnect();
    if (mFileBuf) {
        MemFree(mFileBuf, __FILE__, 0x359);
        mFileBuf = nullptr;
    }
    mFileBufSize = 0;
    mFileBufRecvPos = 0;
}

void HttpGet::Send() {
    if (mState == kHttpGet_Pending) {
        SetState(kHttpGet_Connecting);
    }
}

bool HttpGet::IsDownloaded() { return mState == kHttpGet_Downloaded; }
bool HttpGet::HasFailed() { return mState == kHttpGet_Failed; }

char *HttpGet::DetachBuffer() {
    if (mState != kHttpGet_Downloaded) {
        return nullptr;
    }
    char *buffer = mFileBuf;
    mFileBuf = nullptr;
    return buffer;
}

void HttpGet::StartReceiving() {
    if (mRecvBuf) {
        MemFree(mRecvBuf, __FILE__, 0x344);
        mRecvBuf = nullptr;
    }
    mRecvBuf = _MemAllocTemp(0x1000, __FILE__, 0x346, "HttpGet", 0);
}

void HttpGet::SafeDisconnect() {
    if (mSocket) {
        mSocket->Disconnect();
        RELEASE(mSocket);
    }
    if (mRecvBuf) {
        MemFree(mRecvBuf, __FILE__, 0x351);
        mRecvBuf = nullptr;
    }
    mRecvBufPos = 0;
}

void HttpGet::StartConnection() {
    MILO_ASSERT(!mSocket, 0x2FF);
    mSocket = NetworkSocket::Create(true);
    if (mSocket->Fail()) {
        mFailType = kHttpFail_Send;
        SetState(kHttpGet_Failed);
    } else {
        mSocket->Connect(mIP, mPort);
    }
}

bool HttpGet::HasTimedOut() {
    mTimer.Split();
    return mTimer.Ms() > mTimeoutMs;
}

unsigned int HttpGet::GetBufferSize() { return mFileBufSize; }

void HttpGet::SetTimeout(float timeout) { mTimeoutMs = timeout; }

bool HttpGet::CanRetry() {
    return mRetryCount < kMaxRetries;
}

void HttpGet::AddRequiredHeaders() {
    String newLine;
    newLine = MakeString("\r\n");
    String headers("Host: ");
    headers += NetworkSocket::IPIntToString(mIP);
    headers += ":";
    headers += MakeString("%d", mPort);
    headers += newLine;
    headers += "Content-Type: application/octet-stream";
    headers += newLine;
    headers += "Connection: close";
    headers += newLine;
    mHeaders += headers.c_str();
}

void HttpGet::SetState(State newState) {
    if (mState == (int)newState) return;

    do {
        switch (mState) {
        case kHttpGet_Connecting:
            if ((int)newState == kHttpGet_Sending) break;
            SafeShutdown();
            break;
        case kHttpGet_Sending:
            if ((int)newState == kHttpGet_SendingBody) break;
            // fall through
        case kHttpGet_SendingBody:
            if ((int)newState == kHttpGet_ReceivingHeaders) break;
            SafeShutdown();
            break;
        case kHttpGet_ReceivingHeaders:
            if ((int)newState == kHttpGet_ReceivingBody) break;
            SafeShutdown();
            break;
        case kHttpGet_ReceivingBody:
            if ((int)newState == kHttpGet_Downloaded) {
                SafeDisconnect();
                break;
            }
            SafeShutdown();
            break;
        }

        if (((int)newState == kHttpGet_Failed || (int)newState == kHttpGet_FailedSend)
            && mState != kHttpGet_Failed && mState != kHttpGet_FailedSend) {
            mPrevState = (State)mState;
        }

        mState = (int)newState;
        mTimer.Restart();

        switch ((int)newState) {
        case kHttpGet_ReceivingBody:
            StartReceiving();
            return;
        case kHttpGet_Sending:
            StartSending();
            return;
        case kHttpGet_Connecting:
            StartConnection();
            return;
        case kHttpGet_Nil:
            SafeShutdown();
            return;
        case kHttpGet_Failed:
            return;
        default:
            if ((int)newState != kHttpGet_FailedSend) {
                return;
            }
            // FailedSend - retry logic
            if (CanRetry()) {
                newState = kHttpGet_Connecting;
                mRetryCount++;
            } else {
                newState = kHttpGet_Failed;
            }
            break;
        }
    } while (mState != (int)newState);
}

void HttpGet::Poll() {
    if (mState == kHttpGet_Pending) return;
    if (!mSocket) return;

    if (mSocket->Fail()) {
        mFailType = kHttpFail_Send;
        SetState(kHttpGet_FailedSend);
        return;
    }

    switch (mState) {
    case kHttpGet_Connecting:
        if (mSocket->CanSend()) {
            SetState(kHttpGet_Sending);
            return;
        }
        if (HasTimedOut()) {
            mFailType = kHttpFail_Timeout;
            SetState(kHttpGet_FailedSend);
        }
        return;
    case kHttpGet_SendingBody:
        Sending();
        return;
    case kHttpGet_ReceivingHeaders:
        if (mSocket->CanRead()) {
            if (mFlags & 1) {
                SetState(kHttpGet_Downloaded);
            } else {
                SetState(kHttpGet_ReceivingBody);
            }
            return;
        }
        if (HasTimedOut()) {
            mFailType = kHttpFail_Timeout;
            SetState(kHttpGet_FailedSend);
        }
        return;
    case kHttpGet_ReceivingBody: {
        int recvd = mSocket->Recv(
            (char *)mRecvBuf + mRecvBufPos, kRecvBufSize - mRecvBufPos
        );
        if (HasTimedOut()) {
            mFailType = kHttpFail_Timeout;
            SetState(kHttpGet_FailedSend);
            return;
        }
        if (recvd == 0) return;
        mTimer.Restart();
        mRecvBufPos = recvd + mRecvBufPos;

        if ((u32)mFileBuf == 0U) {
            int headerEnd = 0;
            int lineCount = 0;
            if (!ValidateHeader((char *)mRecvBuf, mRecvBufPos, &headerEnd, &lineCount)) {
                return;
            }

            std::vector<String> lines;
            {
                String empty;
                lines.resize(lineCount, empty);
            }
            ParseHeader((char *)mRecvBuf, headerEnd, &lines);
            int statusCode = ParseStatusCode(lines);
            mHttpStatus = statusCode;

            if (statusCode != 200) {
                bool is4xx;
                if (statusCode >= 400 && statusCode <= 499) {
                    is4xx = true;
                } else {
                    is4xx = false;
                }
                if (is4xx) {
                    mFailType = kHttpFail_ClientError;
                } else {
                    bool is5xx;
                    if (statusCode >= 500 && statusCode <= 599) {
                        is5xx = true;
                    } else {
                        is5xx = false;
                    }
                    if (is5xx) {
                        mFailType = kHttpFail_ServerError;
                    } else {
                        mFailType = kHttpFail_None;
                    }
                }
                SetState(kHttpGet_Failed);
            } else {
                if (mFlags & 2) {
                    SetState(kHttpGet_Downloaded);
                } else {
                    int contentLen = GetContentLength(lines);
                    mFileBufSize = contentLen;
                    if (contentLen < 0) {
                        mFailType = kHttpFail_None;
                        SetState(kHttpGet_Failed);
                    } else if (contentLen == 0) {
                        SetState(kHttpGet_Downloaded);
                    } else {
                        mFileBuf = (char *)MemAlloc(
                            contentLen, __FILE__, 0x2BB, "HttpGet", 0
                        );
                        MILO_ASSERT(mFileBuf, 0x2BC);

                        int bodyStart = headerEnd + 1;
                        if (mRecvBufPos > bodyStart) {
                            int len = mRecvBufPos - bodyStart;
                            MILO_ASSERT(len <= mFileBufSize, 0x2C8);
                            memcpy(
                                mFileBuf,
                                (char *)mRecvBuf + bodyStart,
                                len
                            );
                            mRecvBufPos = 0;
                            mFileBufRecvPos = len;
                            MILO_ASSERT(mFileBufRecvPos <= mFileBufSize, 0x2CE);
                            if (mFileBufRecvPos == mFileBufSize) {
                                SetState(kHttpGet_Downloaded);
                            }
                        } else {
                            mRecvBufPos = 0;
                            mFileBufRecvPos = 0;
                        }
                    }
                }
            }
            return;
        }

        MILO_ASSERT(mFileBufSize >= mFileBufRecvPos + mRecvBufPos, 0x2E1);
        memcpy(mFileBuf + mFileBufRecvPos, mRecvBuf, mRecvBufPos);
        mFileBufRecvPos += mRecvBufPos;
        mRecvBufPos = 0;
        if (mFileBufRecvPos == mFileBufSize) {
            SetState(kHttpGet_Downloaded);
        }
        return;
    }
    default:
        MILO_FAIL("Bad State (%d) in HttpGet::Poll\n", mState);
        return;
    }
}

HttpPost::HttpPost(unsigned int ip, unsigned short port, const char *cc, unsigned char uc)
    : HttpGet(ip, port, cc, uc, nullptr) {
    String newLine;
    newLine = MakeString("\r\n");
    String post("POST ");
    post += mPath.c_str();
    post += " ";
    post += "HTTP/1.1";
    post += newLine;
    post += "Host: ";
    post += NetworkSocket::IPIntToString(ip);
    post += ":";
    post += MakeString("%d", mPort);
    post += newLine;
    post += "Content-Type: application/x-www-form-urlencoded";
    post += newLine;
    post += "Connection: close";
    post += newLine;
    mRequestHeaders = post.c_str();
}

HttpPost::~HttpPost() {}

void HttpPost::SetContentLength(unsigned int len) {
    MILO_ASSERT(mContent, 0x3C1);
    mContentLength = len;
    mBytesRemaining = len;
    mRequestHeaders += "Content-Length: ";
    mRequestHeaders += MakeString("%d\r\n", mContentLength);
    mRequestHeaders += MakeString("\r\n");
}

bool HttpPost::CanRetry() {
    if (mRetryCount < 3) {
        mBytesRemaining = mContentLength;
        return true;
    }
    return false;
}

void HttpPost::StartSending() {
    auto socket = mSocket;
    MILO_ASSERT(socket, 0x3CD);
    if (socket->CanSend()) {
        mHeaderLength = mRequestHeaders.length();
        if (socket->Send(mRequestHeaders.c_str(), mHeaderLength) == mHeaderLength) {
            SetState(kHttpGet_SendingBody);
            return;
        }
    }
    mFailType = kHttpFail_Send;
    SetState(kHttpGet_FailedSend);
}

void HttpPost::Sending() {
    MILO_ASSERT(mSocket, 0x3EF);
    String debugStr;
    int start = mContentLength - mBytesRemaining;
    if (start < (int)mContentLength) {
        do {
            debugStr += MakeString("%c", mContent[start]);
            start = start + 1;
        } while (start < (int)mContentLength);
    }
    int sent = mSocket->Send(
        mContent + mContentLength - mBytesRemaining, mBytesRemaining
    );
    if (sent == -1) {
        mFailType = kHttpFail_Send;
        SetState(kHttpGet_FailedSend);
    } else if (sent != mBytesRemaining) {
        mBytesRemaining = mBytesRemaining - sent;
    } else {
        mBytesRemaining = 0;
        SetState(kHttpGet_ReceivingHeaders);
    }
}
