#pragma once
#include "game/Defines.h"
#include "obj/Object.h"
#include "ui/UIListProvider.h"
#include "ui/UIPanel.h"

class CampaignSourceProvider : public UIListProvider, public Hmx::Object {
public:
    CampaignSourceProvider() {}
    virtual ~CampaignSourceProvider() {}
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const;

    void Update();

    std::vector<Symbol> unk20; // 0x2c
};

class CampaignSongInfoPanel : public UIPanel {
public:
    CampaignSongInfoPanel();
    OBJ_CLASSNAME(CampaignSongInfoPanel);
    OBJ_SET_TYPE(CampaignSongInfoPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~CampaignSongInfoPanel() {}
    virtual void Enter();
    virtual void Load();
    virtual void Unload();

    Symbol SelectedSource() const;
    ScoreType SelectedScoreType() const;
    void Refresh();
    void SelectDefaultInstrument();
    int GetCareerScore() const;
    int GetSongCount() const;
    int GetSongsCompleted(Difficulty) const;
    int GetStarCount() const;
    int GetStarsEarned(Difficulty) const;
    const char *GetInstrumentIcon();
    Symbol GetMusicLibraryBackScreen();
    Symbol GetMusicLibraryNextScreen();
    void CreateAndSubmitMusicLibraryTask();
    void Launch();
    NEW_OBJ(CampaignSongInfoPanel);
    static void Init() { REGISTER_OBJ_FACTORY(CampaignSongInfoPanel); }

    // 0x3c, and it is the ONLY own data member. Ground truth for that claim is
    // the vbase adjustor, not a member access: Hmx::Object is a *virtual* base
    // (via UIPanel), so MSVC appends it after the own data plus a 4-byte
    // vtordisp, and every override of an Object virtual subtracts that vbase
    // offset from `this`. Retail's Handle does `subi r3, r26, 0x44` => vbase at
    // 0x44 => vtordisp at 0x40 => own data is exactly [0x3c,0x40) = one word.
    // (cl.exe /d1reportSingleClassLayoutCampaignSongInfoPanel confirms our side
    // of that identity: with three own members it reported vbase at 76 = 0x4c
    // and "$vftable@Object@: | -76", matching the -0x4c we were emitting.)
    //
    // An earlier pass added `unk40`/`unk44` because ?Unload@CampaignSongInfoPanel
    // matched 100% while doing lwz/stw 0x44(this). That evidence is bad: that row
    // maps to 0x8261feb8, ~0x29000 outside this unit's 0x825f5-0x825f6 cluster,
    // i.e. an ICF fold-alias of some other panel's identically-shaped Unload
    // (UIPanel::Unload(); RELEASE(ptr); is a prime folding candidate). rb3-Wii
    // has one member here and releases it in Unload; that is restored.
    CampaignSourceProvider *mCampaignSourceProvider; // 0x3c
};