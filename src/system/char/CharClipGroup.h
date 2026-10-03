#pragma once
#include "char/CharClip.h"
#include "obj/Object.h"
#include "utl/MemMgr.h"

/** "A related group of animations.  Gives you the lru one.  Usually no extension." */
class CharClipGroup : public virtual Hmx::Object {
public:
    OBJ_CLASSNAME(CharClipGroup);
    OBJ_SET_TYPE(CharClipGroup);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // Retail's Object-table slot 2 is a vtordisp thunk (0x8238FB08) onto
    // 0x8238F270 (lane W16-OT; rb3-Wii has the same override).
    virtual void Replace(ObjRef *, Hmx::Object *);

    OBJ_MEM_OVERLOAD_INLINE_DEL(0x14);
    NEW_OBJ(CharClipGroup)

    CharClip *GetClip();
    CharClip *GetClip(int);
    void DeleteRemaining(int);
    bool HasClip(CharClip *) const;
    void AddClip(CharClip *);
    void SetClipFlags(int);
    CharClip *FindClip(const char *) const;
    void Randomize();
    void RandomizeIndex();
    void Sort();
    void MakeMRU(int);
    void MakeMRU(CharClip *);

protected:
    CharClipGroup();

    /** "LRU list of clips belonging to this group" */
    ObjVector<ObjOwnerPtr<CharClip> > mClips; // 0x4
    int mWhich; // 0x14
    int mFlags; // 0x18
};
