#include "world/LightPresetManager.h"
#include "world/Dir.h"
#include "math/Utl.h"
#include "math/Rand.h"
#include "obj/Msg.h"
#include "world/LightPreset.h"
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

// Retail 0x824B88B0.
void LightPresetManager::SetPresetsEquivalent(bool b) {
    if (b) {
        mPresetPrev = mPresetNew;
        mTimePrev = mTimeNew;
    } else {
        mPresetNew = mPresetPrev;
        mTimeNew = mPresetPrev ? TheTaskMgr.Time(mPresetPrev->Units()) : 0;
        mTimePrev = mPresetPrev ? TheTaskMgr.Time(mPresetPrev->Units()) : 0;
    }
}

// Retail 0x824B8A10.
void LightPresetManager::GetPresets(LightPreset *&prev, LightPreset *&next) {
    prev = mPresetPrev;
    next = mPresetNew;
}

// Retail 0x824B8C50: a tail call into OnKeyframeCmd, no overlay update.
void LightPresetManager::SchedulePstKey(int cmd) {
    if (!mIgnoreLightingEvents) {
        if (mPresetNew)
            mPresetNew->OnKeyframeCmd((LightPreset::KeyframeCmd)cmd);
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
        UpdateOverlay();
#endif
    }
}

// Retail 0x824B8CF0. The preset pointers are re-stored after StartPreset:
// retail keeps the assignments that sat inside its (compiled-out) asserts.
void LightPresetManager::StompPresets(LightPreset *presetA, LightPreset *presetB) {
    if (presetA && presetB && presetA != presetB) {
        StartPreset(presetA, false);
        StartPreset(presetB, true);
        mBlend = 0.5f;
        mPresetPrev = presetA;
        mPresetNew = presetB;
    } else if (presetA && presetA == presetB) {
        StartPreset(presetA, true);
        SetPresetsEquivalent(true);
        mBlend = 1.0f;
        mPresetPrev = presetA;
        mPresetNew = presetA;
    }
}

// Retail 0x824B9370.
void LightPresetManager::SendLightingMessage(Symbol s) {
    char buf[0x100];
    static Message msg("");
    strcpy(buf, "lighting_");
    strcpy(buf + 9, s.Str());
    msg.SetType(buf);
    mParent->Handle(msg, false);
}

// Retail 0x824B9C60.
LightPreset *LightPresetManager::PickRandomPreset(Symbol s) {
    int count = mPresets[s].size();
    if (count == 0)
        return 0;
    return mPresets[s][RandomInt(0, count)];
}

// Retail 0x824B9CD8: the preset select is inline; a missing preset is ignored.
void LightPresetManager::SetLighting(Symbol s, bool b) {
    if (!mIgnoreLightingEvents) {
        mLastCategory = s;
        LightPreset *p = PickRandomPreset(mLastCategory);
        if (p)
            StartPreset(p, b);
    }
}

// Retail 0x824B9D38 (no missing-preset notifies in retail).
void LightPresetManager::Interp(Symbol s1, Symbol s2, float f) {
    mBlend = f;
    if (!mPresetNew) {
        SendLightingMessage(s2);
        SetLighting(s2, true);
    }
    if (!mPresetPrev) {
        SetLighting(s1, false);
    }
    if (mBlend == 0) {
        SetPresetsEquivalent(false);
    }
    if (mPresetNew && mPresetPrev) {
        Symbol prevCat = mPresetPrev->Category();
        if (prevCat != s1 && mPresetNew->Category() == s1) {
            SetPresetsEquivalent(true);
        } else if (mPresetNew->Category() != s2 && prevCat == s2) {
            SetPresetsEquivalent(false);
        }
        if (mPresetNew->Category() != s2) {
            SendLightingMessage(s2);
            SetLighting(s2, true);
        }
        if (mPresetPrev->Category() != s1) {
            SetLighting(s1, false);
        }
    }
}

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
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    UpdateOverlay(); // not in retail StartPreset
#endif
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
        float f7;
        if (mOverrideDuration > 0.0f) {
            f7 = (time - mTimeOverride) / mOverrideDuration;
        } else {
            f7 = 1.0f;
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
        float max = Max(0.0f, fpu * (time - u30));
        if (pprev != 0 && pprev != pnew) {
            float time2 = TheTaskMgr.Time(pprev->Units());
            float fpu2 = pprev->FramesPerUnit();
            float max2 = Max(0.0f, (time2 - u34) * fpu2);
            pprev->SetFrameEx(max2, 1.0f - blend, false);
            pnew->SetFrameEx(max, blend, false);
            mSingleBlend = false;
        } else {
            pnew->SetFrameEx(max, 1.0f, mSingleBlend);
            mSingleBlend = true;
        }
    }
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    UpdateOverlay(); // not in retail Poll (0x824B8A28)
#endif
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
