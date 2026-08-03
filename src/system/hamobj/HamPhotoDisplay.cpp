#include "hamobj/HamPhotoDisplay.h"
#include "gesture/GestureMgr.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Dir.h"
#include "rndobj/Env.h"
#include "rndobj/Mesh.h"
#include "utl/BinStream.h"

HamPhotoDisplay::HamPhotoDisplay() : mMesh1(this), mMesh2(this), mIndex1(0), mIndex2(0) {}

BEGIN_HANDLERS(HamPhotoDisplay)
    HANDLE_SUPERCLASS(RndDir)
END_HANDLERS

BEGIN_PROPSYNCS(HamPhotoDisplay)
    SYNC_PROP(mesh1, mMesh1)
    SYNC_PROP(mesh2, mMesh2)
    SYNC_PROP_SET(
        index1, mIndex1, {
            int _idx1;
            if (_val.Type() == kDataInt) _idx1 = _val.Int();
            else _idx1 = (int)_val.Float();
            mIndex1 = _idx1;
        }
    )
    SYNC_PROP_SET(
        index2, mIndex2, {
            int _idx2;
            if (_val.Type() == kDataInt) _idx2 = _val.Int();
            else _idx2 = (int)_val.Float();
            mIndex2 = _idx2;
        }
    )
    SYNC_SUPERCLASS(RndDir)
END_PROPSYNCS

// NOTE (lane DI-2/A, deferred): RB3-360 retail's Save DIVERGES from dc3's here.
// Retail's call order is WriteEndian -> operator<< -> RndDir::Save (SAVE_SUPERCLASS
// LAST) and it makes exactly ONE operator<< call, not two.  Reordering alone was
// measured and REGRESSES 83.2 -> 67.9 mpn: with two operator<< calls the reorder
// needs a third callee-save (r29) and an 0x80 frame, so the prologue degrades to
// `bl __savegprlr_29` where retail uses manual std r30/r31 with an 0x70 frame --
// which is itself evidence that retail really does have only one operator<<.
// The two levers are coupled and cannot be landed separately.
// Blocked on a layout question that is NOT local to this TU: retail's Hmx::Object
// virtual base sits at +0x228, ours at +0x200 (+0x28), while RndDir's sub-object is
// at +0x1E0 on BOTH sides.  dc3's own header comments (mMesh1 0x1fc / mMesh2 0x210,
// i.e. ObjPtr == 20 bytes) are dc3's offsets -- our ObjPtr<RndMesh> is 12 bytes --
// so closing this needs a tree-wide ObjPtr/RndDir size decision, not a Save edit.
// Left at dc3's verbatim body (83.2 mpn) on purpose; do not "fix" by dropping a
// member save without first settling which member retail writes.
BEGIN_SAVES(HamPhotoDisplay)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(RndDir)
    if (!IsProxy()) {
        bs << mMesh1 << mMesh2;
    }
END_SAVES

BEGIN_COPYS(HamPhotoDisplay)
    COPY_SUPERCLASS(RndDir)
    CREATE_COPY(HamPhotoDisplay)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMesh1)
        COPY_MEMBER(mMesh2)
    END_COPYING_MEMBERS
END_COPYS

void HamPhotoDisplay::Init() { REGISTER_OBJ_FACTORY(HamPhotoDisplay); }

INIT_REVS(1, 0)

void HamPhotoDisplay::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(1, 0);
    RndDir::PreLoad(d.stream);
    d.PushRev(this);
}

void HamPhotoDisplay::PostLoad(BinStream &bs) {
    BinStreamRev d(bs, bs.PopRev(this));
    RndDir::PostLoad(d.stream);
    if (!IsProxy() || d.rev < 1) {
        d >> mMesh1;
        d >> mMesh2;
    }
}

void HamPhotoDisplay::DrawShowing() {
    if (!mDraws.empty()) {
        RndEnvironTracker tracker(mEnv, &WorldXfm().v);
        FOREACH (it, mDraws) {
            if (*it == mMesh1) {
                DrawPhotoMesh(mMesh1, 0);
            } else if (*it == mMesh2) {
                DrawPhotoMesh(mMesh2, 1);
            } else {
                (*it)->Draw();
            }
        }
    }
}

void HamPhotoDisplay::DrawPhotoMesh(RndMesh *mesh, int i2) {
    if (TheGestureMgr && TheGestureMgr->GetLiveCameraInput()) {
        RndMat *snapshot =
            TheGestureMgr->GetLiveCameraInput()->GetSnapshot(i2 == 0 ? mIndex1 : mIndex2);
        mesh->SetMat(snapshot);
    }
    mesh->Draw();
}
