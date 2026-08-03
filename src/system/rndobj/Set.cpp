#include "rndobj/Set.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/PropSync.h"
#include "utl/BinStream.h"

// ---------------------------------------------------------------------------
// Retail RB3 stores the load revision in file-scope statics gRev/gAltRev written
// at load time, i.e. the obj/ObjMacros.h rev dialect -- not obj/Object.h's local
// BinStreamRev wrapper.  Proven from retail asm for ?Load@RndSet@@ (target 124 B):
// after `bl BinStream::ReadEndian` it does
//     mr r10,r11 ; srwi r11,r11,16 ; sth r11,gAltRev ; sth r10,gRev
// and then `bl Hmx::Object::Load` with r4 = the ORIGINAL bs.  There is no
// BinStream ctor/dtor pair and no ??_7BinStreamRev@@6B@ vtable store, which the
// Object.h dialect emits unconditionally (it cost 8 surplus bytes and the whole
// r28-r31 save set).  Same treatment, same reasoning, as ui/UILabel.cpp.
//
// Bracketed with push_macro/pop_macro so the dialect cannot leak into any TU that
// whole-file-#includes this one via the COMDAT-scatter lever.
// ---------------------------------------------------------------------------
#pragma push_macro("INIT_REVS")
#pragma push_macro("LOAD_REVS")
#pragma push_macro("ASSERT_REVS")
#pragma push_macro("LOAD_SUPERCLASS")
#undef INIT_REVS
#undef LOAD_REVS
#undef ASSERT_REVS
#undef LOAD_SUPERCLASS
// Declaration order is codegen-load-bearing: retail's .bss puts gAltRev at the
// base and gRev at +4 (`sth r11,gAltRev(r9)` then `sth r10,4(r8)`), and MSVC lays
// these two out in declaration order, so gAltRev must be declared first.
#define INIT_REVS(objType)                                                               \
    static unsigned short gAltRev = 0;                                                   \
    static unsigned short gRev = 0;
#define LOAD_REVS(bs)                                                                    \
    int rev;                                                                             \
    bs >> rev;                                                                           \
    gRev = getHmxRev(rev);                                                               \
    gAltRev = getAltRev(rev);
// retail has no version guard here, matching ObjMacros.h's non-VERSION_SZBE69_B8
// expansion.
#define ASSERT_REVS(rev1, rev2)
#define LOAD_SUPERCLASS(parent) parent::Load(bs);

RndSet::RndSet() : mObjects(this) {}

BEGIN_HANDLERS(RndSet)
    HANDLE(allowed_objects, OnAllowedObjects)
    HANDLE_SUPERCLASS(Hmx::Object)
    for (ObjPtrList<Hmx::Object>::iterator it = mObjects.begin(); it != mObjects.end();
         ++it) {
        (*it)->Handle(_msg, true);
    }
END_HANDLERS

BEGIN_PROPSYNCS(RndSet)
    SYNC_PROP(objects, mObjects)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
    if (_op == kPropSet) {
        for (ObjPtrList<Hmx::Object>::iterator it = mObjects.begin();
             it != mObjects.end();
             ++it) {
            (*it)->SetProperty(_prop, _val);
        }
    }
END_PROPSYNCS

BEGIN_SAVES(RndSet)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mObjects;
END_SAVES

BEGIN_COPYS(RndSet)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndSet)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mObjects)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(RndSet)

BEGIN_LOADS(RndSet)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    bs >> mObjects;
END_LOADS

void RndSet::SetTypeDef(DataArray *def) {
    Hmx::Object::SetTypeDef(def);
    if (def) {
        DataArray *cfg = TypeDef()->FindArray("editor");
        mProps.resize(cfg->Size() - 1);
        for (int i = 1; i < cfg->Size(); i++) {
            const DataArray *thisArr = cfg->Array(i);
            DataNode &thisNode = thisArr->Node(1);
            if (thisNode.Type() != kDataSymbol) {
                MILO_NOTIFY("%s not top-level property in %s", thisArr->Sym(0), Name());
            }
            mProps[i - 1] = thisArr->Sym(0);
        }
        ObjPtrList<Hmx::Object>::iterator it = mObjects.begin();
        while (it != mObjects.end()) {
            if (!AllowedObject(*it)) {
                MILO_NOTIFY("%s not allowed in set", (*it)->Name());
                it = mObjects.erase(it);
            } else
                ++it;
        }
    } else {
        mProps.clear();
    }
}

bool RndSet::AllowedObject(Hmx::Object *o) {
    if (!o || o == this)
        return false;
    else {
        for (int i = 0; i < mProps.size(); i++) {
            if (o->Property(mProps[i], false) == nullptr) {
                return false;
            }
        }
        return true;
    }
}

DataNode RndSet::OnAllowedObjects(DataArray *) {
    std::list<Hmx::Object *> objList;
    for (ObjDirItr<Hmx::Object> it(Dir(), true); it != 0; ++it) {
        if (AllowedObject(it))
            objList.push_back(it);
    }
    DataArrayPtr ptr(new DataArray(objList.size()));
    int count = 0;
    for (std::list<Hmx::Object *>::iterator it = objList.begin(); it != objList.end();
         it++) {
        ptr->Node(count++) = *it;
    }
    return ptr;
}

#pragma pop_macro("LOAD_SUPERCLASS")
#pragma pop_macro("ASSERT_REVS")
#pragma pop_macro("LOAD_REVS")
#pragma pop_macro("INIT_REVS")
