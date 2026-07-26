#pragma once
#include "meta/HAQManager.h"
#include "meta/MetaMusicManager.h"
#include "meta/SongPreview.h"
#include "meta_band/Campaign.h"
#include "meta_band/NameGenerator.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "synth/MetaMusic.h"
#include "tour/Tour.h"
#include "ui/UIPanel.h"

class CurrentScreenChangedMsg;

DECLARE_MESSAGE(XMPStateChangedMsg, "xmp_state_changed")
XMPStateChangedMsg(int i) : Message(Type(), i) {}
bool Success() const { return mData->Int(2); }
END_MESSAGE

class MetaPanel : public UIPanel {
public:
    MetaPanel();
    OBJ_CLASSNAME(MetaPanel);
    OBJ_SET_TYPE(MetaPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~MetaPanel();

    virtual void Draw();
    virtual void Enter();
    virtual void Exit();
    virtual bool Exiting() const;
    virtual void Poll();
    virtual void Load();
    virtual void Unload();
    virtual bool IsLoaded() const;
    virtual void PollForLoading();
    virtual void FinishLoad();

    static bool sUnlockAll;
    static bool sIsPlaytest;
    static bool sLaunchedGoalMsgsOnly;
    static void Init();
    NEW_OBJ(MetaPanel);
    static void Register() { REGISTER_OBJ_FACTORY(MetaPanel); }
    static DataNode ToggleUnlockAll(DataArray *);
    static DataNode ToggleIsPlaytest(DataArray *);
    static DataNode ToggleLaunchedGoalMsgsOnly(DataArray *);

    void SyncGameTimer();
protected:
    int PickLoopIndex(int);
public:
    void UpdatePostProc();
    void OnSendBackSoundMsgToAll();
    void UpdateMusicMuteState();
    void UpdateMetaMusic(Symbol);
    DataNode OnMsg(const CurrentScreenChangedMsg &);
    DataNode OnMsg(const XMPStateChangedMsg &);

    Tour *mTour; // 0x3c
    Campaign *mCampaign; // 0x40
    NameGenerator *mNameGenerator; // 0x44
    MetaMusicManager *mMetaMusicMgr; // 0x48
    HAQManager *mHAQMgr; // 0x4c
    std::vector<int> mRecentIndices; // 0x50
    int unk58; // 0x5c
    MetaMusic *mMusic; // 0x60
    SongPreview mSongPreview; // 0x64 (sizeof 0x70 -- see SongPreview.h)
    // 0xd4 -- proved by `?OnMsg@MetaPanel@@QAA?AVDataNode@@ABVXMPStateChangedMsg@@@Z`
    // (`stb r11, 0xd4(r29)`), which requires sizeof(SongPreview) == 0x70.
    // MetaPanel therefore ends here: own data through 0xD8, virtual `Hmx::Object`
    // base at 0xDC, sizeof 0x104. `?Handle@MetaPanel@@UAA...` (170 instructions,
    // 9 vbase-relative refs) confirms 0xDC.
    // MAP MISPAIR (do not "fix" by padding): `??_GMetaPanel@@UAAPAXI@Z`
    // (@0x82606218) and `?NewObject@MetaPanel@@SAPAVObject@Hmx@@XZ` (@0x8256ead8)
    // want vbase 0xEC / sizeof 0x114 -- mutually consistent with each other and
    // with neither Handle nor OnMsg, i.e. they are paired to a *different*
    // 0x114-byte panel class. Padding MetaPanel to 0x114 to satisfy them costs
    // Handle + OnMsg + 3 EH funclets (measured -2 net).
    bool unkd4;
};