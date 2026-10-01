#pragma once
#include "obj/Data.h"
#include "utl/Symbol.h"
#include <hash_map>

// hash<Symbol> hashes the interned char* word identity. Guarded: the band3
// accomplishment headers define the same specialization.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

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

    // RB3-only API (absent from DC3's LightPresetManager); retail bodies in
    // this unit's .text, see LightPresetManager.cpp.
    void GetPresets(LightPreset *&, LightPreset *&);
    void Interp(Symbol, Symbol, float);
    void SchedulePstKey(int);
    void StompPresets(LightPreset *, LightPreset *);
    LightPreset *PickRandomPreset(Symbol);
    void SetPresetsEquivalent(bool);
    void SendLightingMessage(Symbol);
    void SetLighting(Symbol, bool);

protected:
    DataNode OnToggleLightingEvents(DataArray *);
    DataNode OnForcePreset(DataArray *);
    DataNode OnForceTwoPresets(DataArray *);

    void UpdateOverlay();
    void StartPreset(LightPreset *, bool);

    // Retail RB3's mPresets is an stlport hash_map, NOT std::map -- the retail
    // LightPresetManager ctor (0x824A6758) calls hashtable(100, hf, eql, alloc)
    // via 0x82268810/0x82268698 and sets _M_max_load_factor=1.0f at +0x18,
    // making the container 0x1c (vs map's 0x18) and sizeof(LightPresetManager)
    // 0x54 (WorldDir tail proof). It uses the stock hash<Symbol>: retail's
    // operator[] (0x824B9A58) and _M_insert share the hash<Symbol> _M_find and
    // resize bodies with the other Symbol-keyed hash_maps.
    std::hash_map<Symbol, std::vector<LightPreset *> > mPresets; // 0x4 (0x1c)
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
