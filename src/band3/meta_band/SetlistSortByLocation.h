#pragma once
#include "meta_band/SongSort.h"
#include "meta_band/SongSortNode.h"
#include "utl/Str.h"
#include "utl/Symbols.h"

class LocationCmp : public SongSortCmp {
public:
    enum SetlistHeaderType {
        kHeaderBattles = 0,
        kHeaderCustom = 1,
        kHeaderInternal = 2
    };
    LocationCmp(SavedSetlist::SetlistType, const char *, const char *, int, const char *);
    virtual ~LocationCmp() {}
    virtual int Compare(SongSortCmp const *, SongNodeType) const;
    virtual const LocationCmp *GetLocationCmp() const { return this; }

    static Symbol SetlistHeaderTypeToSym(SetlistHeaderType);

    const char *mCmp; // 0x4
    SavedSetlist::SetlistType mSetlistType; // 0x8
    const char *mOwnerName; // 0xc
    int mId; // 0x10
    String mName; // 0x14
    int mField20; // 0x20
    int mField24; // 0x24
};

class SetlistSortByLocation : public SetlistSort {
public:
    SetlistSortByLocation() { mShortName = by_location; }
    virtual ~SetlistSortByLocation() {}
    virtual ShortcutNode *NewShortcutNode(SetlistSortNode *) const;
    virtual HeaderSortNode *NewHeaderNode(SetlistSortNode *) const;
    virtual ShortcutNode *NewShortcutNode(FunctionSortNode *) const;
    virtual HeaderSortNode *NewHeaderNode(FunctionSortNode *) const;
    virtual SetlistSortNode *NewSetlistNode(SetlistRecord *) const;
    virtual FunctionSortNode *NewFunctionNode(Symbol) const;
};