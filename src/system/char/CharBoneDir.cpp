#include "char/CharBoneDir.h"
#include "char/CharBone.h"
#include "utl/SongInfoCopy.h"
#include "char/CharUtl.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"

const std::vector<TrackChannels> &SongInfoCopy::GetTracks() const { return mTrackChannels; }

ObjectDir *sResources;
DataArray *CharBoneDir::sCharClipTypes;

CharBoneDir::CharBoneDir()
    : mRecenter(this), mMoveContext(0), mBakeOutFacing(true), mFilterContext(0),
      mFilterBones(this) {}

CharBoneDir::~CharBoneDir() {}

BEGIN_HANDLERS(CharBoneDir)
    HANDLE_EXPR(get_context_flags, GetContextFlags())
    HANDLE_SUPERCLASS(ObjectDir)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(CharBoneDir::Recenter)
    SYNC_PROP(targets, o.mTargets)
    SYNC_PROP(average, o.mAverage)
    SYNC_PROP(slide, o.mSlide)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(CharBoneDir)
    SYNC_PROP(recenter, mRecenter)
    SYNC_PROP_SET(merge_character, "", MergeCharacter(FilePath(_val.Str())))
    SYNC_PROP(move_context, mMoveContext)
    SYNC_PROP(bake_out_facing, mBakeOutFacing)
    SYNC_PROP_MODIFY(filter_context, mFilterContext, SyncFilter())
    SYNC_PROP(filter_bones, mFilterBones)
    SYNC_PROP(filter_names, mFilterNames)
    SYNC_SUPERCLASS(ObjectDir)
END_PROPSYNCS

BinStream &operator<<(BinStream &bs, CharBoneDir::Recenter &r) {
    bs << r.mTargets;
    bs << r.mAverage;
    bs << r.mSlide;
    return bs;
}

BEGIN_SAVES(CharBoneDir)
    SAVE_REVS(4, 0)
    SAVE_SUPERCLASS(ObjectDir)
    bs << mMoveContext;
    bs << mRecenter;
    bs << mBakeOutFacing;
END_SAVES

BEGIN_COPYS(CharBoneDir)
    COPY_SUPERCLASS(ObjectDir)
    CREATE_COPY(CharBoneDir)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMoveContext)
        COPY_MEMBER(mRecenter)
        COPY_MEMBER(mBakeOutFacing)
    END_COPYING_MEMBERS
END_COPYS

BinStream &operator>>(BinStream &bs, CharBoneDir::Recenter &r) {
    bs >> r.mTargets;
    bs >> r.mAverage;
    bs >> r.mSlide;
    return bs;
}

INIT_REVS(4, 0)

void CharBoneDir::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    ObjectDir::PreLoad(bs);
    d.PushRev(this);
}

void CharBoneDir::PostLoad(BinStream &bs) {
    BinStreamRev d(bs, bs.PopRev(this));
    ObjectDir::PostLoad(bs);
    if (d.rev < 2) {
        bool b;
        d >> b;
    } else {
        d >> mMoveContext;
    }
    if (d.rev < 3) {
        bool b;
        d >> b;
    }
    d >> mRecenter;
    if (d.rev > 3) {
        d >> mBakeOutFacing;
    }
}

void CharBoneDir::ListBones(std::list<CharBones::Bone> &bones, int mask, bool b3) {
    if (mMoveContext & mask) {
        bones.push_back(CharBones::Bone("bone_facing.pos", 1.0f));
        bones.push_back(CharBones::Bone("bone_facing.rotz", 1.0f));
        if (b3) {
            bones.push_back(CharBones::Bone("bone_facing_delta.pos", 1.0f));
            bones.push_back(CharBones::Bone("bone_facing_delta.rotz", 1.0f));
        }
    }
    for (ObjDirItr<CharBone> it(this, false); it != 0; ++it) {
        it->StuffBones(bones, mask);
    }
}

DataNode GetClipTypes(DataArray *a) { return CharBoneDir::GetClipTypes(); }

void CharBoneDir::Init() {
    FilePathTracker tracker(FileRoot());
    sResources = ObjectDir::Main()->New<ObjectDir>("char_resources");
    DataArray *cfg = SystemConfig("objects", "CharBoneDir");
    const char *path = "";
    cfg->FindData("resource_path", path, false);
    sCharClipTypes = SystemConfig("objects", "CharClip", "types");
    if (sCharClipTypes && *path != '\0') {
        for (int i = 1; i < sCharClipTypes->Size(); i++) {
            DataArray *foundarr = sCharClipTypes->Array(i)->FindArray("resource", false);
            if (foundarr) {
                Symbol foundsym = foundarr->Sym(1);
                ObjectDir *thedir = sResources->Find<ObjectDir>(foundsym.Str(), false);
                if (!thedir) {
                    const char *milostr = MakeString("%s/%s.milo", path, foundsym);
                    static int _x = MemFindHeap("char");
                    MemHeapTracker tmp(_x);
                    ObjectDir *loadedDir =
                        DirLoader::LoadObjects(milostr, nullptr, nullptr);
                    if (loadedDir) {
                        loadedDir->SetName(foundsym.Str(), sResources);
                    }
                }
            }
        }
    }
    DataRegisterFunc("get_clip_types", ::GetClipTypes);
}

DataNode CharBoneDir::GetClipTypes() {
    DataArray *arr = new DataArray(sCharClipTypes->Size());
    arr->Node(0) = Symbol();
    for (int i = 1; i < sCharClipTypes->Size(); i++) {
        DataArray *currArr = sCharClipTypes->Array(i);
        arr->Node(i) = currArr->Sym(0);
    }
    arr->SortNodes();
    DataNode ret(arr);
    arr->Release();
    return ret;
}

void CharBoneDir::Terminate() { delete sResources; }

DataNode CharBoneDir::GetContextFlags() {
    if (mContextFlags.Type() == kDataInt) {
        DataArray *cfg = SystemConfig("objects", "CharClip", "types");
        DataArray *arr = new DataArray(cfg->Size() - 1);
        int count = 0;
        Symbol name(Name());
        for (int i = 1; i < cfg->Size(); i++) {
            DataArray *resourceArr = cfg->Array(i)->FindArray("resource", false);
            if (resourceArr && resourceArr->Sym(1) == name) {
                const char *str = resourceArr->Str(2);
                int j;
                for (j = 0; j < count; j++) {
                    if (streq(str, arr->Str(j)))
                        break;
                }
                if (j == count) {
                    arr->Node(count++) = resourceArr->Str(2);
                }
            }
        }
        arr->Resize(count);
        arr->SortNodes();
        mContextFlags = arr;
        arr->Release();
    }
    return mContextFlags;
}

bool SyncSort(CharBone *bone1, CharBone *bone2) {
    return strcmp(bone1->Name(), bone2->Name()) < 0;
}

CharBoneDir *CharBoneDir::FindBoneDirResource(const char *name) {
    return sResources->Find<CharBoneDir>(name, false);
}

CharBoneDir *CharBoneDir::FindResourceFromClipType(Symbol cliptype) {
    DataArray *types = sCharClipTypes->FindArray(cliptype, false);
    if (!types) {
        MILO_NOTIFY("CharClip has no type %s", cliptype);
        return 0;
    } else {
        DataArray *resources = types->FindArray("resource", false);
        if (!resources) {
            MILO_NOTIFY("CharClip %s has no (resource ...) field", cliptype);
            return 0;
        } else {
            CharBoneDir *dir = FindBoneDirResource(resources->Str(1));
            if (!dir)
                MILO_NOTIFY("CharClip %s has no resource", cliptype);
            return dir;
        }
    }
}

void CharBoneDir::StuffBones(CharBones &bones, int i) {
    std::list<CharBones::Bone> blist;
    ListBones(blist, i, true);
    bones.AddBones(blist);
}

void CharBoneDir::StuffBones(CharBones &bones, Symbol sym) {
    DataArray *found = sCharClipTypes->FindArray(sym, false);
    if (!found) {
        MILO_NOTIFY("CharClip has no type %s", sym);
        return;
    }
    DataArray *resource = found->FindArray("resource", false);
    if (!resource) {
        MILO_NOTIFY("CharClip %s has no (resource ...) field", sym);
        return;
    }
    CharBoneDir *dir = FindBoneDirResource(resource->Str(1));
    if (!dir) {
        MILO_NOTIFY("CharClip %s has no resource", sym);
        return;
    }
    dir->StuffBones(bones, DataGetMacro(resource->Str(2))->Int(0));
}

void CharBoneDir::SyncFilter() {
    mFilterBones.clear();
    for (ObjDirItr<CharBone> it(this, true); it != nullptr; ++it) {
        if (mFilterContext & it->PositionContext() || mFilterContext & it->ScaleContext()
            || (it->RotationType() != CharBones::TYPE_END
                && mFilterContext & it->RotationContext())) {
            mFilterBones.push_back(it);
        }
    }
    mFilterBones.sort(SyncSort);
    mFilterNames.clear();
    std::list<CharBones::Bone> bones;
    ListBones(bones, mFilterContext, true);
    FOREACH (it, bones) {
        mFilterNames.push_back(it->name);
    }
    mFilterNames.sort();
    FOREACH (it, mFilterNames) {
        // Retail's stripped log evaluates *it into a BY-VALUE String parameter.
        // That distinction is load-bearing: a by-value param is caller-
        // constructed, so MSVC destroys it through the copy-ctor's `this`
        // return already live in r3 (49 such sites in retail).  The comma-form
        // MILO_LOG -- ((void)("%s\n", String(*it))) -- instead makes the copy a
        // discarded-value temporary, and MSVC re-materializes `addi r3,r31,0x78`
        // before the dtor (only 2 such sites in all of retail).  Same ctor, same
        // frame slot, 4 bytes apart.  MiloStripEval's by-value params reproduce
        // the retail form; MILO_LOG itself must stay comma-form globally.
        //
        // MiloStripEval is declared ONLY `#ifndef HX_NATIVE` (os/Debug.h:89) --
        // it is a retail-codegen device, not a logger. Same guard X1 applied at
        // utl/ChunkStream.cpp:169 and os/Archive.cpp:266 for the identical
        // pattern; natively the site is simply dropped.
#ifndef HX_NATIVE
        MiloStripEval("%s\n", *it);
#endif
    }
}

void CharBoneDir::MergeCharacter(const FilePath &fp) {
    ObjectDir *dir = DirLoader::LoadObjects(fp.c_str(), 0, 0);
    if (!dir)
        // Retail's residue here copy-constructs a String temp from `fp`
        // (FilePath is class-typed / non-POD) -- this is a COPYING site, not
        // an ORDERING site (only one non-format arg), so it needs
        // MiloStripEval directly rather than the file-wide comma-form
        // MILO_NOTIFY. See os/Debug.h:241-259 for the COPYING/ORDERING split.
        MiloStripEval("Could not load %s", fp);
    else {
        std::list<RndTransformable *> tlist;
        for (ObjDirItr<RndTransformable> it(dir, false); it != nullptr; ++it) {
            if (dir != (Hmx::Object *)it) {
                if (CharUtlIsAnimatable(it)) {
                    if (strneq(it->Name(), "bone_", 5) || strneq(it->Name(), "exo_", 4)) {
                        tlist.push_back(it);
                    }
                }
            }
        }
        std::list<RndTransformable *> tlist60;
        while (!tlist.empty()) {
            RndTransformable *backTrans = tlist.front();
            RndTransformable *charTrans = CharUtlFindBoneTrans(backTrans->Name(), this);
            if (!charTrans) {
                backTrans->SetName(backTrans->Name(), this);
                charTrans = backTrans;
            } else {
                charTrans->Copy(backTrans, Hmx::Object::kCopyDeep);
                // Retail inlines the ref-rewiring here rather than calling
                // Hmx::Object::ReplaceRefs: the loop tail redoes backTrans's
                // *virtual-base* adjustment (lwz vptr / lwz vbaseoff / add)
                // on every iteration, which ReplaceRefs -- where `this` is
                // already the Hmx::Object* -- would never emit.
                //
                // Three details are load-bearing, each worth ~1-5pp:
                //  1. No live iterator. Re-read Refs() each pass (Replace
                //     unlinks the node it acts on, so begin() advances by
                //     itself). Caching `it` + `++it` pins one extra
                //     callee-saved reg and shifts EVERY register in the
                //     function down by one (r27->r26->r25->r24->r23).
                //  2. empty(), not begin() != end(). Both compile to the
                //     same `next == this` compare, but ObjRef::iterator is
                //     non-POD, so each begin()/end() materializes a stack
                //     temp -- the != form grew the frame 0x200 -> 0x210 and
                //     shifted every stack offset in the function.
                //  3. static_cast<Hmx::Object*> before the ObjRef* cast.
                //     Replace's `from` is the vbase-adjusted object pointer
                //     (addi r4,r10,0x4); reinterpret_cast'ing backTrans
                //     directly passes the unadjusted RndTransformable*.
                while (!backTrans->Refs().empty()) {
                    RefPtrOf(backTrans->Refs().begin())
                        ->Replace(
                            reinterpret_cast<ObjRef *>(
                                static_cast<Hmx::Object *>(backTrans)
                            ),
                            charTrans
                        );
                }
            }
            tlist60.push_back(charTrans);
            char buf[256];
            strcpy(buf, MakeString("%s.cb", FileGetBase(charTrans->Name())));
            CharBone *bone = CharUtlFindBone(buf, this);
            if (!bone)
                bone = New<CharBone>(buf);
            bone->SetTrans(charTrans);
            tlist.pop_front();
        }

        while (!tlist60.empty()) {
            RndTransformable *parent = tlist60.front()->TransParent();
            if (parent) {
                if (strneq(parent->Name(), "bone_", 5)
                    || strneq(parent->Name(), "exo_", 4)) {
                    if (parent->Dir() != this) {
                        parent->SetName(parent->Name(), this);
                        parent->SetTransParent(nullptr, false);
                    }
                }
            }
            tlist60.pop_front();
        }

        delete dir;
    }
}


// COMDAT-scatter owner-TU includes (sw scatter-scan): retail linker
// interleaved these owners' COMDATs into this TU's .text span.
// ⚠ NATIVE: guarded so char/CharBoneTwist.cpp is compiled STANDALONE.
// cmake/ScatterIncludes.cmake classifies this edge as CONDITIONAL (it warns
// about it at configure time) even though the preprocessor nesting depth here
// is 0, so it declines to prune the includee -- and the includee then gets
// emitted twice: once standalone and once from this TU. An explicit
// `#ifndef HX_NATIVE` is the shape that module documents as always correct
// ("the guard makes the edge inert, and this module only prunes for edges that
// are active"). X360 keeps the scatter-include, which is where it earns its
// COMDAT placement.
#ifndef HX_NATIVE
#define gRev gRev_CharBoneTwist
#define gAltRev gAltRev_CharBoneTwist
#include "char/CharBoneTwist.cpp"
#undef gRev
#undef gAltRev
#endif
