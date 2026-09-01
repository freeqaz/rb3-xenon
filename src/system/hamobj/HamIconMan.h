#pragma once
#include "char/CharClip.h"
#include "hamobj/Difficulty.h"
#include "rndobj/Anim.h"
#include "rndobj/Draw.h"
#include "rndobj/Tex.h"
#include "utl/MemMgr.h"

/** "Icon Man to render icon man in game" */
class HamIconMan : public RndAnimatable, public RndDrawable {
public:
    // Hmx::Object
    virtual ~HamIconMan();
    OBJ_CLASSNAME(HamIconMan);
    OBJ_SET_TYPE(HamIconMan);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndAnimatable
    virtual float EndFrame() { return mEndBeat - mStartBeat; }
    // RndDrawable
    virtual void DrawShowing();

    OBJ_MEM_OVERLOAD(0x17)
    NEW_OBJ(HamIconMan)

protected:
    HamIconMan();

    ObjPtr<RndTex> mTexture; // 0x34
    float mStartBeat; // 0x40
    float mEndBeat; // 0x44
    float mOffset; // 0x48
    /** "Difficulty for iconman to be dancing" */
    Difficulty mDifficulty; // 0x4c
    Symbol mMoveName; // 0x50
    ObjPtr<CharClip> mCharClip; // 0x54
    float mBPMOverride; // 0x60
    bool mPlayIntroTransition; // 0x64
};
