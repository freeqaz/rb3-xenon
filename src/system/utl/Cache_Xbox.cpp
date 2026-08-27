#include "utl/Cache_Xbox.h"
#include "utl/Cache.h"
#include "utl/CacheMgr.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/ThreadCall.h"
#include "utl/Cache.h"
#include "utl/MakeString.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#include "xdk/win_types.h"
#include <cstring>
#include "xdk/XAPILIB.h"

// ---------------------------------------------------------------------------
// CacheXbox vtable slots 6 and 7 (lane CACHEXBOX, 2026-08-27).
//
// target_symbol_map.json used to have these two swapped, which forced this file
// into a mirror-image set of compensating source errors.  Both rows still read
// 100%/100% the whole time -- a matching pair is NOT evidence that either row is
// correctly identified.  The correct assignment, proved on retail bytes with no
// reliance on any name:
//
//   0x827DA730 = GetDirectoryAsync    0x827D9F40 = GetFileSizeAsync
//
// 1. WILDCARD.  CacheIDXbox's vtable (0x8211BCC8) is dtor / 0x827DA328 /
//    0x827DA430 / GetDeviceID.  0x827DA328 builds "%s:\\" + "%s:\\%s" (exact
//    path); 0x827DA430 builds "%s:\\*" (wildcard) and tail-calls its own slot 1
//    for the non-null case, which is GetCacheSearchPath's `return
//    GetCachePath(c)`.  0x827DA730 calls slot 2 (`lwz r11,0x8(r11)`) => it
//    enumerates; 0x827D9F40 calls slot 1 (`lwz r11,0x4(r11)`) => it opens one file.
// 2. FIELD DATAFLOW.  ThreadGetDir (0x827DBAF0) push_backs into the vector at
//    `lwz r3, 0x168(r26)`; the file-size worker (0x827DA7C0) writes a scalar via
//    `lwz r11,0x160(r30); stw r31,0x0(r11)`.  0x827DA730 stores its out-pointer
//    at 0x168 (mCacheDirList), 0x827D9F40 at 0x160 (mData).
// 3. OP DISPATCH.  ThreadStart (0x827DBF20) sends 1 -> ThreadGetDir and
//    2 -> the file-size worker.  0x827DA730 writes op 1, 0x827D9F40 writes op 2.
//    => kOpDirectory == 1, kOpFileSize == 2, as both oracles say.
// 4. VTABLE ORDER.  CacheXbox's vtable at 0x8211BE44 is slot 6 = 0x827DA730,
//    slot 7 = 0x827D9F40; the other eight slots are pinned by unambiguous names
//    and match Cache's declaration order exactly, in which slot 6 is
//    GetDirectoryAsync.
//
// There is no "retail quirk" here; retail is entirely name-intuitive.  The
// earlier claim that it was inverted came from reading `li r10,1` out of
// 0x827DA730 while the map called that address GetFileSizeAsync -- a true byte
// observation attributed to the wrong function.
// ---------------------------------------------------------------------------

CacheIDXbox::CacheIDXbox() { memset(&mContentData, 0, sizeof(XCONTENT_DATA)); }

const char *CacheIDXbox::GetCachePath(const char *c) {
    if (mStrCacheName.empty()) {
        MILO_FAIL("CacheID::GetCachePath - mStrCacheName is empty.\n");
    }
    if (!c) {
        return MakeString("%s:\\", mStrCacheName.c_str());
    } else {
        String s = c;
        s.ReplaceAll('/', '\\');
        if (s.length() != 0 && s[0] == '\\') {
            s.erase(0, 1);
        }
        return MakeString("%s:\\%s", mStrCacheName.c_str(), s.c_str());
    }
}

const char *CacheIDXbox::GetCacheSearchPath(const char *c) {
    if (mStrCacheName.empty()) {
        MILO_FAIL("CacheID::GetCacheSearchPath() - mStrCacheName is empty.\n");
    }
    if (!c) {
        return MakeString("%s:\\*", mStrCacheName.c_str());
    } else {
        return GetCachePath(c);
    }
}

CacheXbox::CacheXbox(const CacheIDXbox &c)
    : mCacheID(c), mData(0), mSize(0), mCacheDirList(0), mCallbackObj(0) {}

bool CacheXbox::IsConnectedSync() {
    return XContentGetDeviceState(mCacheID.DeviceID(), 0) == ERROR_SUCCESS;
}

int CacheXbox::ThreadStart() {
    MILO_ASSERT(!IsDone(), 0x197);
    switch (mOpCur) {
    // Retail fn_827DBF20's cmpwi chain: value 1 -> the two-String recursive
    // FindFirstFile worker (fn_827DBAF0 = ThreadGetDir), value 2 -> the
    // no-argument CreateFile/GetFileSize worker (fn_827DA7C0).  Hence
    // kOpDirectory == 1 and kOpFileSize == 2.
    case kOpDirectory:
        return ThreadGetDir(mThreadStr, "");
    case kOpFileSize:
        return ThreadGetFileSize();
    case kOpRead:
        return ThreadRead();
    case kOpWrite:
        return ThreadWrite();
    case kOpDelete:
        return ThreadDelete();
    default:
        MILO_ASSERT(false, 0x1AB);
        return 0;
    }
}

void CacheXbox::ThreadDone(int res) {
    MILO_ASSERT(!IsDone(), 0x1B4);
    OpType old = mOpCur;
    switch (old) {
    // NB when reading ThreadStart/ThreadDone disassembly: both carry a `this`
    // adjustor of -12 (they live on the ThreadCallback sub-vtable), so an
    // offset seen off the incoming pointer is field offset - 0xc.
    case kOpDirectory:
        mLastResult = (CacheResult)res;
        mThreadStr = gNullStr;
        mCacheDirList = nullptr;
        mCallbackObj = nullptr;
        break;
    case kOpFileSize:
        mLastResult = (CacheResult)res;
        mThreadStr = gNullStr;
        mData = nullptr;
        mCallbackObj = nullptr;
        break;
    case kOpRead:
        mLastResult = (CacheResult)res;
        mThreadStr = gNullStr;
        mData = nullptr;
        mSize = 0;
        mCallbackObj = nullptr;
        break;
    case kOpWrite:
        mLastResult = (CacheResult)res;
        mThreadStr = gNullStr;
        mData = nullptr;
        mSize = 0;
        if (mCallbackObj) {
            static Message msg("cache_write_result", GetLastResult());
            msg[0] = GetLastResult();
            mCallbackObj->Handle(msg, true);
        }
        mCallbackObj = nullptr;
        break;
    case kOpDelete:
        mLastResult = (CacheResult)res;
        mThreadStr = gNullStr;
        mCallbackObj = nullptr;
        break;
    default:
        MILO_ASSERT(false, 0x1E3);
        break;
    }
    mOpCur = kOpNone;
}

bool CacheXbox::GetFileSizeAsync(const char *cc, unsigned int *ui, Hmx::Object *o) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (!ui) {
        mLastResult = kCache_ErrorBadParam;
        return false;
    } else {
        mThreadStr = mCacheID.GetCachePath(cc);
        mData = ui;
        mLastResult = kCache_NoError;
        mOpCur = kOpFileSize;
        ThreadCall(this);
        return true;
    }
}

bool CacheXbox::ReadAsync(const char *cc, void *v, unsigned int ui, Hmx::Object *o) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (cc && v && ui != 0) {
        mThreadStr = mCacheID.GetCachePath(cc);
        mData = v;
        mSize = ui;
        mLastResult = kCache_NoError;
        mOpCur = kOpRead;
        ThreadCall(this);
        return true;
    } else {
        mLastResult = kCache_ErrorBadParam;
        return false;
    }
}

bool CacheXbox::DeleteAsync(const char *cc, Hmx::Object *o) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (!cc) {
        mLastResult = kCache_ErrorBadParam;
        return false;
    } else {
        mThreadStr = mCacheID.GetCachePath(cc);
        mLastResult = kCache_NoError;
        mOpCur = kOpDelete;
        ThreadCall(this);
        return true;
    }
}

bool CacheXbox::GetFreeSpaceSync(u64 *u) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (!u) {
        mLastResult = kCache_ErrorBadParam;
        return false;
    } else {
        ULARGE_INTEGER freeBytes = {0};
        const char *path = mCacheID.GetCachePath(nullptr);
        if (GetDiskFreeSpaceExA(path, &freeBytes, nullptr, nullptr) == 0U) {
            void *err = (void *)GetLastError();
            if ((DWORD)err != 0x15 && (DWORD)err != 0x456 && (DWORD)err != 0x48F && (DWORD)err != 0x651
                && IsDeviceConnected(mCacheID.DeviceID())) {
                MILO_NOTIFY(
                    "CacheXbox::GetFreeSpaceSync(): Unhandled error %u returned from GetDiskFreeSpaceEx().\n",
                    err
                );
                mLastResult = kCache_ErrorUnknown;
                return false;
            } else {
                mLastResult = kCache_ErrorStorageDeviceMissing;
                return false;
            }
        } else {
            XDEVICE_DATA deviceData;
            DWORD err = XContentGetDeviceData(mCacheID.DeviceID(), &deviceData);
            if (err != ERROR_SUCCESS) {
                if (err != 5 && err != 0x15 && err != 0x456 && err != 0x48F
                    && err != 0x651 && IsDeviceConnected(mCacheID.DeviceID())) {
                    MILO_NOTIFY(
                        "CacheXbox::GetFreeSpaceSync(): Unhandled error returned from GetDiskFreeSpaceEx().\n"
                    );
                    mLastResult = kCache_ErrorUnknown;
                    return false;
                } else {
                    mLastResult = kCache_ErrorStorageDeviceMissing;
                    return false;
                }
            } else {
                *u = freeBytes.QuadPart + deviceData.ulDeviceFreeBytes;
                mLastResult = kCache_NoError;
                return true;
            }
        }
    }
    return false;
}

bool CacheXbox::GetDirectoryAsync(
    const char *cc, std::vector<CacheDirEntry> *entries, Hmx::Object *obj
) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (!entries) {
        mLastResult = kCache_ErrorBadParam;
        return false;
    } else {
        MILO_ASSERT(mThreadStr.empty(), 0x108);
        mThreadStr = mCacheID.GetCacheSearchPath(cc);
        MILO_ASSERT(mCacheDirList == NULL, 0x10B);
        mCacheDirList = entries;
        mLastResult = kCache_NoError;
        mOpCur = kOpDirectory;
        ThreadCall(this);
        return true;
    }
}

bool CacheXbox::DeleteSync(const char *cc) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (!cc) {
        mLastResult = kCache_ErrorBadParam;
        return false;
    } else {
        String path = mCacheID.GetCachePath(cc);
        bool res = DeleteFileA(path.c_str());
        if (res) {
            path.erase(path.find_last_of('\\'));
            res = DeleteParentDirs(path);
        }
        XContentFlush(mCacheID.Name(), nullptr);
        if (!res) {
            DWORD err = GetLastError();
            if (!IsDeviceConnected(mCacheID.DeviceID())) {
                mLastResult = kCache_ErrorStorageDeviceMissing;
            } else {
                MILO_NOTIFY(
                    "CacheXbox::DeleteSync() - Unhandled error from DeleteFile(): %d\n",
                    err
                );
                mLastResult = kCache_ErrorUnknown;
            }
            return false;
        } else {
            mLastResult = kCache_NoError;
            return true;
        }
    }
}

bool CacheXbox::WriteAsync(const char *cc, void *v, unsigned int ui, Hmx::Object *obj) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        if (obj) {
            static Message msg("cache_write_result", GetLastResult());
            msg[0] = GetLastResult();
            obj->Handle(msg, true);
        } else
            return false;
    } else if (cc && v && ui != 0) {
        mThreadStr = mCacheID.GetCachePath(cc);
        mData = v;
        mSize = ui;
        mCallbackObj = obj;
        mLastResult = kCache_NoError;
        mOpCur = kOpWrite;
        ThreadCall(this);
        return true;
    } else {
        mLastResult = kCache_ErrorBadParam;
        if (obj) {
            static Message msg("cache_write_result", GetLastResult());
            msg[0] = GetLastResult();
            obj->Handle(msg, true);
        } else
            return false;
    }
    return false;
}

int CacheXbox::ThreadGetFileSize() {
    HANDLE file = CreateFileA(mThreadStr.c_str(), 0, 1, nullptr, 3, 0x80, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (!IsDeviceConnected(mCacheID.DeviceID())) {
            return 8;
        } else if (err == 2) {
            return 6;
        } else {
            MILO_NOTIFY(
                "CacheXbox::GetFileSizeAsync() - Unhandled error from CreateFile(): %d\n",
                err
            );
            return -1;
        }
    } else {
        int ret = 0;
        DWORD fileSize = 0;
        DWORD res = GetFileSize(file, &fileSize);
        if (!(res != -1)) {
            // Byte-verified against retail fn_827DA7C0 (this function; still
            // unpinned in target_symbol_map.json, so objdiff cannot score it):
            // `lwz r11, 0x160(r30); stw r31, 0x0(r11)` -- the size is written
            // through mData (0x160), which is where GetFileSizeAsync parks it.
            int *data = (int *)mData;
            *data = res;
        } else {
            DWORD err = GetLastError();
            if (err != 0) {
                MILO_NOTIFY(
                    "CacheXbox::GetFileSizeAsync() - Unhandled error from GetFileSize(): %d\n",
                    err
                );
                ret = -1;
            }
        }
        CloseHandle(file);
        return !IsDeviceConnected(mCacheID.DeviceID()) ? 8 : ret;
    }
}

int CacheXbox::ThreadWrite() {
    mThreadStr.ReplaceAll('/', '\\');

    int success = 1;
    unsigned int nextPos = mThreadStr.find('\\');
    nextPos = mThreadStr.find('\\', nextPos + 1);

    while (nextPos != FixedString::npos) {
        {
            String dirPath = mThreadStr.substr(0, nextPos);
            int attrs = GetFileAttributesA(dirPath.c_str());
            if (attrs == -1) {
                success = CreateDirectoryA(dirPath.c_str(), nullptr);
                if (success == 0) {
                    break;
                }
            }
            ++nextPos;
        }
        nextPos = mThreadStr.find('\\', nextPos);
    }

    HANDLE hFile = (HANDLE)-1;
    if (success) {
        hFile = CreateFileA(
            mThreadStr.c_str(),
            0x40000000,  // GENERIC_WRITE
            0,           // No sharing
            nullptr,     // No security
            2,           // CREATE_ALWAYS
            0x80,        // FILE_ATTRIBUTE_NORMAL
            nullptr      // No template
        );
    }

    if (hFile == (HANDLE)-1) {
        DWORD err = GetLastError();
        if (err >= 2 && (err <= 3 || err == 0x15)) {
            return 8;
        }
    } else {
        DWORD bytesWritten = 0;
        int result = WriteFile(hFile, mData, mSize, &bytesWritten, nullptr);
        if (result != 0) {
            CloseHandle(hFile);
            XContentFlush(mCacheID.Name(), nullptr);
            return 0;
        }

        GetLastError();
        CloseHandle(hFile);
        XContentFlush(mCacheID.Name(), nullptr);
    }

    if (IsDeviceConnected(mCacheID.DeviceID())) {
        MILO_NOTIFY("CacheXbox::ThreadWrite() - Unhandled error from CreateFile()/WriteFile()\n");
        return -1;
    }

    return 8;
}

CacheDirEntry::CacheDirEntry(const CacheDirEntry &o) : mName(o.mName), mDateTime(o.mDateTime), mSize(o.mSize) {}

int CacheXbox::ThreadRead() {
    HANDLE hFile = CreateFileA(
        mThreadStr.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err >= 2) {
            if (err <= 3) {
                return 8;
            } else if (err != 0x15) {
                if (IsDeviceConnected(mCacheID.DeviceID())) {
                    MILO_NOTIFY(
                        "CacheXbox::ReadAsync() - Unhandled error from CreateFile(): %d\n",
                        err
                    );
                    return -1;
                }
            }
        }
        return 8;
    }

    DWORD bytesRead = 0;
    int result = ReadFile(hFile, mData, mSize, &bytesRead, nullptr);
    bool success = result != 0;
    CloseHandle(hFile);

    if (!success) {
        DWORD err = GetLastError();
        if (!IsDeviceConnected(mCacheID.DeviceID())) {
            return 8;
        }
        MILO_NOTIFY(
            "CacheXbox::ReadAsync() - Unhandled error from ReadFile(): %d", err
        );
        return -1;
    }
    return 0;
}

bool CacheXbox::DeleteParentDirs(String path) {
    path.ReplaceAll('/', '\\');
    String basePath = mCacheID.GetCachePath("");
    if (basePath.length() >= path.length()) {
        return true;
    }
    if (RemoveDirectoryA(path.c_str()) == 0) {
        DWORD err = GetLastError();
        if (err == 0x91) {
            return true;
        }
        return false;
    }
    path.erase(path.find_last_of('\\'));
    return DeleteParentDirs(String(path));
}

int CacheXbox::ThreadDelete() {
    mThreadStr.ReplaceAll('/', '\\');
    bool result = DeleteFileA(mThreadStr.c_str());
    if (result) {
        mThreadStr.erase(mThreadStr.find_last_of('\\'));
        result = DeleteParentDirs(String(mThreadStr));
    }
    if (!result) {
        DWORD err = GetLastError();
        if (!IsDeviceConnected(mCacheID.DeviceID())) {
            return 8;
        }
        MILO_NOTIFY(
            "CacheXbox::DeleteAsync() - Unhandled error from DeleteFile(): %d\n", err
        );
        return -1;
    }
    return 0;
}

int CacheXbox::ThreadGetDir(String searchPath, String basePath) {
    WIN32_FIND_DATAA findData;
    memset(&findData, 0, sizeof(findData));
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    CacheDirEntry entry;
    DWORD err;

    if (hFind == INVALID_HANDLE_VALUE) {
        err = GetLastError();
    } else {
        while (true) {
            if (findData.nFileSizeHigh == 0) {
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    unsigned int len = searchPath.length();
                    unsigned int lastSlash = searchPath.find_last_of('\\');
                    String newSearchPath = MakeString(
                        "%s%s%s", searchPath.substr(0, lastSlash + 1),
                        findData.cFileName,
                        searchPath.substr(lastSlash, len - lastSlash)
                    );
                    String newBasePath = MakeString(
                        "%s%s/", basePath, findData.cFileName
                    );
                    int ret = ThreadGetDir(newSearchPath, newBasePath);
                    if (ret != 0) {
                        CloseHandle(hFind);
                        return ret;
                    }
                } else {
                    entry.mSize = findData.nFileSizeLow;
                    entry.mName = basePath + findData.cFileName;
                    entry.mDateTime.FromFileTime(findData.ftLastWriteTime);
                    mCacheDirList->push_back(entry);
                }
            }

            if (FindNextFileA(hFind, &findData) == 0) {
                break;
            }
        }
        err = GetLastError();
        CloseHandle(hFind);
    }

    if (err == 2 || err == 0x12) {
        return 0;
    }
    if (err == 0x15 || err == 0x456 || err == 0x48f || err == 0x651) {
        return 8;
    }
    if (!IsDeviceConnected(mCacheID.DeviceID())) {
        return 8;
    }
    return -1;
}

// sw2 scatter-include (default/Cache_Xbox <- obj/Dir.cpp)
#define gRev gRev_Dir
#define gAltRev gAltRev_Dir
#include "obj/Dir.cpp"
#undef gRev
#undef gAltRev
