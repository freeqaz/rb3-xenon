#pragma once
#include "meta_band/SongSort.h"
#include "meta_band/SongSortNode.h"
#include "utl/Symbols.h"

class PlaysCmp : public SongSortCmp {
public:
    PlaysCmp(int, const char *);
    virtual ~PlaysCmp() {}
    virtual int Compare(SongSortCmp const *, SongNodeType) const;
    virtual const PlaysCmp *GetPlaysCmp() const { return this; }

    int mPlays; // 0x4
    const char *mName; // 0x8
};

class SongSortByPlays : public SongSort {
public:
    SongSortByPlays() {
        static Symbol by_plays("by_plays");
        mShortName = by_plays;
    }
    // No user-declared destructor: retail's vtable slot 0 for this class is
    // NodeSort's deleting destructor (0x82597ED0), which never stores this
    // class's vtables, so the destructor here is the implicit one.
    virtual ShortcutNode *NewShortcutNode(SongSortNode *) const;
    virtual HeaderSortNode *NewHeaderNode(SongSortNode *) const;
    virtual OwnedSongSortNode *NewSongNode(SongRecord *) const;
    virtual StoreSongSortNode *NewSongNode(class StoreOffer *) const;
};