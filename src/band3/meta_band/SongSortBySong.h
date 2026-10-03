#pragma once
#include "meta_band/SongSort.h"
#include "meta_band/SongSortNode.h"
#include "utl/Symbols.h"

class SongCmp : public SongSortCmp {
public:
    SongCmp(const char *name, Symbol header) : mName(name), mHeaderSym(header) {}
    virtual ~SongCmp() {}
    virtual int Compare(SongSortCmp const *, SongNodeType) const;
    virtual const SongCmp *GetSongCmp() const { return this; }

    const char *mName; // 0x4
    Symbol mHeaderSym; // 0x8
};

class SongSortBySong : public SongSort {
public:
    SongSortBySong() {
        static Symbol by_song("by_song");
        mShortName = by_song;
    }
    // No user-declared destructor: retail's vtable slot 0 for this class is
    // NodeSort's deleting destructor (0x82597ED0), which never stores this
    // class's vtables, so the destructor here is the implicit one.
    virtual void Init();
    virtual ShortcutNode *NewShortcutNode(SongSortNode *) const;
    virtual HeaderSortNode *NewHeaderNode(SongSortNode *) const;
    virtual OwnedSongSortNode *NewSongNode(SongRecord *) const;
    virtual StoreSongSortNode *NewSongNode(class StoreOffer *) const;
};