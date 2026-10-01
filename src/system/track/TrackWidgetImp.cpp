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
        RndMultiMesh::InstanceList::iterator instIt;
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

// Code in this class naturally reads `mesh->mInstances` directly. In this tree
// RndMultiMesh::mInstances is `protected`
// (src/system/rndobj/MultiMesh.h), so
// every such access below goes through the public `Instances()` accessor
// instead -- a trivial one-line inline expected to codegen identically.

MultiMeshWidgetImp::MultiMeshWidgetImp(const ObjPtrList<RndMesh> &meshlist)
    : mMeshes(meshlist) {
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

RndMultiMesh::InstanceList &MultiMeshWidgetImp::Instances() {
    MILO_FAIL("MultiMeshWidgetImp::Instances() called; not implemented");
    // TrackWidgetImp<RndMultiMesh::Instance> now uses RndMultiMesh::InstanceList
    // (see TrackWidgetList in TrackWidgetImp.h), so the multimesh's own list is
    // returned directly; no cast between list specializations is needed.
    return mMultiMeshes.front()->Instances();
}

void MultiMeshWidgetImp::PushInstance(RndMultiMesh::Instance &inst) {
    for (int i = 0; i < mMultiMeshes.size(); i++) {
        mMultiMeshes[i]->Instances().push_back(inst);
    }
}

void MultiMeshWidgetImp::DrawInstances(const ObjPtrList<RndMesh> &meshes, int i2) {
    // No per-mesh GX flag (unk34) is set before DrawShowing() here:
    // that flag is GX-hardware-specific
    // state and has no
    // counterpart anywhere in this tree (RndMultiMesh
    // here carries no unk34-shaped field). Dropped outright, same as
    // TrackWidgetImpBase's removed CheckValid() and TrackWidget::CheckValid()'s
    // no-op reduction elsewhere in this file family -- `unk10` is kept as a
    // member; retail's ctor (0x827E58B8) takes no bool and the object is 0x14,
    // so the member is gone too.
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

// RemoveAt / RemoveUntil below call the base template's Do* helpers, as retail
// does. The others reimplement TrackWidgetImp<T>::Do{Clear,GetFirstInstanceY,
// GetLastInstanceY,Sort}()'s bodies (and, where
// they call it, the base's virtual RemoveInstances()) directly against
// RndMultiMesh::InstanceList, rather than delegating to those helpers. The
// helpers are hardcoded to std::list<T> (default allocator); InstanceList is
// std::list<Instance, TransformListAlloc<Instance>>, a distinct type whose
// allocate()/deallocate() route through the global gTransListAlloc pool
// instead. With a plain default-allocator
// list, the overrides would call straight through to the Do* helpers
// with no friction -- this divergence is X360-retail-specific (same class as
// the GX-only calls dropped elsewhere in this file), and
// MultiMeshWidgetImp already overrides every one of these individually rather
// than relying on the base template's single-list-oriented inline versions,
// consistent with needing bespoke logic here.

void MultiMeshWidgetImp::Clear() {
    for (int i = 0; i < mMultiMeshes.size(); i++) {
        mMultiMeshes[i]->Instances().clear();
        SetDirty(true);
    }
}

// Retail runs the base template's out-of-line DoRemoveUntil / DoRemoveAt over
// each multimesh's instance list.
void MultiMeshWidgetImp::RemoveUntil(float f1, float f2) {
    for (int i = 0; i < mMultiMeshes.size(); i++) {
        DoRemoveUntil(mMultiMeshes[i]->Instances(), f1, f2);
    }
}

void MultiMeshWidgetImp::RemoveAt(float f1, float f2, float f3) {
    for (int i = 0; i < mMultiMeshes.size(); i++) {
        DoRemoveAt(mMultiMeshes[i]->Instances(), f1, f2, f3);
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
    Hmx::Color c1,
    Hmx::Color c2,
    bool b7
)
    : mNeedRebuild(true), mNeedSync(false), mCharsPerInst(i1), mMaxInstances(i2),
      mText(t), mFont(f) {
    if (Valid()) {
        mText->SetFixedLength(mCharsPerInst * mMaxInstances);
        mText->ReserveLines(mMaxInstances);
        mReusableLines.reserve(mMaxInstances);
        mText->SetAlignment(a);
        // retail passes both colours as Hmx::Color by value (r9:r10 + stack)
        mText->SetColor(c1);
        mText->SetAltStyle(
            mText->mStyle.mFont,
            mText->mStyle.mSize,
            &c2,
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
            // No SetMeshForceNoQuantize() call here first --
            // a GX hardware vertex-quantization control with no counterpart
            // on this platform (no "Quantize" member/method exists anywhere in
            // this tree's RndText/RndMesh). Dropped, same rationale as the
            // multi-mesh flag above.
            mText->SyncMeshes();
            mNeedSync = false;
        }
    }
}

void CharWidgetImp::DrawInstances(const ObjPtrList<RndMesh> &, int) {
    if (Valid()) {
#ifdef HX_NATIVE
        // Absent from retail, which draws the text directly.
        mText->SetMeshForceNoUpdate();
#endif
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
