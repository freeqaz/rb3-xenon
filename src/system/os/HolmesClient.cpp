#include "HolmesClient.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Msg.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/HolmesUtl.h"
#include "os/NetworkSocket.h"
#include "os/System.h"
#include "os/Timer.h"
#include "utl/Cache.h"
#include "utl/Loader.h"
#include "utl/MemStream.h"
#include "utl/Option.h"
#include "utl/Symbol.h"
#include "utl/TextFileStream.h"
#include <cstdio>
#include <list>
#include <vector>

#pragma region Statics

#define HOLMES_CURRENT_VERSION 26
#define NETBIOS_NAME_MAX 64

String gLastCachedResource;
CacheResourceResult gLastCacheResult;

class HolmesInput {
public:
    void LoadKeyboard(BinStream &bs);
    void LoadJoypad(BinStream &bs);
    void SendKeyboardMessages();
};

namespace {
    struct HolmesProfileData {
        Timer wait;
        Timer work;
        int count;
        u32 pad;
    };

    struct ReadRequest {
        File *mRequestor;
        void *mBuffer;
        int mBytes;
    };

    BinStream *gHolmesStream;
    MemStream *gStreamBuffer;

    char gMachineName[NETBIOS_NAME_MAX] = { 0 };
    char gShareName[NETBIOS_NAME_MAX] = { 0 };
    bool gStackTraced;

    Holmes::Protocol gPendingResponse;
    int gRealMaxBufferSize;
    HolmesProfileData gProfile[20]; // to match protocol count
    CriticalSection gCrit;
    std::list<ReadRequest> gRequests;
    String gServerName;

    HolmesInput gInput;

    String gHolmesTarget;
    bool gPollStreamEof;

    bool PendingRead(File *f) {
        FOREACH (it, gRequests) {
            if (it->mRequestor == f)
                return true;
        }
        return false;
    }

#pragma region Private details

    void BeginCmd(Holmes::Protocol prot, bool b) {
        if (b) {
            gProfile[prot].count += 1;
        }
        gProfile[prot].work.Start();
    }

    void EndCmd(Holmes::Protocol prot) {
        gProfile[prot].work.Stop();
        if (gRealMaxBufferSize != 0) {
            MILO_NOTIFY_ONCE(
                "HolmesClient buffer exceeded %d < %d", 0x2000d, gRealMaxBufferSize
            );
        }
    }

    const Holmes::Protocol kAsyncOpcodes[5] = {
        Holmes::kReadFile,
        Holmes::kPollKeyboard,
        Holmes::kPollJoypad,
        Holmes::kPrint,
        Holmes::kInvalidOpcode
    };

    void HolmesFlushStreamBuffer() {
        if (gStreamBuffer->Size() > 0x2000d) {
            gRealMaxBufferSize = gStreamBuffer->Size();
        }
        gHolmesStream->Write(gStreamBuffer->Buffer(), gStreamBuffer->Size());
        gStreamBuffer->Seek(0, BinStream::kSeekEnd);
        gStreamBuffer->Compact();
    }

    bool CheckForResponse(Holmes::Protocol prot, bool b) {
        if (gPendingResponse == Holmes::kInvalidOpcode) {
            bool eof;
            if (b) {
                gPollStreamEof = gHolmesStream->Eof();
                eof = gPollStreamEof;
            } else {
                eof = gHolmesStream->Eof();
            }
            if (!eof) {
                START_AUTO_TIMER_CALLBACK("holmes_readopc", 0, 0);
                u8 res;
                gHolmesStream->Read(&res, 1);
                gPendingResponse = (Holmes::Protocol)res;
                MILO_ASSERT(gPendingResponse != Holmes::kInvalidOpcode, 0xef);
            }
        }

        bool compatible = (prot == gPendingResponse);
        if (!compatible) {
            for (int i = 0; i < 5; i++) {
                if (kAsyncOpcodes[i] == gPendingResponse || kAsyncOpcodes[i] == prot) {
                    compatible = true;
                    break;
                }
            }
        }

        if (gHolmesStream->Fail()) {
            MILO_FAIL("holmes closed");
        } else if (!compatible) {
            MILO_FAIL(
                "this shouldn't be happening %s %s\n",
                Holmes::ProtocolDebugString(prot),
                Holmes::ProtocolDebugString(gPendingResponse)
            );
        }

        return prot == gPendingResponse;
    }

    bool CheckReads(bool b);
    void CheckInput(bool b);

    void WaitForAnyResponse(Holmes::Protocol prot) {
        if (gPendingResponse == Holmes::kInvalidOpcode && gHolmesStream->Eof()) {
            AutoSlowFrame frame("Holmes::WaitForAnyResponse", 2.0f);

            HolmesProfileData *profile = &gProfile[prot];

            int count = profile->count;
            profile->count = count + 1;
            if (count == 0) {
                profile->wait.Start();
            }

            float elapsed = profile->wait.SplitMs();
            float timeout = 2000.0f;

            if (gHolmesStream->Eof()) {
                float timeout_step = 1000.0f;
                float timeout_factor = 0.001f;

                do {
                    Timer::Sleep(0);

                    if (!gStackTraced && (profile->wait.SplitMs() - elapsed) > timeout) {
                        const char *proto_str = Holmes::ProtocolDebugString(prot);
                        float time_blocked = timeout * timeout_factor;
                        printf(
                            "Holmes: %s opcode blocked for %f\n",
                            proto_str,
                            time_blocked
                        );
                        timeout += timeout_step;
                    }
                } while (gHolmesStream->Eof());
            }

            profile->count--;
            if (profile->count == 0) {
                profile->wait.Stop();
            }
        }
    }

    void WaitForResponse(Holmes::Protocol prot) {
        while (!CheckForResponse(prot, false)) {
            WaitForAnyResponse(prot);
            if (CheckReads(false) && prot == Holmes::kReadFile)
                return;
            CheckInput(false);
        }
    }

    bool CheckReads(bool b) {
        while (gRequests.begin() != gRequests.end()) {
            if (!CheckForResponse(Holmes::kReadFile, b)) {
                return false;
            }
            BeginCmd(Holmes::kReadFile, false);
            ReadRequest *req = &*gRequests.begin();
            int bytesRead = gHolmesStream->ReadAsync(req->mBuffer, req->mBytes);
            req->mBuffer = (char *)req->mBuffer + bytesRead;
            req->mBytes = req->mBytes - bytesRead;
            EndCmd(Holmes::kReadFile);
            if (bytesRead <= 0) {
                return false;
            }
            if (req->mBytes == 0) {
                std::list<ReadRequest>::iterator it = gRequests.begin();
                gRequests.erase(it);
                gPendingResponse = Holmes::kInvalidOpcode;
                return true;
            }
        }
        return false;
    }

    void WaitForReads() {
        CritSecTracker cst(&gCrit);
        while (gRequests.begin() != gRequests.end()) {
            while (!CheckForResponse(Holmes::kReadFile, false)) {
                WaitForAnyResponse(Holmes::kReadFile);
                if (CheckReads(false))
                    break;
                CheckInput(false);
            }
            CheckReads(false);
        }
    }

    void CheckInput(bool b) {
        if (CheckForResponse(Holmes::kPollKeyboard, b)) {
            BeginCmd(Holmes::kPollKeyboard, true);
            gInput.LoadKeyboard(*gHolmesStream);
            gPendingResponse = Holmes::kInvalidOpcode;
            EndCmd(Holmes::kPollKeyboard);
        }

        if (CheckForResponse(Holmes::kPollJoypad, b)) {
            BeginCmd(Holmes::kPollJoypad, true);
            gInput.LoadJoypad(*gHolmesStream);
            gPendingResponse = Holmes::kInvalidOpcode;
            EndCmd(Holmes::kPollJoypad);
        }
    }

    void HolmesClientPollInternal(bool b) {
        CritSecTracker cst(&gCrit);

        if (!gHolmesStream)
            return;

        CheckInput(b);
        CheckReads(b);
    };

    CacheResourceResult HolmesClientCacheResourceImpl(
        const char *filename, const char *resourceName
    ) {
        AutoSlowFrame frame("HolmesClientCacheFile", 1000.0f);
        CritSecTracker cst(&gCrit);

        BeginCmd(Holmes::kCacheResource, true);
        gLastCachedResource = resourceName;

        MILO_ASSERT(gHolmesStream, 1208);

        *gStreamBuffer << u8(Holmes::kCacheResource);
        *gStreamBuffer << filename;
        HolmesFlushStreamBuffer();
        WaitForResponse(Holmes::kCacheResource);

        u8 result;
        *gHolmesStream >> result;
        gPendingResponse = Holmes::kInvalidOpcode;
        gLastCacheResult = (CacheResourceResult)(s8)result;

        EndCmd(Holmes::kCacheResource);

        return gLastCacheResult;
    }

}

CacheResourceResult HolmesClientCacheResource(const char *filename, const char *resourceName) {
    return HolmesClientCacheResourceImpl(filename, resourceName);
}

#pragma region Public API

bool UsingHolmes(int p1) {
    if (!gHolmesStream)
        return false;

    return CanUseHolmes(p1);
}

NetAddress HolmesResolveIP() {
    if (CanUseHolmes(3))
        return HolmesClient::PlatformResolveIP();
    else
        return NetAddress();
}

namespace {
    bool gInputPolling = false;
}

void HolmesClientPollKeyboard() {
    HolmesClientPollInternal(true);
    if (!gInputPolling) {
        gInputPolling = true;
        gInput.SendKeyboardMessages();
        gInputPolling = false;
    }
}

DataNode DumpHolmesLog(DataArray *) {
    TextFileStream *log = new TextFileStream("holmes.csv", true);
    FileStream &fs = log->File();
    if (!fs.Fail()) {
        *log << HolmesClient::PlatformGetHostName() << ", ";
        *log << -1 << ", ";
        *log << -1 << "\n";
        for (int i = 0; i < 20; i++) {
            int count = gProfile[i].count;
            float wait = gProfile[i].wait.SplitMs();
            float work = gProfile[i].work.SplitMs() - wait;
            *log << Holmes::ProtocolDebugString(i) << ", ";
            *log << count << ", ";
            *log << wait << ", ";
            *log << work << ", ";
        }
        fs.Flush();
    }
    delete log;
    return 0;
}

bool HolmesClientInitOpcode(bool quiet) {
    bool fail = 0;
    *gStreamBuffer << u8(Holmes::kVersion) << HOLMES_CURRENT_VERSION;
    *gStreamBuffer << HolmesClient::PlatformGetHostName();
    *gStreamBuffer << gHolmesTarget;
    *gStreamBuffer << &gMachineName[0x40];
    *gStreamBuffer << FileSystemRoot();
    *gStreamBuffer << u8(TheLoadMgr.GetPlatform());
    *gStreamBuffer << u8(GetGfxMode());
    HolmesFlushStreamBuffer();
    if (!quiet) {
        WaitForAnyResponse(Holmes::kVersion);
        u8 response;
        *gHolmesStream >> response;
        fail = response != 0;
    } else {
        WaitForAnyResponse(Holmes::kVersion);
    }
    s32 host_ver = -1;
    if (!fail) {
        *gHolmesStream >> host_ver;
        fail = host_ver != HOLMES_CURRENT_VERSION;
    }
    if (fail) { // host/client version mismatch
        RELEASE(gHolmesStream);
        RELEASE(gStreamBuffer);
        if (gHostLogging) {
            gPendingResponse = Holmes::kInvalidOpcode;
            return fail;
        }
        if (host_ver >= 0) {
            MILO_FAIL(
                "Holmes version mismatch\nResync/rebuild both projects\nHolmes=%d  Console=%d",
                host_ver,
                HOLMES_CURRENT_VERSION
            );
        } else {
            MILO_FAIL("Holmes protocol mismatch\nCould not connect to console");
        }
    }
    if (!fail) {
        *gHolmesStream >> gServerName;
    }
    if (gHolmesTarget.c_str()[0] != 0) {
        bool b;
        *gHolmesStream >> b;
        if (b == 0) {
            MILO_FAIL("Failed to find holmes target '%s'", gHolmesTarget);
        }
    }
    if (!fail && gMachineName[0x40] == 0) {
        String my_name(gMachineName), host_name;
        *gHolmesStream >> host_name;
        if (host_name.c_str()[0] == 0) {
            MILO_FAIL(
                "Holmes fileroot missing!\nplease add -holmes_target <target> or -holmes_share <rootpath> to your commandline\n(-holmes_target is the preferred usage)"
            );
        }
        HolmesSetFileShare(my_name.c_str(), host_name.c_str());
    }
    gPendingResponse = Holmes::kInvalidOpcode;
    return fail;
}

void HolmesClientInit() {
#ifdef HX_NATIVE
    return; // Holmes remote debug not needed on native
#endif
    if (!UsingCD() || gHostConfig || gHostLogging) {
        MILO_LOG("Trying to connect to Holmes...\n");
        bool conf, log;
        if (!UsingCD()) {
            conf = gHostConfig = 0;
            log = gHostLogging = 0;
        } else {
            conf = gHostConfig;
            log = gHostLogging;
        }
        bool unk = !conf || log ? 0 : 1;
        BeginCmd(Holmes::kVersion, true);
        gHolmesTarget = OptionStr("holmes_target", gNullStr);
        String share(gShareName);
        share = OptionStr("holmes_share", share.c_str());
        share = OptionStr("xb_share", share.c_str());
        gHolmesStream = HolmesClient::PlatformCreateServerStream(unk, share.c_str());
        if (gHolmesStream == nullptr) {
            if (!unk) {
                MILO_FAIL("COULD NOT CONNECT TO HOLMES");
            }
            EndCmd(Holmes::kVersion);
            return;
        }
        bool fail = gHolmesStream->Fail();
        if (!fail) {
            gStreamBuffer = new MemStream(true);
            gStreamBuffer->Reserve(0x2000D);
            fail = HolmesClientInitOpcode(false);
            if (fail != 0 && unk) {
                return;
            }
        }
        if (fail) {
            RELEASE(gHolmesStream);
            RELEASE(gStreamBuffer);
        }
        if (fail && !unk) {
            MILO_FAIL("COULD NOT CONNECT TO HOLMES");
        }
        DataRegisterFunc("dump_holmes_log", DumpHolmesLog);
        EndCmd(Holmes::kVersion);
    }
}

void HolmesClientReInit() {
    CritSecTracker cst(&gCrit);
    if (!gHolmesStream) {
        return;
    }
    BeginCmd(Holmes::kVersion, true);
    HolmesClientInitOpcode(1);
    EndCmd(Holmes::kVersion);
    return;
}

int HolmesClientSysExec(const char *cc) {
    CritSecTracker cst(&gCrit);
    BeginCmd(Holmes::kSysExec, true);
    MILO_ASSERT(gHolmesStream, 750);
    *gStreamBuffer << u8(Holmes::kSysExec) << cc;
    HolmesFlushStreamBuffer();
    WaitForResponse(Holmes::kSysExec);
    int ret;
    *gHolmesStream >> ret;
    gPendingResponse = Holmes::kInvalidOpcode;
    EndCmd(Holmes::kSysExec);
    return ret;
}

int HolmesClientGetStat(const char *filename, FileStat &stat) {
    CritSecTracker cst(&gCrit);
    BeginCmd(Holmes::kGetStat, true);
    MILO_ASSERT(gHolmesStream, 770);
    *gStreamBuffer << u8(Holmes::kGetStat);
    *gStreamBuffer << filename;
    HolmesFlushStreamBuffer();
    WaitForResponse(Holmes::kGetStat);
    bool exists;
    *gHolmesStream >> exists;
    if (exists) {
        *gHolmesStream >> stat;
    }
    gPendingResponse = Holmes::kInvalidOpcode;
    EndCmd(Holmes::kGetStat);
    if (exists)
        return 0;
    else
        return -1;
}

int HolmesClientMkDir(const char *cc) {
    CritSecTracker cst(&gCrit);
    BeginCmd(Holmes::kMkDir, true);
    MILO_ASSERT(gHolmesStream, 818);
    *gStreamBuffer << u8(Holmes::kMkDir);
    *gStreamBuffer << cc;
    HolmesFlushStreamBuffer();
    WaitForResponse(Holmes::kMkDir);
    int ret;
    *gHolmesStream >> ret;
    gPendingResponse = Holmes::kInvalidOpcode;
    EndCmd(Holmes::kMkDir);
    return ret;
}

int HolmesClientDelete(const char *cc) {
    CritSecTracker cst(&gCrit);
    BeginCmd(Holmes::kDelete, true);
    MILO_ASSERT(gHolmesStream, 839);
    *gStreamBuffer << u8(Holmes::kDelete);
    *gStreamBuffer << cc;
    HolmesFlushStreamBuffer();
    WaitForResponse(Holmes::kDelete);
    int ret;
    *gHolmesStream >> ret;
    gPendingResponse = Holmes::kInvalidOpcode;
    EndCmd(Holmes::kDelete);
    return ret;
}

const char *HolmesFileShare() { return gShareName; }

void HolmesSetFileShare(const char *machine, const char *share) {
    strncpy(gMachineName, machine, NETBIOS_NAME_MAX);
    strncpy(gShareName, share, NETBIOS_NAME_MAX);
}

void HolmesClientTerminate() {
    CritSecTracker cst(&gCrit);
    if (gHolmesStream) {
        BeginCmd(Holmes::kTerminate, true);
        DumpHolmesLog(nullptr);
        if (gHolmesStream && !gHolmesStream->Fail()) {
            u8 cmd = Holmes::kTerminate;
            gStreamBuffer->Write(&cmd, 1);
            HolmesFlushStreamBuffer();
        }
        if (gHolmesStream) {
            delete gHolmesStream;
        }
        gHolmesStream = nullptr;
        if (gStreamBuffer) {
            delete gStreamBuffer;
        }
        gStreamBuffer = nullptr;
    }
}

void HolmesClientTruncate(int a, int b) {
    CritSecTracker cst(&gCrit);
    MILO_ASSERT(gHolmesStream, 0x3f3);
    if (gHolmesStream->Fail() && gHostLogging)
        return;
    BeginCmd(Holmes::kTruncateFile, true);
    *gStreamBuffer << u8(Holmes::kTruncateFile);
    *gStreamBuffer << a;
    *gStreamBuffer << b;
    HolmesFlushStreamBuffer();
    WaitForResponse(Holmes::kTruncateFile);
    int x;
    *gHolmesStream >> x;
    gPendingResponse = Holmes::kInvalidOpcode;
    EndCmd(Holmes::kTruncateFile);
}

bool HolmesClientOpen(const char *filename, int mode, unsigned int &fileSize, int &fd) {
    CritSecTracker cst(&gCrit);

    // Handle gHostLogging mode: read/write access checks
    if (gHostLogging) {
        if (mode & 1) {
            if (gHolmesStream == NULL) {
                return false;
            }
        } else {
            if (!gHostConfig) {
                MILO_FAIL("gHostLogging tried to read file");
            }
        }
    }

    if (gHolmesStream == NULL) {
        MILO_ASSERT(gHolmesStream, 866);
    }

    if (gHolmesStream->Fail()) {
        return false;
    }

    BeginCmd(Holmes::kOpenFile, true);
    auto cmd = u8(Holmes::kOpenFile);
    *gStreamBuffer << cmd;
    *gStreamBuffer << filename;
    if (!((mode >> 1) & 1)) {
        *gStreamBuffer << u8((mode >> 8) & 1); // write mode
        *gStreamBuffer << u8((mode >> 9) & 1); // create flag
    }
    auto readFlag = u8((mode >> 1) & 1);
    *gStreamBuffer << u8((mode >> 0x12) & 1); // truncate flag
    *gStreamBuffer << readFlag; // read mode

    HolmesFlushStreamBuffer();
    WaitForResponse(Holmes::kOpenFile);
    *gHolmesStream >> fd;

    if (fd != -1) {
        *gHolmesStream >> fileSize;
    }

    gPendingResponse = Holmes::kInvalidOpcode;
    EndCmd(Holmes::kOpenFile);

    return fd > 0;
}

void HolmesClientWrite(int file, int offset, int length, const void *data) {
    if (length == 0)
        return;
    CritSecTracker cst(&gCrit);
    MILO_ASSERT(gHolmesStream, 0x3b0);
    if (!gHolmesStream->Fail() || gHostLogging == false) {
        BeginCmd(Holmes::kWriteFile, true);
        *gStreamBuffer << u8(Holmes::kWriteFile);
        *gStreamBuffer << offset;
        *gStreamBuffer << file;
        *gStreamBuffer << length;
        gStreamBuffer->Write(data, length);
        HolmesFlushStreamBuffer();
        WaitForResponse(Holmes::kWriteFile);
        unsigned int returnCode;
        *gHolmesStream >> returnCode;
        gPendingResponse = Holmes::kInvalidOpcode;
        EndCmd(Holmes::kWriteFile);
    }
}

void HolmesClientRead(int arg0, int arg1, int arg2, void *arg3, File *arg4) {
    if (arg2 != 0) {
        CritSecTracker cst(&gCrit);
        if (gHolmesStream == 0) {
            MILO_ASSERT(gHolmesStream, 0x3c7);
        }
        BeginCmd((Holmes::Protocol)5, true);
        BinStream *buf = gStreamBuffer;
        *buf << u8(5);
        int fd = arg0;
        *buf << fd;
        int offset = arg1;
        *buf << offset;
        int bytes = arg2;
        *buf << bytes;
        HolmesFlushStreamBuffer();

        ReadRequest req;
        req.mRequestor = arg4;
        req.mBuffer = arg3;
        req.mBytes = arg2;
        gRequests.insert(gRequests.end(), req);

        EndCmd((Holmes::Protocol)5);
    }
}

bool HolmesClientReadDone(File *f) {
    CritSecTracker cst(&gCrit);
    bool ret = PendingRead(f);
    if (ret) {
        HolmesClientPoll();
        ret = PendingRead(f);
    }
    return !ret;
}

void HolmesClientStackTrace(const char *cc, struct StackData *stack, int i, String &ret) {
    ret = "";
    CritSecTracker cst(&gCrit);
    if (!gHolmesStream || gHolmesStream->Fail()) {
        return;
    }
    BeginCmd(Holmes::kStackTrace, true);
    *gStreamBuffer << u8(Holmes::kStackTrace);
    *gStreamBuffer << cc;
    *gStreamBuffer << i;
    int j;
    for (j = 0; j < i; j++) {
        *gStreamBuffer << stack->mFailThreadStack[j];
    }
    HolmesFlushStreamBuffer();
    gStackTraced = true;
    WaitForResponse(Holmes::kStackTrace);
    *gHolmesStream >> ret;
    gPendingResponse = Holmes::kInvalidOpcode;
    EndCmd(Holmes::kStackTrace);
}

void HolmesClientSendMessage(const Message &msg) {
    DataNode dn(msg);
    CritSecTracker cst(&gCrit);
    if (gHolmesStream && !gHolmesStream->Fail()) {
        BeginCmd(Holmes::kSendMessage, true);
        *gStreamBuffer << u8(Holmes::kSendMessage) << dn;
        HolmesFlushStreamBuffer();
        WaitForResponse(Holmes::kSendMessage);
        int ret;
        *gHolmesStream >> ret;
        gPendingResponse = Holmes::kInvalidOpcode;
        EndCmd(Holmes::kSendMessage);
    }
}

void HolmesClientClose(File *file, int handle) {
    CritSecTracker cst(&gCrit);

    BeginCmd(Holmes::kCloseFile, true);
    MILO_ASSERT(gHolmesStream, 1012);

    if (PendingRead(file)) {
        WaitForReads();
    }

    *gStreamBuffer << u8(Holmes::kCloseFile) << handle;
    HolmesFlushStreamBuffer();
    EndCmd(Holmes::kCloseFile);
}

void HolmesClientEnumerate(
    const char *path,
    void (*callback)(const char *, const char *),
    bool recurse,
    const char *ext,
    bool dirs
) {
    CritSecTracker cst(&gCrit);
    BeginCmd(Holmes::kEnumerate, true);

    *gStreamBuffer << u8(Holmes::kEnumerate);
    BinStream &bs = *gStreamBuffer << path;
    bs << u8(recurse);
    BinStream &bs2 = bs << ext;
    bs2 << u8(dirs);
    HolmesFlushStreamBuffer();

    std::vector<RecurseInfo> entries;
    WaitForResponse(Holmes::kEnumerate);

    while (true) {
        bool more;
        *gHolmesStream >> more;
        if (!more)
            break;
        entries.push_back(RecurseInfo());
        *gHolmesStream >> entries.back().mDir >> entries.back().mFile;
    }

    gPendingResponse = Holmes::kInvalidOpcode;

    for (unsigned int i = 0; i < entries.size(); i++) {
        callback(entries[i].mDir.c_str(), entries[i].mFile.c_str());
    }

    EndCmd(Holmes::kEnumerate);
}

bool CanUseHolmes(int p1) {
    if (!UsingCD())
        return true;

    if (gHostConfig != false && (p1 & 2U) != 0)
        return true;

    if (gHostLogging != false && (p1 & 1U) != 0)
        return true;

    return false;
}

void HolmesToLocal(char *p1, const char *p2) {
    String temp;
    temp = HolmesXboxPath(gServerName.c_str(), p2);

    const char *src = temp.c_str();
    char *dst = p1;

    s8 c;
    do {
        c = *src;
        *dst = c;
        src++;
        dst++;
    } while (c != 0);
}

char const *HolmesFileHostName() { return gMachineName; }

void HolmesClientPoll() {
    CritSecTracker cst(&gCrit);

    if (!gHolmesStream)
        return;

    gPollStreamEof = false;
    HolmesClientPollInternal(true);
}

bool HolmesClientCacheFile(char *arg0, const char *arg1) {
    CritSecTracker cst(&gCrit);
    AutoSlowFrame slow("HolmesClientCacheFile", 25.0f);

    BeginCmd(Holmes::kCacheFile, true);

    String str(arg1);
    HolmesToLocal(arg0, arg1);

    if (*arg0 == 0) {
        EndCmd(Holmes::kCacheFile);
        return false;
    }

    bool result = false;
    u8 fileInfo[0x20];
    int attrResult = GetFileAttributesExA(arg0, (GET_FILEEX_INFO_LEVELS)0, fileInfo);
    bool fileExists = (attrResult - 1) != (-1);
    if ((str != gLastCachedResource) && (gLastCacheResult > 0 || fileExists)) {
        EndCmd(Holmes::kCacheFile);
        return true;
    }

    u8 cmd = Holmes::kCacheFile;
    gStreamBuffer->Write(&cmd, 1);
    *gStreamBuffer << str;

    u8 hasFileFlag = fileExists;
    gStreamBuffer->Write(&hasFileFlag, 1);

    if (fileExists) {
        gStreamBuffer->WriteEndian(&*(s64*)(fileInfo + 0x14), 8);
    }

    HolmesFlushStreamBuffer();
    WaitForResponse(Holmes::kCacheFile);

    u8 response = 0;
    *gHolmesStream >> response;
    gPendingResponse = Holmes::kInvalidOpcode;

    if (response != 0) {
        result = true;
    }

    EndCmd(Holmes::kCacheFile);
    return result;
}

#ifdef HX_NATIVE
void HolmesClientPrint(const char *) {
    // Holmes remote debug not available on native
}
#endif
