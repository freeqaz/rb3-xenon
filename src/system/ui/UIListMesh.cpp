#define RB3_TU_OBJPTR_FORCEINLINE_CTOR
#define RB3_TU_OBJPTR_DEFER_OWNER
#include "ui/UIListMesh.h"
#include "obj/Object.h"
#include "rndobj/Cam.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "ui/UIListSlot.h"
#include "ui/UI.h"
#include "utl/Loader.h"
#ifdef HX_NATIVE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

#ifdef HX_NATIVE
namespace {
bool DebugChooseModeMesh() {
    static int enabled = -1;
    if (enabled == -1) {
        const char *env = getenv("MILO_DEBUG_CHOOSE_MODE");
        enabled = (env && env[0] && strcmp(env, "0") != 0) ? 1 : 0;
    }
    return enabled != 0;
}
}
#endif

#pragma region UIListMesh

// Retail inlines ObjPtr<RndMesh>(this) but calls ObjPtr<RndMat>'s ctor
// out-of-line; the declared-only specialization forces that call.
template <>
ObjPtr<RndMat>::ObjPtr(Hmx::Object *, RndMat *);

UIListMesh::UIListMesh() : mMesh(this), mDefaultMat(this) {}

BEGIN_HANDLERS(UIListMesh)
    HANDLE_SUPERCLASS(UIListSlot)
END_HANDLERS

BEGIN_PROPSYNCS(UIListMesh)
    SYNC_PROP(mesh, mMesh)
    SYNC_PROP(default_mat, mDefaultMat)
    SYNC_SUPERCLASS(UIListSlot)
END_PROPSYNCS

BEGIN_SAVES(UIListMesh)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(UIListSlot)
    bs << mMesh << mDefaultMat;
END_SAVES

BEGIN_COPYS(UIListMesh)
    COPY_SUPERCLASS(UIListSlot)
    CREATE_COPY_AS(UIListMesh, m)
    MILO_ASSERT(m, 0x9F);
    COPY_MEMBER_FROM(m, mMesh)
    COPY_MEMBER_FROM(m, mDefaultMat)
END_COPYS

// RB3 retail keeps no BinStreamRev here: the packed rev is split into two
// mutable TU shorts (alt at +0, rev at +4), no guard, no Push/PopRev -- the
// ui/UIButton.cpp dialect.
#pragma push_macro("INIT_REVS")
#pragma push_macro("LOAD_REVS")
#undef INIT_REVS
#undef LOAD_REVS
#define INIT_REVS(rev, alt)                                                              \
    static unsigned short gAltRev = alt;                                                 \
    static unsigned short gRev = rev;
#define LOAD_REVS(bs)                                                                    \
    int rev;                                                                             \
    bs >> rev;                                                                           \
    gRev = getHmxRev(rev);                                                               \
    gAltRev = getAltRev(rev);

INIT_REVS(0, 0)

// Retail 0x82814B90.
void UIListMesh::Load(BinStream &bs) {
    LOAD_REVS(bs)
    UIListSlot::Load(bs);
    bs >> mMesh >> mDefaultMat;
}

#pragma pop_macro("LOAD_REVS")
#pragma pop_macro("INIT_REVS")

void UIListMesh::Draw(
    const UIListWidgetDrawState &drawstate,
    const UIListState &liststate,
    const Transform &tf,
    UIComponent::State compstate,
    Box *box,
    DrawCommand cmd
) {
    if (mMesh) {
        // RB3 retail (UIListMesh::Draw) has no edit-mode material save/restore:
        // it only brackets the slot draw with the mesh's local transform.
#ifdef HX_NATIVE
        float somefloat = 1.0f;
        RndMat *themat = 0;
        if (TheLoadMgr.EditMode()) {
            themat = mMesh->Mat();
            if (themat)
                somefloat = themat->Alpha();
        }
#endif
        Transform xfm1 = mMesh->LocalXfm();
        UIListSlot::Draw(drawstate, liststate, tf, compstate, box, cmd);
        mMesh->SetLocalXfm(xfm1);
#ifdef HX_NATIVE
        if (TheLoadMgr.EditMode()) {
            mMesh->SetMat(themat);
            if (themat) {
                themat->SetAlpha(somefloat);
            }
        }
#endif
    }
}

UIListSlotElement *UIListMesh::CreateElement(UIList *uilist) {
    MILO_ASSERT(mMesh, 0x5b);
    UIListSlotElement *element = new UIListMeshElement(this);
    return element;
}

RndMat *UIListMesh::DefaultMat() const { return mDefaultMat; }
RndTransformable *UIListMesh::RootTrans() { return mMesh; }

#pragma endregion UIListMesh
#pragma region UIListMeshElement

inline void
UIListMeshElement::Draw(const Transform &tf, float f, UIColor *col, Box *box) {
    RndMesh *mesh = mListMesh->Mesh();
    MILO_ASSERT(mesh, 0x1B);
    mesh->SetWorldXfm(tf);
    if (box != nullptr) {
        Box localbox;
        localbox.mMin = box->mMin;
        localbox.mMax = box->mMax;
        CalcBox(mesh, localbox);
        box->GrowToContain(localbox.mMin, false);
        box->GrowToContain(localbox.mMax, false);
    } else if (mMat != nullptr) {
        float alpha = mMat->Alpha();
#ifdef HX_NATIVE
        static int sChooseModeMeshDiag = 0;
        if (DebugChooseModeMesh() && sChooseModeMeshDiag < 80) {
            RndCam *cam = RndCam::Current();
            Vector2 screenPos(0.0f, 0.0f);
            float depth = cam ? cam->WorldToScreen(tf.v, screenPos) : 0.0f;
            RndCam *uiCam = TheUI ? TheUI->GetCam() : nullptr;
            Vector2 uiScreenPos(0.0f, 0.0f);
            float uiDepth = uiCam ? uiCam->WorldToScreen(tf.v, uiScreenPos) : 0.0f;
            printf(
                "DC3 UIListMeshElement::Draw mesh=%s slot=%s mat=%s alphaIn=%.3f matAlpha=%.3f color=%s pos=(%.2f,%.2f,%.2f) basisX=(%.2f,%.2f,%.2f) basisY=(%.2f,%.2f,%.2f) basisZ=(%.2f,%.2f,%.2f) cam=%s screen=(%.3f,%.3f) depth=%.3f uiCam=%s uiScreen=(%.3f,%.3f) uiDepth=%.3f\n",
                PathName(mesh),
                mListMesh ? mListMesh->MatchName() : "<null>",
                PathName(mMat),
                f,
                alpha,
                col ? PathName(col) : "<null>",
                tf.v.x,
                tf.v.y,
                tf.v.z,
                tf.m.x.x,
                tf.m.x.y,
                tf.m.x.z,
                tf.m.y.x,
                tf.m.y.y,
                tf.m.y.z,
                tf.m.z.x,
                tf.m.z.y,
                tf.m.z.z,
                cam ? PathName(cam) : "<null>",
                screenPos.x,
                screenPos.y,
                depth,
                uiCam ? PathName(uiCam) : "<null>",
                uiScreenPos.x,
                uiScreenPos.y,
                uiDepth
            );
            sChooseModeMeshDiag++;
        }
#endif
        bool restoreShowing = false;
#ifdef HX_NATIVE
        if (!mesh->Showing()) {
            mesh->SetShowing(true);
            restoreShowing = true;
        }
#endif
        mesh->SetMat(mMat);
        mMat->SetAlpha(f * alpha);
        if (col != nullptr) {
            const Hmx::Color &c = col->GetColor();
            mMat->SetColor(c.red, c.green, c.blue);
        }
        mesh->DrawShowing();
        mMat->SetAlpha(alpha);
#ifdef HX_NATIVE
        if (restoreShowing) {
            mesh->SetShowing(false);
        }
#endif
    }
}

#pragma endregion UIListMeshElement
