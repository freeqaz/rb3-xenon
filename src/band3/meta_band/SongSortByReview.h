#pragma once
#include "meta_band/SongSort.h"
#include "meta_band/SongSortNode.h"
#include "utl/Symbols.h"

class ReviewCmp : public SongSortCmp {
public:
    ReviewCmp(int, const char *);
    virtual ~ReviewCmp() {}
    virtual int Compare(SongSortCmp const *, SongNodeType) const;
    virtual const ReviewCmp *GetReviewCmp() const { return this; }

    int mReview; // 0x4
    const char *mName; // 0x8
    Symbol mHeaderSym; // 0xc
};

class SongSortByReview : public SongSort {
public:
    SongSortByReview() {
        static Symbol by_review("by_review");
        mShortName = by_review;
    }
    // No user-declared destructor: retail's vtable slot 0 for this class is
    // NodeSort's deleting destructor (0x82597ED0), which never stores this
    // class's vtables, so the destructor here is the implicit one.
    virtual bool CustomForNode(ShortcutNode *, UIListCustom *, Hmx::Object *) const;
    virtual bool TextForNode(ShortcutNode *, UIListLabel *, UILabel *) const;
    virtual ShortcutNode *NewShortcutNode(SongSortNode *) const;
    virtual HeaderSortNode *NewHeaderNode(SongSortNode *) const;
    virtual OwnedSongSortNode *NewSongNode(SongRecord *) const;
    virtual StoreSongSortNode *NewSongNode(class StoreOffer *) const;
};
