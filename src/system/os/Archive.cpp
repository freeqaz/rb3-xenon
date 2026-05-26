#include "os/Archive.h"
#include "math/Sort.h"
#include "os/Block.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "stl/_algo.h"
#include "utl/BinStream.h"
#include "utl/FileStream.h"
#include "utl/HxGuid.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Option.h"
#include <cstdio>

Archive *TheArchive;
bool gDebugArkOrder;
int kArkBlockSize = 0x10000;

#pragma region ArkHash

ArkHash::~ArkHash() {
    MemFree(mHeap);
    MemFree(mTable);
}

int ArkHash::AddString(const char *str) {
    int hashIdx = HashString(str, mTableSize);
    int startIdx = hashIdx;
    MILO_ASSERT(hashIdx < mTableSize, 0xB4);
    while (mTable[hashIdx] != nullptr) {
        if (streq(mTable[hashIdx], str))
            return hashIdx;
        hashIdx++;
        if (hashIdx == mTableSize)
            hashIdx = 0;
        if (startIdx == hashIdx) {
            MILO_FAIL("ERROR: Hash table full!!!");
        }
    }
    int len = strlen(str);
    MILO_ASSERT(mFree + len + 1 < mHeapEnd, 200);
    memcpy(mFree, str, len);
    MILO_ASSERT(hashIdx < mTableSize, 0xCA);
    mTable[hashIdx] = mFree;
    mFree += len + 1;
    return hashIdx;
}

int ArkHash::GetHashValue(const char *c) const {
    int hashIdx = HashString(c, mTableSize);
    MILO_ASSERT(hashIdx < mTableSize, 0xD4);
    while (mTable[hashIdx]) {
        if (streq(mTable[hashIdx], c))
            return hashIdx;
        if (++hashIdx == mTableSize)
            hashIdx = 0;
    }
    return -1;
}

const char *ArkHash::operator[](int idx) const {
    MILO_ASSERT(idx < mTableSize, 0xE7);
    return mTable[idx];
}

void ArkHash::Read(BinStream &bs, int len) {
    MemFree(mHeap);
    MemFree(mTable);

    int heapSize;
    bs >> heapSize;
    mHeap = (char *)MemAlloc(heapSize + len, __FILE__, 0x112, "ArkHash");
    mFree = mHeap + heapSize;
    mHeapEnd = mHeap + (heapSize + len);

    bs.Read(mHeap, heapSize);
    memset(mFree, 0, mHeapEnd - mFree);

    bs >> mTableSize;
    mTable = (char **)MemAlloc(mTableSize * sizeof(char *), __FILE__, 0x11A, "ArkHash");
    char **pEnd = mTable + mTableSize;
    char **p = mTable;
    while (p != pEnd) {
        int offset;
        bs >> offset;
                if (offset != 0) {
            *p++ = (mHeap + offset);
        } else {
            *p++ = nullptr;
        }
    }
}

#pragma endregion
#pragma region Archive

bool Archive::DebugArkOrder() { return gDebugArkOrder; }

Archive::~Archive() {}

Archive::Archive(const char *name, int heap_headroom)
    : mNumArkfiles(0), mBasename(name), mMode(kRead), mMaxArkfileSize(0),
      mIsPatched(false), mPermissionCodes(0), mPermissionCount(0) {
    Read(heap_headroom);
}

bool Archive::HasArchivePermission(int x) const {
    for (int i = 0; i < mPermissionCount; i++) {
        if (mPermissionCodes[i] == x)
            return true;
    }
    return false;
}

void Archive::SetArchivePermission(int i, const int *ci) {
    mPermissionCount = i;
    mPermissionCodes = ci;
}

void Archive::GetGuid(HxGuid &guid) const { guid = mGuid; }

BinStream &operator>>(BinStream &bs, FileEntry &entry) {
    bs >> entry.mOffset >> entry.mHashedName >> entry.mHashedPath >> entry.mSize
        >> entry.mUCSize;
    return bs;
}

void Archive::Enumerate(
    const char *dir,
    void (*cb)(const char *, const char *),
    bool recurse,
    const char *pattern
) {
    char dtbPath[256];
    char folderPath[256];
    bool isDtb = false;

    if (pattern && strstr(pattern, ".dta")) {
        isDtb = true;
        sprintf(dtbPath, "%s/gen/%s.dtb", FileGetPath(pattern), FileGetBase(pattern));
        pattern = dtbPath;
        if (!recurse) {
            sprintf(folderPath, "%s/gen", dir);
            dir = folderPath;
        }
    }

    const char *dirp = dir;
    do {
        dirp++;
    } while ('\0' != *(dirp - 1));
    int dirLen = dirp - dir - 1;

    bool matches = false;
    const char *lastPath = nullptr;

    auto& _ref0 = mHashTable;
    FOREACH (it, mFileEntries) {
        const char *curPath = _ref0[it->HashedPath()];

        if (lastPath != curPath) {
            if (recurse) {
                matches = !strncmp(curPath, dir, dirLen)
                    && (curPath[dirLen] == '\0' || curPath[dirLen] == '/'
                        || curPath[dirLen] == '\\');
            } else {
                matches = strcmp(curPath, dir) == 0;
            }
            lastPath = curPath;
        }

        if (!matches) continue;

        const char *curName = _ref0[it->HashedName()];
        if (pattern) {
            const char *buf = MakeString("%s/%s", curPath, curName);
            if (!FileMatch(buf, pattern)) continue;
        }

        if (isDtb) {
            const char *path = FileGetPath(curPath);
            char *base = (char *)FileGetBase(curName);
            const char *dtaName = MakeString("%s.dta", base);
            cb(path, dtaName);
        } else {
            cb(curPath, curName);
        }
    }
}

int Archive::GetArkfileCachePriority(int arkfileNum) const {
    MILO_ASSERT(arkfileNum < mArkfileCachePriority.size(), 0x332);
    return mArkfileCachePriority[arkfileNum];
}

int Archive::GetArkfileNumBlocks(int filenum) const {
    return (mArkfileSizes[filenum] - 1) / kArkBlockSize + 1;
}

void Archive::SetLocationHardDrive() {
    for (int i = 0; i < mArkfileNames.size(); i++) {
        String path = TheContentMgr.TitleContentPath();
        mArkfileNames[i] = MakeString("%s/%s", path.c_str(), mArkfileNames[i].c_str());
    }
}

const char *Archive::GetArkfileName(int filenum) const {
    MILO_ASSERT(filenum < mArkfileNames.size(), 0x356);
    return mArkfileNames[filenum].c_str();
}

bool Archive::GetFileInfo(
    const char *file,
    int &arkfileNum,
    unsigned long long &byteOffset,
    int &fileSize,
    int &fileUCSize
) {
    if (file && *file) {
        String name(FileGetName(file));
        String path(FileGetPath(file));
        int nameValue = mHashTable.GetHashValue(name.c_str());
        int pathValue = mHashTable.GetHashValue(path.c_str());
        if (nameValue != -1 && pathValue != -1) {
            FileEntry entry;
            entry.mHashedName = nameValue;
            entry.mHashedPath = pathValue;
            auto it = std::lower_bound(mFileEntries.begin(), mFileEntries.end(), entry);
            if (it != mFileEntries.end() && it->HashedName() == nameValue
                && it->HashedPath() == pathValue) {
                arkfileNum = 0;
                unsigned long long u7 = 0;
                for (; arkfileNum < mNumArkfiles; arkfileNum++) {
                    unsigned long long u6 = mArkfileSizes[arkfileNum] + u7;
                    if (it->mOffset < u6)
                        break;
                    u7 = u6;
                }
                MILO_ASSERT(arkfileNum < mNumArkfiles, 0x183);
                byteOffset = it->mOffset - u7;
                fileSize = it->mSize;
                fileUCSize = it->mUCSize;
                return true;
            }
            arkfileNum = 0;
            byteOffset = 0;
            fileSize = 0;
            fileUCSize = 0;
            return false;
        }
    }
    return false;
}

void Archive::Read(int heap_headroom) {
    MILO_LOG("Reading the archive\n");
    FileStream arkhdr(MakeString("%s.hdr", mBasename), FileStream::kReadNoArk, true);
    MILO_ASSERT(!arkhdr.Fail(), 0x265);
    arkhdr.EnableReadEncryption();
    int version;
    arkhdr >> version;
    if (version != 6) {
        MILO_FAIL(" ERROR: %s  unsupported archive version %d", mBasename, version);
    } else {
        arkhdr >> mGuid;
        arkhdr >> mNumArkfiles;
        arkhdr >> mArkfileSizes;
        if (version == 3) {
            for (int i = 0; i < mArkfileSizes.size(); i++) {
                mArkfileNames.push_back(MakeString("%s_%d.ark", mBasename, i));
            }
        } else
            arkhdr >> mArkfileNames;

        if (version > 5)
            arkhdr >> mArkfileCachePriority;
        else {
            for (int i = 0; i < mArkfileSizes.size(); i++) {
                mArkfileCachePriority.push_back(-1);
            }
        }

        mHashTable.Read(arkhdr, heap_headroom);
        arkhdr >> mFileEntries;
        arkhdr.DisableEncryption();
    }
}

void Archive::Merge(Archive &shadow) {
    std::vector<FileEntry> extraFileEntries;
    MILO_ASSERT(shadow.mNumArkfiles == 1, 0x3C0);
    unsigned long long totalSize = 0;
    for (size_t i = 0; i < mArkfileSizes.size(); i++) {
        totalSize += mArkfileSizes[i];
    }
    auto& _ref1 = mHashTable;
    FOREACH (it, shadow.mFileEntries) {
        const char *name = shadow.mHashTable[it->mHashedName];
        const char *path = shadow.mHashTable[it->mHashedPath];
        FileEntry entry;
        entry.mHashedName = _ref1.AddString(name);
        entry.mHashedPath = _ref1.AddString(path);
        auto fileIt = std::lower_bound(mFileEntries.begin(), mFileEntries.end(), entry);
        if (fileIt != mFileEntries.end() && fileIt->mHashedName == entry.HashedName()
            && fileIt->mHashedPath == entry.HashedPath()) {
            fileIt->mOffset = it->mOffset + totalSize;
            fileIt->mSize = it->mSize;
            fileIt->mUCSize = it->mUCSize;
        } else {
            FileEntry toAdd;
            toAdd.mOffset = it->mOffset + totalSize;
            toAdd.mHashedName = entry.HashedName();
            toAdd.mHashedPath = entry.HashedPath();
            toAdd.mUCSize = it->mUCSize;
            toAdd.mSize = it->mSize;
            extraFileEntries.push_back(toAdd);
        }
    }
    mNumArkfiles++;
    mArkfileSizes.push_back(shadow.mArkfileSizes[0]);
    mArkfileNames.push_back(shadow.mArkfileNames[0]);
    mArkfileCachePriority.push_back(shadow.mArkfileCachePriority[0]);
    mFileEntries.insert(
        mFileEntries.end(), extraFileEntries.begin(), extraFileEntries.end()
    );
    std::sort(mFileEntries.begin(), mFileEntries.end());
    mIsPatched = true;
}

void ArchiveInit() {
    if (UsingCD() || OptionBool("force_ark", false)) {
        Symbol plat = PlatformSymbol(TheLoadMgr.GetPlatform());
        const char *hdrName;
        bool hardDrive = false;
        bool b4 = false;
        if (UsingCD()) {
            String titlePath(TheContentMgr.TitleContentPath());
            if (!titlePath.empty()) {
                b4 = true;
                hardDrive = b4;
                hdrName = MakeString("%s/gen/patch_%s", titlePath.c_str(), plat);
            }
        } else {
            hdrName = MakeString("gen/patch_%s", plat);
            b4 = true;
        }
        if (b4 && FileExists(MakeString("%s.hdr", hdrName), 0x10000, nullptr)) {
            Archive archive(hdrName, 0);
            if (hardDrive) {
                archive.SetLocationHardDrive();
            }
            TheArchive = new Archive(MakeString("gen/main_%s", plat), archive.HashFill());
            TheArchive->Merge(archive);
        } else {
            TheArchive = new Archive(MakeString("gen/main_%s", plat), 0);
        }
        static int preinitArk = 1;
        TheArchive->SetArchivePermission(1, &preinitArk);
    }
    gDebugArkOrder = OptionBool("debug_arkorder", false);
    TheBlockMgr.Init();
}
