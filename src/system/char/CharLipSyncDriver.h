#pragma once
#include "CharLipSync.h"
#include "char/CharBones.h"
#include "char/CharClip.h"
#include "char/CharDriver.h"
#include "char/CharLipSync.h"
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Highlight.h"
#include "utl/MemMgr.h"

/** "Drives lip sync animation" */
class CharLipSyncDriver : public RndHighlightable,
                          public CharWeightable,
                          public CharPollable {
    friend class HamCharacter;
    friend class BandCharacter;
public:
    // Hmx::Object
    virtual ~CharLipSyncDriver();
    virtual void Highlight();
    OBJ_CLASSNAME(CharLipSyncDriver);
    OBJ_SET_TYPE_ENGINE(CharLipSyncDriver);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // CharPollable
    virtual void Poll();
    virtual void Enter();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    // laneAT-f4: retail keeps THIS class's operator new out-of-line + ICF-folded
    // (target CharLipSyncDriver::NewObject calls the folded `??2CriticalSection@@SAPAXI@Z`
    // thunk with NO StaticClassName call), unlike the OBJ_MEM_OVERLOAD majority.
    // MEM_OVERLOAD is the literal-name, noinline, foldable form.
    MEM_OVERLOAD(CharLipSyncDriver, 0x17)
    NEW_OBJ(CharLipSyncDriver)

    void Sync();
    void ClearLipSync();
    void SetClips(ObjectDir *);
    bool SetLipSync(CharLipSync *);
    // dc3-only blend API kept as compat shims for hamobj (DC3 game layer);
    // retail RB3 has no override-blend state fields (see layout below).
    void ResetOverrideBlend();
    void BlendInOverrideClip(CharClip *, float, float);
    void BlendInOverrides(float);
    void BlendOutOverrides(float);
    void ScaleAddViseme(CharClip *, float);
    void SetSongOffset(float offset) { mSongOffset = offset; }
    void SetOverrideWeight(float weight) { mOverrideWeight = weight; }
    float GetOverrideWeight() const { return mOverrideWeight; }
    CharClip *OverrideClip() const { return mOverrideClip; }
    // rb3-Wii oracle + retail (BandDirector::OnGetFaceOverrideClips inline,
    // Ghidra 0x82284F48): falls back to the clip dir when no override is set.
    ObjectDir *OverrideDir() const {
        if (mOverrideOptions)
            return mOverrideOptions;
        else
            return mClips;
    }
    CharLipSync *LipSync() const { return mLipSync; }
    CharLipSync::PlayBack *GetPlayBack() const { return mMainPlayback; }

protected:
    CharLipSyncDriver();

    // Retail RB3-360 layout (verified against retail CharLipSyncDriver::Save
    // @0x823795C8: Hmx::Object vbase at +0xac, ObjPtr = 0xc bytes).
    // Matches rb3-Wii oracle field order (each 360 offset = Wii offset - 4).

    /** "The lipsync file to use" */
    ObjPtr<CharLipSync> mLipSync; // 0x28
    /** "pointer to the visemes" */
    ObjPtr<ObjectDir> mClips; // 0x34
    ObjPtr<CharClip> mBlinkClip; // 0x40
    /** "Will use this song if set, except for blinks" */
    ObjPtr<CharLipSyncDriver> mSongOwner; // 0x4c
    /** "offset within song in seconds, resets on song change" */
    float mSongOffset; // 0x58
    /** "should we loop this song, resets on song change" */
    bool mLoop; // 0x5c
    CharLipSync::PlayBack *mMainPlayback; // 0x60 (Wii: mSongPlayer)
    /** "The CharBones object to add or blend into." */
    ObjPtr<CharBonesObject> mBones; // 0x64
    /** "Test charclip to apply, does nothing else" */
    ObjPtr<CharClip> mTestClip; // 0x70
    /** "weight to apply this clip with" */
    float mTestWeight; // 0x7c
    /** "default clip to be used as the override - maybe be overriden programatically" */
    ObjPtr<CharClip> mOverrideClip; // 0x80
    /** "weight to blend override clip. this is mostly here for testing,
        because its likely to be set programatically." */
    float mOverrideWeight; // 0x8c
    /** "an optional clipset that provides list of clips to override face with -
        viseme clipset is used otherwise" */
    ObjPtr<ObjectDir> mOverrideOptions; // 0x90
    /** "is the override clip applied addtively on top of face mocap?
        If false, it will blend." */
    bool mApplyOverrideAdditively; // 0x9c
    /** "This will be used instead of the song, if set" */
    ObjPtr<CharDriver> mAlternateDriver; // 0xa0
};
