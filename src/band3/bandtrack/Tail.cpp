#include "bandtrack/Tail.h"
#include "bandtrack/GraphicsUtl.h"
#include "math/Mtx.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include <math.h>

Tail::Tail(GemRepTemplate &tmp)
    : mGroup(Hmx::Object::New<RndGroup>()), mTail1(Hmx::Object::New<RndMesh>()),
      mTail2(Hmx::Object::New<RndMesh>()), unk10(0), unk14("normal"), mSlot(-1),
      mTemplate(tmp), mTailGeomOwner(0), unk28(0), unk4e4(0), unk4e8(0), unk4ec(0),
      unk4f0(0), unk4f4(0) {
    mGroup->AddObject(mTail1);
    mTail1->SetTransParent(mGroup, false);
    mGroup->AddObject(mTail2);
    mTail2->SetTransParent(mGroup, false);
}

Tail::~Tail() {
    UnhookAllParents(mGroup);
    ReleaseMeshes();
    RELEASE(mGroup);
    RELEASE(mTail1);
    RELEASE(mTail2);
}

void Tail::Init(
    int i1,
    const Transform &tf,
    bool b3,
    Symbol s,
    RndGroup *grp,
    const Tail::SlideInfo &info,
    Tail *tail
) {
    mState = 0;
    mSlot = i1;
    mSlideInfo = info;
    if (mSlideInfo.unk0) {
        static float severity = 3.5f;
        mInterpolator.Reset(
            Vector2(mSlideInfo.unk4, 0), Vector2(mSlideInfo.unk8, mSlideInfo.unkc), severity
        );
    }
    ConfigureMeshes(tail);
    SetType(s, b3);
    mGroup->SetLocalXfm(tf);
    grp->AddObject(mGroup);
    mWhammy.Clear();
    unk4e4 = 0;
    unk4e8 = 0;
    unk4e0 = 0;
    unk4ec = 0;
    unk4f0 = 0;
    unk4f4 = false;
    UpdateVerts(mTemplate.kTailMinAlpha, false);
}

void Tail::MoveSlot(const Transform &tf) {
    if (mGroup) {
        Transform tf38(tf);
        tf38.v.y = mGroup->LocalXfm().v.y;
        mGroup->SetLocalXfm(tf38);
    }
}

void Tail::SetType(Symbol s, bool b) {
    unk14 = s;
    if (unk14 == "unison") {
        unk14 = "star";
    }
    bool isStar = unk14 == "star";
    RndMat *tailMat;
    if (mState == 1) {
        tailMat = mTemplate.GetTailMiss();
    } else if (isStar) {
        tailMat = mTemplate.GetTailBonus();
    } else if (b) {
        tailMat = mTemplate.GetTailChord();
    } else {
        tailMat = mTemplate.GetSlotMat(0, mSlot);
    }
    MILO_ASSERT(tailMat, 0x8A);
    mTail1->SetMat(tailMat);
    mTail2->SetMat(tailMat);
    mTail1->SetShowing(unk14 != "invisible");
    mTail2->SetShowing(false);
    if (mState == 2)
        Hit();
}

void Tail::ConfigureMeshes(Tail *tail) {
    if (tail) {
        mTailGeomOwner = tail->mTailGeomOwner;
        unk28 = false;
    } else {
        mTailGeomOwner = mTemplate.GetTail();
        unk28 = true;
    }
    MILO_ASSERT(mTailGeomOwner, 0xAC);
    mTail1->SetGeomOwner(mTailGeomOwner);
    mTail2->SetGeomOwner(mTailGeomOwner);
    Hmx::Matrix3 m38;
    m38.Identity();
    m38.x.x = -1.0f;
    mTail2->SetLocalRot(m38);
}

void Tail::ReleaseMeshes() {
    if (unk28)
        mTemplate.ReturnTail(mTailGeomOwner);
    mTailGeomOwner = nullptr;
    mTail1->SetGeomOwner(mTail1);
    mTail2->SetGeomOwner(mTail2);
    mTail1->SetShowing(false);
    mTail2->SetShowing(false);
}

void Tail::SetDuration(float f1, float f2, float f3) {
    if (mState != 4) {
        if (mState == 2) {
            unk10 = Max(f1, unk10);
        } else {
            unk10 = Max(f2, unk10);
        }
        unk10 = Min(unk10, f3);
        unk4ec = f3 - unk10;
    }
}

void Tail::Hit() {
    mState = 2;
    if (!mSlideInfo.unk0) {
        mTail2->SetShowing(true);
    }
    if (unk28)
        mWhammy.Clear();
}

void Tail::Release() {
    if (mState != 4) {
        mState = 3;
        HandleMistake();
    }
}

void Tail::Done() {
    if (mState == 2) {
        mState = 4;
        unk10 = 0;
        unk4ec = 0;
        mTail1->SetShowing(false);
        mTail2->SetShowing(false);
    }
}

void Tail::HandleMistake() { mTail2->SetShowing(false); }

void Tail::Poll(float, float whammy, float) {
    if (mTailGeomOwner) {
        bool t3 = mState == 2 && !mSlideInfo.unk0;
        float fvar1 = t3 ? mTemplate.kTailOffsetX * mTemplate.GetTailScaleX() : 0;
        mTail1->SetLocalPos(-fvar1, unk10, 0);
        mTail2->SetLocalPos(fvar1, unk10, 0);
        if (unk28) {
            float alpha;
            if (t3) {
                float delta = TheTaskMgr.DeltaSeconds();
                static float pulseRate = 1.0f / mTemplate.kTailPulseRate;
                for (float time = TheTaskMgr.Seconds(TaskMgr::kRealTime) - delta;
                     time < TheTaskMgr.Seconds(TaskMgr::kRealTime);
                     time += pulseRate) {
                    GemRepTemplate *tmp = &mTemplate;
                    unk4e4 = Interp(unk4e4, whammy, tmp->kTailPulseSmoothing);
                    float negWhammy = -unk4e4;
                    float ampMin = tmp->kTailAmplitudeRange.x;
                    float f4 = Interp(
                        tmp->kTailFrequencyRange.x,
                        tmp->kTailFrequencyRange.y,
                        negWhammy
                    );
                    mWhammy.Set(
                        Interp(ampMin, tmp->kTailAmplitudeRange.y, negWhammy)
                        * sinf(unk4e0)
                    );
                    unk4e0 += pulseRate * f4;
                }
                unk4e8 = Interp(unk4e8, whammy, mTemplate.kTailAlphaSmoothing);
                alpha = Interp(mTemplate.kTailMinAlpha, mTemplate.kTailMaxAlpha, -unk4e8);
            } else {
                unk4e0 = 0;
                alpha = mTemplate.kTailMinAlpha;
                unk4e8 = 0;
            }

            if (unk4ec != unk4f0 || t3 || mSlideInfo.unk0 || t3 != unk4f4) {
                UpdateVerts(alpha, t3);
                unk4f0 = unk4ec;
                unk4f4 = t3;
            }
        }
    }
}

void Tail::UpdateVerts(float alpha, bool active) {
    if (!unk28) return;

    int tailFlag = 0;
    if (active || mSlideInfo.unk0) tailFlag = 1;
    GemRepTemplate::TailType tailType = (GemRepTemplate::TailType) !tailFlag;
    float scaleX = mTemplate.mTailScaleX;
    int total_sections = mTemplate.GetNumTailSections(tailType);
    float sectionLen = mTemplate.GetTailSectionLength(tailType);
    float clamped = Clamp<float>(0, mTemplate.kTailMaxLength, unk4ec);
    float capLen = Clamp<float>(0, mTemplate.mTailSectionLength[0], clamped);
    int capInc = capLen > 0;
    float midLen = clamped - capLen;
    int midSections = 0;
    float midStart = 0;
    if (midLen > 0) {
        midSections = (int) (float) ceil(midLen / sectionLen);
        midStart = -(((float) (midSections - 1) * sectionLen) - midLen);
    }
    int used_sections = capInc + midSections;
    MILO_ASSERT(used_sections <= total_sections, 0x1C3);

    GemRepTemplate &templ = mTemplate;
    int vertCount = templ.GetRequiredVertCount(used_sections);
    bool resized = false;
    RndMesh::VertVector &verts = mTailGeomOwner->Verts();
    if (vertCount != verts.size()) {
        verts.resize(vertCount);
        resized = true;
    }

    RndMesh::Vert *tailBegin = &templ.mTailVerts[0];
    RndMesh::Vert *out = verts.begin();
    RndMesh::Vert *tailEnd = &templ.mTailVerts[templ.mTailVerts.size()];
    float yWorld = unk10;
    float curY = 0;

    float baseOfs = 0;
    if (active) {
        baseOfs = baseOfs + mWhammy[0];
    }
    if (mSlideInfo.unk0) {
        baseOfs += mInterpolator.Eval(yWorld);
    }

    float zScale = 1.0f;
    for (RndMesh::Vert *src = tailBegin; src != tailEnd; ++src, ++out) {
        out->pos.y = 0;
        out->pos.x = scaleX * (src->pos.x + baseOfs);
        out->pos.z = src->pos.z * zScale;
        out->tex.x = src->tex.x;
        out->tex.y = src->tex.y;
    }

    curY += midStart;
    yWorld += midStart;

    for (int i = 0; i < midSections; ) {
        float ofs = 0;
        if (active) {
            int idx = (int) (0.5f * curY);
            ofs += mWhammy[idx];
        }
        if (mSlideInfo.unk0) {
            ofs += mInterpolator.Eval(yWorld);
        }
        for (RndMesh::Vert *src = tailBegin; src != tailEnd; ++src, ++out) {
            out->pos.y = curY;
            out->pos.x = scaleX * (src->pos.x + ofs);
            out->pos.z = src->pos.z * zScale;
            out->tex.x = src->tex.x;
            out->tex.y = src->tex.y;
        }
        ++i;
        if (i == midSections) break;
        curY += sectionLen;
        yWorld += sectionLen;
    }

    if (capLen > 0) {
        curY += capLen;
        yWorld += capLen;
        float ofs = 0;
        if (active) {
            int idx = (int) (0.5f * curY);
            ofs += mWhammy[idx];
        }
        if (mSlideInfo.unk0) {
            ofs += mInterpolator.Eval(yWorld);
        }
        RndMesh::Vert *csrc = &templ.mCapVerts[0];
        RndMesh::Vert *cend = &templ.mCapVerts[templ.mCapVerts.size()];
        for (; csrc != cend; ++csrc, ++out) {
            out->pos.y = curY;
            out->pos.x = scaleX * (csrc->pos.x + ofs);
            out->pos.z = csrc->pos.z * zScale;
            out->tex.x = csrc->tex.x;
            out->tex.y = csrc->tex.y;
        }
    }
    MILO_ASSERT(out == verts.end(), 0x219);

    int syncFlags = 0x9F;
    if (resized) {
        std::vector<RndMesh::Face> &faces = mTailGeomOwner->Faces();
        RndMesh::Face zeroFace;
        zeroFace.v1 = 0; zeroFace.v2 = 0; zeroFace.v3 = 0;
        int faceCount = mTemplate.GetRequiredFaceCount(used_sections);
        if ((unsigned int) faceCount < (unsigned int) faces.size()) {
            faces.erase(faces.begin() + faceCount, faces.end());
        } else {
            faces.insert(faces.end(), faceCount - faces.size(), zeroFace);
        }
        unsigned short v = 0;
        for (int i = 0; i < used_sections; i++) {
            faces[i*2].Set(v, v + 1, v + 2);
            faces[i*2 + 1].Set(v + 2, v + 1, v + 3);
            v += 2;
        }
        syncFlags |= 0x20;
    }
    mTailGeomOwner->Sync(syncFlags);
}
