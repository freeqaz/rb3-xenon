#pragma once
// Ported from rb3-Wii src/system/bandobj/EndingBonus.h (ObjPtr<T,ObjectDir> -> ObjPtr<T>).
#include "obj/ObjMacros.h"
#include "rndobj/Dir.h"
#include "rndobj/EventTrigger.h"
#include "obj/Task.h"
#include "bandobj/TrackInstruments.h"
#include "bandobj/BandLabel.h"
#include "bandobj/UnisonIcon.h"

class EndingBonus : public RndDir {
public:
    class MiniIconData {
    public:
        MiniIconData(EndingBonus *b, UnisonIcon *u)
            : mIcon(b, u), mFailed(0), mSucceeded(0), mDisabled(0), mUsed(0) {}

        void Reset();
        void SetUsed(bool);
        void Succeeded();
        void Failed();
        void SetProgress(float);

        ObjPtr<UnisonIcon> mIcon; // 0x0
        bool mFailed; // 0xc
        bool mSucceeded; // 0xd
        bool mDisabled; // 0xe
        bool mUsed; // 0xf
    };

    EndingBonus();
    OBJ_CLASSNAME(EndingBonusDir);
    OBJ_SET_TYPE(EndingBonusDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual ~EndingBonus() {}
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();

    void Start(bool);
    void Success();
    void CodaEnd();
    void SetScore(int);
    void UnisonSucceed();
    void UnisonEnd();
    void Reset();
    void SetupEnding(bool);
    void SetIconText(int, const char *);
    void PlayerSuccess(int);
    void PlayerFailure(int);
    void SetSuppressUnisonDisplay(bool);
    void SetupUnison(int);
    void UnisonStart(int);
    void SetIconOrder(int, bool);
    void DisablePlayer(int);
    void EnablePlayer(int);
    void SetProgress(int, float);

    DataNode OnReset(DataArray *);

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(EndingBonus)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(EndingBonus)

    bool mSuppressUnisonDisplay; // 0x1dc
    bool mInUnison; // 0x1dd
    int mScore; // 0x1e0
    bool mSucceeded; // 0x1e4
    ObjPtr<Task> mCodaEndTask; // 0x1e8
    std::vector<TrackInstrument> mTrackOrder; // 0x1f4
    std::vector<MiniIconData> mIconData; // 0x200
    ObjPtr<BandLabel> mScoreLabel; // 0x20c
    ObjPtr<EventTrigger> mUnisonStartTrig; // 0x218
    ObjPtr<EventTrigger> mUnisonEndTrig; // 0x224
    ObjPtr<EventTrigger> mUnisonSucceedTrig; // 0x230
    ObjPtr<EventTrigger> mStartTrig; // 0x23c
    ObjPtr<EventTrigger> mEndTrig; // 0x248
    ObjPtr<EventTrigger> mSucceedTrig; // 0x254
    ObjPtr<EventTrigger> mResetTrig; // 0x260
};
