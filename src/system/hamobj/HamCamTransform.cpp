#include "hamobj/HamCamTransform.h"
#include "flow/Flow.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Object.h"
#ifdef HX_NATIVE
#include "obj/Dir.h"
#endif
#include "rndobj/Anim.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "world/CameraShot.h"

HamCamTransform::HamCamTransform() : mAreas(this) {}

HamCamTransform::~HamCamTransform() {
#ifdef HX_NATIVE
    if (!ObjectDir::InDeleteObjects())
#endif
        ClearOldCrowds();
}

void HamCamTransform::Enter() { Setup(false); }

void HamCamTransform::ClearOldCrowds() {
    for (int i = 0; i != mAreas.size(); i++) {
        TransformArea &area = mAreas[i];
        if (area.mArea) {
            for (ObjPtrList<HamCamShot>::iterator sit = area.mCamshots.begin();
                 sit != area.mCamshots.end();
                 ++sit) {
                HamCamShot *shot = *sit;
                if (shot) {
                    shot->ClearCrowds();
                }
            }
        }
    }
}

INIT_REVS(3, 0)

BEGIN_COPYS(HamCamTransform)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(HamCamTransform)
    BEGIN_COPYING_MEMBERS
        mAreas.clear();
        for (int i = 0; i != c->mAreas.size(); i++) {
            mAreas.push_back(TransformArea(this, c->mAreas[i]));
        }
    END_COPYING_MEMBERS
END_COPYS

BinStream &operator>>(BinStreamRev &d, ObjVector<TransformArea> &areas) {
    int count;
    d.stream >> count;
    areas.resize(count);
    for (int i = 0; i < count; i++) {
        areas[i].Load(d);
    }
    return d.stream;
}

BEGIN_LOADS(HamCamTransform)
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    d >> mAreas;
END_LOADS

void HamCamTransform::Setup(bool update) {
    ClearOldCrowds();
    for (int i = 0; i != mAreas.size(); i++) {
        TransformArea &area = mAreas[i];
        if (area.mArea) {
            for (ObjPtrList<HamCamShot>::iterator sit = area.mCamshots.begin();
                 sit != area.mCamshots.end();
                 ++sit) {
                HamCamShot *shot = *sit;
                if (shot) {
                    shot->SetTransParent(area.mArea, false);
                    for (ObjPtrList<RndAnimatable>::iterator ait = area.mAnims.begin();
                         ait != area.mAnims.end();
                         ++ait) {
                        if (*ait) {
                            shot->AddAnim(*ait);
                        }
                    }
                    if (area.mFlow) {
                        area.mFlow->Activate();
                    }
                    bool hasCrowd = false;
                    for (ObjVector<TransformCrowd>::iterator cit = area.mCrowds.begin();
                         cit != area.mCrowds.end();
                         ++cit) {
                        CamShotCrowd crowd(shot);
                        crowd.mCamShot = shot;
                        crowd.mCrowd = cit->mCrowd;
                        crowd.mCrowdRotate = cit->mCrowdRotate;
                        shot->AddCrowd(crowd);
                        if (!hasCrowd && cit->mCrowd) {
                            hasCrowd = true;
                        }
                    }
                }
            }
        }
    }
    if (update && TheLoadMgr.EditMode()) {
        DataNode node(DataReadString("milo cur_anim"), kDataArray);
        node.Array(0)->Release();
        DataNode result = node.Array(0)->Execute(true);
        if (result.Type() == kDataObject) {
            HamCamShot *anim = result.Obj<HamCamShot>();
            if (anim) {
                anim->StartAnim();
                anim->SetFrame(anim->GetFrame(), 1.0f);
            }
        }
    }
}

BEGIN_HANDLERS(HamCamTransform)
    HANDLE_ACTION(update_camshots, Setup(true))
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(TransformCrowd)
    SYNC_PROP(crowd, o.mCrowd)
    SYNC_PROP(crowd_rotate, (int &)o.mCrowdRotate)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(TransformArea)
    SYNC_PROP(area, o.mArea)
    SYNC_PROP(camshots, o.mCamshots)
    SYNC_PROP(anims, o.mAnims)
    SYNC_PROP(crowds, o.mCrowds)
    SYNC_PROP(flow, o.mFlow)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(HamCamTransform)
    SYNC_PROP(areas, mAreas)
    SYNC_SUPERCLASS(RndPollable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(HamCamTransform)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mAreas;
END_SAVES

BinStream &operator<<(BinStream &bs, const TransformCrowd &c) {
    c.Save(bs);
    return bs;
}

void TransformCrowd::Save(BinStream &bs) const { bs << mCrowd << mCrowdRotate; }

BinStream &operator>>(BinStream &bs, TransformCrowd &c) {
    c.Load(bs);
    return bs;
}

void TransformCrowd::Load(BinStream &bs) {
    bs >> mCrowd;
    bs >> (BinStreamEnum<CrowdRotate> &)mCrowdRotate;
}

TransformArea::TransformArea(Hmx::Object *owner)
    : mArea(owner), mCamshots(owner), mAnims(owner), mCrowds(owner), mFlow(owner) {}

TransformArea::TransformArea(Hmx::Object *owner, const TransformArea &other)
    : mArea(other.mArea), mCamshots(other.mCamshots), mAnims(other.mAnims),
      mCrowds(other.mCrowds), mFlow(other.mFlow) {}

BinStream &operator<<(BinStream &bs, const TransformArea &a) {
    a.Save(bs);
    return bs;
}

void TransformArea::Save(BinStream &bs) const {
    bs << mArea;
    bs << mCamshots;
    bs << mAnims;
    bs << mCrowds;
    bs << mFlow;
}

BinStream &operator>>(BinStreamRev &d, TransformArea &a) {
    a.Load(d);
    return d.stream;
}

void TransformArea::Load(BinStreamRev &d) {
    d >> mArea;
    mCamshots.Load(d.stream, false, nullptr, true);
    d >> mAnims;
    if (d.rev > 1) {
        d >> mCrowds;
    }
    if (d.rev > 2) {
        d >> mFlow;
    }
}
