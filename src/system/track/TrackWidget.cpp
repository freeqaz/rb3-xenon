#include "compiler_macros.h"
#include "decomp.h"
#include "obj/ObjMacros.h"
#include "track/TrackWidget.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Mat.h"
#include "rndobj/Text.h"
#include "utl/Loader.h"
#include "utl/Symbols.h"
#include "utl/BinStream.h"
#include "track/TrackDir.h"
#include "track/TrackWidgetImp.h"
#include <list>

// rb3-Wii defines IsFloatOne in math/Utl.h, but adding it there perturbs codegen
// in unrelated units that include Utl.h (extra inline COMDAT shifts inlining).
// CheckScales below is the only consumer in this port, so it is scoped here to
// keep math/Utl.h byte-identical to main.
inline bool IsFloatOne(float f) {
    return fabs(f - 1.0f) < 0.000099999997f ? true : false;
}

// RB3-360 retail keeps the two rev words in ONE INTERNAL-LINKAGE align(4) pair,
// not in the DECLARE_REVS class statics: measured on retail bytes, the class
// statics make MSVC emit TWO `lis` pairs (`lis r9, ?gAltRev@TrackWidget@@2GA`
// alongside the base for gRev) because it cannot relate two EXTERNAL symbols,
// while retail folds both words onto one base register at +0x0/+0x4.
// DECLARE_REVS is dropped from the header in the same edit so the unqualified
// `gRev` in the member functions below resolves to these file-scope words
// rather than being shadowed by the class-scope declaration.
static struct {
    unsigned short altRev;
    unsigned short pad;
    unsigned short rev;
} gRevs_TrackWidget;
#define gRev gRevs_TrackWidget.rev
#define gAltRev gRevs_TrackWidget.altRev
TrackWidget::TrackWidget()
    : mMeshes(this), mMeshesLeft(this), mMeshesSpan(this), mMeshesRight(this),
      mEnviron(this), mBaseLength(1), mBaseWidth(1), mXOffset(0), mYOffset(0),
      mZOffset(0), mTrackDir(0), mImp(0), mFont(this), mTextObj(this),
      mTextAlignment(RndText::kMiddleCenter), mTextColor(1, 1, 1), mAltTextColor(1, 1, 1),
      mMat(this), mActive(0), mWideWidget(0), mAllowRotation(0), mAllowShift(0),
      mAllowLineRotation(0), mWidgetType(kImmediateWidget), mMaxMeshes(-1),
      mCharsPerInst(0), mMaxTextInstances(0) {
    SyncImp();
}

TrackWidget::~TrackWidget() { RELEASE(mImp); }

BEGIN_COPYS(TrackWidget)
    CREATE_COPY_AS(TrackWidget, tw)
    MILO_ASSERT(tw, 66);
    COPY_SUPERCLASS_FROM(Hmx::Object, tw)
    COPY_SUPERCLASS_FROM(RndDrawable, tw)
    COPY_MEMBER_FROM(tw, mMeshes)
    COPY_MEMBER_FROM(tw, mWideWidget)
    COPY_MEMBER_FROM(tw, mMeshesLeft)
    COPY_MEMBER_FROM(tw, mMeshesSpan)
    COPY_MEMBER_FROM(tw, mMeshesRight)
    COPY_MEMBER_FROM(tw, mEnviron)
    COPY_MEMBER_FROM(tw, mBaseLength)
    COPY_MEMBER_FROM(tw, mBaseWidth)
    COPY_MEMBER_FROM(tw, mAllowRotation)
    COPY_MEMBER_FROM(tw, mFont)
    COPY_MEMBER_FROM(tw, mTextObj)
    COPY_MEMBER_FROM(tw, mTextAlignment)
    COPY_MEMBER_FROM(tw, mCharsPerInst)
    COPY_MEMBER_FROM(tw, mMaxTextInstances)
    COPY_MEMBER_FROM(tw, mWidgetType)
    COPY_MEMBER_FROM(tw, mMat)
    COPY_MEMBER_FROM(tw, mTextColor)
    COPY_MEMBER_FROM(tw, mAltTextColor)
    COPY_MEMBER_FROM(tw, mXOffset)
    COPY_MEMBER_FROM(tw, mYOffset)
    COPY_MEMBER_FROM(tw, mZOffset)
    COPY_MEMBER_FROM(tw, mAllowShift)
    COPY_MEMBER_FROM(tw, mAllowLineRotation)
END_COPYS

BEGIN_SAVES(TrackWidget)
    SAVE_REVS(15, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mMeshes;
    bs << mWideWidget;
    bs << mMeshesLeft;
    bs << mMeshesSpan;
    bs << mMeshesRight;
    bs << mEnviron;
    bs << mBaseLength;
    bs << mBaseWidth;
    bs << mAllowRotation;
    bs << mFont;
    bs << mTextObj;
    bs << mCharsPerInst;
    bs << mMaxTextInstances;
    bs << mWidgetType;
    bs << mMat;
    bs << (int)mTextAlignment;
    bs << mTextColor;
    bs << mAltTextColor;
    bs << mYOffset;
    bs << mXOffset;
    bs << mZOffset;
    bs << mAllowShift;
    bs << mAllowLineRotation;
END_SAVES

// The packed-rev aggregate + gRev/gAltRev macros are declared once at the top of
// this file (an equivalent lever landed there independently); reusing that single
// definition here.  A second `static struct { ... } gRevs_TrackWidget;` at this
// point is a C2371 redefinition, and the `__declspec(align(4))` spelling is a
// no-op anyway -- altRev already sits at offset +0, so both spellings yield the
// same {altRev@0, pad@2, rev@4} layout retail addresses as 0(r29)/4(r29).

BEGIN_LOADS(TrackWidget)
    LOAD_REVS(bs)
    ASSERT_REVS(15, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    if (gRev != 0)
        LOAD_SUPERCLASS(RndDrawable)
    bs >> mMeshes;
    if (gRev > 4) {
        bs >> mWideWidget;
        bs >> mMeshesLeft;
        bs >> mMeshesSpan;
        bs >> mMeshesRight;
    }
    bs >> mEnviron;
    if (gRev > 2)
        bs >> mBaseLength;
    if (gRev > 8)
        bs >> mBaseWidth;
    if (gRev > 1 && gRev < 8) {
        bool bbb = 0;
        bs >> bbb;
        if (bbb)
            mWidgetType = kMultiMeshWidget;
    }
    if (gRev > 3) {
        bs >> mAllowRotation;
    }
    if (gRev > 5) {
        bs >> mFont;
        if (gRev < 8) {
            bool bbb = 0;
            bs >> bbb;
            if (bbb)
                mWidgetType = kTextWidget;
        }
    }
    if (gRev > 6) {
        bs >> mTextObj;
        bs >> mCharsPerInst;
        bs >> mMaxTextInstances;
    }
    if (gRev > 7) {
        bs >> mWidgetType;
        bs >> mMat;
    }
    if (gRev > 9)
        bs >> (int &)mTextAlignment;
    if (gRev > 10) {
        bs >> mTextColor;
        bs >> mAltTextColor;
    }
    if (gRev > 0xB)
        bs >> mYOffset;
    if (gRev > 0xC) {
        bs >> mXOffset;
        bs >> mZOffset;
    }
    if (gRev > 0xD) {
        bs >> mAllowShift;
    }
    if (gRev > 0xE) {
        bs >> mAllowLineRotation;
    }
    SyncImp();
END_LOADS
#undef gAltRev
#undef gRev

// Retail X360 (MILO_DEBUG off) has no TrackWidgetImpBase::CheckValid virtual
// (see TrackWidgetImp.h), so this reduces to an empty no-op that /Ob2 inlines
// away at the SyncImp() call site. Kept unconditional because macros.h forces
// MILO_DEBUG on tree-wide.
void TrackWidget::CheckValid() const {}

void TrackWidget::Init() { mImp->Init(); }

void TrackWidget::DrawShowing() {
    if (!mImp->Empty()) {
        if (mEnviron && mEnviron != RndEnviron::Current())
            mEnviron->Select(nullptr);
        mImp->DrawInstances(mMeshes, mMaxMeshes);
    }
}

void TrackWidget::Poll() {
    if (mTrackDir) {
        float cutOffY = mTrackDir->CutOffY();
        mImp->RemoveUntil(cutOffY, mBaseLength);
        mImp->Poll();
    }
}

FORCE_LOCAL_INLINE
bool TrackWidget::Empty() { return mImp->Empty(); }
END_FORCE_LOCAL_INLINE

FORCE_LOCAL_INLINE
int TrackWidget::Size() const { return mImp->Size(); }
END_FORCE_LOCAL_INLINE

float TrackWidget::GetFirstInstanceY() { return mImp->GetFirstInstanceY(); }

void TrackWidget::AddInstance(Transform tf, float f) {
    if (f)
        tf.m.y.y = NewYOffset(f) / mBaseLength;
    ApplyOffsets(tf);
    if (mImp->AddInstance(tf, 0) && mTrackDir->WarnOnResort()) {
        MILO_WARN("%s instances resorted", Name());
    }
    UpdateActiveStatus();
}

void TrackWidget::AddTextInstance(const Transform &Ct, class String s, bool b) {
    Transform t = Ct;
    ApplyOffsets(t);
    int ret = mImp->AddTextInstance(t, s, b);
    if (ret && mTrackDir->WarnOnResort())
        MILO_WARN("%s instances resorted", Name());
    UpdateActiveStatus();
}

void TrackWidget::AddMeshInstance(const Transform &Ct, RndMesh *m, float f) {
    if (mImp->AddMeshInstance(Ct, m, f) && mTrackDir->WarnOnResort())
        MILO_WARN("%s instances resorted", Name());
    UpdateActiveStatus();
}

void TrackWidget::RemoveAt(float f) { mImp->RemoveAt(NewYOffset(f), mXOffset, -1.0f); }

void TrackWidget::RemoveAt(float f, int i) {
    float sToY = mTrackDir->SecondsToY(f);
    float y = mYOffset + sToY;
    float x_added = mXOffset + mTrackDir->SlotAt(i).v.x;
    float f4;
    if (i > 0)
        f4 = Abs<float>(x_added - mTrackDir->SlotAt(i - 1).v.x) / 2.0f;
    else
        f4 = Abs<float>(mTrackDir->SlotAt(i + 1).v.x - x_added) / 2.0f;
    mImp->RemoveAt(y, x_added, f4);
}

void TrackWidget::ApplyOffsets(Transform &t) {
    t.v.x += mXOffset;
    t.v.y += mYOffset;
    t.v.z += mZOffset;
}

void TrackWidget::Clear() { mImp->Clear(); }

void TrackWidget::SetTextAlignment(RndText::Alignment a) {
    if (a == mTextAlignment)
        return;
    mTextAlignment = a;
    SyncImp();
}

void TrackWidget::Mats(std::list<class RndMat *> &mats, bool) {
    FOREACH (it, mMeshes) {
        RndMesh *cur = *it;
        if (cur) {
            RndMat *mat = cur->Mat();
            if (mat) {
                MatShaderOptions opts;
                if (mWidgetType == kMultiMeshWidget) {
                    int constraint = cur->TransConstraint();
                    int mask = 0xC;
                    // Match-only pattern: explicit zero-then-set produces the
                    // rlwinm+rlwimi sequence the original uses for the i5 bit,
                    // and the inlined SetLast5 keeps `pack & ~0x1F` live across
                    // the branch so it doesn't have to be reloaded.
                    opts.shader_struct.i5 = 0;
                    opts.shader_struct.i5
                        = (constraint == RndTransformable::kConstraintFastBillboardXYZ);
                    int packCleared = opts.pack & ~0x1F;
                    if ((opts.pack >> 5) & 1) mask = 0xD;
                    opts.pack = packCleared | (mask & 0x1F);
                } else {
                    opts.SetLast5(0x12);
                }
                mat->SetShaderOpts(opts);
                mats.push_back(mat);
            }
        }
    }
}

void TrackWidget::SyncImp() {
    RELEASE(mImp);
    switch (mWidgetType) {
    case kTextWidget:
        mImp = new CharWidgetImp(
            mFont,
            mTextObj,
            mCharsPerInst,
            mMaxTextInstances,
            mTextAlignment,
            Hmx::Color32(mTextColor),
            Hmx::Color32(mAltTextColor),
            mAllowLineRotation
        );
        break;
    case kMatWidget:
        mImp = new MatWidgetImp(mMat);
        break;
    case kMultiMeshWidget:
        mImp = new MultiMeshWidgetImp(mMeshes, mAllowRotation);
        break;
    default:
        mImp = new ImmediateWidgetImp(mAllowRotation);
        break;
    }
    CheckValid();
    if (LOADMGR_EDITMODE)
        Init();
}

void TrackWidget::SetScale(float f) { mImp->SetScale(f); }

void TrackWidget::CheckScales() const {
    if (!mAllowRotation) {
        FOREACH (it, mMeshes) {
            RndMesh *cur = *it;
            if (!IsFloatOne(cur->LocalXfm().m.x.x) || !IsFloatOne(cur->LocalXfm().m.y.y)
                || !IsFloatOne(cur->LocalXfm().m.z.z)) {
                MILO_WARN(
                    "TrackWidget: %s does not have unit scale, but will be drawn on track with unit scale!",
                    cur->Name()
                );
            }
        }
    }
}

DataNode TrackWidget::OnSetMeshes(const DataArray *da) {
    mMeshes.clear();
    for (int i = 2; i < da->Size(); i++) {
        mMeshes.push_back(da->Obj<RndMesh>(i));
    }
    return 0;
}

DataNode TrackWidget::OnAddInstance(const DataArray *da) {
    Transform t;
    t.Reset();
    t.v.x = da->Float(2);
    t.v.y = da->Float(3);
    t.v.z = da->Float(4);
    AddInstance(t, 0);
    return 0;
}

DataNode TrackWidget::OnAddTextInstance(const DataArray *da) {
    Transform t;
    t.Reset();
    t.v.x = da->Float(2);
    t.v.y = da->Float(3);
    t.v.z = da->Float(4);
    class String s(da->Str(5));
    AddTextInstance(t, s, false);
    return 0;
}

DataNode TrackWidget::OnAddMeshInstance(const DataArray *da) {
    Transform t;
    t.Reset();
    t.v.x = da->Float(2);
    t.v.y = da->Float(3);
    t.v.z = da->Float(4);
    AddMeshInstance(t, da->Obj<RndMesh>(5), 0);
    return 0;
}

void TrackWidget::UpdateActiveStatus() {
    if (!mActive && !Empty()) {
        mTrackDir->AddActiveWidget(this);
        mActive = true;
    }
}

void TrackWidget::SetInactive() { mActive = false; }

BEGIN_HANDLERS(TrackWidget)
    HANDLE_ACTION(clear, Clear())
    HANDLE(set_meshes, OnSetMeshes)
    HANDLE(add_instance, OnAddInstance)
    HANDLE(add_text_instance, OnAddTextInstance)
    HANDLE(add_mesh_instance, OnAddMeshInstance)
    HANDLE_EXPR(size, Size())
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(575)
END_HANDLERS

#pragma push
#pragma pool_data off
BEGIN_PROPSYNCS(TrackWidget)
    SYNC_PROP_MODIFY_ALT(meshes, mMeshes, CheckScales())
    SYNC_PROP(wide_widget, mWideWidget)
    SYNC_PROP(meshes_left, mMeshesLeft)
    SYNC_PROP(meshes_span, mMeshesSpan)
    SYNC_PROP(meshes_right, mMeshesRight)
    SYNC_PROP(environ, mEnviron)
    SYNC_PROP_MODIFY_ALT(allow_rotation, mAllowRotation, SyncImp())
    SYNC_PROP(base_length, mBaseLength)
    SYNC_PROP(base_width, mBaseWidth)
    SYNC_PROP(max_meshes, mMaxMeshes)
    SYNC_PROP_MODIFY_ALT(font, mFont, SyncImp())
    SYNC_PROP_MODIFY_ALT(text_obj, mTextObj, SyncImp())
    SYNC_PROP_MODIFY_ALT(text_alignment, (int &)mTextAlignment, SyncImp())
    SYNC_PROP_MODIFY_ALT(chars_per_inst, mCharsPerInst, SyncImp())
    SYNC_PROP_MODIFY_ALT(max_text_instances, mMaxTextInstances, SyncImp())
    SYNC_PROP_MODIFY_ALT(text_color, mTextColor, SyncImp())
    SYNC_PROP_MODIFY_ALT(alt_text_color, mAltTextColor, SyncImp())
    SYNC_PROP_MODIFY_ALT(mat, mMat, SyncImp())
    SYNC_PROP_MODIFY_ALT(widget_type, mWidgetType, SyncImp())
    SYNC_PROP(x_offset, mXOffset)
    SYNC_PROP(y_offset, mYOffset)
    SYNC_PROP(z_offset, mZOffset)
    SYNC_PROP(allow_shift, mAllowShift)
    SYNC_PROP_MODIFY_ALT(allow_line_rotation, mAllowLineRotation, SyncImp())
    SYNC_SUPERCLASS(RndDrawable)
END_PROPSYNCS
#pragma pop

// sw3 cross-dialect scatter-include (default/TrackWidget <- char/ClipCollide.cpp) [Object owner]
#ifndef SW_SCATTER_OWNER_INCLUDE
#define SW_SCATTER_OWNER_INCLUDE
#define gRev gRev_ClipCollide
#define gAltRev gAltRev_ClipCollide
#include "obj/dialect_object_push.h"
#include "char/ClipCollide.cpp"
#include "obj/dialect_object_pop.h"
#undef gRev
#undef gAltRev
#undef SW_SCATTER_OWNER_INCLUDE
#endif
