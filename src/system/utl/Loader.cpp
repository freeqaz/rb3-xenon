#include "utl/Loader.h"
#include "Loader.h"
#include "MemTrack.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "os/Archive.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/Platform.h"
#include "os/System.h"
#include "utl/ChunkStream.h"
#include "utl/FilePath.h"
#include "utl/MemMgr.h"
#include "utl/Option.h"
#include "utl/Std.h"

#ifdef HX_NATIVE
bool (*LoadMgr::sFileOpenCallback)(const char *);
#endif

LoadMgr TheLoadMgr;
int gLoadCount;

struct LoaderGlitchContext {
    String file;            // 0x0
    const char *name;       // 0x8
    const char *fromState;  // 0xC
    LoaderPos toPos;        // 0x10
};

void FrontLoaderGlitchCB(float elapsed, void *v) {
    LoaderGlitchContext *ctx = (LoaderGlitchContext *)v;
    TheDebug << MakeString("Loader %s %s took %f (%s to %s)\n",
                           LoadMgr::LoaderPosString(ctx->toPos, true),
                           ctx->file,
                           elapsed,
                           ctx->name,
                           ctx->fromState);
}

const char *WhiteSpace(int count) {
    int len = 0x80;
    MILO_ASSERT(count < len, 0x179);
    MILO_ASSERT(count >= 0, 0x17A);
    return &"                                                                                                                                "
        [0x80 - count];
}

#pragma region Loader

Loader::Loader(const FilePath &fp, LoaderPos pos)
    :
#ifdef HX_NATIVE
      mLoadCount(0), mPos(pos), mFile(fp), mLoadStartMs(-1), mHeap(GetCurrentHeapNum()) {
#else
      mLoadCount(0), mPos(pos), mFile(fp), mHeap(GetCurrentHeapNum()) {
#endif
    MILO_ASSERT(MemNumHeaps() == 0 || (mHeap != kNoHeap && mHeap != kSystemHeap), 0x1F0);
    TheLoadMgr.Loaders().push_front(this);
    if (mPos == kLoadFront) {
        TheLoadMgr.Loading().push_front(this);
    } else if (mPos == kLoadStayBack) {
        TheLoadMgr.Loading().push_back(this);
    } else {
        std::list<Loader *>::iterator it = TheLoadMgr.Loading().end();
        while (it != TheLoadMgr.Loading().begin()) {
            --it;
            if ((*it)->GetPos() <= kLoadBack) {
                ++it;
                break;
            }
        }
        TheLoadMgr.Loading().insert(it, this);
    }
}

Loader::~Loader() {
    TheLoadMgr.Loading().remove(this);
    TheLoadMgr.Loaders().remove(this);
#ifdef HX_NATIVE
    if (mLoadStartMs != -1) {
        gLoadCount--;
    }
#endif
}

#pragma endregion
#pragma region FileLoader

FileLoader::FileLoader(
    const FilePath &fp,
    const char *cc,
    LoaderPos pos,
    int i4,
    bool b5,
    bool b6,
    BinStream *bs
)
    : Loader(fp, pos), mFile(nullptr), mStream(bs), mBuffer(nullptr), mBufLen(0),
      mAccessed(false), mTemp(b5), mWarn(b6), mFlags(i4), mFilename(cc), mBytesLoaded(0),
      mChunkSize(-1), mState(nullptr) {
    if (mStream) {
        mState = &FileLoader::LoadStream;
    } else {
        mState = &FileLoader::OpenFile;
    }
}

FileLoader::~FileLoader() {
    if (!mAccessed) {
        MemFree((void *)mBuffer);
        delete mFile;
    }
}

const char *FileLoader::DebugText() {
    return MakeString("FileLoader: %s", LoaderFile().c_str());
}
bool FileLoader::IsLoaded() const { return mState == &FileLoader::DoneLoading; }
void FileLoader::PollLoading() { (this->*mState)(); }
int FileLoader::GetSize() { return mBufLen; }
void FileLoader::DoneLoading() {}

void FileLoader::AllocBuffer() {
    const char *filename = mFilename.c_str();
#ifdef HX_NATIVE
    MemHeapTracker tmp(MemFindHeap("main"));
    BeginMemTrackFileName(filename);
    if (mTemp) {
        mBuffer =
            (const char *)_MemAllocTemp(mBufLen, __FILE__, 0x241, "Temp Resource", 0);
    } else {
        mBuffer = (const char *)MemAlloc(
            mBufLen, __FILE__, 0x243, Symbol(FileGetExt(filename)).Str()
        );
    }
    EndMemTrackFileName();
#else
    // RB3-360 retail: no heap push or mem-track bracketing; the allocation
    // name is still built (Symbol of the extension) and then dropped.
    if (mTemp) {
        mBuffer =
            (const char *)_MemAllocTemp(mBufLen, __FILE__, 0x241, "Temp Resource", 0);
    } else {
        Symbol ext(FileGetExt(filename));
        mBuffer = (const char *)MemAlloc(mBufLen, __FILE__, 0x243, ext.Str());
    }
#endif
}

void FileLoader::LoadFile() {
    int asdf;
    if (mFile->ReadDone(asdf)) {
        if (mFile->Fail()) {
            mBufLen = 0;
            MemFree((void *)mBuffer);
            mBuffer = nullptr;
        }
        RELEASE(mFile);
        mState = &FileLoader::DoneLoading;
    }
}

void FileLoader::LoadStream() {
    while (mStream->Eof() != NotEof) {
        if (TheLoadMgr.CheckSplit())
            return;
    }
    if (!mBuffer) {
        int size;
        *mStream >> size;
        if (size == -1) {
            *mStream >> mChunkSize;
            *mStream >> mBufLen;
        } else {
            mChunkSize = 0;
            mBufLen = size;
        }
        AllocBuffer();
    }
    int i2 = mChunkSize > 0 ? 0x10000 : mBufLen;
    while (true) {
        int i3 = Min(mBufLen - mBytesLoaded, i2);
        while (mStream->Eof() != NotEof) {
            if (TheLoadMgr.CheckSplit())
                return;
        }
        if (i3 == 0)
            break;
        mStream->Read((void *)(mBuffer + mBytesLoaded), i3);
        mBytesLoaded += i3;
    }
    mState = &FileLoader::DoneLoading;
}

void FileLoader::OpenFile() {
    Archive *old = TheArchive;
    const char *fname = mFilename.c_str();
    bool b1 = gHostFile && FileMatch(fname, gHostFile);
    if (b1) {
        SetUsingCD(false);
        TheArchive = nullptr;
    }
    mFile = NewFile(fname, mFlags | 2);
    if (b1) {
        SetUsingCD(true);
        TheArchive = old;
    }

    if (!mFile && *fname != '\0' && mWarn) {
        MILO_NOTIFY(
            "Could not load: %s (actually %s)",
            FileLocalize(Loader::mFile.c_str(), 0),
            fname
        );
    }
    if (mFile && !mFile->Fail()) {
        mBufLen = mFile->Size();
        AllocBuffer();
        mFile->ReadAsync((void *)mBuffer, mBufLen);
        mState = &FileLoader::LoadFile;
    } else {
        mState = &FileLoader::DoneLoading;
    }
}

char *FileLoader::GetBuffer(int *size) {
    MILO_ASSERT(IsLoaded(), 0x2B7);
    if (size)
        *size = mBufLen;
    mAccessed = true;
    return (char *)mBuffer;
}

void FileLoader::SaveData(BinStream &bs, void *v, int size) {
    MILO_ASSERT(size >= 0, 0x314);
    bs << -1;
    bs << 1;
    bs << size;
    for (int i3 = 0;;) {
        int i2 = Min(size - i3, 0x10000);
        if (i2 == 0)
            return;
        bs.Write((char *)v + i3, i2);
        i3 += i2;
        MarkChunk(bs);
    }
}

#pragma endregion
#pragma region LoadMgr

// Retail's ctor leaves mPlatform/mEditMode/mCacheMode to TheLoadMgr's static
// zero-init (no stores at +0x58/+0x5c/+0x5d); native keeps explicit inits.
LoadMgr::LoadMgr()
    :
#ifdef HX_NATIVE
      mPlatform(kPlatformXBox), mEditMode(false), mCacheMode(false),
#endif
      mPeriod(10.0f), mAsyncUnload(0), mLoaderPos(kLoadFront) {
}

LoadMgr::~LoadMgr() {}

void LoadMgr::StartAsyncUnload() { mAsyncUnload++; }
void LoadMgr::FinishAsyncUnload() { mAsyncUnload--; }
int LoadMgr::AsyncUnload() const { return mAsyncUnload; }

const char *LoadMgr::LoaderPosString(LoaderPos pos, bool abbrev) {
    static const char *names[4] = {
        "kLoadFront", "kLoadBack", "kLoadFrontStayBack", "kLoadStayBack"
    };
    static const char *abbrevs[4] = { "F", "B", "FSB", "SB" };
    MILO_ASSERT(pos >= 0 && pos <= kLoadStayBack, 0x121);
    if (abbrev)
        return abbrevs[pos];
    else
        return names[pos];
}

void LoadMgr::Print() {
    FOREACH (it, mLoading) {
        TheDebug << (*it)->LoaderFile().c_str() << " "
                 << LoaderPosString((*it)->GetPos(), false) << "\n";
    }
}

void LoadMgr::SetEditMode(bool) {
    static DataNode &edit_mode = DataVariable("edit_mode");
    edit_mode = 0;
}

Loader *LoadMgr::ForceGetLoader(const FilePath &fp) {
    if (fp.empty())
        return nullptr;
    else {
        Loader *gotten = GetLoader(fp);
        if (!gotten) {
            gotten = TheLoadMgr.AddLoader(fp, kLoadFront);
            if (!gotten) {
                MILO_NOTIFY("Don't recognize file %s", FilePath(fp));
            }
        }
        if (gotten) {
            TheLoadMgr.PollUntilLoaded(gotten, 0);
        }
        return gotten;
    }
}

#ifdef HX_NATIVE
void LoadMgr::PollFrontLoader() {
    if (!mLoading.empty()) {
        mLoading.front()->PollLoading();
    }
}
#else
void LoadMgr::PollFrontLoader() {
    // Retail polls the front loader on its own heap and at its own position; it
    // does no glitch reporting or load timing here.
    Loader *front = mLoading.front();
    LoaderPos savedPos = mLoaderPos;
    mLoaderPos = front->mPos;
    {
        MemHeapTracker tmp(front->mHeap);
        front->PollLoading();
    }
    mLoaderPos = savedPos;
}
#endif

void LoadMgr::Poll() {
    if (mPeriod > 0) {
        mTimer.Restart();
        mCurrentPeriod = mPeriod;
        while (!mLoading.empty()) {
            PollFrontLoader();
            if (!mLoading.empty()) {
                if (mLoading.front()->IsLoaded()) {
                    mLoading.pop_front();
                }
            }
            if (CheckSplit())
                return;
        }
    }
}

void LoadMgr::RegisterFactory(const char *cc, LoaderFactoryFunc *func) {
#ifdef HX_NATIVE
    // DC3's duplicate-extension scan; retail (RB3) registers unconditionally.
    FOREACH (it, mFactories) {
        if (it->first == cc) {
            MILO_NOTIFY("More than one LoadMgr factory for extension \"%s\"!", cc);
        }
    }
#endif
    mFactories.push_back(std::pair<String, LoaderFactoryFunc *>(cc, func));
}

Loader *LoadMgr::GetLoader(const FilePath &fp) const {
    if (fp.empty())
        return nullptr;
    else {
        Loader *theLoader = nullptr;
        FOREACH (it, mLoaders) {
            if ((*it)->LoaderFile() == fp) {
                theLoader = *it;
                break;
            }
        }
        return theLoader;
    }
}

Loader *LoadMgr::AddLoader(const FilePath &file, LoaderPos pos) {
    if (file.empty())
        return nullptr;
    if (sFileOpenCallback) {
        sFileOpenCallback(file.c_str());
    }
    const char *ext = FileGetExt(file.c_str());
    FOREACH (it, mFactories) {
        if (it->first == ext) {
            return (it->second)(file, pos);
        }
    }
    return new FileLoader(file, file.c_str(), pos, 0, false, true, nullptr);
}

void LoadMgr::PollUntilLoaded(Loader *ldr1, Loader *ldr2) {
#ifdef HX_NATIVE
    AutoGlitchReport hang(50.0f, __FUNCTION__);
    float saved_period = mCurrentPeriod;
#else
// Retail RB3-360 does NOT wrap this call in AutoGlitchReport -- dc3 is a
// NEWER build and added the hang-report wrapper (+ctor/dtor pair, +f30 saved
// reg, +0x50-byte frame, +period save/restore) to this function later.
// fn_827BF608's full instruction listing has none of that; in its place
// retail stamps a re-entrance guard using the mLoadCount field the header
// already declares ("snapshot of gLoadCount for re-entrance detection") but
// which was otherwise dead code -- written only by the ctor, never read/set
// here. Verified against the target listing: idx 6/14/18-20 are
// `lis/lwz/addi/stw` doing exactly `ldr1->mLoadCount = ++gLoadCount;`, and
// idx 41-43 (`beq/lwz/cmpw`, present in target, absent from base pre-fix) is
// `if (ldr1->mLoadCount != loadCount) break;` re-checked after each
// PollFrontLoader() -- guards against a nested PollUntilLoaded (invoked from
// inside PollFrontLoader) racing this one's wait.
    int loadCount = ++gLoadCount;
    ldr1->mLoadCount = loadCount;
#endif
#ifdef HX_WEB
    int maxIter = 10000; // Safety valve: don't block browser event loop forever
#endif
    while (!ldr1->IsLoaded()) {
#ifdef HX_WEB
        if (--maxIter <= 0) {
            MILO_WARN("PollUntilLoaded: timeout waiting for %s", ldr1->DebugText());
            break;
        }
#endif
        mCurrentPeriod = 1e+30f;
        if (ldr2 && ldr2 == mLoading.front()) {
// Retail RB3-360 EXCLUDES this.  Retail's LoadMgr::PollUntilLoaded is
// fn_827BF608 (identified map-independently: ForceGetLoader fn_827BFEC0 does
// `mr r3, TheLoadMgr; li r5,0; mr r4, gotten; bl fn_827BF608`).  In that whole
// 0xF8-byte body the second parameter ldr2 (r5) is NEVER READ -- r5 is only
// reused later as a scratch arg to pop_front.  ldr2 is used ONLY inside this
// guard, so a provably-dead ldr2 is exactly what excluding the guard predicts
// (and is impossible if retail included it).  Gate on HX_NATIVE.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
            MILO_FAIL(
                "PollUntilLoaded circular dependency %s on %s",
                ldr2->DebugText(),
                ldr1->DebugText()
            );
#endif
        }
        PollFrontLoader();
        if (!ListFind(mLoading, ldr1))
            break;
#ifndef HX_NATIVE
        if (ldr1->mLoadCount != loadCount)
            break;
#endif
        if (mLoading.front()->IsLoaded()) {
            mLoading.pop_front();
        }
    }
#ifdef HX_NATIVE
    mCurrentPeriod = saved_period;
#endif
}

#pragma endregion
#pragma region Handlers

DataNode OnSetLoadMgrDebug(DataArray *a) {
    TheLoadMgr.SetCacheMode(a->Int(1));
    return 0;
}

DataNode OnSetEditMode(DataArray *a) {
    TheLoadMgr.SetEditMode(a->Int(1));
    return 0;
}

DataNode OnSetLoaderPeriod(DataArray *a) {
    return TheLoadMgr.SetLoaderPeriod(a->Float(1));
}

DataNode OnSysPlatformSym(DataArray *a) {
#ifdef HX_NATIVE
    return PlatformSymbol(TheLoadMgr.GetPlatform());
#else
    // retail passes the constant (li r4, 2 = kPlatformXBox), like "sysplatform"
    return PlatformSymbol(kPlatformXBox);
#endif
}

DataNode OnLoadMgrPrint(DataArray *a) {
    TheLoadMgr.Print();
    return 0;
}

void LoadMgr::Init() {
    SetEditMode(false);
// Retail RB3-360's LoadMgr::Init contains NO `bl ?OptionBool@@YA_NPBD_N@Z` at
// all (objdiff "Base only" on the 212 B retail body), so the null_platform
// override is not in the retail source.  Dropping it also drops the r29/r30
// pair retail never saves -- retail's prologue is a bare stw r12,-0x8(r1) with
// a 0x70 frame where ours was __savegprlr_29 with 0x80.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    if (OptionBool("null_platform", false))
        mPlatform = kPlatformNone;
#endif
// Retail RB3-360 EXCLUDES these two dev registrations.  Retail's LoadMgr::Init
// builds exactly four literal Symbols -- set_edit_mode, set_loader_period,
// sysplatform_sym, sysplatform -- with no loadmgr_debug/loadmgr_print ctor in
// the body (retail_props.py over band.exe).  A DEV build keeps
// them unguarded; retail decides.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    DataRegisterFunc("loadmgr_debug", OnSetLoadMgrDebug);
    DataRegisterFunc("loadmgr_print", OnLoadMgrPrint);
#endif
    DataRegisterFunc("set_edit_mode", OnSetEditMode);
    DataRegisterFunc("set_loader_period", OnSetLoaderPeriod);
    DataRegisterFunc("sysplatform_sym", OnSysPlatformSym);
// Retail emits `li r11,0x2` here, NOT a load of mPlatform from this+0x58 --
// i.e. retail's source is a compile-time constant, and kPlatformXBox == 2
// (os/Platform.h).  Keep the member read for the native host build, where the
// platform is genuinely not the 360.
#ifdef HX_NATIVE
    DataVariable("sysplatform") = (int)mPlatform;
#else
    DataVariable("sysplatform") = (int)kPlatformXBox;
#endif
}
