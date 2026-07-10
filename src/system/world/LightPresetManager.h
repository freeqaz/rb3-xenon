#pragma once
#include "obj/Data.h"
#include "utl/Symbol.h"
#include <hash_map>

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

    // RB3 BandDirector deps (not in dc3 LightPresetManager — stubbed for compile).
    void GetPresets(LightPreset *&, LightPreset *&);
    void Interp(Symbol, Symbol, float);
    void SchedulePstKey(int);
    void StompPresets(LightPreset *, LightPreset *);
    LightPreset *PickRandomPreset(Symbol);

protected:
    DataNode OnToggleLightingEvents(DataArray *);
    DataNode OnForcePreset(DataArray *);
    DataNode OnForceTwoPresets(DataArray *);

    void UpdateOverlay();
    void StartPreset(LightPreset *, bool);

    // Layout-only hasher stand-in: retail RB3's mPresets is an stlport
    // hash_map, NOT std::map — the retail LightPresetManager ctor (0x824A6758)
    // calls hashtable(100, hf, eql, alloc) via 0x82268810/0x82268698 and sets
    // _M_max_load_factor=1.0f at +0x18, making the container 0x1c (vs map's
    // 0x18) and sizeof(LightPresetManager) 0x54 (WorldDir tail proof).
    struct SymbolHash {
        size_t operator()(Symbol s) const { return (size_t)s.Str(); }
    };
    std::hash_map<Symbol, std::vector<LightPreset *>, SymbolHash> mPresets; // 0x4 (0x1c)
    Symbol mLastCategory; // 0x20
    WorldDir *mParent; // 0x24
    LightPreset *mPresetOverride; // 0x28
    LightPreset *mPresetNew; // 0x2c
    LightPreset *mPresetPrev; // 0x30
    float mTimeNew; // 0x34
    float mTimePrev; // 0x38
    float mTimeOverride; // 0x3c
    bool mSingleBlend; // 0x40
    float mBlend; // 0x44
    float mOverrideDuration; // 0x48
    int mOverrideMode; // 0x4c
    bool mIgnoreLightingEvents; // 0x50
};
