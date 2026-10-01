#pragma once
#include "obj/Object.h"
#include "synth/FxSend.h"

class FxSendPitchShift : public FxSend {
public:
    OBJ_CLASSNAME(FxSendPitchShift);
    OBJ_SET_TYPE(FxSendPitchShift);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    // Out of line in retail (0x827122A0: stfs f1,0x54; tail-call
    // OnParametersChanged). Called by GemPlayer::SetPitchShiftRatio.
    void SetRatio(float ratio);

    OBJ_MEM_OVERLOAD_INLINE_DEL(0x10);
    NEW_OBJ(FxSendPitchShift)

protected:
    FxSendPitchShift();

    float mRatio; // 0x54
};
