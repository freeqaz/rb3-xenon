#include "world/Instance.h"
#include "math/Rot.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/DirLoader.h"
#include "obj/PropSync_p.h"
#include "obj/Utl.h"
#include "rndobj/Dir.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include "rndobj/Utl.h"
#include "utl/MemMgr.h"

template <>
bool PropSync(
    ObjDirPtr<WorldInstance> &ptr, DataNode &node, DataArray *prop, int i, PropOp op
) {
    if (op == kPropGet) {
        DataNode tmp(ptr.GetFile());
        node = tmp;
    } else {
        const char *str = node.Str(NULL);
        FilePath fp(str);
        ptr.LoadFile(fp, false, true, kLoadFront, false);
    }
    return true;
}

#pragma region WorldInstance

WorldInstance::WorldInstance() : mSharedGroup(0), mSharedGroup2(0) {}

WorldInstance::~WorldInstance() {
    if (mSharedGroup2)
        mSharedGroup2->ClearPollMaster();
    delete mSharedGroup2;
}

BEGIN_HANDLERS(WorldInstance)
    HANDLE_SUPERCLASS(RndDir)
END_HANDLERS

BEGIN_PROPSYNCS(WorldInstance)
    SYNC_PROP_MODIFY(instance_file, mDir, SyncDir())
    SYNC_PROP_SET(shared_group, mSharedGroup ? mSharedGroup->Group() : NULL, )
    SYNC_PROP_SET(poll_master, mSharedGroup ? (mSharedGroup->PollMaster() == this) : 0, )
    SYNC_SUPERCLASS(RndDir)
END_PROPSYNCS

BEGIN_SAVES(WorldInstance)
    SAVE_REVS(3, 0)
    bs << mDir.GetFile();
    SaveInlined(mDir.GetFile(), true, kInlineCachedShared);
    SAVE_SUPERCLASS(RndDir)
    SavePersistentObjects(bs);
END_SAVES

BEGIN_COPYS(WorldInstance)
    COPY_SUPERCLASS(RndDir)
END_COPYS

void WorldInstance::SavePersistentObjects(BinStream &bs) {
    if (!IsProxy())
        return;
    int hashUsed = HashTableUsedSize();
    int strUsed = StrTableUsedSize();
    DeleteTransientObjects();
    for (ObjDirItr<Hmx::Object> it(this, false); it != nullptr; ++it) {
        if (it != this) {
            MILO_ASSERT(dynamic_cast<ObjectDir *>((Hmx::Object *)it) == NULL, 0x12F);
            it->PreSave(bs);
        }
    }
    bs.WriteEndian(&hashUsed, 4);
    bs.WriteEndian(&strUsed, 4);
    std::list<Hmx::Object *> objects;
    for (ObjDirItr<Hmx::Object> it(this, false); it != nullptr; ++it) {
        if (it != this) {
            objects.push_back(it);
        }
    }
    DirLoader::ClassAndNameSort sorter;
    objects.sort(sorter);
    int count = objects.size();
    bs.WriteEndian(&count, 4);
    for (std::list<Hmx::Object *>::iterator it = objects.begin(); it != objects.end(); ++it) {
        bs << (*it)->ClassName();
        bs << (*it)->Name();
    }
    for (std::list<Hmx::Object *>::iterator it = objects.begin(); it != objects.end(); ++it) {
        (*it)->Save(bs);
    }
    if (!bs.Cached()) {
        for (std::list<Hmx::Object *>::iterator it = objects.begin(); it != objects.end(); ++it) {
            (*it)->PostSave(bs);
        }
    }
}

void WorldInstance::PostSave(BinStream &bs) { SyncDir(); }

void WorldInstance::PreSave(BinStream &) {}

void WorldInstance::DrawShowing() {
    RndDir::DrawShowing();
    if (mSharedGroup) {
        mSharedGroup->Draw(WorldXfm());
    }
}

RndDrawable *WorldInstance::CollideShowing(const Segment &s, float &f, Plane &pl) {
    if (RndDir::CollideShowing(s, f, pl))
        return this;
    if (mSharedGroup) {
        if (mSharedGroup->Collide(WorldXfm(), s, f, pl)) {
            return this;
        }
    }
    return 0;
}

void WorldInstance::Poll() {
    if (mSharedGroup)
        mSharedGroup->TryPoll(this);
    RndDir::Poll();
}

void WorldInstance::Enter() {
    if (mSharedGroup)
        mSharedGroup->TryEnter(this);
    RndDir::Enter();
}

float WorldInstance::GetDistanceToPlane(const Plane &pl, Vector3 &v) {
    float dist = RndDir::GetDistanceToPlane(pl, v);
    if (mSharedGroup) {
        Vector3 v28;
        float grpdist = mSharedGroup->DistanceToPlane(WorldXfm(), pl, v28);
        if (dist > grpdist) {
            v = v28;
            dist = grpdist;
        }
    }
    return dist;
}

bool WorldInstance::MakeWorldSphere(Sphere &s, bool b) {
    if (b) {
        RndDir::MakeWorldSphere(s, true);
        if (mSharedGroup) {
            Sphere s28;
            mSharedGroup->MakeWorldSphere(WorldXfm(), s28);
            s.GrowToContain(s28);
        }
        return true;
    } else {
        if (mSphere.GetRadius()) {
            Multiply(mSphere, WorldXfm(), s);
            return true;
        } else
            return false;
    }
}

INIT_REVS(3, 0)

void WorldInstance::PreLoad(BinStream &bs) {
    if (IsProxy())
        DeleteObjects();
    LOAD_REVS(bs);
    ASSERT_REVS(3, 0);
    if (d.rev > 0) {
        FilePath fp;
        bs >> fp;
        PreLoadInlined(fp, true, kInlineCachedShared);
    } else
        bs >> mDir;

    // ⚠ ORDER DIVERGENCE FROM rb3-Wii, OBSERVED AND DELIBERATELY NOT "FIXED".
    //
    // rb3-Wii's faithful RB3 decomp (rb3/src/system/world/Instance.cpp) pushes
    // the rev BEFORE RndDir::PreLoad; this body (byte-identical to DC3's) pushes
    // it after. The mirrored transposition exists in PostLoad below.
    //
    // X4a tried swapping both to rb3-Wii's order on the theory that it was the
    // venue-load stream corruptor. THAT THEORY IS REFUTED: BinStream::PushRev /
    // PopRev (utl/BinStream.cpp:284, :144) only push/pop a process-wide
    // `sRevStack` — they never touch the byte stream — and BOTH orderings are
    // internally consistent LIFO (xenon pushes last and pops first; rb3-Wii
    // pushes first and pops last). The swap was built and run: the venue failure
    // reproduced with a byte-identical 3494-line log and the identical
    // `version 41` / `String chars 774778671` numbers. Zero runtime effect.
    //
    // So this is left alone. It remains an open MATCH question — PreLoad is
    // 76.92% and PostLoad 59.07% in default/Instance, and rb3-Wii is the better
    // oracle than DC3 for RB3 game-era code — but it is a match lane's call,
    // backed by an A/B, not something to change on a refuted runtime theory.
    RndDir::PreLoad(bs);
    if (mProxyFile.length() != 0) {
        MILO_NOTIFY(
            "WorldInstance %s was created as RndDir. Object needs to be deleted and recreated.",
            Name()
        );
    }
    bs.PushRev(packRevs(d.altRev, d.rev), this);
}

void WorldInstance::LoadPersistentObjects(BinStreamRev &bs) {
    if (IsProxy()) {
        if (bs.rev > 2) {
            // allocate more hashtable and stringtable space
            int hashSize, stringSize;
            bs >> hashSize;
            bs >> stringSize;
            hashSize *= 2;
            Reserve(hashSize, stringSize);
        }
        // create the persistent objects using their ClassName and Name
        // then push them into our persistent object list
        std::list<Hmx::Object *> objlist;
        int count;
        bs >> count;
        while (count-- != 0) {
            Symbol objClassName;
            bs >> objClassName;
            char objName[0x80];
            bs.stream.ReadString(objName, 0x80);

            if (!Hmx::Object::RegisteredFactory(objClassName)) {
#ifdef HX_NATIVE
                // X4c: see the twin at obj/DirLoader.cpp. This is the PERSISTENT
                // path and it is the unrecoverable one -- it DeleteObjects() and
                // returns on the first miss, with no stream marker to re-sync on,
                // so anything after it in the stream is lost. Note it therefore
                // emits AT MOST ONE message per WorldInstance load.
                MILO_NOTIFY(
                    "[persistent] %s: Can't make %s", mStoredFile.c_str(), objClassName
                );
#else
                MILO_NOTIFY("%s: Can't make %s", mStoredFile.c_str(), objClassName);
#endif
                DeleteObjects();
                return;
            }

            Hmx::Object *obj = Hmx::Object::NewObject(objClassName);
            obj->SetName(objName, this);
            objlist.push_back(obj);
        }

        String dirNameStr;
        ObjectDir *dirDir = nullptr;
        DataArray *dirTypeDef = nullptr;
        ObjDirPtr<ObjectDir> subDir;
        if (mDir) {
            dirNameStr = mDir->Name();
            dirDir = mDir->Dir();
            dirTypeDef = (DataArray *)mDir->TypeDef();
            subDir = mDir;
            AppendSubDir(subDir);
        }
        while (!objlist.empty()) {
            Hmx::Object *cur = objlist.front();
            cur->PreLoad(bs.stream);
            cur->PostLoad(bs.stream);
            objlist.pop_front();
        }
        if (mDir) {
            RemoveSubDir(subDir);
            mDir->SetName(dirNameStr.c_str(), dirDir);
            mDir->SetTypeDef(dirTypeDef);
        }
    }
}

void WorldInstance::DeleteTransientObjects() {
    if (!(!Dir() || Dir() == DirLoader::TopSaveDir()
        || Dir()->InlineSubDirType() != kInlineAlways)) {
        for (ObjDirItr<Hmx::Object> obj(this, false); obj != nullptr; ++obj) {
            if (this != obj) {
#ifdef HX_NATIVE
                // ⛔ X4a: `auto refs = obj->Refs();` IS AN UNTERMINATED WALK, and
                //    it hangs the first venue load 100% of the time.
                //
                // Hmx::Object::Refs() returns `const ObjRef &mRefs` — the LIVE
                // RING HEAD (obj/Object.h:1973). `auto` deduces ObjRef BY VALUE,
                // so `refs` is a COPY of the head node. ObjRef::end() is
                // `iterator((ObjRef *)this)` (:206), i.e. the address of the copy
                // — and no node in the real ring ever points at that stack
                // address. The walk runs …→ last → &obj->mRefs → first → … and
                // `it != refs.end()` is NEVER true.
                //
                // ★ It does not even need a non-empty ring to hang. On an EMPTY
                // ring obj->mRefs.next == &obj->mRefs, so begin() yields the real
                // head, end() yields &refs, they differ, and ++it lands back on
                // the head forever. And because the head is a plain ObjRef whose
                // RefOwner() is null (:157), the `if` never fires — so it spins
                // SILENTLY at 100% CPU with no output and no crash. MEASURED:
                // rb3-render on world/venue/small_club/small_club_01 sat at
                // 11m39s wall / 11m35s CPU inside
                // DeleteTransientObjects → __dynamic_cast, stack captured under
                // gdb; the venue is the first asset in the ladder that loads a
                // WorldInstance proxy, so X2 and X3 could not reach this.
                //
                // ★ THIS IS A TRANSCRIPTION DEFECT, NOT A DESIGN. Three
                // independent witnesses:
                //   1. rb3-Wii's faithful decomp (rb3/src/system/world/Instance.cpp)
                //      writes `std::vector<ObjRef *> refs = obj->Refs();` — its
                //      Refs() returns a VECTOR SNAPSHOT BY VALUE — then iterates
                //      that vector (rbegin/rend). Mutating the ring is safe there
                //      because the vector is detached.
                //   2. The residue proves it: rb3-Wii wraps `MemDoTempAllocations`
                //      around the COPY, because building the vector ALLOCATES.
                //      xenon kept the scope and dropped the allocation it existed
                //      to scope — a loop that allocates nothing.
                //   3. DC3 (dc3-decomp/.../Instance.cpp) splices matching refs onto
                //      a private local ring with MoveBefore and then ReplaceList()s
                //      them in one shot — never walking a ring it is mutating.
                //
                // Fixed with idiom (3), which is ALSO what this very file's
                // WorldInstance::SyncDir already does ~50 lines below under the
                // same #ifdef (`ObjRef refs; refs.Clear(); … MoveBefore(&refs);
                // refs.ReplaceList(p->to);`). So this is not a new mechanism —
                // it is the surrounding, working code, applied to the one site
                // that diverged from it. MoveBefore returns the spliced node's
                // OLD PREDECESSOR, so `it = it->MoveBefore(&refs)` leaves ++it
                // resuming correctly in the source ring.
                //
                // ⚠ The X360 arm below is preserved TOKEN-FOR-TOKEN, statement
                // order included, because default/Instance is 85% fn-matched and
                // this body is scored there. That neutrality is MEASURED, not
                // argued — see docs/plans/x4a-venue-render-2026-08-02.md.
                ObjectDir *dir_ref = Dir();
                Hmx::Object *to = mDir->Find<Hmx::Object>(obj->Name(), true);
                MILO_ASSERT(obj->ClassName() == to->ClassName(), 0x1CB);
                {
                    MemDoTempAllocations m;
                    ObjRef refs;
                    refs.Clear();
                    for (ObjRef::iterator it = obj->Refs().begin();
                         it != obj->Refs().end(); ++it) {
                        if (RefPtrOf(it)->RefOwner()
                            && RefPtrOf(it)->RefOwner()->Dir() == this) {
                            it = it->MoveBefore(&refs);
                        }
                    }
                    refs.ReplaceList(to);
                }
                delete obj;
#else
                auto refs = obj->Refs();
                ObjectDir *dir_ref = Dir();
                Hmx::Object *to = mDir->Find<Hmx::Object>(obj->Name(), true);
                MILO_ASSERT(obj->ClassName() == to->ClassName(), 0x1CB);
                {
                    MemDoTempAllocations m;
                    for (ObjRef::iterator it = refs.begin(); it != refs.end(); ++it) {
                        if (RefPtrOf(it)->RefOwner() && RefPtrOf(it)->RefOwner()->Dir() == this) {
                            // ObjRef::Replace(Hmx::Object*) is an elided stub off
                            // HX_NATIVE; dispatch the real ring Replace (slot +8)
                            // with the outgoing object as `from`.
                            RefPtrOf(it)->Replace(
                                reinterpret_cast<ObjRef *>((Hmx::Object *)obj), to
                            );
                        }
                    }
                }
                delete obj;
#endif
            }
        }
    } else {
        DeleteObjects();
    }
}

void WorldInstance::SetProxyFile(const FilePath &fp, bool override) {
    MILO_ASSERT(!override, 0x246);
    DeleteObjects();
    mDir.LoadFile(fp, false, true, kLoadFront, false);
    SyncDir();
    if (mDir) {
        Hmx::Object::Copy(mDir, kCopyShallow);
    }
}

void WorldInstance::PostLoad(BinStream &bs) {
    // ⚠ See the note in PreLoad above: this is the mirrored half of an ordering
    // divergence from rb3-Wii (which calls RndDir::PostLoad FIRST and pops
    // after). X4a's theory that it corrupted the venue stream is REFUTED —
    // PopRev does not read the stream — and swapping it was measured to change
    // nothing at runtime. Left as-is; open as a match question, not a bug fix.
    int revs = bs.PopRev(this);
    BinStreamRev d(bs, revs);
    RndDir::PostLoad(bs);
    if (d.rev > 0) {
        ObjDirPtr<ObjectDir> dirPtr = PostLoadInlined();
        mDir = dynamic_cast<WorldInstance *>((ObjectDir *)dirPtr);
    } else {
        mDir.PostLoad(0);
    }
    if (d.rev > 1) {
        LoadPersistentObjects(d);
    }
    SyncDir();
}

void WorldInstance::SyncDir() {
    if (IsProxy()) {
        DeleteTransientObjects();
        mSharedGroup = nullptr;
        if (mDir) {
            RndGroup *grp = mDir->Find<RndGroup>("shared.grp", false);
            if (!mDir->mSharedGroup2 && grp) {
                mDir->mSharedGroup2 = new SharedGroup(grp);
            }
            mSharedGroup = mDir->mSharedGroup2;
            Sphere sphere = mDir->mSphere;
            Vector3 v98;
            MakeScale(WorldXfm().m, v98);
            float f21 = Max(v98.y, v98.z);
            f21 = Max(v98.x, f21);
            if (f21 > 1.0f)
                sphere.radius *= f21;
            SetSphere(sphere);
            static Symbol grpSym("Group");
            static Symbol texSym("Tex");
            static Symbol cubeSym("CubeTex");
            static Symbol movieSym("Movie");
            static Symbol synthSym("SynthSample");
            std::list<ObjPair> objPairs;
            objPairs.push_back(ObjPair(mDir, this));
            for (ObjDirItr<Hmx::Object> it(mDir, false); it != nullptr; ++it) {
                bool curMesh = NULL != dynamic_cast<RndMesh *>(&*it);
                if (!grp || (it != grp && !GroupedUnder(grp, it))) {
                iterate:
                    if (it->ClassName() == texSym
                        || it->ClassName() == cubeSym
                        || it->ClassName() == synthSym
                        || it->ClassName() == movieSym)
                        continue;
                    if (it == mDir)
                        continue;
                    EventTrigger *trig = dynamic_cast<EventTrigger *>(&*it);
                    if (trig && trig->HasTriggerEvents()) {
                        MILO_NOTIFY("%s must be in shared.grp", PathName(it));
                    } else {
                        Hmx::Object *foundObj = FindObject(it->Name(), false);
                        if (!foundObj) {
                            foundObj = Hmx::Object::NewObject(it->ClassName());
                            Hmx::Object::CopyType ty = Hmx::Object::kCopyShallow;
                            if (it->ClassName() == grpSym || curMesh)
                                ty = Hmx::Object::kCopyDeep;
                            CopyObject(it, foundObj, ty, true);
                        }
                        objPairs.push_back(ObjPair(it, foundObj));
                    }
                } else if (curMesh) {
                    grp->RemoveObject(it);
                    goto iterate;
                }
            }

            std::list<ObjPair>::const_iterator p = objPairs.begin();
            for (; p != objPairs.end(); ++p) {
                MILO_ASSERT(p->from->Dir(), 0x2CA);
#ifdef HX_NATIVE
                ObjRef refs;
                refs.Clear();
                Hmx::Object *pFrom = p->from;
                for (ObjRef::iterator it = pFrom->Refs().begin(); it != pFrom->Refs().end(); ++it) {
                    if (RefPtrOf(it)->RefOwner() && !RefPtrOf(it)->RefOwner()->Dir()) {
                        it = it->MoveBefore(&refs);
                    }
                }
                refs.ReplaceList(p->to);
#else
                // RB3 retail: Hmx::Object::mRefs is a std::list<ObjRefOwner *>
                // (sentinel {next,prev}@0x20 == STLport _List_node_base; ring
                // entries are 0xc-byte _List_node<ObjRefOwner *>). Retail
                // snapshots it by copy-construction, then dispatches Replace on
                // each entry whose owner lives outside any dir. RefOwner() is
                // deliberately re-called (retail does not cache it).
                std::list<ObjRefOwner *> fromRefs(
                    *reinterpret_cast<const std::list<ObjRefOwner *> *>(&p->from->Refs())
                );
                for (std::list<ObjRefOwner *>::const_iterator it = fromRefs.begin();
                     it != fromRefs.end();
                     ++it) {
                    if ((*it)->RefOwner() && !(*it)->RefOwner()->Dir()) {
                        (*it)->Replace(reinterpret_cast<ObjRef *>(p->from), p->to);
                    }
                }
#endif
            }

            Reserve(mDir->HashTableSize(), mDir->StrTableSize());

            p = objPairs.begin();
            for (; p != objPairs.end(); ++p) {
                if (p->to != this) {
                    p->to->SetName(p->from->Name(), this);
                }
            }

            if (f21 > 1.0f) {
                for (ObjDirItr<RndTransformable> it(this, true); it != nullptr; ++it) {
                    if (GenerationCount(this, it) > 0) {
                        RndDrawable *draw = dynamic_cast<RndDrawable *>(&*it);
                        if (draw) {
                            Sphere s = draw->GetSphere();
                            s.radius *= f21;
                            draw->SetSphere(s);
                        }
                    }
                }
            }
        }
        SyncObjects();
    }
}

#pragma endregion WorldInstance
#pragma region SharedGroup

SharedGroup::SharedGroup(RndGroup *group) : mGroup(group), mPollMaster(this) {
    AddPolls(group);
}

void SharedGroup::ClearPollMaster() { mPollMaster = nullptr; }

void SharedGroup::AddPolls(RndGroup *grp) {
    const ObjPtrList<Hmx::Object> &objs = grp->Objects();
    for (ObjPtrList<Hmx::Object>::iterator it = objs.begin(); it != objs.end(); ++it) {
        RndPollable *poll = dynamic_cast<RndPollable *>(*it);
        if (poll)
            mPolls.push_back(poll);
        else {
            RndGroup *group = dynamic_cast<RndGroup *>(*it);
            if (group)
                AddPolls(group);
        }
    }
}

void SharedGroup::TryPoll(WorldInstance *inst) {
    if (!mPollMaster)
        mPollMaster = inst;
    else if (mPollMaster != inst)
        return;
    FOREACH (it, mPolls) {
        (*it)->Poll();
    }
}

void SharedGroup::TryEnter(WorldInstance *inst) {
    if (!mPollMaster)
        mPollMaster = inst;
    else if (mPollMaster != inst)
        return;
    FOREACH (it, mPolls) {
        (*it)->Enter();
    }

    Hmx::Object *src = dynamic_cast<Hmx::Object *>(mPollMaster->Dir());
    if (src) {
        Hmx::Object *src2 = dynamic_cast<Hmx::Object *>(mGroup->Dir());
        if (src2)
            src2->ChainSource(src, 0);
    }
}

float SharedGroup::DistanceToPlane(const Transform &tf, const Plane &pl, Vector3 &v) {
    mGroup->SetWorldXfm(tf);
    return mGroup->GetDistanceToPlane(pl, v);
}

void SharedGroup::MakeWorldSphere(const Transform &tf, Sphere &s) {
    mGroup->SetWorldXfm(tf);
    mGroup->MakeWorldSphere(s, true);
}

bool SharedGroup::Collide(const Transform &tf, const Segment &s, float &f, Plane &pl) {
    mGroup->SetWorldXfm(tf);
    return mGroup->Collide(s, f, pl);
}

void SharedGroup::Draw(const Transform &tf) {
    mGroup->SetWorldXfm(tf);
    // Retail-360: RndGroup has no Draw() of its own (Draw() is non-virtual here,
    // see rndobj/Draw.h) -- every Draw call site is a direct bl to the single
    // RndDrawable::Draw cull-wrapper body, reached via the +0x10 subobject.
    mGroup->RndDrawable::Draw();
}

#pragma endregion SharedGroup
