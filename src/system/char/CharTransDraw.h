#pragma once
#include "char/Character.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Draw.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

/** "Defers drawing translucent parts of characters until this object is drawn" */
class CharTransDraw : public RndDrawable {
public:
    // Hmx::Object
    virtual ~CharTransDraw();
    OBJ_CLASSNAME(CharTransDraw)
    OBJ_SET_TYPE(CharTransDraw)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    // RndDrawable
    virtual void DrawShowing();

    void SetDrawModes(Character::DrawMode);

    OBJ_MEM_OVERLOAD(0x14)
    NEW_OBJ(CharTransDraw);

    /** "The Characters whose translucent bits we will draw" */
    ObjPtrList<Character> mChars; // 0x40
    // NO `bool mForceDraw` IN RB3 RETAIL -- a DC3-era addition.  Evidence:
    //  (1) rb3-Wii's CharTransDraw (RB3's OWN engine generation) declares only
    //      mChars and its BEGIN_PROPSYNCS has only SYNC_PROP(chars, mChars);
    //  (2) the exact NUL-terminated property Symbol "force_draw\0" has ZERO
    //      occurrences in retail band.exe while this class's sibling property
    //      "chars\0" has EIGHT -- so the probe could have fired and did not;
    //  (3) vbase_census: ours 0x0/0x40/0x6c vs retail 0x0/0x3c/0x68, a uniform
    //      -4 of every base subobject == one surplus 4-byte slot in this
    //      class's own prefix (MSVC places virtual bases after all non-virtual
    //      members, so a trailing member shifts them).
    // NOTE: CharTransDraw.cpp has NO pinned .text in splits.txt, so this fix is
    // metric-INVISIBLE (measured Dmatched +0 with 4 real leg-B recompiles).  It
    // is landed for correctness and to unblock a future pin.

protected:
    CharTransDraw();
};
