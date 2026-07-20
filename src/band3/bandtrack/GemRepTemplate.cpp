#include "GemRepTemplate.h"

#ifdef HX_NATIVE
#include <cstdio>
#include <cstdlib>
#include "game/BandUser.h"
#endif

#include "bandtrack/GraphicsUtl.h"
#include "bandtrack/TrackConfig.h"
#include "beatmatch/TrackType.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"
#include <algorithm>

// needs to be an enum for value names to show in asserts
enum {
    kNumGemSlotMats = 1
};

GemRepTemplate::GemRepTemplate(const TrackConfig &tc)
    : mConfig(SystemConfig("track_graphics", "gem")),
      kTailPulseRate(mConfig->FindFloat("tail_pulse_rate")),
      kTailPulseSmoothing(mConfig->FindFloat("tail_pulse_smoothing")),
      kTailOffsetX(mConfig->FindFloat("tail_offset_x")),
      kTailMinAlpha(mConfig->FindFloat("tail_min_alpha")),
      kTailMaxAlpha(mConfig->FindFloat("tail_max_alpha")),
      kTailAlphaSmoothing(mConfig->FindFloat("tail_alpha_smoothing")),
      kTailFadeDistance(mConfig->FindFloat("tail_fade_distance")),
      kTailMaxLength(mConfig->FindFloat("tail_max_length")),
      kTailFrequencyRange(
          mConfig->FindFloat("tail_min_freq"), mConfig->FindFloat("tail_max_freq")
      ),
      kTailAmplitudeRange(
          mConfig->FindFloat("tail_min_amp"), mConfig->FindFloat("tail_max_amp")
      ),
      mTrackCfg(tc), mTailClipY(0), mTailScaleX(1.0f), mObjectDir(NULL) {
    mSlotMats = new RndMat *[tc.GetMaxSlots()];
#ifdef HX_NATIVE
    {
        // Stomp-watch (RB3_STATS_DBG): label these non-RndMesh VertVectors so
        // the Mesh.cpp vert-alloc ring can attribute their buffers by name.
        extern void RB3StompLabelVV(void *, const char *);
        RB3StompLabelVV(&mTailVerts, "GemRepTemplate.mTailVerts");
        RB3StompLabelVV(&mCapVerts, "GemRepTemplate.mCapVerts");
    }
#endif
}

GemRepTemplate::~GemRepTemplate() {
    DeleteAll(mTails);
    delete[] mSlotMats;
}

void GemRepTemplate::Init(ObjectDir *dir) {
    mObjectDir = dir;
    int maxslots = mTrackCfg.GetMaxSlots();
#ifdef HX_NATIVE
    if (getenv("GEM_DBG")) {
        fprintf(stderr,
                "[GEM_DBG] GemRepTemplate::Init: dir=%p maxslots=%d trackSym='%s'\n",
                (void*)dir, maxslots,
                mTrackCfg.GetBandUser()->GetTrackSym().Str());
    }
#endif
    for (int i = 0; i < maxslots; i++) {
        SetSlotMat(0, i, GetMatByTag("tail", i));
    }
    mTailMiss = mObjectDir->Find<RndMat>("tail_miss.mat", true);
    mTailBonus = mObjectDir->Find<RndMat>("tail_bonus.mat", true);
    mTailChord = mObjectDir->Find<RndMat>("tail_chord.mat", true);
    TrackType ty = mTrackCfg.GetBandUser()->GetTrackType();
    Symbol clipPropSym(gNullStr);
    Symbol scalePropSym(gNullStr);
    switch (ty) {
    case kTrackRealKeys:
        clipPropSym = real_keys_sustain_y_pos;
        scalePropSym = real_keys_sustain_x_scale;
        break;
    case kTrackRealGuitar:
    case kTrackRealBass:
        clipPropSym = real_guitar_sustain_y_pos;
        scalePropSym = real_guitar_sustain_x_scale;
        break;
    default:
        clipPropSym = guitar_sustain_y_pos;
        scalePropSym = guitar_sustain_x_scale;
        break;
    }
    MILO_ASSERT(clipPropSym != gNullStr, 0x6B);
    MILO_ASSERT(scalePropSym != gNullStr, 0x6C);
    mTailClipY = mObjectDir->Property(clipPropSym, true)->Float();
    mTailScaleX = mObjectDir->Property(scalePropSym, true)->Float();
    SetupTailVerts();
}

RndMesh *GemRepTemplate::GetTail() {
    RndMesh *m;
    if (mTails.empty())
        m = CreateTail();
    else {
        m = mTails.back();
        mTails.pop_back();
    }
    return m;
}

void GemRepTemplate::ReturnTail(RndMesh *m) {
    if (m != NULL) {
        UnhookAllParents(m);
        mTails.push_back(m);
    }
}

RndMesh *GemRepTemplate::CreateTail() {
    int count = mNumTailSections[0];
    RndMesh *m = Hmx::Object::New<RndMesh>();
    m->SetMutable(0x3F);
    RndMesh::VertVector &verts = m->Verts();
    verts.resize(GetRequiredVertCount(count));
    std::vector<RndMesh::Face> &faces = m->Faces();
    faces.reserve(GetRequiredFaceCount(count));
    return m;
}

#pragma push
#pragma force_active on
inline int GemRepTemplate::GetRequiredVertCount(int i) const {
    return mTailVerts.size() * (i + 1);
}

inline int GemRepTemplate::GetRequiredFaceCount(int i) const {
    return (mTailVerts.size() - 1) * i * 2;
}
#pragma pop

RndMat *GemRepTemplate::GetMatByTag(const char *c, int slot) {
    const char *s = mConfig->FindArray("mat_formats")->FindStr(c);
    const char *slotColor =
        slot < mTrackCfg.GetMaxSlots() ? mTrackCfg.GetSlotColor(slot) : "star";
    const char *matName = MakeString("%s.mat", MakeString(s, slotColor));
#ifdef HX_NATIVE
    if (getenv("GEM_DBG")) {
        fprintf(stderr,
                "[GEM_DBG]   GetMatByTag tag='%s' slot=%d color='%s' -> '%s'\n",
                c, slot, slotColor, matName);
    }
#endif
    return mObjectDir->Find<RndMat>(matName, true);
}

bool VertLess(const RndMesh::Vert &v1, const RndMesh::Vert &v2) {
    float y2 = v2.pos.y;
    float y1 = v1.pos.y;
    if (std::fabs(y1 - y2) < 0.1f) {
        return v1.pos.x < v2.pos.x;
    } else
        return y1 < y2;
}

void GemRepTemplate::SetupTailVerts() {
    mTailVerts = mObjectDir->Find<RndMesh>("tail02.mesh", true)->Verts(); // where assert
    MILO_ASSERT(!(mTailVerts.size()%2), 212);
    std::sort(mTailVerts.begin(), mTailVerts.end(), VertLess);
    mCapVerts = mTailVerts;

    int i4 = mTailVerts.size() / 2;
    for (int i = 0; i < i4; i++) {
        mCapVerts[i + i4] = mTailVerts[i];
    }
    mTailVerts.resize(i4);
    mCapVerts.resize(i4);
    mNumTailSections[0] = 90;
    mTailSectionLength[0] = kTailMaxLength / mNumTailSections[0];
    mNumTailSections[1] = 2;
    mTailSectionLength[1] = kTailMaxLength - mTailSectionLength[0];
}

int GemRepTemplate::GetNumTailSections(GemRepTemplate::TailType type) const {
    MILO_ASSERT(type < kNumTailTypes, 232);
    return mNumTailSections[type];
}

float GemRepTemplate::GetTailSectionLength(GemRepTemplate::TailType type) const {
    MILO_ASSERT(type < kNumTailTypes, 238);
    return mTailSectionLength[type];
}

RndMat *GemRepTemplate::GetSlotMat(int matIndex, int slotIndex) const {
    MILO_ASSERT_RANGE(matIndex, 0, kNumGemSlotMats, 244);
    MILO_ASSERT_RANGE(slotIndex, 0, mTrackCfg.GetMaxSlots(), 245);
    return mSlotMats[matIndex + slotIndex];
}

void GemRepTemplate::SetSlotMat(int matIndex, int slotIndex, RndMat *mat) {
    MILO_ASSERT_RANGE(matIndex, 0, kNumGemSlotMats, 251);
    MILO_ASSERT_RANGE(slotIndex, 0, mTrackCfg.GetMaxSlots(), 252);
    mSlotMats[matIndex + slotIndex] = mat;
}
