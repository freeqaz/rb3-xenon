#pragma once
#include "obj/Data.h"
#include "ui/UIPanel.h"
#include "utl/NetLoader.h"

class SetlistToStorePanel : public UIPanel {
public:
    SetlistToStorePanel() : mAllMetadata(nullptr) {}
    OBJ_CLASSNAME(SetlistToStorePanel);
    OBJ_SET_TYPE(SetlistToStorePanel);
    NEW_OBJ(SetlistToStorePanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~SetlistToStorePanel() {}
    virtual void Enter();
    virtual void Poll();
    virtual void Load();
    virtual void Unload();

    void GetSongsFromMusicLibrary();
    void LoadSongMetadata();
    /** Kicks off the per-song metadata net-loaders.  Retail calls this from
     *  Poll() (fn_826429A0) whenever mSongs and mLoaders have drifted out of
     *  step, i.e. the loader set no longer covers the song set.  Decl-only:
     *  its body sits outside this unit's pinned .text span, so it is unscored
     *  and the match build never links. */
    void StartMetadataLoaders();

    std::vector<DataNetLoader *> mLoaders; // 0x3c
    DataArray *mAllMetadata; // 0x48
    std::vector<int> mSongs; // 0x4c
    std::vector<String> mSongNames; // 0x58
    int unk54; // 0x64
    Timer unk58; // 0x68
};