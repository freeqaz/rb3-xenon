#include "world/LightPresetManager.h"
#include "world/Dir.h"
#include "math/Utl.h"
#include "obj/Object.h"

void PrintPreset(const char *str, LightPreset *preset) {
    if (preset) {
        MILO_LOG("%s: %s ", str, preset->Name());
        if (preset->Manual()) {
            MILO_LOG(
                "Manual (Keyframe: %d), frame %f\n",
                preset->GetCurrentKeyframe(),
                preset->GetFrame()
            );
        } else {
            MILO_LOG(
                "Animated (Keyframe: %d), frame %f\n",
                preset->GetCurrentKeyframe(),
                preset->GetFrame()
            );
        }
    } else
        MILO_LOG("%s: [NONE]\n", str);
}

LightPresetManager::LightPresetManager(WorldDir *dir)
    : mParent(dir), mPresetOverride(0), mPresetNew(0), mPresetPrev(0), mTimeNew(0), mTimePrev(0),
      mTimeOverride(0), mSingleBlend(0), mBlend(1.0f), mOverrideDuration(0), mOverrideMode(0), mIgnoreLightingEvents(0) {
    MILO_ASSERT(mParent, 0x22);
}

LightPresetManager::~LightPresetManager() {}

BEGIN_CUSTOM_HANDLERS(LightPresetManager)
    HANDLE(toggle_lighting_events, OnToggleLightingEvents)
    HANDLE(force_preset, OnForcePreset)
    HANDLE(force_two_presets, OnForceTwoPresets)
    HANDLE_ACTION(reset_presets, Reset())
END_CUSTOM_HANDLERS

void LightPresetManager::Reset() {
    mPresetNew = 0;
    mPresetPrev = 0;
    mPresetOverride = 0;
    mTimeNew = 0;
    mTimePrev = 0;
    mTimeOverride = 0;
    mSingleBlend = false;
    mLastCategory = Symbol();
    mIgnoreLightingEvents = false;
    mBlend = 1.0f;
    mOverrideMode = 0;
    mOverrideDuration = 0;
}

void LightPresetManager::Enter() { Reset(); }

void LightPresetManager::SyncObjects() {
    mPresets.clear();
    for (ObjDirItr<LightPreset> it(mParent, true); it != nullptr; ++it) {
        if (it->PlatformOk()) {
            mPresets[it->Category()].push_back(it);
        }
    }
}

void LightPresetManager::UpdateOverlay() {
    RndOverlay *o = RndOverlay::Find("light_preset", true);
    if (o->Showing()) {
        TextStream *ts = TheDebug.Reflect();
        TheDebug.SetReflect(o);
        MILO_LOG("Last Category: %s\n", mLastCategory.Str());
        PrintPreset("PresetNew", mPresetNew);
        PrintPreset("PresetPrev", mPresetPrev);
        PrintPreset("PresetOverride", mPresetOverride);
        MILO_LOG("Blend: %f\n", mBlend);
        TheDebug.SetReflect(ts);
    }
}

void LightPresetManager::StartPreset(LightPreset *preset, bool b) {
    MILO_ASSERT(preset, 0xAF);
    LightPreset **toSet = b ? &mPresetNew : &mPresetPrev;
    *toSet = preset;
    preset->StartAnim();
    float time = TheTaskMgr.Time(preset->Units());
    if (b)
        mTimeNew = time;
    else
        mTimePrev = time;
    mSingleBlend = false;
    UpdateOverlay();
}

void LightPresetManager::ForcePreset(LightPreset *p, float f) {
    if (p) {
        if (mPresetOverride != p || mOverrideMode == 1) {
            mPresetOverride = p;
            mTimeOverride = TheTaskMgr.Time(p->Units());
            mOverrideDuration = f;
            mOverrideMode = 0;
        }
        return;
    } else if (mPresetOverride) {
        mTimeOverride = TheTaskMgr.Time(mPresetOverride->Units());
        mOverrideDuration = f;
        mOverrideMode = 1;
    }
}

void LightPresetManager::ForcePresets(LightPreset *p1, LightPreset *p2, float f) {
    if (p1 && p2 && p1 != p2) {
        StartPreset(p1, false);
        StartPreset(p2, true);
        mBlend = 0.5f;
    } else
        ForcePreset(p1, f);
}

DataNode LightPresetManager::OnToggleLightingEvents(DataArray *da) {
    return mIgnoreLightingEvents = !mIgnoreLightingEvents;
}

void LightPresetManager::Poll() {
    LightPreset *pnew = mPresetNew;
    LightPreset *pprev = mPresetPrev;
    float u30 = mTimeNew;
    float u34 = mTimePrev;
    float blend = mBlend;

    if (mPresetOverride) {
        float time = TheTaskMgr.Time(mPresetOverride->Units());
        float f7 = 1.0f;
        if (mOverrideDuration > 0.0f) {
            f7 = (time - mTimeOverride) / mOverrideDuration;
        }
        float t = Clamp<float>(0.0f, 1.0f, f7);
        if (mOverrideMode == 1) {
            t = 1.0f - t;
        }
        if (t > 0.0f) {
            pprev = pnew;
            pnew = mPresetOverride;
            u34 = u30;
            u30 = mTimeOverride;
            blend = t;
        } else if (mOverrideMode == 1) {
            mPresetOverride = 0;
            mTimeOverride = 0.0f;
            mOverrideDuration = 0.0f;
            mOverrideMode = 0;
        }
    }

    if (pnew) {
        float time = TheTaskMgr.Time(pnew->Units());
        float fpu = pnew->FramesPerUnit();
        float max = (0.0f > -((time - u30) * fpu)) ? (time - u30) * fpu : 0.0f;
        if (pprev != 0 && pprev != pnew) {
            float time2 = TheTaskMgr.Time(pprev->Units());
            float fpu2 = pprev->FramesPerUnit();
            float max2 = (0.0f > -((time2 - u34) * fpu2)) ? (time2 - u34) * fpu2 : 0.0f;
            pprev->SetFrameEx(max2, 1.0f - blend, false);
            pnew->SetFrameEx(max, blend, false);
            mSingleBlend = false;
        } else {
            pnew->SetFrameEx(max, 1.0f, mSingleBlend);
            mSingleBlend = true;
        }
    }
    UpdateOverlay();
}

DataNode LightPresetManager::OnForcePreset(DataArray *da) {
    LightPreset *p = da->Obj<LightPreset>(2);
    ForcePreset(p, da->Size() > 2 ? da->Float(3) : 0);
    return 0;
}

DataNode LightPresetManager::OnForceTwoPresets(DataArray *da) {
    LightPreset *p1 = da->Obj<LightPreset>(2);
    LightPreset *p2 = da->Obj<LightPreset>(3);
    ForcePresets(p1, p2, da->Size() > 3 ? da->Float(4) : 0);
    return 0;
}
