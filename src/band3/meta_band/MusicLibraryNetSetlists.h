#pragma once
#include "meta_band/SavedSetlist.h"
#include "meta_band/SongRecord.h"
#include "meta_band/SongSortNode.h"
#include "net_band/DataResults.h"
#include "obj/Object.h"
#include "rndobj/Tex.h"
#include "utl/NetCacheLoader.h"

class MusicLibraryNetSetlists : public Hmx::Object {
public:
    class SetlistArtRecord {
    public:
        Symbol unk0;
        RndTex *unk4;
        SetlistArtRecord() : unk0(gNullStr), unk4(0) {}
    };
    MusicLibraryNetSetlists();
    virtual ~MusicLibraryNetSetlists();
    virtual DataNode Handle(DataArray *, bool);

    void Poll();
    void CleanUp();
    void RefreshSetlists();
    bool IsSetlistArtReady(Symbol) const;
    RndTex *GetSetlistArt(Symbol) const;
    void RefreshSetlistArt();
    void FinishGettingSetlistArt(bool);
    void RefreshArchivedBattles();
    void CleanUpArt();
    void ParseDataResultsIntoSetlists(bool);

    DataNode OnMsg(const RockCentralOpCompleteMsg &);

    bool mFailed; // 0x28
    bool mSucceeded; // 0x29
    std::vector<NetSavedSetlist *> unk20;
    std::vector<NetSavedSetlist *> unk28;
    DataResultList mDataResults; // 0x44
    bool unk48;
    RndTex *mPendingSetlistArt; // 0x60
    Symbol unk50;
    NetCacheLoader *mSetlistArtLoader; // 0x68
    std::list<SetlistArtRecord> mSetlists; // 0x6c
};