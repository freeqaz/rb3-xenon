#pragma once
#include "bandobj/PatchDir.h"
#include "game/Defines.h"
#include "meta_band/BandProfile.h"
#include "net_band/DataResults.h"
#include "net_band/RockCentralMsgs.h"
#include "os/PlatformMgr.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include "utl/Str.h"

class EditSetlistPanel : public UIPanel {
public:
    // ⚠ RANGE GUARDS (lane CG-4). C++ [dcl.enum]/7 gives an unscoped enum with
    // no fixed underlying type the range of the smallest BIT-FIELD holding its
    // enumerators -- not "up to the largest enumerator". Enumerators {0,4} give
    // range [0,7], and an EMPTY enumerator-list behaves as a single enumerator 0
    // => range [0,0]. All three enums below are cast to, and COMPARED against,
    // values outside that range (EditState 8/15/16/22 at EditSetlistPanel.cpp
    // :86,:199,:331,:372,:406,:409; FailureReason 1..7 at :203-:218 and
    // :334-:345; UIState 1/2 at :485-:532), so a compiler entitled to assume the
    // range may fold those tests away -- the same defect class as the missing
    // Character::DrawMode value 4 that killed character shadows.
    // MEASURED on this box: g++ folds by default and clang -fstrict-enums folds
    // memory loads; clang WITHOUT -fstrict-enums (what native/ uses today) does
    // NOT fold, so this is latent/portability, not currently live.
    // These are RANGE GUARDS, not recovered names: the rb3-Wii oracle declares
    // FailureReason and UIState equally empty and EditState equally short, so it
    // cannot supply the real enumerators and inventing them would be fiction.
    // X360-neutral: an enumerator emits no code and the underlying type stays
    // int (whole-binary A/B measured Δ0 on matched/masked/honest/code%).
    enum EditState {
        kEntering = 0,
        kCheckingProfanity = 4,
        kEditState_MaxUsed = 22
    };
    enum FailureReason {
        kFailureReason_MaxUsed = 7
    };
    enum UIState {
        kUIState_MaxUsed = 2
    };
    EditSetlistPanel();
    OBJ_CLASSNAME(EditSetlistPanel);
    OBJ_SET_TYPE(EditSetlistPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~EditSetlistPanel();
    virtual void Enter();
    virtual bool Exiting() const;
    virtual void Poll();
    virtual void Unload();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    void CleanupStringVerify();
    void SetEditState(EditState);
    void SetUIState(UIState);
    void VerifyStrings(const char *, const char *);
    bool CreateSetlist(bool);
    bool EditSetlist(LocalBandUser *, LocalSavedSetlist *);
    bool CreateBattle();
    Symbol GetMessageToken();
    Symbol GetTitleToken();
    RndTex *GetArtTex();
    void DoneEditing();
    void FailWithReason(FailureReason);
    void VerifyStringsComplete(bool, bool);
    int SymToDayCount(Symbol);
    int SymToTimeUnits(Symbol);
    Symbol DayCountToSym(int);
    void MessageOK();

    DataNode OnMsg(const UITransitionCompleteMsg &);
    DataNode OnMsg(const RockCentralOpCompleteMsg &);
    DataNode OnMsg(const DWCProfanityResultMsg &);
    NEW_OBJ(EditSetlistPanel);
    static void Init() { REGISTER_OBJ_FACTORY(EditSetlistPanel); }

    String mSetlistName; // 0x38
    String mSetlistDescription; // 0x44
    ScoreType unk50; // 0x50
    int unk54;
    int unk58;
    PatchDescriptor mSetlistArt; // 0x5c
    bool unk64;
    DataResultList unk68;
    int unk80;
    int unk84;
    BandProfile *mProfile; // 0x88
    LocalSavedSetlist *mEditingSetlist; // 0x8c
    unsigned short **unk90;
    char *unk94;
    void *unk98; // async profanity-check IO handle; word-sized on Xbox 360 retail
    int unk9c; // mode? 0 = create setlist, 1 = edit setlist, 2 = create battle
    EditState mEditState; // 0xa0
    FailureReason unka4; // 0xa4
};