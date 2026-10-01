// Retail inlines the owner-only ObjPtr ctor in this TU (SmasherPlateInfo's
// mSmasherPlate(this) expands to three stores, not a `bl ??0ObjPtr`).
#define RB3_OBJPTR_INLINE_OWNER_CTOR
#include "bandobj/GemTrackResourceManager.h"
#include "obj/Msg.h"
#include "bandobj/ArpeggioShape.h"

// ArpeggioShape / ArpeggioShapePool. Retail places these directly before
// GemTrackResourceManager's own functions (0x823557D8-0x82356288), in this order.

void ArpeggioShape::HookupToParentGroup() {
    if (mChordLabel->Showing())
        mParentGroup->AddObjectAtFront(mChordLabel);
    if (mFretNumbersChord->Showing())
        mParentGroup->AddObjectAtFront(mFretNumbersChord);
    if (mChordShapeMesh->Showing())
        mParentGroup->AddObjectAtFront(mChordShapeMesh);
}

void ArpeggioShape::UnhookFromParentGroup() {
    if (mChordLabel->Showing())
        mParentGroup->RemoveObject(mChordLabel);
    if (mFretNumbersChord->Showing())
        mParentGroup->RemoveObject(mFretNumbersChord);
    if (mChordShapeMesh->Showing())
        mParentGroup->RemoveObject(mChordShapeMesh);
}

void ArpeggioShape::SetChordShape(RndMesh *mesh) {
    if (!mesh)
        mesh = mChordShapeMesh;
    mChordShapeMesh->SetGeomOwner(mesh);
    mChordShapeMesh->SetShowing(true);
}

void ArpeggioShape::Reset() {
    UnhookFromParentGroup();
    mChordShapeMesh->SetGeomOwner(mChordShapeMesh);
    mChordShapeMesh->SetShowing(false);
    mFretNumbersChord->SetText("");
    mFretNumbersChord->SetShowing(false);
    mChordLabel->SetText("");
    mChordLabel->SetShowing(false);
    mFadeMatAnim->StopAnimation();
    mFadeMatAnim->SetFrame(0, 1);
}

void ArpeggioShape::FadeOutChordShape() {
    if (mChordShapeMesh->Showing() || mFretNumbersChord->Showing()) {
        static Symbol dest("dest");
        mFadeMatAnim->Animate(0, false, 0, RndAnimatable::k30_fps, 0, 1, 0.1f, 1, dest);
        mFretNumbersChord->SetShowing(false);
    }
}

void ArpeggioShape::ShowChordShape(bool show) {
    mChordShapeMesh->SetShowing(show);
    mFretNumbersChord->SetShowing(show);
}

float ArpeggioShape::GetYPos() const { return unk0->WorldXfm().v.y; }

ArpeggioShape::ArpeggioShape(
    RndGroup *group,
    RndMesh *mesh,
    RndText *fretNumbers,
    RndText *label,
    RndMat *mat,
    RndMatAnim *fade
)
    : unk0(Hmx::Object::New<RndTransformable>()), mParentGroup(group),
      mChordShapeMesh(Hmx::Object::New<RndMesh>()),
      mFretNumbersChord(Hmx::Object::New<RndText>()),
      mChordLabel(Hmx::Object::New<RndText>()), mChordShapeMat(Hmx::Object::New<RndMat>()),
      mFadeMatAnim(Hmx::Object::New<RndMatAnim>()) {
    mChordShapeMesh->Copy(mesh, Hmx::Object::kCopyDeep);
    mFretNumbersChord->Copy(fretNumbers, Hmx::Object::kCopyDeep);
    mChordLabel->Copy(label, Hmx::Object::kCopyDeep);
    mChordShapeMat->Copy(mat, Hmx::Object::kCopyDeep);
    mFadeMatAnim->Copy(fade, Hmx::Object::kCopyDeep);
    mChordShapeMesh->SetTransParent(unk0, false);
    mFretNumbersChord->SetTransParent(unk0, false);
    mChordLabel->SetTransParent(unk0, false);
    mFadeMatAnim->SetMat(mChordShapeMat);
    mChordShapeMesh->SetMat(mChordShapeMat);
}

void ArpeggioShape::SetChordLabel(const String &label, float x, bool leftAligned) {
    mChordLabel->SetText(label.c_str());
    Vector3 pos(mChordLabel->LocalXfm().v);
    pos.x = x;
    mChordLabel->SetLocalPos(pos);
    mChordLabel->SetShowing(label != "");
    if (leftAligned) {
        if (mChordLabel->GetAlignment() != RndText::kBottomLeft)
            mChordLabel->SetAlignment(RndText::kBottomLeft);
    } else {
        if (mChordLabel->GetAlignment() != RndText::kBottomRight)
            mChordLabel->SetAlignment(RndText::kBottomRight);
    }
}

void ArpeggioShape::SetFretNumber(const String &str, const Vector3 &pos) {
    mFretNumbersChord->SetText(str.c_str());
    mFretNumbersChord->SetLocalPos(pos);
    mFretNumbersChord->SetShowing(str != "");
}

void ArpeggioShape::SetYPos(float y) {
    Transform xfm(Hmx::Matrix3(1, 0, 0, 0, 1, 0, 0, 0, 1), Vector3(0, y, 0));
    unk0->SetWorldXfm(xfm);
}

void ArpeggioShapePool::ReleaseArpeggioShape(ArpeggioShape *&shape) {
    shape->Reset();
    mShapes.push_front(shape);
    shape = nullptr;
}

void ArpeggioShapePool::CreateArpeggioShape() {
    ArpeggioShape *shape = new ArpeggioShape(
        mShapesGroup,
        mChordShapeMesh,
        mFretNumbersChord,
        mChordLabel,
        mChordShapeMat,
        mFadeMatAnim
    );
    mShapes.push_front(shape);
    mCurrentPoolSize++;
}

ArpeggioShapePool::ArpeggioShapePool(ObjectDir *dir, RndGroup *group, int size)
    : mChordShapeMesh(dir->Find<RndMesh>("chord_shape.mesh", true)),
      mFretNumbersChord(dir->Find<RndText>("fret_numbers_chord.txt", true)),
      mChordLabel(dir->Find<RndText>("chord_label.txt", true)), mShapesGroup(group),
      mChordShapeMat(dir->Find<RndMat>("chord_shape.mat", true)),
      mFadeMatAnim(dir->Find<RndMatAnim>("fade.mnm", true)), mInitialPoolSize(size),
      mCurrentPoolSize(0) {
    for (int i = 0; i < mInitialPoolSize; i++) {
        CreateArpeggioShape();
    }
}

ArpeggioShape *ArpeggioShapePool::GetArpeggioShape() {
    if (mShapes.empty())
        CreateArpeggioShape();
    ArpeggioShape *shape = mShapes.front();
    mShapes.pop_front();
    return shape;
}


GemTrackResourceManager::GemTrackResourceManager(ObjectDir *dir) : unk1c(this, dir) {
    InitSmasherPlates();
}

GemTrackResourceManager::~GemTrackResourceManager() {}

void GemTrackResourceManager::InitSmasherPlates() {
    SmasherPlateInfo info(this);
    info.mSmasherPlate = unk1c->Find<RndDir>("smasher_plate_guitar", true);
    info.mTrackInst = kInstGuitar;
    unk28.push_back(info);
    info.mSmasherPlate = unk1c->Find<RndDir>("smasher_plate_bass", true);
    info.mTrackInst = kInstBass;
    unk28.push_back(info);
    info.mSmasherPlate = unk1c->Find<RndDir>("smasher_plate_keys", true);
    info.mTrackInst = kInstKeys;
    unk28.push_back(info);
    info.mSmasherPlate = unk1c->Find<RndDir>("smasher_plate_drum", true);
    info.mTrackInst = kInstDrum;
    unk28.push_back(info);
    info.mSmasherPlate = unk1c->Find<RndDir>("smasher_plate_real_guitar", true);
    info.mTrackInst = kInstRealGuitar;
    unk28.push_back(info);
    info.mSmasherPlate = unk1c->Find<RndDir>("smasher_plate_real_bass", true);
    info.mTrackInst = kInstRealBass;
    unk28.push_back(info);
    info.mSmasherPlate = unk1c->Find<RndDir>("smasher_plate_real_keys", true);
    info.mTrackInst = kInstRealKeys;
    unk28.push_back(info);
    for (int i = 0; i < unk28.size(); i++) {
        static Message setup_draworder("setup_draworder", 0);
        unk28[i].mSmasherPlate->HandleType(setup_draworder);
    }
}

RndDir *GemTrackResourceManager::GetFreeSmasherPlate(TrackInstrument inst) {
    if (inst == kInstPending)
        return 0;
    else {
        for (int i = 0; i < unk28.size(); i++) {
            SmasherPlateInfo &curinfo = unk28[i];
            if (curinfo.mTrackInst == inst && !curinfo.mInUse) {
                curinfo.mInUse = true;
                return curinfo.mSmasherPlate;
            }
        }
        MILO_WARN("Could not find free smasher plate for instrument %d", inst);
    }
    return 0;
}

void GemTrackResourceManager::ReleaseSmasherPlate(RndDir *plate) {
    for (int i = 0; i < unk28.size(); i++) {
        SmasherPlateInfo &info = unk28[i];
        if (info.mSmasherPlate == plate) {
            MILO_ASSERT(info.mInUse, 0x60);
            info.mInUse = false;
            info.mSmasherPlate = plate;
            return;
        }
    }
    MILO_WARN("Tried to release invalid smasher 0x%08x", plate);
}