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

    std::vector<Symbol> unk20; // 0x20
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

    CampaignSourceProvider *mCampaignSourceProvider; // 0x3c - Enter() stores here, not at +8 as the stale comment claimed
    int unk40; // 0x40
    // 0x44 - retail Unload() RELEASEs this slot (lwz/stw 0x44(this)), distinct
    // from the 0x3c provider Enter() creates. Whatever populates it lives in a
    // not-yet-ported function; typed as the same provider class for RELEASE.
    CampaignSourceProvider *unk44; // 0x44
};