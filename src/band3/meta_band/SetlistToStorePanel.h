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
    // W16-HR: implicit dtor (retail ??1 resets no derived vptrs)
    virtual void Enter();
    virtual void Poll();
    virtual void Load();
    virtual void Unload();

    void GetSongsFromMusicLibrary();
    void LoadSongMetadata();
    /** The store path of one song's metadata (retail fn_82642918). */
    void GetSongMetadataPath(int songID, String &path);
    /** Starts a metadata net-loader for each song in mSongs that has none yet,
     *  at most 20 per call.  Retail calls it from LoadSongMetadata whenever
     *  mSongs and mLoaders differ in size (fn_826429A0). */
    void StartMetadataLoaders();

    std::vector<DataNetLoader *> mLoaders; // 0x3c
    DataArray *mAllMetadata; // 0x48
    std::vector<int> mSongs; // 0x4c
    std::vector<String> mSongNames; // 0x58
    int unk54; // 0x64
    Timer unk58; // 0x68
};