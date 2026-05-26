#pragma once
#include "obj/Data.h"

class LightPreset;
class WorldDir;

class LightPresetManager {
public:
    LightPresetManager(WorldDir *);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~LightPresetManager();

    void Reset();
    void SyncObjects();
    void Enter();
    void Poll();
    void ForcePreset(LightPreset *, float);
    void ForcePresets(LightPreset *, LightPreset *, float);

protected:
    DataNode OnToggleLightingEvents(DataArray *);
    DataNode OnForcePreset(DataArray *);
    DataNode OnForceTwoPresets(DataArray *);

    void UpdateOverlay();
    void StartPreset(LightPreset *, bool);

    std::map<Symbol, std::vector<LightPreset *> > mPresets; // 0x4
    Symbol mLastCategory; // 0x1c
    WorldDir *mParent; // 0x20
    LightPreset *mPresetOverride; // 0x24
    LightPreset *mPresetNew; // 0x28
    LightPreset *mPresetPrev; // 0x2c
    float mTimeNew; // 0x30
    float mTimePrev; // 0x34
    float mTimeOverride; // 0x38
    bool mSingleBlend; // 0x3c
    float mBlend; // 0x40
    float mOverrideDuration; // 0x44
    int mOverrideMode; // 0x48
    bool mIgnoreLightingEvents; // 0x4c
};
