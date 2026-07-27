#pragma once
#include "flow/FlowLabelProvider.h"
#include "flow/FlowNode.h"
#include "flow/FlowPtr.h"
#include "synth/Sound.h"

/** "Plays a sound cue" */
class FlowSound : public FlowNode, public FlowLabelProvider {
public:
    // Hmx::Object
    virtual ~FlowSound();
    OBJ_CLASSNAME(FlowSound)
    OBJ_SET_TYPE(FlowSound)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    // FlowNode
    virtual bool Activate();
    virtual void Deactivate(bool);
    virtual void ChildFinished(FlowNode *);
    virtual void RequestStop();
    virtual void RequestStopCancel();
    virtual void Execute(QueueState);
    virtual bool IsRunning();
    virtual void UpdateIntensity();

    // laneAT-f4 opt-out: the retail bytes show FlowSound's operator new was kept
    // OUT OF LINE and ICF-folded (its `new` site is a single
    // `bl ??2<folded>@@SAPAXI@Z` with NO StaticClassName call), unlike the
    // OBJ_MEM_OVERLOAD majority which retail inlined. Classified from the
    // CTOR relocation, not the symbol name -- see
    // /home/free/tmp/laneAT/f4/newobj_classify.py.
    MEM_OVERLOAD(FlowSound, 0x19)
    NEW_OBJ(FlowSound)

protected:
    FlowSound();

    void OnSoundSelected();
    void OnMarkerEvent(Symbol);

    /** "do not wait for sound to finish before finishing flow execution" */
    bool mImmediateRelease; // 0x5c
    /** "How should we handle stop requests?" */
    StopMode mStopMode; // 0x60
    bool mHasMarkerFired; // 0x64
    int mStopMarkerType; // 0x68
    bool mStopRequested; // 0x6c
    /** "The sound file to play" */
    FlowPtr<Sound> mSound; // 0x70
    /** "Volume of the sound, in Db" */
    float mVolume; // 0x90
    /** "pitch adjustment of the sound in semitones" */
    float mTranspose; // 0x94
    /** "Pan of the sound, -4 to +4" */
    float mPan; // 0x98
    bool mIsPlaying; // 0x9c
    /** "If true, we stop all instances of this sound from playing" */
    bool mForceStop; // 0x9d
    /** "Do we pass on running intensity to volume?" */
    bool mUseIntensity; // 0x9e
    float mCurrentIntensity; // 0xa0
    // RB3 retail FlowSound carries 0x28 more bytes than DC3 models here:
    // sizeof(FlowSound)==0xd4 (new(0xd4) at 0x82557840) and the FlowLabelProvider
    // second base lands at 0xcc (??_GFlowSound subi 0xcc), vs 0xa4 without this.
    // DC3 is newer and dropped these members; the exact fields are unknown (no
    // rb3-Wii Flow oracle), so reserve the gap so derived/sibling layouts match.
    char mUnkA4[0x28]; // 0xa4
};
