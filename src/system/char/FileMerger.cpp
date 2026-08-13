// This TU #includes CharIKHand.cpp below (unity-build merge), so ObjPtr_p.h's
// per-site owner-only ctor gate must be set HERE, before the first transitive
// include of obj/Object.h (line 1 -> FileMerger.h -> Object.h) -- the template
// body in ObjPtr_p.h is textually fixed at that first inclusion, so a #define
// placed later in this file (e.g. at the top of the merged CharIKHand.cpp
// section) has no effect on this TU's compile. CharIKHand::CharIKHand()
// constructs mHand(this)/mFinger(this)/mElbowCollide(this) as ObjPtr<T>, and
// retail's target asm never calls the two-arg ObjPtr ctor for any of the
// three (Function Call Diff showed all 3 `bl ...ObjPtr...ctor` as base-only) --
// it inlines three raw stores per member in the order {mOwner, vptr-lis,
// mObject=0, vptr-addi, vptr-store}, exactly the
// RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT shape documented at its definition
// site in obj/Object.h. Scope check: the only other ObjPtr(this) site pulled
// into this merged TU is RndMorph::RndMorph()'s mTarget(this) (via the
// `#include "rndobj/Morph.cpp"` further down) -- but that ctor is not pinned
// by any .text range under the FileMerger.cpp splits.txt entry (it's scored
// under the separate standalone `default/Morph` unit instead), so it is not
// diff-scored here and this gate cannot regress it.
#define RB3_OBJPTR_INLINE_OWNER_CTOR
#define RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT

#include "char/FileMerger.h"
#include "char/FileMergerOrganizer.h"
#include "CharClipGroup.h"
#include "char/CharPollGroup.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Utl.h"
#include "os/Debug.h"
#include "rndobj/Group.h"
#include "rndobj/Mat.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/PoolAlloc.h"

#ifdef HX_NATIVE
bool FileMerger::sDisableAll;
#endif
FileMerger *FileMerger::sFmDeleting;

class NullLoader : public Loader {
public:
    NullLoader(const FilePath &fp, LoaderPos pos, Loader::Callback *cb)
        : Loader(fp, pos), mCallback(cb) {}
    virtual ~NullLoader() {
        if (mCallback)
            mCallback->FailedLoading(this);
    }
    virtual const char *DebugText() {
        return MakeString("NullLoader: %s", mFile.c_str());
    }
    virtual bool IsLoaded() const { return false; }
    virtual const char *StateName() const { return "NullLoader"; }

    POOL_OVERLOAD(NullLoader, 0x1F);

protected:
    virtual void PollLoading() {
        mCallback->FinishLoading(this);
        mCallback = nullptr;
        delete this;
    }

    Loader::Callback *mCallback; // 0x18
};

void FileMerger::Merger::Clear() {
    mLoaded.Set(FilePath::Root().c_str(), "");
#ifdef HX_NATIVE
    if (!ObjectDir::InDeleteObjects())
#endif
    {
        Hmx::Object *owner = mLoadedObjects.Owner();
        if (owner != sFmDeleting) {
            static Message msg("on_pre_clear", 0);
            msg[0] = mName;
            owner->HandleType(msg);
        }
    }
#ifdef HX_NATIVE
    if (ObjectDir::InDeleteObjects()) {
        // During cascade, these objects are dir-owned and will be cleaned
        // up by DeleteObjects. Just clear the list without deleting.
        mLoadedObjects.clear();
    } else
#endif
    {
        while (!mLoadedObjects.empty()) {
            Hmx::Object *front = mLoadedObjects.front();
            delete front;
        }
    }
#ifdef HX_NATIVE
    if (ObjectDir::InDeleteObjects()) {
        // During cascade, subdirs are handled by DeleteSubDirs.
        mLoadedSubdirs.clear();
    } else
#endif
    {
        ObjectDir *mergerDir = MergerDir();
        if (mergerDir) {
            while (!mLoadedSubdirs.empty()) {
                ObjectDir *curSubdir = mLoadedSubdirs.front();
                mLoadedSubdirs.pop_front();
                mergerDir->RemoveSubDir(curSubdir);
            }
        } else {
            mLoadedSubdirs.clear();
        }
    }
}

FileMerger::FileMerger()
    : mMergers(this), mAsyncLoad(0), mLoadingLoad(0), mCurLoader(0), mFilter(0),
      mHeap(GetCurrentHeapNum()), mOrganizer(this) {
    MILO_ASSERT(MemNumHeaps() == 0 || (mHeap != kNoHeap && mHeap != kSystemHeap), 0x86);
}

FileMerger::~FileMerger() {
    FileMerger *old = sFmDeleting;
    sFmDeleting = this;
    Clear();
    sFmDeleting = old;
}

BEGIN_HANDLERS(FileMerger)
    HANDLE_EXPR(loaded, FindMerger(_msg->Sym(2), true)->mLoaded)
    HANDLE(select, OnSelect)
    HANDLE(start_load, OnStartLoad)
    HANDLE_ACTION(clear, Clear())
    HANDLE_ACTION(clear_selections, ClearSelections())
    HANDLE_EXPR(merger_index, FindMergerIndex(_msg->Sym(2), _msg->Int(3)))
    HANDLE_EXPR(is_loading, 0)
    HANDLE_ACTION(clear_filter, mFilter = nullptr)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(FileMerger::Merger)
    SYNC_PROP(name, o.mName)
    SYNC_PROP(selected, o.mSelected)
    SYNC_PROP_SET(loaded, o.mLoaded, )
    SYNC_PROP(dir, o.mDir)
    SYNC_PROP(proxy, o.mProxy)
    SYNC_PROP(subdirs, (int &)o.mSubdirs)
    SYNC_PROP(preclear, o.mPreClear) {
        static Symbol _s("loaded_objects");
        if (sym == _s && (_op & (kPropSize | kPropGet))) {
            return PropSync(o.mLoadedObjects, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol _s("loaded_subdirs");
        if (sym == _s && (_op & (kPropSize | kPropGet))) {
            return PropSync(o.mLoadedSubdirs, _val, _prop, _i + 1, _op);
        }
    }
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(FileMerger)
    SYNC_PROP(mergers, mMergers)
    SYNC_PROP(disable_all, sDisableAll)
    SYNC_PROP_SET(loading_load, mLoadingLoad, )
    SYNC_PROP_SET(async_load, mAsyncLoad, )
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BinStream &operator<<(BinStream &bs, const FileMerger::Merger &fm) {
    bs << fm.mName;
    bs << fm.mSelected;
    bs << fm.mLoaded;
    bs << fm.mDir;
    bs << fm.mProxy;
    bs << fm.mSubdirs;
    bs << fm.mPreClear;
    return bs;
}

BEGIN_SAVES(FileMerger)
    SAVE_REVS(5, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mMergers;
END_SAVES

BEGIN_COPYS(FileMerger)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(FileMerger)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMergers)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(FileMerger)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void FileMerger::PreSave(BinStream &) { Clear(); }
void FileMerger::PostSave(BinStream &) { StartLoadInternal(false, false); }

BinStreamRev &operator>>(BinStreamRev &d, FileMerger::Merger &fm) {
    d >> fm.mName;
    d >> fm.mSelected;
    d >> fm.mLoaded;
    d >> fm.mDir;
    if (d.rev > 0) {
        if (d.rev != 4) {
            d >> fm.mProxy;
        }
        d >> (int &)fm.mSubdirs;
        if (d.rev > 2) {
            d >> fm.mPreClear;
        }
    }
    return d;
}

INIT_REVS(5, 0)

void FileMerger::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(5, 0)
    Hmx::Object::Load(bs);
    if (d.rev < 2) {
        String str;
        d >> str;
    }
    d >> mMergers;
    // StartLoadInternal fires change_files (which lets DTA type handlers
    // wire merger properties, e.g. {$hamdirector set merger $this}),
    // then iterates mergers to start loading any files that were selected
    // during change_files (e.g. the song .milo queued by load_game_song).
    StartLoadInternal(true, true);
}

void FileMerger::FinishLoading(Loader *ldr) {
    // Retail computes the ObjectDir HERE and passes it to NotifyFileLoaded --
    // `bl DirLoader::GetDir` (fn_82754A00, returns ObjectDir*) lands in r5, the
    // 2nd-parameter register, guarded by the dynamic_cast result. See the
    // NotifyFileLoaded signature note below.
    DirLoader *dl = dynamic_cast<DirLoader *>(ldr);
    ObjectDir *dlDir = dl ? dl->GetDir() : nullptr;
    Merger *merger = NotifyFileLoaded(ldr, dlDir);
    if (dl) {
        if (!sDisableAll) {
            if (merger->mProxy) {
                ObjectDir *dir = Dir()->Find<ObjectDir>(dl->GetDir()->Name(), false);
                if (dir) {
                    ReserveToFit(dl->GetDir(), dir, 0);
                    MergeDirs(dl->GetDir(), dir, *this);
                    dir->SyncObjects();
                } else {
                    ObjectDir *loaderDir = dl->GetDir();
                    ReserveToFit(nullptr, Dir(), 2);
                    loaderDir->SetName(loaderDir->Name(), Dir());
                    merger->mLoadedObjects.push_back(loaderDir);
                }
            } else {
                ObjectDir *mergerDir = merger->MergerDir();
                ReserveToFit(dl->GetDir(), mergerDir, 0);
                MergeDirs(dl->GetDir(), mergerDir, *this);
            }
        }
        // Retail deletes the loader (or its dir) HERE, not in PostMerge, and
        // OUTSIDE the sDisableAll guard: the target reaches this block from the
        // sDisableAll-true edge too (`bne .L_8234DEC8`), while the whole thing
        // sits under `if (dl)`.
        if (!merger->mProxy) {
            delete dl->GetDir();
        } else
            delete dl;
    }
    PostMerge(merger, true);
}

void FileMerger::FailedLoading(Loader *l) {
    MILO_ASSERT(l == mCurLoader, 0x204);
    MILO_ASSERT(l->LoaderFile() == mFilesPending.front()->loading, 0x205);
    static Message msg("on_load_failed", 0);
    msg[0] = mFilesPending.front()->mName;
    HandleType(msg);
    PostMerge(mFilesPending.front(), false);
    // Retail does the dynamic_cast AFTER the PostMerge call, not as an argument
    // to it: the `bl __RTDynamicCast` at 0x8234D5F4 follows `bl PostMerge` at
    // 0x8234D5D4. The `!b3 &&` guard DC3 needs disappears because this arm only
    // ever runs on the failure path.
    DirLoader *dl = dynamic_cast<DirLoader *>(l);
    if (dl && !dl->IsLoaded()) {
        dl->SetDeleteSelf(true);
    }
}

MergeFilter::Action FileMerger::Filter(Hmx::Object *o1, Hmx::Object *o2, ObjectDir *dir) {
    Action a;
    if (mFilter) {
        a = mFilter->Filter(o1, o2, dir);
    } else {
        a = MergeAction(o1, o2, dir);
    }
    if (a == 1 && !o2) {
        mFilesPending.front()->mLoadedObjects.push_back(o1);
    }
    return a;
}

__declspec(noinline) void FileMerger::AddSubdir(ObjectDir *dir) {
    mFilesPending.front()->mLoadedSubdirs.push_back(dir);
}

MergeFilter::SubdirAction FileMerger::FilterSubdir(ObjectDir *o1, ObjectDir *o2) {
    SubdirAction a;
    Merger *merger = mFilesPending.front();
    if (mFilter) {
        a = mFilter->FilterSubdir(o1, o2);
    } else {
        a = DefaultSubdirAction(o1, merger->mSubdirs);
    }
    if (a == kMergeReplace && !o2->HasSubDir(o1)) {
        AddSubdir(o1);
    }
    return a;
}

bool FileMerger::OriginalPath(Hmx::Object *obj, String &str) {
    Merger *merger = InMerger(obj);
    if (merger) {
        str = merger->mLoaded;
        return true;
    } else {
        return false;
    }
}

void FileMerger::Clear() {
    for (int i = 0; i < mMergers.size(); i++) {
        mMergers[i].Clear();
    }
    if (mCurLoader) {
        Merger *merger = mFilesPending.front();
        mFilesPending.clear();
        mFilesPending.push_front(merger);
        DeleteCurLoader();
    }
}

bool FileMerger::StartLoad(bool b) { return StartLoadInternal(b, false); }

#ifdef HX_NATIVE
void FileMerger::ForceReleaseOrganizer() {
    // Cancel any pending async loads and release from the organizer.
    // Used by tests where the game loop isn't running to drain TheLoadMgr.
    if (mCurLoader) {
        DeleteCurLoader();
        mCurLoader = nullptr;
    }
    mFilesPending.clear();
    mOrganizer = this;
}
#endif

FileMerger::Merger *FileMerger::FindMerger(Symbol name, bool warn) {
    int idx = FindMergerIndex(name, warn);
    if (idx != -1) {
        return &mMergers[idx];
    } else {
        return nullptr;
    }
}

void FileMerger::ClearSelections() {
    for (int i = 0; i < mMergers.size(); i++) {
        mMergers[i].mSelected.Set(FilePath::Root().c_str(), "");
    }
    if (mCurLoader) {
        Merger *front = mFilesPending.front();
        mFilesPending.clear();
        mFilesPending.push_front(front);
        DeleteCurLoader();
    }
}

int FileMerger::FindMergerIndex(Symbol name, bool warn) {
    for (int i = 0; i < mMergers.size(); i++) {
        if (mMergers[i].mName == name) {
            return i;
        }
    }
    if (warn) {
        MILO_NOTIFY("%s could not find Merger %s", PathName(this), name);
    }
    return -1;
}

FileMerger::Merger *FileMerger::InMerger(Hmx::Object *o) {
    for (int i = 0; i < mMergers.size(); i++) {
        Merger &cur = mMergers[i];
        if (cur.IsObjectLoaded(o)) {
            return &cur;
        }
    }
    return nullptr;
}

void FileMerger::DeleteCurLoader() {
    if (mCurLoader) {
        DirLoader *d = dynamic_cast<DirLoader *>(mCurLoader);
        if (d)
            d->SetForceFailCallback(true);
        delete mCurLoader;
    }
}

MergeFilter::Action
FileMerger::MergeAction(Hmx::Object *o1, Hmx::Object *o2, ObjectDir *dir) {
    if (!o2) {
        return (Action)1;
    }
    const char *name = o1->Name();
    DirLoader *dl = static_cast<DirLoader *>(mCurLoader);
    ObjectDir *dlDir = dl->GetDir();
    if (o1 == dlDir) {
        MsgSource *src1 = dynamic_cast<MsgSource *>(o2);
        if (src1) {
            MsgSource *src2 = dynamic_cast<MsgSource *>(dlDir);
            if (src2) {
                src1->MergeSinks(src2);
            }
        }
        return (Action)2;
    } else {
        if (strnicmp("spot_", name, 5) == 0 || strnicmp("bone_", name, 5) == 0
            || dynamic_cast<RndGroup *>(o2) || dynamic_cast<CharClipGroup *>(o2)
            || dynamic_cast<CharPollGroup *>(o2)) {
            return (Action)0;
        }
        if (!dynamic_cast<RndMat *>(o2)) {
            RndTex *tex = dynamic_cast<RndTex *>(o2);
            if (tex && !tex->File().empty()) {
                MILO_LOG(
                    "%s replacing texture %s with %s\n",
                    PathName(this),
                    PathName(o2),
                    PathName(o1)
                );
                return (Action)1;
            } else if (o2->Dir() != dir) {
                MILO_NOTIFY(
                    "%s trying to replace subdir'd object %s with %s, bad because subdirs are shared",
                    PathName(this),
                    PathName(o2),
                    PathName(o1)
                );
                return (Action)2;
            } else
                return (Action)1;
        }
    }
    return (Action)2;
}

bool FileMerger::NeedsLoading(FileMerger::Merger &merger) {
    FOREACH (it, mFilesPending) {
        if (*it == &merger) {
            return merger.mSelected != merger.loading || merger.mForceReload;
        }
    }
    return merger.mLoaded != merger.mSelected || merger.mForceReload;
}

void FileMerger::LaunchNextLoader() {
    MILO_ASSERT(!mFilesPending.empty(), 0x182);
    MILO_ASSERT(!mCurLoader, 0x183);
    int pos;
    // Determine loader position based on current loader state
    if (Dir()->Loader() && !Dir()->Loader()->IsLoaded()) {
        if (Dir()->Loader()->GetPos() != kLoadStayBack) {
            if (Dir()->Loader()->GetPos() != kLoadFrontStayBack)
                goto next;
        }
        pos = 2;
    } else {
        pos = 0;
    }

// Create the next loader with the determined position
next:
    FilePath &fp = mFilesPending.front()->loading;
    MemHeapTracker tmp(mHeap);
    if (fp.empty()) {
        mCurLoader = new NullLoader(fp, (LoaderPos)pos, mOrganizer);
    } else if (DirLoader::ShouldBlockSubdirLoad(fp)) {
        mCurLoader = new NullLoader(fp, (LoaderPos)pos, mOrganizer);
    } else {
#ifdef HX_NATIVE
        mCurLoader = new DirLoader(
            fp, (LoaderPos)pos, mOrganizer, nullptr, nullptr, false,
            // Pass merger's Dir as parent so ObjPtr fallback can resolve
            // objects in the world ObjectDir during deserialization.
            // On Xbox, FileMerger flattens objects into the same scope.
            Dir()
        );
#else
        mCurLoader =
            new DirLoader(fp, (LoaderPos)pos, mOrganizer, nullptr, nullptr, false);
#endif
    }
}

void FileMerger::Select(Symbol name, const FilePath &fp, bool b3) {
    Merger *merger = FindMerger(name, true);
    if (merger) {
        merger->SetSelected(fp, b3);
    }
}

bool FileMerger::StartLoadInternal(bool async, bool loading) {
#if !defined(MILO_VIEWER)
    // The game relies on change_files to translate high-level selections like
    // HamCharacter::mOutfit into concrete merger paths before loading.
    // milo-viewer configures char.fm explicitly via --char-setup, so keep the
    // viewer override there instead of short-circuiting the game binary.
    static Message msg("change_files", 0, 0);
    msg[0] = async;
    msg[1] = loading;
    HandleType(msg);
#endif
    for (int i = 0; i < mMergers.size(); i++) {
        Merger &cur = mMergers[i];
        if (NeedsLoading(cur)) {
            AppendLoader(cur);
        }
    }
    Merger *tmp = nullptr;
    if (mCurLoader) {
        tmp = mFilesPending.front();
        mFilesPending.pop_front();
    }
    mFilesPending.sort(FileMergerSort());
    if (mCurLoader)
        mFilesPending.push_front(tmp);
    mAsyncLoad = async;
    mLoadingLoad = loading;
    if (mFilesPending.empty() || mCurLoader || mOrganizer != this) {
        return false;
    } else {
        if (async) {
            TheFileMergerOrganizer->AddFileMerger(this);
        } else {
            LaunchNextLoader();
            while (!mFilesPending.empty()) {
                TheLoadMgr.Poll();
            }
        }
        return true;
    }
}

FileMerger::Merger *FileMerger::NotifyFileLoaded(Loader *l, ObjectDir *dir) {
    // Signature RESTORED to the RB3-era (rb3-Wii) shape by lane FILEMERGER-1;
    // we had carried DC3's newer `(Loader *, DirLoader *)`. Adjudicated on
    // retail bytes, not on the oracle: retail's FinishLoading computes
    // `d ? d->GetDir() : nullptr` into r5 -- and fn_82754A00 is mapped
    // `?GetDir@DirLoader@@QAAPAVObjectDir@@XZ`, i.e. it RETURNS ObjectDir* --
    // and both `msg[1] = <param>` sites below apply the ObjectDir->Hmx::Object
    // virtual-base adjust (`lwz r11,4(p); lwz r11,4(r11); add; addi r11,r11,4`)
    // directly to the raw parameter, a conversion DirLoader does not have
    // (DirLoader : Loader, ObjRefOwner; no vbtable).
    // ⚠ COUPLED TO scripts/target_symbol_map.json: objdiff pairs target<->base
    // by MANGLED NAME, so row 0x823927c8 was re-spelled in the same change.
    // Census first: this spelling occupied exactly ONE map row, not at 100%, so
    // the re-mangle risked no already-matching bytes.
    MILO_ASSERT_FMT(
        l->LoaderFile() == mFilesPending.front()->loading,
        "%s != %s",
        l->LoaderFile(),
        mFilesPending.front()->loading
    );
    MILO_ASSERT(l == mCurLoader, 0x217);
    Merger *m = mFilesPending.front();
    m->Clear();
    if (!sDisableAll) {
        static Message msg("on_pre_merge", 0, 0, 0);
        msg[0] = m->mName;
        msg[1] = dir;
        msg[2] = m->MergerDir();
        HandleType(msg);
        // RB3-360-retail-exclusive game hacks -- present in NO oracle (absent
        // from both rb3-Wii and DC3), recovered from the retail disassembly.
        // The five literals decode straight out of band.exe .rdata:
        // 0x8200100C "main", 0x8201232C "body_realtime_clips",
        // 0x82012318 "body_tempo_clips", 0x820137FC "hack_fix_clips_pre_merge",
        // 0x82047D90 "crowd_anim".  `hack_fix_clips_pre_merge` is the handler
        // slot BandCharacter.cpp already flags as misnamed (see its comment at
        // the `toggle_interests_overlay` arm) -- this is its sender.
        if (Dir()) {
            if (Type() == "main") {
                if (m->mName == "body_realtime_clips"
                    || m->mName == "body_tempo_clips") {
                    static Message hackMsg("hack_fix_clips_pre_merge", 0, 0);
                    hackMsg[0] = dir;
                    hackMsg[1] = m->mName;
                    Dir()->Handle(hackMsg, true);
                }
            } else if (Type() == "crowd_anim") {
                // Crowd animations must play in real time, never beat-aligned.
                if (m->MergerDir()) {
                    for (ObjDirItr<CharClip> it(m->MergerDir(), true); it; ++it) {
                        it->SetBeatAlignMode(CharClip::kPlayRealTime);
                    }
                }
            }
        }
        m->mLoaded = m->loading;
        m->loading.SetRoot("");
    }
    return m;
}

void FileMerger::AppendLoader(FileMerger::Merger &merger) {
    merger.mForceReload = false;
    FOREACH (it, mFilesPending) {
        if (*it == &merger) {
            if (mCurLoader) {
                if (it == mFilesPending.begin()) {
                    DeleteCurLoader();
                    break;
                }
            }
            mFilesPending.erase(it);
            break;
        }
    }
    merger.loading = merger.mSelected;
    if (merger.mPreClear)
        merger.Clear();
    mFilesPending.push_back(&merger);
    if (TheLoadMgr.EditMode()) {
        static Message checkSync("check_sync", "", "");
        checkSync[0] = merger.loading;
        checkSync[1] = merger.mName;
        HandleType(checkSync);
    }
}

// Retail arity is THREE args, not four -- `PostMerge(Merger *, bool)`, the
// rb3-Wii shape; we had carried DC3's `(Merger *, DirLoader *, bool)`. Proven
// independently at BOTH call sites on retail bytes: FinishLoading loads only
// r3/r4/r5 (`mr r3,r26; mr r4,r27; li r5,1`) and FailedLoading likewise
// (`mr r3,r29; lwz r4,0x8(r11); li r5,0`) -- r6 is never written before either
// `bl`, and the callee tests its 3rd argument with `clrlwi. r25,r29,24`, an
// 8-bit bool. Map row 0x82391a60 re-spelled in the same change (1 row, 1.63%).
//
// The body is correspondingly SMALLER than DC3's: retail sends ONE message and
// deletes nothing. The `delete dl` block moved to FinishLoading and the
// SetDeleteSelf tail to FailedLoading (see both). DC3's second message,
// "on_post_delete", DOES NOT EXIST IN RETAIL -- a binary-safe scan of band.exe
// finds 0 occurrences ascii+utf16, against exactly 1 each for on_post_merge /
// on_pre_merge / on_load_failed / change_files, so the screen is controlled.
// Retail's surviving "on_post_merge" carries DC3's on_post_delete ARGUMENTS
// (name, MergerDir, empty) -- read off the target, not inferred from the name.
void FileMerger::PostMerge(FileMerger::Merger *merger, bool b3) {
    mCurLoader = nullptr;
    mFilesPending.pop_front();
    mFilter = nullptr;
    if (b3) {
        static Message msg("on_post_merge", 0, 0, 0);
        msg[0] = merger->mName;
        msg[1] = merger->MergerDir();
        msg[2] = mFilesPending.empty();
        HandleType(msg);
    }
    if (b3 || mOrganizer == this) {
        if (mFilesPending.empty()) {
            MILO_ASSERT(!mCurLoader, 0x290);
        } else if (!mCurLoader)
            LaunchNextLoader();
    }
}

DataNode FileMerger::OnSelect(const DataArray *a) {
    FilePath fp(a->Str(3));
    Select(a->Sym(2), fp, false);
    return 0;
}

DataNode FileMerger::OnStartLoad(const DataArray *a) {
    StartLoadInternal(a->Size() == 3 ? a->Int(2) : true, false);
    return 0;
}

// RB3 retail linker interleaved CharIKHand.cpp's COMDATs into this TU's .text
// span (CharIKHand::Poll/PollDeps + a template op). Compile its bodies here so
// objdiff can pair them (sw scatter-scan). gRev/gAltRev collide with this TU's
// own INIT_REVS (both file-scope static const), so rename for the include; they
// are compile-time literals used only inside CharIKHand's functions -> byte-neutral.
#define gRev gRev_CharIKHand
#define gAltRev gAltRev_CharIKHand
#include "char/CharIKHand.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/FileMerger <- rndobj/Morph.cpp)
#define gRev gRev_Morph
#define gAltRev gAltRev_Morph
#include "rndobj/Morph.cpp"
#undef gRev
#undef gAltRev
