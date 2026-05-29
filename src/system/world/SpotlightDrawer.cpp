#include "world/SpotlightDrawer.h"
#include "char/Character.h"
#include "math/Geo.h"
#include "math/Key.h"
#include "obj/Object.h"
#include "os/Platform.h"
#include "os/System.h"
#include "rndobj/BoxMap.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/Rnd.h"
#include "rndobj/Stats_NG.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "world/Spotlight.h"

RndEnviron *SpotlightDrawer::sEnviron;
SpotlightDrawer *SpotlightDrawer::sDefault;
int SpotlightDrawer::sNeedBoxMap = -1;
bool SpotlightDrawer::sHaveAdditionals;
bool SpotlightDrawer::sHaveLenses;
bool SpotlightDrawer::sHaveFlares;
std::vector<SpotlightDrawer::SpotlightEntry> SpotlightDrawer::sLights;
std::vector<SpotlightDrawer::SpotMeshEntry> SpotlightDrawer::sCans;
std::vector<Spotlight *> SpotlightDrawer::sShadowSpots;
bool SpotlightDrawer::sNoBeams;
#ifdef HX_NATIVE
SpotlightDrawer *SpotlightDrawer::sCurrent;
bool SpotlightDrawer::sNeedDraw;
#endif

SpotlightDrawer::SpotlightDrawer() : mParams(this) { mOrder = -100000; }

SpotlightDrawer::~SpotlightDrawer() {
    if (sCurrent == this) {
        DeSelect();
        ClearAndShrink(sLights);
        ClearAndShrink(sShadowSpots);
        ClearAndShrink(sCans);
    }
}

BEGIN_HANDLERS(SpotlightDrawer)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_ACTION(select, Select())
    HANDLE_ACTION(deselect, DeSelect())
END_HANDLERS

BEGIN_PROPSYNCS(SpotlightDrawer)
    SYNC_PROP(total, mParams.mIntensity)
    SYNC_PROP(base_intensity, mParams.mBaseIntensity)
    SYNC_PROP(smoke_intensity, mParams.mSmokeIntensity)
    SYNC_PROP(color, mParams.mColor)
    SYNC_PROP(proxy, mParams.mProxy)
    SYNC_PROP(light_influence, mParams.mLightingInfluence)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_COPYS(SpotlightDrawer)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY_AS(SpotlightDrawer, c)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mParams)
    END_COPYING_MEMBERS
END_COPYS

SpotDrawParams &SpotDrawParams::operator=(const SpotDrawParams &other) {
    mIntensity = other.mIntensity;
    mBaseIntensity = other.mBaseIntensity;
    mSmokeIntensity = other.mSmokeIntensity;
    mHalfDistance = other.mHalfDistance;
    mLightingInfluence = other.mLightingInfluence;
    mColor = other.mColor;
    mTexture = other.mTexture;
    mProxy = other.mProxy;
    return *this;
}

SpotDrawParams::SpotDrawParams(SpotlightDrawer *owner)
    : mIntensity(1.0f), mColor(1.0f, 1.0f, 1.0f), mBaseIntensity(0.1f),
      mSmokeIntensity(0.5f), mHalfDistance(250.0f), mLightingInfluence(1.0f),
      mTexture(owner, 0), mProxy(owner, 0), mOwner(owner) {
    MILO_ASSERT(owner, 0x37c);
}

void SpotDrawParams::Save(BinStream &bs) {
    bs << mIntensity;
    bs << mBaseIntensity;
    bs << mSmokeIntensity;
    bs << mHalfDistance;
    bs << mColor;
    bs << mTexture;
    bs << mProxy;
    bs << mLightingInfluence;
}

BEGIN_SAVES(SpotlightDrawer)
    SAVE_REVS(6, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    mParams.Save(bs);
END_SAVES

void SpotlightDrawer::Init() {
    sEnviron = Hmx::Object::New<RndEnviron>();
    sEnviron->SetUseApproxes(false);
    REGISTER_OBJ_FACTORY(SpotlightDrawer)
    SpotlightDrawer* ptr = Hmx::Object::New<SpotlightDrawer>();
    ptr->mParams.mLightingInfluence = 0.0f;
    sDefault = ptr;
    ptr->Select();
}

void SpotlightDrawer::Select() {
    if (sCurrent != this) {
        if (sCurrent) {
            TheRnd.UnregisterPostProcessor(sCurrent);
        }
        sCurrent = this;
        TheRnd.RegisterPostProcessor(this);
    }
    sNeedBoxMap = -1;
}

void SpotlightDrawer::ListDrawChildren(std::list<RndDrawable *> &draws) {
    draws.push_back(mParams.mProxy);
}

void SpotlightDrawer::DrawMeshVec(std::vector<SpotMeshEntry> &entries) {
    if (entries.size() != 0) {
        std::vector<SpotMeshEntry>::iterator it = entries.begin();
        RndMesh *canMesh = it->mCanMesh;
        RndMultiMesh *multiMesh = canMesh->CreateMultiMesh();
        multiMesh->Instances().push_back(RndMultiMesh::Instance(it->mTransform));
        RndMesh *envMesh = it->mEnvMesh;
        reinterpret_cast<RndHighlightable *>(envMesh)->Highlight();
        std::vector<SpotMeshEntry>::iterator itEnd = entries.end();
        for (++it; it != itEnd; ++it) {
            bool envChanged = it->mEnvMesh != envMesh;
            bool canChanged = it->mCanMesh != canMesh;
            if (envChanged || canChanged) {
                multiMesh->DrawShowing();
                if (envChanged && envMesh) {
                    envMesh = it->mEnvMesh;
                    reinterpret_cast<RndHighlightable *>(envMesh)->Highlight();
                }
                if (canChanged) {
                    canMesh = it->mCanMesh;
                    multiMesh = canMesh->CreateMultiMesh();
                }
            }
            multiMesh->Instances().push_back(RndMultiMesh::Instance(it->mTransform));
        }
        multiMesh->DrawShowing();
    }
}

void SpotlightDrawer::DrawBeams(
    SpotlightDrawer::SpotlightEntry *spotIter,
    SpotlightDrawer::SpotlightEntry *const &spotEnd
) {
    MILO_ASSERT(spotIter != spotEnd, 0x2c7);
    for (; spotIter != spotEnd; ++spotIter) {
        Spotlight *sl = spotIter->mSpotlight;
        Spotlight::BeamDef &def = sl->mBeam;
        if (def.mBeam) {
            MILO_ASSERT(def.mBeam->Showing(), 0x2e4);
            def.mBeam->DrawShowing();
        }
    }
}

void SpotlightDrawer::DrawFlares(
    SpotlightDrawer::SpotlightEntry *spotIter,
    SpotlightDrawer::SpotlightEntry *const &spotEnd
) {
    MILO_ASSERT(spotIter != spotEnd, 0x2f4);
    for (; spotIter != spotEnd; ++spotIter) {
        Spotlight *sl = spotIter->mSpotlight;
        if (sl->GetFlare() && sl->GetFlare()->GetMat()) {
            sl->GetFlare()->Draw();
        }
    }
}

void SpotlightDrawer::DrawAdditional(
    SpotlightDrawer::SpotlightEntry *spotIter,
    SpotlightDrawer::SpotlightEntry *const &spotEnd
) {
    MILO_ASSERT(spotIter != spotEnd, 0x298);
    for (; spotEnd != spotIter; ++spotIter) {
        Spotlight *sl = spotIter->mSpotlight;
        auto _tmp0 = sl->GetAdditionalObjects();
        FOREACH (it, _tmp0) {
            RndDrawable *add = *it;
            MILO_ASSERT(add != sl, 0x2a3);
            if (add != sl)
                add->Draw();
        }
    }
}

void SpotlightDrawer::DrawLenses(
    SpotlightDrawer::SpotlightEntry *spotIter,
    SpotlightDrawer::SpotlightEntry *const &spotEnd
) {
    MILO_ASSERT(spotIter != spotEnd, 0x2b1);
    for (; spotEnd != spotIter; ++spotIter) {
        Spotlight *sl = spotIter->mSpotlight;
        if (Spotlight::sDiskMesh) {
            MILO_ASSERT(sl->LensMesh(), 0x2b9);
            Spotlight::sDiskMesh->SetMat(sl->LensMesh());
            Spotlight::sDiskMesh->Draw();
        }
    }
}

void SpotlightDrawer::SortLights() {
    if (sLights.size() > 2) {
        std::sort(sLights.begin(), sLights.end(), ByColor());
    }
    if (sCans.size() > 2) {
        std::sort(sCans.begin(), sCans.end(), ByEnvMesh());
    }
}

void SpotlightDrawer::ClearPostDraw() {
    ClearLights();
    sNeedDraw = false;
}

void SpotlightDrawer::DrawShowing() {
    if (sCurrent && sCurrent != sDefault && sCurrent != this) {
        MILO_NOTIFY_ONCE(
            "Drawing 2 spotlightdrawers in one frame, %s and %s",
            PathName(sCurrent),
            PathName(this)
        );
    } else {
        Select();
    }
}

void SpotlightDrawer::SetAmbientColor(const Hmx::Color &c) {
    sEnviron->SetAmbientColor(c);
    sEnviron->Select(nullptr);
}

void SpotlightDrawer::RemoveFromLists(Spotlight *spot) {
    for (std::vector<SpotlightEntry>::iterator it = sLights.begin(); it != sLights.end();) {
        if (it->mSpotlight == spot) {
            it = sLights.erase(it);
        } else {
            ++it;
        }
    }
    for (std::vector<SpotMeshEntry>::iterator it = sCans.begin(); it != sCans.end();) {
        if (it->mSpotlight == spot) {
            it = sCans.erase(it);
        } else {
            ++it;
        }
    }
    for (std::vector<Spotlight *>::iterator it = sShadowSpots.begin();
         it != sShadowSpots.end();) {
        if (*it == spot) {
            it = sShadowSpots.erase(it);
        } else {
            ++it;
        }
    }
}

void SpotlightDrawer::DrawLight(Spotlight *spot) {
    if (!spot)
        return;

    const Hmx::Color& color = spot->Color();
    float intensity = spot->Intensity();

    float baseR = color.red * intensity * 255.0f;
    float baseG = color.green * intensity * 255.0f;
    float baseB = color.blue * intensity * 255.0f;

    uint packedColor = ((uint)baseB & 0xff) << 16 | ((uint)baseG & 0xff) << 8 | (uint)baseR & 0xff;

    bool shouldProcess = (baseR > 5) || ((baseG > 3) || (baseB > 7));

    if (shouldProcess && spot->GetTarget()) {
        GfxMode gfxMode = GetGfxMode();

        if (gfxMode == kOldGfx && spot->TargetShadow()) {
            sShadowSpots.push_back(spot);
        }

        SpotlightEntry entry;
        entry.mColorKey = packedColor;
        entry.mSpotlight = spot;
        sLights.push_back(entry);

        if (sHaveAdditionals || spot->GetAdditionalObjects().size() > 0) {
            sHaveAdditionals = true;
        }

        if (sHaveFlares && (!spot->GetFlare() || spot->mFlareOffset == 0)) {
            sHaveFlares = false;
        } else {
            sHaveFlares = true;
        }

        if (sHaveLenses || spot->LensMesh()) {
            sHaveLenses = true;
        }

        if (sNeedBoxMap == (int)TheRnd.GetFrameID()) {
            static bool boxMapLogged = false;
            if (!boxMapLogged) {
                boxMapLogged = true;
                const char* objName = PathName(spot);
                MILO_WARN("%s drawn after SpotlightEnder", objName);
            }
        }

        sNeedDraw = true;
    }

    RndMesh* lightCanMesh = spot->mLightCanMesh;
    if (lightCanMesh && !spot->mLightCanSort) {
        const Transform& xfm = spot->WorldXfm();
        float nearDist = spot->mLightCanOffset;
        if (nearDist <= 0.0f) {
            SpotMeshEntry meshEntry;
            meshEntry.mCanMesh = lightCanMesh;
            meshEntry.mEnvMesh = nullptr;
            meshEntry.mSpotlight = spot;
            meshEntry.mTransform = xfm;
            sCans.push_back(meshEntry);
            sNeedDraw = true;
        }
    }
}

bool SpotlightDrawer::DrawNGSpotlights() {
    return GetGfxMode() == kNewGfx && TheLoadMgr.GetPlatform() != kPlatformPC;
}

void SpotlightDrawer::DeSelect() {
    if (sCurrent != this)
        return;
    if (sDefault != this) {
        sDefault->Select();
    } else {
        PostProcessor *pp = sCurrent ? static_cast<PostProcessor *>(sCurrent) : nullptr;
        TheRnd.UnregisterPostProcessor(pp);
        sCurrent = nullptr;
    }
}

void SpotlightDrawer::ApplyLightingApprox(BoxMapLighting &boxMap, float f2) const {
    MILO_ASSERT(boxMap.NumQueuedLights() == 0, 0x20b);
    std::vector<SpotlightEntry>::iterator it = sLights.begin();
    std::vector<SpotlightEntry>::iterator itEnd = sLights.end();
    for (; it != itEnd; ++it) {
        Spotlight *curSpotlight = it->mSpotlight;
        const Transform &xfm = curSpotlight->WorldXfm();
        Hmx::Color c50(curSpotlight->Color());
        Multiply(c50, f2, c50);
        Multiply(c50, curSpotlight->Intensity(), c50);
        BoxMapLighting::LightParams_Spot *params;
        if (!boxMap.ParamsAt(params))
            break;
        params->mPosition = xfm.v;
        params->mDirection = xfm.m.y;
        params->mColor = c50;
        params->mTopRadius = curSpotlight->mBeam.mTopRadius;
        params->mBottomRadius = curSpotlight->mBeam.mBottomRadius * 2.0f;
        params->mBeamLength = curSpotlight->mBeam.mLength * 2.0f;
        boxMap.CacheData(*params);
    }
}

void SpotlightDrawer::DrawShadow() {
    std::vector<Spotlight *>::iterator it = sShadowSpots.begin();
    std::vector<Spotlight *>::iterator itEnd = sShadowSpots.end();
    for (; it != itEnd; ++it) {
        Spotlight *shadowSpot = *it;
        MILO_ASSERT(shadowSpot->GetTarget() && shadowSpot->TargetShadow(), 0x288);
        RndDrawable *draw = dynamic_cast<RndDrawable *>(shadowSpot->GetTarget());
        if (draw) {
            draw->DrawShadow(shadowSpot->WorldXfm(), 1.5f);
        }
    }
}

void SpotlightDrawer::UpdateBoxMap() {
    if ((unsigned int)sNeedBoxMap != TheRnd.GetFrameID()) {
        RndEnviron::sGlobalLighting.Clear();
        float lightingInf = mParams.mLightingInfluence;
        if (lightingInf > 0) {
            ApplyLightingApprox(RndEnviron::sGlobalLighting, lightingInf);
        }
        sNeedBoxMap = TheRnd.GetFrameID();
    }
}

void SpotDrawParams::Load(BinStreamRev &d) {
    d >> mIntensity;
    if (d.rev > 3) {
        d >> mBaseIntensity >> mSmokeIntensity >> mHalfDistance;
    } else {
        float i, j, k, l;
        d >> i >> j >> k >> l;
        if (k < 0.5f) {
            mSmokeIntensity = 0.5f;
            mBaseIntensity = 0.1f;
        } else {
            mBaseIntensity = 0.15f;
            mSmokeIntensity = 1.0f;
        }
    }
    d >> mColor;
    if (d.rev < 4) {
        int a;
        Key<float> b, c;
        d >> a;
        d.stream >> b;
        d.stream >> c;
    }
    d >> mTexture;
    d >> mProxy;
    if (d.rev < 3) {
        bool b;
        d >> b;
    }
    if (d.rev > 4)
        d >> mLightingInfluence;
}

INIT_REVS(6, 0)

BEGIN_LOADS(SpotlightDrawer)
    LOAD_REVS(bs)
    ASSERT_REVS(6, 0)
    if (d.rev > 0) {
        if (d.rev > 5) {
            Hmx::Object::Load(d.stream);
        }
        RndDrawable::Load(d.stream);
    } else {
        Hmx::Object::Load(d.stream);
    }
    mOrder = -100000;
    mParams.Load(d);
END_LOADS

class LensExtract {};

template <class T>
void DrawAccessories(
    SpotlightDrawer::SpotlightEntry *const &,
    SpotlightDrawer::SpotlightEntry *const &
);

template <>
void DrawAccessories<LensExtract>(
    SpotlightDrawer::SpotlightEntry *const &spotBegin,
    SpotlightDrawer::SpotlightEntry *const &spotEnd
) {
    SpotlightDrawer::SpotlightEntry *it = spotBegin;
    RndMat *curMat = nullptr;
    RndMesh *curDisk = nullptr;
    RndMultiMesh *multiMesh = nullptr;
    if (it == spotEnd)
        return;
    do {
        Spotlight *sl = it->mSpotlight;
        if (sl->LensMesh() != nullptr) {
            RndMesh *disk = Spotlight::GetDiskMesh();
            RndMultiMesh *nextMesh;
            if (disk != curDisk) {
                nextMesh = disk->CreateMultiMesh();
            } else {
                nextMesh = multiMesh;
            }
            const Transform &lensXfm = sl->LensXfm();
            bool visible;
            if (!disk->Showing()) {
                visible = false;
            } else {
                Sphere sphere = disk->GetSphere();
                if (sphere.radius > 0.0f) {
                    Multiply(sphere, lensXfm, sphere);
                    visible = !(sphere > RndCam::Current()->WorldFrustum());
                } else {
                    visible = true;
                }
            }
            if (visible) {
                bool diskChanged = (curDisk != disk);
                RndMat *lensMat = sl->LensMesh();
                bool matChanged = (curMat != lensMat);
                if ((diskChanged || matChanged) && multiMesh != nullptr
                    && !multiMesh->Instances().empty()) {
                    multiMesh->DrawShowing();
                    multiMesh->Instances().resize(0, RndMultiMesh::Instance());
                }
                if (diskChanged) {
                    curDisk = disk;
                    nextMesh = disk->CreateMultiMesh();
                }
                if (matChanged || diskChanged) {
                    curMat = lensMat;
                    curDisk->SetMat(lensMat);
                }
                RndMultiMesh::Instance inst(lensXfm);
                nextMesh->Instances().insert(nextMesh->Instances().end(), inst);
                multiMesh = nextMesh;
            }
        }
        ++it;
    } while (it != spotEnd);
    if (multiMesh != nullptr && !multiMesh->Instances().empty()) {
        multiMesh->DrawShowing();
        multiMesh->Instances().resize(0, RndMultiMesh::Instance());
    }
}

void SpotlightDrawer::DrawWorld() {
#ifdef HX_NATIVE
    int numLights = sLights.size();
    if (numLights < TheNgStats->mSpotlights) {
        numLights = TheNgStats->mSpotlights;
    }
    TheNgStats->mSpotlights = numLights;
#endif
    if ((!sLights.empty() || !sCans.empty()) && Showing()) {
        SortLights();
        DrawMeshVec(sCans);
        sCans.resize(0);
        RndEnviron *cur = RndEnviron::sCurrent;
        if (!sLights.empty()) {
            Vector3 *pos = RndEnviron::CurrentPos();
            MILO_ASSERT(sEnviron->GetUseApprox() == false, 0x1dc);
            sEnviron->Select(nullptr);
            if (GetGfxMode() == kOldGfx) {
                DrawShadow();
            }
            std::vector<SpotlightEntry>::iterator itEnd = sLights.end();
            if (sLights.begin() != itEnd) {
                std::vector<SpotlightEntry>::iterator it = sLights.begin();
                do {
                    Spotlight *spot = it->mSpotlight;
                    Hmx::Color c;
                    float intensity = spot->Intensity();
                    c.Set(
                        spot->Color().red * intensity,
                        spot->Color().green * intensity,
                        spot->Color().blue * intensity,
                        1.0f
                    );
                    const SpotlightEntry *e1 = &(*it);
                    const SpotlightEntry *e2 = &(*it) + 1;
                    for (; e2 != &(*itEnd); ++e2) {
                        if (e2->mColorKey != it->mColorKey)
                            break;
                    }
                    SetAmbientColor(c);
                    if (sHaveAdditionals) {
                        DrawAdditional(
                            const_cast<SpotlightEntry *>(e1),
                            const_cast<SpotlightEntry *const &>(e2)
                        );
                    }
                    if (sHaveLenses) {
                        DrawAccessories<LensExtract>(
                            const_cast<SpotlightEntry *>(e1),
                            const_cast<SpotlightEntry *const &>(e2)
                        );
                    }
                    if (!DrawNGSpotlights() && !sNoBeams
                        && TheRnd.GetDrawMode() != Rnd::kDrawOcclusionDepth) {
                        DrawBeams(
                            const_cast<SpotlightEntry *>(e1),
                            const_cast<SpotlightEntry *const &>(e2)
                        );
                    }
                    if (sHaveFlares) {
                        DrawFlares(
                            const_cast<SpotlightEntry *>(e1),
                            const_cast<SpotlightEntry *const &>(e2)
                        );
                    }
                    it = sLights.begin()
                        + (e2 - &(*sLights.begin()));
                } while (it != itEnd);
            }
            if (cur) {
                cur->Select(pos);
            }
        }
    }
}

void SpotlightDrawer::ClearLights() {
    sLights.resize(0);
    sShadowSpots.resize(0);
    sCans.resize(0);
    sHaveAdditionals = false;
    sHaveLenses = false;
    sHaveFlares = false;
}

void SpotlightDrawer::EndWorld() {
    UpdateBoxMap();
    if (sNeedDraw) {
        DrawWorld();
        ClearPostDraw();
    }
    if (TheRnd.DisablePP()) {
        ClearLights();
    }
    MILO_ASSERT(!sNeedDraw, 0x165);
}

#ifndef HX_NATIVE
// Manual specialization for single-element erase to match target memcpy codegen
// Target uses pointer comparison, then loop with dst in r3 and src = dst + 0x50
namespace stlpmtx_std {
typedef SpotlightDrawer::SpotMeshEntry SpotMeshEntry_;
template <>
SpotMeshEntry_* vector<SpotMeshEntry_, StlNodeAlloc<SpotMeshEntry_>>::_M_erase(
    SpotMeshEntry_* __pos,
    const __false_type&
) {
    SpotMeshEntry_* __next = __pos + 1;
    if (__next != this->_M_finish) {
        int __count = (this->_M_finish - __next) / 0x50;
        SpotMeshEntry_* __dst = __pos;
        do {
            SpotMeshEntry_* __src = __dst + 1;
            memcpy(__dst, __src, 0x50);
            __count--;
            __dst = __src;
        } while (__count != 0);
    }

    this->_M_finish--;
    return __pos;
}
}  // namespace stlpmtx_std
#endif
