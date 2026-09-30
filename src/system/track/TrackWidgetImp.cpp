#include "compiler_macros.h"
#include "decomp.h"
#include "obj/ObjMacros.h"
#include "track/TrackWidgetImp.h"
#include "math/Mtx.h"
#include "os/Debug.h"
#include "rndobj/Mesh.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/Text.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "utl/Loader.h"

void ImmediateWidgetImp::DrawInstances(const ObjPtrList<RndMesh> &meshes, int i2) {
    Transform tf68;
    int idx = 0;
    for (ObjPtrList<RndMesh>::iterator it = meshes.begin(); it != meshes.end();
         ++idx, ++it) {
        if (i2 >= 0 && idx >= i2)
            break;
        RndMesh *mesh = *it;
        if (LOADMGR_EDITMODE) {
            tf68 = mesh->LocalXfm();
        }
        std::list<RndMultiMesh::Instance>::iterator instIt;
        if (mesh->HasDynamicConstraint()) {
            for (instIt = mInstances.begin(); instIt != mInstances.end(); ++instIt) {
                mesh->SetLocalXfm(instIt->mXfm);
                mesh->DrawShowing();
            }
        } else if (mesh->NumBones() > 0) {
            RndTransformable *t = mesh->BoneTransAt(0);
            Vector3 va8(0, 0, 0);
            Transform tf98;
            for (instIt = mInstances.begin(); instIt != mInstances.end(); ++instIt) {
                tf98 = instIt->mXfm;
                va8.y = tf98.m.y.y;
                tf98.m.y.y = 1;
                t->SetLocalPos(va8);
                mesh->SetWorldXfm(tf98);
                mesh->DrawShowing();
            }
        } else if (mAllowRotation) {
            mesh->WorldXfm();
            for (instIt = mInstances.begin(); instIt != mInstances.end(); ++instIt) {
                mesh->SetWorldPos(instIt->mXfm.v);
                mesh->DrawShowing();
            }
        } else {
            for (instIt = mInstances.begin(); instIt != mInstances.end(); ++instIt) {
                mesh->SetWorldXfm(instIt->mXfm);
                mesh->DrawShowing();
            }
        }
        if (LOADMGR_EDITMODE) {
            mesh->SetLocalXfm(tf68);
        }
    }
}

// The rb3-Wii oracle accesses `mesh->mInstances` directly throughout this
// class (RndMultiMesh::mInstances is public there). In this tree
// RndMultiMesh::mInstances is `protected` (src/system/rndobj/MultiMesh.h), so
// every such access below goes through the public `Instances()` accessor
// instead -- a trivial one-line inline expected to codegen identically.

MultiMeshWidgetImp::MultiMeshWidgetImp(const ObjPtrList<RndMesh> &meshlist, bool b)
    : mMeshes(meshlist), unk10(b) {
    for (int i = 0; i < mMeshes.size(); i++) {
        mMultiMeshes.push_back(Hmx::Object::New<RndMultiMesh>());
    }
}

MultiMeshWidgetImp::~MultiMeshWidgetImp() {
    for (int i = 0; i < mMultiMeshes.size(); i++)
        delete mMultiMeshes[i];
}

void MultiMeshWidgetImp::Init() {
    int idx = 0;
    FOREACH (it, mMeshes) {
        RndMesh *cur = *it;
        mMultiMeshes[idx]->SetMesh(cur);
        idx++;
    }
}

std::list<RndMultiMesh::Instance> &MultiMeshWidgetImp::Instances() {
    MILO_FAIL("MultiMeshWidgetImp::Instances() called; not implemented");
    // RndMultiMesh::Instances() returns InstanceList, i.e.
    // std::list<Instance, TransformListAlloc<Instance>> (src/system/rndobj/
    // MultiMesh.h) -- a distinct type from this override's declared
    // std::list<Instance> (default allocator), because TransformListAlloc
    // routes allocate()/deallocate() through the global gTransListAlloc pool
    // instead of the default node allocator. That divergence doesn't exist on
    // rb3-Wii, where RndMultiMesh::mInstances is a plain default-allocator
    // list, so the oracle returns it with no friction at all. This path is
    // explicitly marked unimplemented above (and MILO_FAIL is a no-op in this
    // retail build), so the reference is never expected to be dereferenced for
    // a real allocate/deallocate; the cast below only exists to satisfy the
    // declared return type on this known-dead path, never as a general bridge
    // between the two list specializations.
    return reinterpret_cast<std::list<RndMultiMesh::Instance> &>(mMultiMeshes.front()->Instances()
    );
}

void MultiMeshWidgetImp::PushInstance(RndMultiMesh::Instance &inst) {
    for (int i = 0; i < mMultiMeshes.size(); i++) {
        mMultiMeshes[i]->Instances().push_back(inst);
    }
}

void MultiMeshWidgetImp::DrawInstances(const ObjPtrList<RndMesh> &meshes, int i2) {
    // rb3-Wii gates this loop with `if (unk10) { dynamic_cast<WiiMultiMesh*>
    // (mesh)->unk34 = true; }` before every DrawShowing() -- a Wii/GX-specific
    // multi-mesh flag (WiiMultiMesh, src/rndwii/MultiMesh.h) that has no
    // counterpart anywhere in this tree (src/rndwii does not exist; RndMultiMesh
    // here carries no unk34-shaped field). Dropped outright, same as
    // TrackWidgetImpBase's removed CheckValid() and TrackWidget::CheckValid()'s
    // no-op reduction elsewhere in this file family -- `unk10` is kept as a
    // stored member (see TrackWidgetImp.h) for layout/ABI parity with callers
    // that construct MultiMeshWidgetImp, it is just never acted on here.
    int count = 0;
    for (int i = 0; i < mMultiMeshes.size(); i++) {
        RndMultiMesh *mesh = mMultiMeshes[i];
        mesh->DrawShowing();
        count++;
        if (i2 >= 0 && count >= i2)
            break;
    }
}

bool MultiMeshWidgetImp::Empty() {
    if (mMultiMeshes.empty())
        return true;
    else
        return mMultiMeshes.front()->Instances().empty();
}

int MultiMeshWidgetImp::Size() {
    if (mMultiMeshes.empty())
        return 0;
    else
        return mMultiMeshes.front()->Instances().size();
}

// The methods below reimplement TrackWidgetImp<T>::Do{Clear,RemoveAt,
// RemoveUntil,GetFirstInstanceY,GetLastInstanceY,Sort}()'s bodies (and, where
// they call it, the base's virtual RemoveInstances()) directly against
// RndMultiMesh::InstanceList, rather than delegating to those helpers. The
// helpers are hardcoded to std::list<T> (default allocator); InstanceList is
// std::list<Instance, TransformListAlloc<Instance>>, a distinct type whose
// allocate()/deallocate() route through the global gTransListAlloc pool
// instead. On rb3-Wii, RndMultiMesh::mInstances is a plain default-allocator
// list, so the oracle's equivalents call straight through to the Do* helpers
// with no friction -- this divergence is X360-retail-specific (same class as
// the WiiMultiMesh/SetMeshForceNoQuantize drops elsewhere in this file), and
// MultiMeshWidgetImp already overrides every one of these individually rather
// than relying on the base template's single-list-oriented inline versions,
// consistent with needing bespoke logic here.

void MultiMeshWidgetImp::Clear() {
    for (int i = 0; i < mMultiMeshes.size(); i++) {
        mMultiMeshes[i]->Instances().clear();
        SetDirty(true);
    }
}

void MultiMeshWidgetImp::RemoveUntil(float f1, float f2) {
    for (int i = 0; i < mMultiMeshes.size(); i++) {
        RndMultiMesh *mesh = mMultiMeshes[i];
        RndMultiMesh::InstanceList &insts = mesh->Instances();
        if (!insts.empty()) {
            RndMultiMesh::InstanceList::iterator it = insts.begin();
            RndMultiMesh::InstanceList::iterator begin = it;
            for (; it != insts.end() && f2 * it->mXfm.m.y.y + it->mXfm.v.y < f1; ++it) {
            }
            if (it != begin) {
                insts.erase(insts.begin(), it);
                SetDirty(true);
            }
        }
    }
}

void MultiMeshWidgetImp::RemoveAt(float f1, float f2, float f3) {
    for (int i = 0; i < mMultiMeshes.size(); i++) {
        RndMultiMesh::InstanceList &insts = mMultiMeshes[i]->Instances();
        if (!insts.empty()) {
            RndMultiMesh::InstanceList::iterator it5c = insts.end();
            RndMultiMesh::InstanceList::iterator it = insts.begin();
            for (; it != insts.end(); ++it) {
                if (IsFabsZero(it->mXfm.v.y - f1)) {
                    if (f3 < 0 || Abs<float>(it->mXfm.v.x - f2) <= f3) {
                        if (it5c == insts.end()) {
                            it5c = it;
                        }
                    } else if (it5c != insts.end()) {
                        insts.erase(it5c, it);
                        SetDirty(true);
                        it5c = insts.end();
                    }
                } else if (it->mXfm.v.y > f1) {
                    if (it5c != insts.end()) {
                        insts.erase(it5c, it);
                        SetDirty(true);
                        it5c = insts.end();
                    }
                    break;
                }
            }
            if (it5c != insts.end()) {
                insts.erase(it5c, it);
                SetDirty(true);
            }
        }
    }
}

float MultiMeshWidgetImp::GetFirstInstanceY() {
    if (mMultiMeshes.empty())
        return 0;
    else {
        RndMultiMesh::InstanceList &insts = mMultiMeshes.front()->Instances();
        MILO_ASSERT(!insts.empty(), 0x8F);
        return insts.front().mXfm.v.y;
    }
}

float MultiMeshWidgetImp::GetLastInstanceY() {
    if (mMultiMeshes.empty())
        return 0;
    else {
        RndMultiMesh::InstanceList &insts = mMultiMeshes.front()->Instances();
        MILO_ASSERT(!insts.empty(), 0x95);
        return insts.back().mXfm.v.y;
    }
}

void MultiMeshWidgetImp::Sort() {
    for (int i = 0; i < mMultiMeshes.size(); i++) {
        RndMultiMesh *mesh = mMultiMeshes[i];
        RndMultiMesh::InstanceList &insts = mesh->Instances();
        insts.sort(WidgetInstanceCmp<RndMultiMesh::Instance>());
    }
}

CharWidgetImp::CharWidgetImp(
    RndFont *f,
    RndText *t,
    int i1,
    int i2,
    RndText::Alignment a,
    Hmx::Color32 c1,
    Hmx::Color32 c2,
    bool b7
)
    : mNeedRebuild(true), mNeedSync(false), mCharsPerInst(i1), mMaxInstances(i2),
      mText(t), mFont(f) {
    if (Valid()) {
        mText->SetFixedLength(mCharsPerInst * mMaxInstances);
        mText->ReserveLines(mMaxInstances);
        mReusableLines.reserve(mMaxInstances);
        mText->SetAlignment(a);
        Hmx::Color col1;
        col1.UnpackAlpha(c1.FullColor());
        mText->SetColor(col1);
        Hmx::Color col2;
        col2.UnpackAlpha(c2.FullColor());
        mText->SetAltStyle(
            mText->mStyle.mFont,
            mText->mStyle.mSize,
            &col2,
            mText->mStyle.mZOffset,
            mText->mStyle.mItalics,
            true
        );
        mText->mRotateLineVerts = b7;
        Transform tf60;
        tf60.Reset();
        mText->SetWorldXfm(tf60);
    }
}

void CharWidgetImp::PushInstance(TextInstance &inst) {
    if (Valid()) {
        Instances().push_back(inst);
        TextInstance &firstInst = Instances().back();
        MILO_ASSERT(mText, 0x159);
        if (mText->GetAlignment() & 4) {
            int len = firstInst.mText.length();
            if (mCharsPerInst - len > 0) {
                String newStr(mCharsPerInst - len, ' ');
                firstInst.mText =
                    MakeString("%s%s", newStr.c_str(), firstInst.mText.c_str());
            }
        }
        if (mReusableLines.empty()) {
            RndText::Style &style =
                firstInst.mUseAltStyle ? mText->mAltStyle : mText->mStyle;
            firstInst.mLineId = mText->AddLineUTF8(
                firstInst.mText, firstInst.mXfm, style, nullptr, &mNeedSync, mCharsPerInst
            );
            if (firstInst.mLineId == -1) {
                MILO_WARN(
                    "CharWidgetImp::PushInstance() - AddLineUTF8 failed; please alert Track/HUD coder"
                );
            }
        } else {
            firstInst.mLineId = mReusableLines.back();
            RndText::Style &style =
                firstInst.mUseAltStyle ? mText->mAltStyle : mText->mStyle;
            mText->ReplaceLineText(
                firstInst.mLineId,
                firstInst.mText,
                firstInst.mXfm,
                style,
                nullptr,
                &mNeedSync,
                mCharsPerInst
            );
            mReusableLines.pop_back();
        }
    }
}

std::list<TextInstance> &CharWidgetImp::Instances() { return mInstances; }

void CharWidgetImp::SetScale(float f) { SetLocalScale(mText, Vector3(f, 1, f)); }

void CharWidgetImp::RemoveInstances(
    std::list<TextInstance> &insts,
    std::list<TextInstance>::iterator from,
    std::list<TextInstance>::iterator to
) {
    if (Valid()) {
        std::list<TextInstance>::iterator it = from;
        for (; it != to; ++it) {
            TextInstance &cur = *it;
            int id = cur.mLineId;
            if (id < 0 || id >= mMaxInstances) {
                MILO_WARN("widget instance in unexpected state - rebuilding");
                mNeedRebuild = true;
            } else {
                Transform tf50;
                tf50.Reset();
                mText->ReplaceLineText(
                    id,
                    "",
                    tf50,
                    cur.mUseAltStyle ? mText->mAltStyle : mText->mStyle,
                    nullptr,
                    &mNeedSync,
                    mCharsPerInst
                );
                mReusableLines.push_back(id);
            }
        }
        insts.erase(from, to);
    }
}

void CharWidgetImp::Poll() {
    if (Valid()) {
        if (mNeedRebuild) {
            mText->SetText("");
            mReusableLines.clear();
            std::list<TextInstance>::iterator it = Instances().begin();
            std::list<TextInstance>::iterator itEnd = Instances().end();
            for (; it != itEnd; ++it) {
                TextInstance &cur = *it;
                int id;
                cur.mLineId = id = mText->AddLineUTF8(
                    cur.mText,
                    cur.mXfm,
                    cur.mUseAltStyle ? mText->mAltStyle : mText->mStyle,
                    nullptr,
                    &mNeedSync,
                    mCharsPerInst
                );
                if (id == -1) {
                    MILO_WARN(
                        "CharWidgetImp::Poll() - AddLineUTF8 failed; please alert Track/HUD coder"
                    );
                }
            }
            mNeedRebuild = false;
        }
        if (mNeedSync) {
            // rb3-Wii also calls mText->SetMeshForceNoQuantize() here first --
            // a Wii GX hardware vertex-quantization control with no counterpart
            // on this platform (no "Quantize" member/method exists anywhere in
            // this tree's RndText/RndMesh). Dropped, same rationale as the
            // WiiMultiMesh branch above.
            mText->SyncMeshes();
            mNeedSync = false;
        }
    }
}

void CharWidgetImp::DrawInstances(const ObjPtrList<RndMesh> &, int) {
    if (Valid()) {
        mText->SetMeshForceNoUpdate();
        mText->DrawShowing();
    }
}

void CharWidgetImp::Clear() { TrackWidgetImp<TextInstance>::Clear(); }

void MatWidgetImp::DrawInstances(const ObjPtrList<RndMesh> &, int) {
    std::list<MeshInstance>::iterator it = Instances().begin();
    std::list<MeshInstance>::iterator itEnd = Instances().end();
    for (; it != itEnd; ++it) {
        RndMesh *curMesh = it->mMesh;
        curMesh->SetWorldXfm(it->mXfm);
        curMesh->SetMat(mMat);
        curMesh->DrawShowing();
    }
}

int MatWidgetImp::AddMeshInstance(Transform tf, RndMesh *mesh, float f3) {
    bool b2 = false;
    if (!Empty() && tf.v.y < GetLastInstanceY()) {
        b2 = true;
    }
    MeshInstance inst(tf, mesh);
    PushInstance(inst);
    if (b2)
        Sort();
    return b2;
}
