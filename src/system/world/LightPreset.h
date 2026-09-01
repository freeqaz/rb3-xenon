#pragma once
#include "SpotlightDrawer.h"
#include "math/Color.h"
#include "math/Mtx.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Platform.h"
#include "rndobj/Anim.h"
#include "rndobj/Env.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Lit.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"
#include "world/LightHue.h"
#include "world/Spotlight.h"
#include "world/SpotlightDrawer.h"
#include <deque>

/** "Represents an animated sequence of states of certain
    objects in the world. For now, we store states for Spotlight and
    Environment objects." */
class LightPreset : public RndAnimatable {
public:
    struct EnvironmentEntry {
        EnvironmentEntry();
        void Save(BinStream &) const;
        void Load(BinStream &);
        void Animate(const EnvironmentEntry &, float);
        bool operator!=(const EnvironmentEntry &) const;

        /** "Ambient color" */
        Hmx::Color mAmbientColor; // 0x0
        /** "Fog showing?" */
        bool mFogEnable; // 0x10
        /** "Intensity from smoke" */
        float mFogStart; // 0x14
        /** "Intensity from smoke" */
        float mFogEnd; // 0x18
        /** "Intensity from smoke" */
        Hmx::Color mFogColor; // 0x1c
    };

    struct EnvLightEntry {
        EnvLightEntry();
        void Save(BinStream &) const;
        void Load(BinStream &);
        void Animate(const EnvLightEntry &, float);
        bool operator!=(const EnvLightEntry &) const;

        EnvLightEntry &operator=(const EnvLightEntry &e) {
            memcpy(this, &e, sizeof(*this));
            return *this;
        }

        Hmx::Quat mOrientation; // 0x0
        /** "Light's position" */
        Vector3 mPosition; // 0x10
        /** "Light's color" */
        Hmx::Color mColor; // 0x20
        /** "Falloff distance for point lights" */
        float mRange; // 0x30
        /** "Light type" */
        RndLight::Type mLightType; // 0x34
        /** "Light transform" */
        Hmx::Matrix3 mRotation; // 0x38
    };

    struct SpotlightEntry {
        enum {
            kEnabled = 1
            // there's a flag for 2 but idk what it is
        };

        SpotlightEntry(Hmx::Object *owner);
        void Save(BinStream &) const;
        void Load(BinStreamRev &);
        bool operator!=(const SpotlightEntry &) const;
        void CalculateDirection(Spotlight *, Hmx::Quat &) const;
        void Animate(Spotlight *, const SpotlightEntry &, float);

        float mIntensity; // 0x0
        int mColor; // 0x4 - packed
        unsigned char mFlags; // 0x8
        ObjPtr<RndTransformable> mTarget; // 0xc
        Hmx::Quat mRotation; // 0x18
        Hmx::Matrix3 mRotationMatrix; // 0x28
    };

    struct SpotlightDrawerEntry {
        SpotlightDrawerEntry();
        void Save(BinStream &) const;
        void Load(BinStreamRev &);
        bool operator!=(const SpotlightDrawerEntry &) const;

        /** "Global intensity scale" */
        float mTotalIntensity; // 0x0
        /** "Intensity of smokeless beam" */
        float mBaseIntensity; // 0x4
        /** "Intensity from smoke" */
        float mSmokeIntensity; // 0x8
        /** "The amount the spotlights will influence the real lighting of the world" */
        float mLightInfluence; // 0xc
    };

    struct Keyframe {
        Keyframe(Hmx::Object *);
        void Save(BinStream &) const;
        void Load(BinStreamRev &);
        void LegacyLoadP9(BinStreamRev &);
        void LegacyLoadStageKit(BinStream &);

        /** "Description of the keyframe" */
        String mDescription; // 0x0
        ObjVector<SpotlightEntry> mSpotlightEntries; // 0xc
        std::vector<EnvironmentEntry> mEnvironmentEntries; // 0x1c
        std::vector<EnvLightEntry> mLightEntries; // 0x28
        std::vector<SpotlightDrawerEntry> mSpotlightDrawerEntries; // 0x34
        /** "Post-processing to apply during the video venue (RB3 retail)" */
        ObjPtr<RndPostProc> mVideoVenuePostProc; // 0x40
        /** "Trigger to fire when keyframe starts blending (deprecated)" */
        ObjPtrList<EventTrigger> mTriggers; // 0x4c
        std::vector<bool> mSpotlightChanges; // 0x60
        std::vector<bool> mEnvironmentChanges; // 0x74
        std::vector<bool> mLightChanges; // 0x88
        std::vector<bool> mSpotlightDrawerChanges; // 0x9c
        /** "Duration of the keyframe" */
        float mDuration; // 0xb0
        /** "Fade-out time of the keyframe" */
        float mFadeOutTime; // 0xb4
        float mFrame; // 0xb8
        // RB3 retail StageKit LED keyframe fields (absent in DC3/rb3-Wii dev).
        int mLedRed; // 0xbc
        int mLedBlue; // 0xc0
        int mLedGreen; // 0xc4
        int mLedYellow; // 0xc8
        int mLedRedPattern; // 0xcc
        int mLedBluePattern; // 0xd0
        int mLedGreenPattern; // 0xd4
        int mLedYellowPattern; // 0xd8
        int mStrobeSetting; // 0xdc
    };

    enum KeyframeCmd {
        kPresetKeyframeFirst,
        kPresetKeyframeNext,
        kPresetKeyframePrev,
        kPresetKeyframeNum
    };

    enum PresetObject {
        kPresetSpotlight,
        kPresetSpotlightDrawer,
        kPresetEnv,
        kPresetLight
    };

    friend const char *GetName(LightPreset *preset, int idx, PresetObject obj);

    // Hmx::Object
    virtual ~LightPreset();
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(LightPreset);
    OBJ_SET_TYPE(LightPreset);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndAnimatable
    virtual void StartAnim();
    virtual void SetFrame(float, float);
    virtual float EndFrame() { return mEndFrame; }

    OBJ_MEM_OVERLOAD(0x1B)
    NEW_OBJ(LightPreset)
    void SetHue(LightHue *hue) { mHue = hue; }
    Symbol Category() const { return mCategory; }
    bool Manual() const { return mManual; }
    void TranslateColor(const Hmx::Color &col, Hmx::Color &res);

    int GetCurrentKeyframe(void) const;
    bool PlatformOk(void) const;
    void SetSpotlight(Spotlight *, int);
    void OnKeyframeCmd(KeyframeCmd);
    void ResetEvents();
    void SetFrameEx(float, float, bool);

    // RB3 BandDirector deps (stubs — port from rb3-Wii LightPreset when revisited).
    class RndPostProc *GetCurrentPostProc() const;
    float LegacyFadeIn() const;
    static void StaticResetEvents();

protected:
    LightPreset();

    void Clear();
    void CacheFrames();
    void GetKey(float, int &, int &, float &) const;
    void RemoveLight(int);
    void RemoveSpotlight(int);
    void RemoveSpotlightDrawer(int);
    void RemoveEnvironment(int);
    void AddLight(RndLight *);
    void AddSpotlightDrawer(SpotlightDrawer *);
    void AddEnvironment(RndEnviron *);
    void AdvanceManual(LightPreset::KeyframeCmd);
    int NextManualFrame(LightPreset::KeyframeCmd) const;
    void FillLightPresetData(RndLight *, LightPreset::EnvLightEntry &);
    void AnimateLightFromPreset(RndLight *, const LightPreset::EnvLightEntry &, float);
    void ApplyState(LightPreset::Keyframe const &);
    void SetKeyframe(Keyframe &);
    void AnimateEnvFromPreset(RndEnviron *, const EnvironmentEntry &, float);
    void AnimateSpotFromPreset(Spotlight *, const SpotlightEntry &, float);
    void FillEnvPresetData(RndEnviron *, EnvironmentEntry &);
    void FillSpotlightDrawerPresetData(SpotlightDrawer *, SpotlightDrawerEntry &);
    void AddSpotlight(Spotlight *, bool);
    void FillSpotPresetData(Spotlight *, SpotlightEntry &, int);
    void Animate(float);
    void AnimateState(const Keyframe &prev, const Keyframe &cur, float t);
    void SyncNewSpotlights();
    void SyncKeyframeTargets();

    DataNode OnSetKeyframe(DataArray *);
    DataNode OnViewKeyframe(DataArray *);

    static std::deque<std::pair<KeyframeCmd, float> > sManualEvents;

    // RB3 retail uses plain std::vector<T*> here (stride 4), NOT ObjPtrVec
    // (0x1c-byte Node ring). Confirmed via asm: mSpotlights begin@member+0
    // with stride-4 pointer iteration. Each plain vector is 0xc bytes, so the
    // four together are 0x40 smaller than the ObjPtrVec form — that, minus the
    // re-added mLegacyFadeIn (+4), is the +0x3C tail shift seen across the unit.
    ObjVector<Keyframe> mKeyframes; // 0x10
    std::vector<Spotlight *> mSpotlights; // 0x20
    std::vector<RndEnviron *> mEnvironments; // 0x2c
    std::vector<RndLight *> mLights; // 0x38
    std::vector<SpotlightDrawer *> mSpotlightDrawers; // 0x44
    /** "Whether this preset loops its animation" */
    bool mLooping; // 0x50
    /** "Category for preset-picking" */
    Symbol mCategory; // 0x54
    /** "Limit this shot to given platform" - the options are kPlatformNone/PS3/Xbox */
    Platform mPlatformOnly; // 0x58
    /** "Triggers to fire upon selection (deprecated)" */
    ObjPtrList<EventTrigger> mSelectTriggers; // 0x5c
    /** "How long this preset should fade in from the previous one" */
    float mLegacyFadeIn; // 0x70
    /** "Whether this is a manual keyframe (keyframes controlled by MIDI)" */
    bool mManual; // 0x74
    ObjVector<SpotlightEntry> mSpotlightState; // 0x78
    std::vector<EnvironmentEntry> mEnvironmentState; // 0x88
    std::vector<EnvLightEntry> mLightState; // 0x94
    std::vector<SpotlightDrawerEntry> mSpotlightDrawerState; // 0xa0
    Keyframe *mLastKeyframe; // 0xac
    float mLastBlend; // 0xb0
    float mStartBeat; // 0xb4
    float mManualFrameStart; // 0xb8
    int mManualFrame; // 0xbc
    int mLastManualFrame; // 0xc0
    float mManualFadeTime; // 0xc4
    float mEndFrame; // 0xc8
    /** "Whether the keyframes are locked (no editing allowed)" */
    bool mLocked; // 0xcc
    LightHue *mHue; // 0xd0
};
