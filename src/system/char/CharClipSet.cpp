// Retail inlines the owner-only ObjPtr<RndDir> ctor for mPreviewChar (3 raw
// stores, no `bl`) while keeping mPreviewClip/mStillClip out-of-line -- a
// per-site decision, not per-TU (see the ObjPtr block in obj/Object.h). Opt
// this TU into the inline majority (mPreviewChar) and opt the other two back
// out via the explicit two-arg spelling below. char/ is PCH-excluded so the
// #define ordering here is safe (no /FI decomp_pch.h preempting it).
#define RB3_OBJPTR_INLINE_OWNER_CTOR
// ...and take retail's owner-only ctor SHAPE: the base ctor gets only the
// owner, the derived body assigns mObject, so the mObject store lands after
// the derived vptr store instead of floating up into an earlier load-use
// stall. Worth ??0CharClipSet@@IAA@XZ 96.6 -> 100 and fn_823D0AFC 99.9 -> 100;
// whole binary 41631 -> 41633, zero regressions. Rationale + the two spellings
// that measurably do NOT work are documented at the gate in obj/Object.h.
#define RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT
#include "char/CharClipSet.h"
#include "char/CharBoneDir.h"
#include "char/CharClip.h"
#include "char/CharClipGroup.h"
#include "obj/ObjPtrVec_impl.h"
#include "char/Character.h"
#include "char/CharBonesMeshes.h"
#include "char/CharUtl.h"
#include "char/CharForeTwist.h"
#include "char/CharUpperTwist.h"
#include "char/CharNeckTwist.h"
#include "char/CharPollable.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Draw.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"

CharClipSet::CharClipSet()
    : mCharFilePath(), mPreviewChar(this), mPreviewClip(this, nullptr),
      mStillClip(this, nullptr) {
    ResetPreviewState();
    SetRate(k1_fpb);
}

CharClipSet::~CharClipSet() {}

BEGIN_HANDLERS(CharClipSet)
    HANDLE_ACTION(randomize_groups, RandomizeGroups())
    HANDLE_ACTION(sort_groups, SortGroups())
    HANDLE_ACTION(recenter_all, RecenterAll())
    HANDLE_ACTION(load_character, LoadCharacter())
    HANDLE(list_clips, OnListClips)
    HANDLE_SUPERCLASS(ObjectDir)
END_HANDLERS

BEGIN_PROPSYNCS(CharClipSet)
    SYNC_PROP(char_file_path, mCharFilePath)
    SYNC_PROP(preview_clip, mPreviewClip)
    SYNC_PROP(still_clip, mStillClip)
    SYNC_PROP(filter_flags, mFilterFlags)
    SYNC_PROP_SET(bpm, mBpm, SetBpm(_val.Int()))
    SYNC_PROP(preview_walk, mPreviewWalk)
    SYNC_SUPERCLASS(ObjectDir)
END_PROPSYNCS

BEGIN_SAVES(CharClipSet)
    SAVE_REVS(24, 0)
    SAVE_SUPERCLASS(ObjectDir)
    if (!IsProxy()) {
        bs << mCharFilePath;
        bs << mPreviewClip;
        bs << mFilterFlags;
        bs << mBpm;
        bs << mPreviewWalk;
        bs << mStillClip;
    }
END_SAVES

BEGIN_COPYS(CharClipSet)
    COPY_SUPERCLASS(ObjectDir)
    CREATE_COPY(CharClipSet)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mCharFilePath)
        COPY_MEMBER(mPreviewClip)
        COPY_MEMBER(mFilterFlags)
        COPY_MEMBER(mBpm)
        COPY_MEMBER(mPreviewWalk)
        COPY_MEMBER(mStillClip)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(CharClipSet)
    ObjectDir::Load(bs);
END_LOADS

void CharClipSet::PreSave(BinStream &bs) {
    if (mPreviewChar)
        mPreviewChar->SetName("", nullptr);
    if (bs.Cached()) {
        ResetPreviewState();
        ResetEditorState();
    }
}

void CharClipSet::PostSave(BinStream &bs) {
    ObjectDir::PostSave(bs);
    if (mPreviewChar) {
        mPreviewChar->SetName("preview_character", this);
        mPreviewChar->Enter();
    }
}

INIT_REVS(0x18, 0)

void CharClipSet::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(0x18, 0);
    MILO_ASSERT(d.rev > 3, 0x4E);
    ObjectDir::PreLoad(bs);
    bs.PushRev(packRevs(d.altRev, d.rev), this);
}

// Retail stores the PostLoad-side rev/altRev in fixed global storage (proven
// via an absolute lis+addi load of lbl_82CBF924/928 in the target
// disassembly, build/45410914/asm/CharClipSet.s) rather than in a
// stack-resident BinStreamRev, unlike the dc3-ported source this TU started
// from. CharClipSet.h uses the Object.h macro dialect (no DECLARE_REVS), so
// adding class statics is the risky route -- these file-statics are
// TU-local and get the same addressing-mode codegen (lis+addi to a fixed
// address) without touching the shared header's macro dialect.
static unsigned short sPostLoadRev;
static unsigned short sPostLoadAltRev;

void CharClipSet::PostLoad(BinStream &bs) {
    // ★ THE POP MUST PRECEDE ObjectDir::PostLoad. PushRev/PopRev is a LIFO
    // stack keyed by `this`, and PreLoad above pushes ObjectDir's entries
    // first and CharClipSet's packRevs LAST -- so CharClipSet's value is on
    // top and has to come off first. Popping after the base call makes
    // ObjectDir::PostLoad consume OUR rev as its own, then read ObjectDir's
    // packRevs as `revs2` and the inlined-dir count `i20` as `offset`,
    // tripping MILO_ASSERT_RANGE_EQ(offset, 0, mSubDirs.size()) (Dir.cpp
    // 0x466) on the first real CharClipSet load. Measured: every rb3-milo run
    // aborted in BandCharDesc::Init() -> DirLoader::LoadObjects ->
    // CharClipSet::PostLoad. This ordering is what the TU had before
    // f278d4d7; that commit's actual match lever is the file-scope statics
    // below, which are untouched here.
    int revs = bs.PopRev(this);
    sPostLoadRev = getHmxRev(revs);
    sPostLoadAltRev = getAltRev(revs);
    ObjectDir::PostLoad(bs);
    if (IsProxy())
        return;
    if (sPostLoadRev < 0x11) {
        int x, y;
        bs >> x;
        bs >> y;
    }
    if (sPostLoadRev >= 0xF && sPostLoadRev < 0x11) {
        int x;
        bs >> x;
    }
    if (sPostLoadRev < 9) {
        FilePath fp;
        bs >> fp;
        if (!fp.empty())
            MILO_NOTIFY(
                "Set the type and resave %s, graph_path was \"%s\"",
                PathName(this),
                fp.c_str()
            );
    }
    if (sPostLoadRev < 6) {
        String str;
        bs >> str;
        MILO_NOTIFY("You'll need to reexport some clips into this clipset");
    }
    if (sPostLoadRev < 7) {
        int x;
        bs >> x;
    }
    if (sPostLoadRev < 0x18) {
        int count = 0;
        for (ObjDirItr<CharClip> it(this, true); it != 0; ++it) {
            count++;
        }
        for (int i = 0; i < count; i++) {
            ObjPtr<CharClip> clipPtr(this, nullptr);
            bs >> clipPtr;
            int x, y;
            bs >> x;
            bs >> y;
        }
    }
    if (sPostLoadRev > 0xD) {
        if (sPostLoadRev < 0x18) {
            bool b1, b2;
            bs >> b1;
            if (sPostLoadRev > 0x12)
                bs >> b2;
        }
    } else {
        int count;
        bs >> count;
        for (int i = 0; i < count; i++) {
            Symbol s;
            bs >> s;
        }
    }
    if (sPostLoadRev > 4 && sPostLoadRev < 0x18) {
        int count;
        bs >> count;
        char buf[0x100];
        for (int i = 0; i < count; i++) {
            bs.ReadString(buf, 0x100);
        }
        bs >> count;
        for (int i = 0; i < count; i++) {
            bs.ReadString(buf, 0x100);
        }
        bool b;
        bs >> b;
    }
    if (sPostLoadRev > 9 && sPostLoadRev < 24) {
        Symbol s;
        bs >> s;
        int x;
        bs >> x;
    }
    if (sPostLoadRev == 0xB) {
        bool b;
        bs >> b;
    }
    if (sPostLoadRev < 0xC && !Type().Null())
        MILO_NOTIFY(
            "%s may have a bug in the transition graph, need to resave from milo",
            PathName(this)
        );
    if (sPostLoadRev < 0xD) {
        static Message filter_clips_msg("filter_clips");
        Handle(filter_clips_msg, false);
    }
    if (sPostLoadRev > 0x11) {
        bs >> mCharFilePath;
        bs >> mPreviewClip;
    }
    if (sPostLoadRev > 0x13)
        bs >> mFilterFlags;
    if (sPostLoadRev > 0x14)
        bs >> mBpm;
    if (sPostLoadRev > 0x15)
        bs >> mPreviewWalk;
    if (sPostLoadRev > 0x16)
        bs >> mStillClip;
}

void CharClipSet::SetFrame(float frame, float blend) {
    if (mPreviewClip && mPreviewChar) {
        RndAnimatable::SetFrame(frame, 1);
        CharBonesMeshes mesh1;
        CharBonesMeshes mesh2;
        mesh1.SetName("preview_anim", mPreviewChar);
        mPreviewClip->StuffBones(mesh1);
        mesh2.SetName("preview", this);
        mPreviewClip->StuffBones(mesh2);
        mesh2.Zero();
        mesh1.Zero();
        CharClip *relative = mPreviewClip->Relative();
        if (relative) {
            CharClip *theClip = mStillClip ? mStillClip : mPreviewClip;
            theClip->ScaleAdd(mesh1, 1, frame, 0);
            mPreviewClip->RotateTo(mesh1, 1, frame);
            theClip->ScaleAdd(mesh2, 1, frame, 0);
            mPreviewClip->RotateTo(mesh2, 1, frame);
        } else {
            mPreviewClip->ScaleAdd(mesh1, 1, frame, 0);
            mPreviewClip->ScaleAdd(mesh2, 1, frame, 0);
        }
        mesh1.PoseMeshes();
        mesh2.PoseMeshes();
        if (mPreviewWalk) {
            RndTransformable *pelvisTrans =
                CharUtlFindBoneTrans("bone_pelvis", mesh1.Dir());
            float *rotZPtr = (float *)mesh1.FindPtr("bone_facing.rotz");
            Vector3 *posPtr = (Vector3 *)mesh1.FindPtr("bone_facing.pos");
            if (pelvisTrans && posPtr && rotZPtr) {
                Transform &pelvisXfm = pelvisTrans->DirtyLocalXfm();
                if (rotZPtr) {
                    RotateAboutZ(pelvisXfm.m, *rotZPtr, pelvisXfm.m);
                    RotateAboutZ(pelvisXfm.v, *rotZPtr, pelvisXfm.v);
                    Normalize(pelvisXfm.m, pelvisXfm.m);
                }
                pelvisXfm.v += *posPtr;
            }
            auto _tmp1 = mPreviewClip->GetResource();
            for (ObjDirItr<CharBone> it(_tmp1, false);
                 it != nullptr;
                 ++it) {
                if (it->BakeOutAsTopLevel()) {
                    String str(it->Name());
                    if (str.find(".cb") != String::npos) {
                        str = str.substr(0, str.length() - 3);
                    }
                    RndTransformable *t = CharUtlFindBoneTrans(str.c_str(), mesh1.Dir());
                    if (t && posPtr && rotZPtr) {
                        Transform &xfm = t->DirtyLocalXfm();
                        if (rotZPtr) {
                            RotateAboutZ(xfm.m, *rotZPtr, xfm.m);
                            RotateAboutZ(xfm.v, *rotZPtr, xfm.v);
                            Normalize(xfm.m, xfm.m);
                        }
                        xfm.v += *posPtr;
                    }
                }
            }
        }

        for (ObjDirItr<CharPollable> it(mPreviewChar, true); it != nullptr; ++it) {
            if (dynamic_cast<CharForeTwist *>(&*it)
                || dynamic_cast<CharUpperTwist *>(&*it)
                || dynamic_cast<CharNeckTwist *>(&*it)) {
                it->Poll();
            }
        }
    }
}

float CharClipSet::StartFrame() {
    if (mPreviewClip)
        return mPreviewClip->StartBeat();
    else
        return 0;
}

float CharClipSet::EndFrame() {
    if (mPreviewClip)
        return mPreviewClip->EndBeat();
    else
        return 0;
}

void CharClipSet::Draw() {
    if (mPreviewChar) {
        mPreviewChar->DrawShowing();
    }
}

void CharClipSet::DrawShowing() {
    if (mPreviewChar) {
        mPreviewChar->DrawShowing();
    }
}

void CharClipSet::ListDrawChildren(std::list<RndDrawable *> &draws) {
    if (mPreviewChar) {
        RndDir *ptr = mPreviewChar;
        draws.insert(draws.end(), ptr);
    }
}

void CharClipSet::ResetEditorState() {
    ResetPreviewState();
    ObjectDir::ResetEditorState();
}

void CharClipSet::SetBpm(int bpm) {
    static Symbol sBpm("bpm");
    mBpm = bpm;
}

void CharClipSet::ResetPreviewState() {
    delete mPreviewChar;
    mPreviewClip = 0;
    mStillClip = 0;
    mCharFilePath.SetRoot("");
    mFilterFlags = 0;
    mBpm = 90;
    mPreviewWalk = false;
}

void CharClipSet::RandomizeGroups() {
    for (ObjDirItr<CharClipGroup> it(this, false); it != nullptr; ++it) {
        it->Randomize();
    }
}

void CharClipSet::SortGroups() {
    for (ObjDirItr<CharClipGroup> it(this, false); it != nullptr; ++it) {
        it->Sort();
    }
}

void CharClipSet::LoadCharacter() {
    MILO_ASSERT(TheLoadMgr.EditMode(), 0x14b);
    if (Dir() == this) {
        delete mPreviewChar;
        ObjectDir *loadedDir =
            dynamic_cast<RndDir *>(DirLoader::LoadObjects(mCharFilePath, 0, 0));
        mPreviewChar = dynamic_cast<RndDir *>(loadedDir);
        Character *theChar = dynamic_cast<Character *>(loadedDir);
        if (mPreviewChar && !theChar) {
            for (ObjDirItr<Character> it(mPreviewChar, true); it != nullptr; ++it) {
                mPreviewChar = it;
                break;
            }
        }
        if (mPreviewChar) {
            mPreviewChar->Enter();
            mPreviewChar->SetName("preview_character", this);
        }
    } else {
        MILO_NOTIFY(
            "Preview character can only be loaded if the CharClipSet is the top-level directory."
        );
    }
}

void CharClipSet::RecenterAll() { MILO_NOTIFY("You can only recenter clips from PC"); }

DataNode CharClipSet::OnListClips(DataArray *) {
    std::list<CharClip *> clips;
    for (ObjDirItr<CharClip> it(this, true); it != nullptr; ++it) {
        if ((mFilterFlags & it->Flags()) == mFilterFlags) {
            clips.push_back(it);
        }
    }
    clips.sort(ObjNameSort());
    DataArray *arr = new DataArray(clips.size() + 1);
    arr->Node(0) = NULL_OBJ;
    int idx = 1;
    for (std::list<CharClip *>::iterator it = clips.begin(); it != clips.end(); ++it) {
        arr->Node(idx++) = *it;
    }
    DataNode ret(arr, kDataArray);
    arr->Release();
    return ret;
}
