#include "Data.h"
#include "obj/DataUtl.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/Symbol.h"

DataNode *TypeProps::KeyValue(Symbol key, bool fail) const {
    if (mMap) {
        for (int i = mMap->Size() - 2; i >= 0; i -= 2) {
            // (int) casts produce signed cmpw; direct pointer compare produces unsigned cmplw
#ifdef HX_NATIVE
            if (mMap->Node(i).UncheckedStr() == key.Str()) {
#else
            if ((int)mMap->Node(i).UncheckedStr() == (int)key.Str()) {
#endif
                return &mMap->Node(i + 1);
            }
        }
    }
    if (fail) {
        MILO_FAIL("Key %s not found", key);
    }
    return nullptr;
}

void GetSaveFlags(DataArray *arr, bool &proxy, bool &none) {
    static Symbol proxy_save = "proxy_save";
    static Symbol no_save = "no_save";
    if (arr) {
        for (int i = 2; i < arr->Size(); i++) {
            if (arr->Sym(i) == proxy_save)
                proxy = true;
            else if (arr->Sym(i) == no_save)
                none = true;
            else
                MILO_NOTIFY("Unknown type def attribute %s", arr->Sym(i));
        }
    }
}

// Indexed access over the flat (k0 v0 k1 v1 ...) map: pair idx is nodes 2*idx, 2*idx+1.
Symbol TypeProps::Key(int idx) const { return mMap->Node(idx * 2).UncheckedSym(); }

DataNode &TypeProps::Value(int idx) const { return mMap->Node(idx * 2 + 1); }

#ifdef HX_NATIVE
namespace {
    // Native image of retail's per-value `Hmx::Object::Release(old, this)` in
    // the setters (SetKeyValue fn_82765D40 +0x170, SetArrayValue fn_827665D0
    // +0x48, RemoveArrayValue fn_82766650 +0x48). Retail Release
    // (fn_8275B378) walks the object's ring, erases the FIRST entry owned by
    // `this` (compare +0x58, erase +0x7C) and returns: one ring entry per
    // overwritten value. Natively each held value is one node of mObjects (an
    // owner-control ObjPtrList), so the image is to free exactly one node
    // holding `obj`. ObjPtrList::remove(obj) frees EVERY such node (retail
    // 0x82452950 keeps walking), which dropped the ring entry of any other
    // prop holding the same object, and that prop then dangled when the
    // object died. Lane A33.
    void ReleaseOneTypePropsNode(ObjPtrList<Hmx::Object> &objects, Hmx::Object *obj) {
        ObjPtrList<Hmx::Object>::iterator it = objects.find(obj);
        if (it != objects.end())
            objects.erase(it);
    }
}
#endif

int TypeProps::Size() const {
    if (mMap)
        return mMap->Size() / 2;
    else
        return 0;
}

void TypeProps::ClearKeyValue(Symbol key) {
    if (mMap) {
        int i = mMap->Size() - 2;
        while (i >= 0) {
#ifdef HX_NATIVE
            if (mMap->Node(i).UncheckedStr() == key.Str()) {
#else
            if ((int)mMap->Node(i).UncheckedStr() == (int)key.Str()) {
#endif
                DataNode &val = mMap->Node(i + 1);
                if (val.Type() == kDataObject) {
                    Hmx::Object *obj = val.UncheckedObj();
                    if (obj) {
#ifdef HX_NATIVE
                        // No TU5 body in this unit; one node per value, as
                        // in the setters below.
                        ReleaseOneTypePropsNode(mObjects, obj);
#endif
                    }
                }
                mMap->Remove(i);
                mMap->Remove(i);
                if (mMap->Size() == 0 && mMap) {
                    mMap->Release();
                    mMap = nullptr;
                }
                return;
            }
            i -= 2;
        }
    }
}

void TypeProps::SetKeyValue(Symbol key, const DataNode &value, bool b) {
    if (b && value.Type() == kDataObject) {
        Hmx::Object *o = value.UncheckedObj();
        if (o) {
#ifdef HX_NATIVE
            mObjects.push_back(o);
#else
            o->AddRef(this); // retail: the TypeProps is the ObjRefOwner
#endif
        }
    }
    if (!mMap) {
        mMap = new DataArray(2);
        mMap->Node(0) = key;
        mMap->Node(1) = value;
    } else {
        int nodeCnt = mMap->Size();
        for (int cnt = nodeCnt - 2; cnt >= 0; cnt -= 2) {
#ifdef HX_NATIVE
            const char *symstr = mMap->Node(cnt).UncheckedStr();
            if (symstr == key.Str()) {
#else
            // key.Str() is read in the compare, not into a named local: a separate
            // local re-orders the node-address add (offset, base) against retail.
            int symstr = (int)mMap->Node(cnt).UncheckedStr();
            if (symstr == (int)key.Str()) {
#endif
                DataNode &valNode = mMap->Node(cnt + 1);
                if (valNode.Type() == kDataObject) {
                    Hmx::Object *o = valNode.UncheckedObj();
                    if (o) {
#ifdef HX_NATIVE
                        ReleaseOneTypePropsNode(mObjects, o);
#else
                        o->Release(this);
#endif
                    }
                }
                valNode = value;
                return;
            }
        }
        mMap->Resize(nodeCnt + 2);
        mMap->Node(nodeCnt) = key;
        mMap->Node(nodeCnt + 1) = value;
    }
}

void TypeProps::ReplaceObject(DataNode &n, Hmx::Object *from, Hmx::Object *to) {
    Hmx::Object *fromObj = n.UncheckedObj();
    if (fromObj == from) {
#ifdef HX_NATIVE
        // The ring bookkeeping (retail's Release +0x2C / AddRef +0x6C) is done
        // once per ring step in Replace, on the mObjects node that is the
        // step's `from`; see there.
#else
        // retail (0x82766338): the property's ring ref moves with the value
        fromObj->Release(this);
#endif
        n = to;
        if (to) {
#ifndef HX_NATIVE
            to->AddRef(this);
#endif
        }
    }
}

#ifdef HX_NATIVE
namespace {
    // Native half of a TypeProps ring step. Natively a typed prop's object is
    // held by a node of TypeProps::mObjects (an owner-control ObjPtrList), and
    // the ring hands TypeProps::Replace that NODE as `from` (ObjPtrList::
    // ReplaceNode, obj/ObjPtr_p.h). Retail has one ring entry per held value
    // (AddRef(this) per value) and moves it with the value: Release from +0x2C,
    // AddRef to +0x6C (fn_82766338). The native image of that is to retarget
    // this step's node to `to`, keeping one node per held value.
    //
    // The node is RETARGETED, never freed, even when `to` is null: this runs
    // inside the node's own Replace (Node::Replace -> ReplaceNode -> here), and
    // the walks that drive it -- ReplaceRefs' snapshot, NullifyAllRefs'
    // `pending` drain, ReplaceList -- must not meet a node freed under them.
    // Set() unlinks the node from the dying object's ring (that is what makes
    // the drain and ReplaceList advance) and, for a non-null `to`, links it into
    // `to`'s ring. A null node holds nothing and sits on no ring; it is freed
    // with the rest at the next ReleaseObjects (ClearAll, Load, operator=). Only
    // `from` is touched: another node holding the same object (the same object
    // in two props) is its own ring entry and gets its own step.
    void RetargetTypePropsNode(ObjPtrList<Hmx::Object> &objects, ObjRef *from, Hmx::Object *to) {
        for (ObjPtrList<Hmx::Object>::iterator it = objects.begin(); it != objects.end(); ++it) {
            if (static_cast<ObjRef *>(it.mNode) == from) {
                objects.Set(it, to);
                return;
            }
        }
    }
}
#endif

void TypeProps::Replace(ObjRef *from, Hmx::Object *to) {
#ifdef HX_NATIVE
    // Retail fn_827663D8 walks every value (kDataObject -> ReplaceObject +0x64;
    // kDataArray -> each kDataObject element, ReplaceObject +0xA8), and
    // ReplaceObject (fn_82766338) compares the held object with `from` (+0x20,
    // `cmplw cr6,r3,r5`) -- on X360 the dying object. Natively `from` is the
    // mObjects node (see RetargetTypePropsNode), so that compare never matched:
    // the value kept the dying object's address and the node stayed aimed at
    // it, for ClearAll's mObjects.clear() to Release on freed memory. Natively
    // the dying object is the one the node still holds. Lane A27.
    Hmx::Object *fromObj = from ? from->GetObj() : nullptr;
    if (!fromObj)
        return; // a null node: a null value must not "match" it
    RetargetTypePropsNode(mObjects, from, to);
    if (!mMap)
        return;
#else
    if (!mMap)
        return;
    // X360: `from` IS the dying object (see RefIs in Object.h + Object.cpp:192).
    // ObjRef::GetObj() is an elided stub returning nullptr off HX_NATIVE.
    Hmx::Object *fromObj = reinterpret_cast<Hmx::Object *>(from);
#endif
    for (int i = mMap->Size() - 1; i > 0; i -= 2) {
        DataNode &node = mMap->Node(i);
        if (node.Type() == kDataObject) {
            ReplaceObject(node, fromObj, to);
        } else if (node.Type() == kDataArray) {
            DataArray *inner = node.UncheckedArray();
            for (int j = inner->Size() - 1; j >= 0; j--) {
                DataNode &node2 = inner->Node(j);
                if (node2.Type() == kDataObject) {
                    ReplaceObject(node2, fromObj, to);
                }
            }
        }
    }
}

void TypeProps::ReleaseObjects() {
#ifdef HX_NATIVE
    if (mMap)
        mObjects.clear();
#else
    // Retail X360: no mObjects side-list; release each embedded object's ref
    // to this TypeProps (as an ObjRefOwner) directly via the ring.
    if (mMap) {
        for (int i = mMap->Size() - 1; i > 0; i -= 2) {
            DataNode &node = mMap->Node(i);
            if (node.Type() == kDataObject) {
                Hmx::Object *obj = node.UncheckedObj();
                if (obj) {
                    obj->Release(this);
                }
            } else if (node.Type() == kDataArray) {
                DataArray *inner = node.UncheckedArray();
                for (int j = inner->Size() - 1; j >= 0; j--) {
                    DataNode &node2 = inner->Node(j);
                    if (node2.Type() == kDataObject) {
                        Hmx::Object *obj = node2.UncheckedObj();
                        if (obj)
                            obj->Release(this);
                    }
                }
            }
        }
    }
#endif
}

void TypeProps::AddRefObjects() {
#ifdef HX_NATIVE
    if (mMap) {
        for (int i = mMap->Size() - 1; i > 0; i -= 2) {
            DataNode &node = mMap->Node(i);
            if (node.Type() == kDataObject) {
                Hmx::Object *obj = node.UncheckedObj();
                if (obj) {
                    mObjects.push_back(obj);
                }
            } else if (node.Type() == kDataArray) {
                DataArray *inner = node.UncheckedArray();
                for (int j = inner->Size() - 1; j >= 0; j--) {
                    DataNode &node2 = inner->Node(j);
                    if (node2.Type() == kDataObject) {
                        Hmx::Object *obj = node2.UncheckedObj();
                        if (obj)
                            mObjects.push_back(obj);
                    }
                }
            }
        }
    }
#else
    // Retail X360: no mObjects side-list; add each embedded object's ref
    // to this TypeProps (as an ObjRefOwner) directly via the ring.
    if (mMap) {
        for (int i = mMap->Size() - 1; i > 0; i -= 2) {
            DataNode &node = mMap->Node(i);
            if (node.Type() == kDataObject) {
                Hmx::Object *obj = node.UncheckedObj();
                if (obj) {
                    obj->AddRef(this);
                }
            } else if (node.Type() == kDataArray) {
                DataArray *inner = node.UncheckedArray();
                for (int j = inner->Size() - 1; j >= 0; j--) {
                    DataNode &node2 = inner->Node(j);
                    if (node2.Type() == kDataObject) {
                        Hmx::Object *obj = node2.UncheckedObj();
                        if (obj)
                            obj->AddRef(this);
                    }
                }
            }
        }
    }
#endif
}

#include "rndobj/Lit.h"

void TypeProps::ClearAll() {
    ReleaseObjects();
    if (mMap) {
        mMap->Release();
        mMap = nullptr;
    }
}

DataArray *TypeProps::GetArray(Symbol prop, DataArray *typeDef) {
    DataNode *n = KeyValue(prop, false);
    DataArray *ret;
    if (!n) {
#ifdef HX_NATIVE
        if (!typeDef) {
            MILO_WARN("TypeProps::GetArray: %s has no TypeDef for key %s", PathName(mOwner), prop);
            return nullptr;
        }
#endif
        MILO_ASSERT(typeDef, 0x18);
        DataArray *keyArray = typeDef->FindArray(prop);
        DataArray *cloned = keyArray->Array(1)->Clone(true, false, 0);
        SetKeyValue(prop, cloned, true);
        ret = cloned;
        cloned->Release();
    } else {
        MILO_ASSERT(n->Type() == kDataArray, 0x1F);
        ret = n->UncheckedArray();
    }
    return ret;
}

void TypeProps::SetArrayValue(Symbol key, int i, const DataNode &value, DataArray *typeDef) {
    DataArray *arr = GetArray(key, typeDef);
#ifdef HX_NATIVE
    if (!arr) {
        MILO_WARN("TypeProps::SetArrayValue: null array for key %s", key);
        return;
    }
#endif
    DataNode &n = arr->Node(i);
    if (n.Type() == kDataObject) {
        Hmx::Object *obj = n.UncheckedObj();
        if (obj) {
#ifdef HX_NATIVE
            ReleaseOneTypePropsNode(mObjects, obj);
#else
            obj->Release(this); // retail X360: the TypeProps is the ObjRefOwner
#endif
        }
    }
    n = value;
    if (n.Type() == kDataObject) {
        Hmx::Object *obj = n.UncheckedObj();
        if (obj) {
#ifdef HX_NATIVE
            mObjects.push_back(obj);
#else
            obj->AddRef(this);
#endif
        }
    }
}

void TypeProps::RemoveArrayValue(Symbol prop, int i, DataArray *typeDef) {
    DataArray *a = GetArray(prop, typeDef);
    DataNode &n = a->Node(i);
    if (n.Type() == kDataObject) {
        Hmx::Object *obj = n.UncheckedObj();
        if (obj) {
#ifdef HX_NATIVE
            ReleaseOneTypePropsNode(mObjects, obj);
#else
            obj->Release(this); // retail X360: the TypeProps is the ObjRefOwner
#endif
        }
    }
    a->Remove(i);
}

void TypeProps::InsertArrayValue(Symbol key, int i, const DataNode &value, DataArray *typeDef) {
    DataArray *arr = GetArray(key, typeDef);
    arr->Insert(i, value);
    if (value.Type() == kDataObject) {
        Hmx::Object *obj = value.UncheckedObj();
        if (obj) {
#ifdef HX_NATIVE
            mObjects.push_back(obj);
#else
            obj->AddRef(this);
#endif
        }
    }
}

void TypeProps::Load(BinStream &bs, bool oldRev) {
    ReleaseObjects();
    DataArray *def = RefOwner()->TypeDef();
    Hmx::Object *theThis = nullptr;
    if (def)
        theThis = DataSetThis(mOwner);
    if (mMap && gLoadingProxyFromDisk && def) {
        DataArray *oldMap = mMap;
        bs >> mMap;
        int oldMapSize = oldMap->Size();
        for (int i = 0; i < oldMapSize; i += 2) {
            Symbol val = oldMap->Sym(i);
            if (oldRev) {
                bool proxy = false;
                bool none = false;
                GetSaveFlags(def->FindArray(val, false), proxy, none);
                if (proxy || none)
                    continue;
            }
            SetKeyValue(val, oldMap->Node(i + 1), false);
        }
        oldMap->Release();
    } else {
        if (mMap) {
            mMap->Release();
            mMap = nullptr;
        }
        bs >> mMap;
    }
    if (def) {
        // Retail (fn_827661C0) has no edit-mode outdated-property report.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
        if (mMap && TheLoadMgr.EditMode()) {
            for (int i = 0; mMap && i < mMap->Size(); i += 2) {
                DataArray *found = def->FindArray(mMap->Sym(i), false);
                if (found && found->Type(1) != kDataCommand) {
                    if (!found->Node(1).CompatibleType(mMap->Type(i + 1))) {
                        MILO_LOG(
                            "%s: type based property \"%s\" is outdated, will clear on save\n",
                            PathName(mOwner),
                            mMap->Sym(i)
                        );
                    }
                }
            }
        }
#endif
        DataSetThis(theThis);
        AddRefObjects();
    }
}

TypeProps &TypeProps::operator=(const TypeProps &t) {
    ClearAll();
    if (t.mMap) {
        mMap = t.mMap->Clone(true, false, 0);
    }
    AddRefObjects();
    return *this;
}

// W16-TW: native compiles this retail body too. It used a DC3 editor-era arm
// (EditMode type-check pass, EditorDir key stashing) that never advanced its key
// index past an object-valued key whose object is null or has no dir (measured:
// DirLoader::SaveObjects on the shipped ui/main/gen/attract_overlay.milo spun
// forever in it), wrote kept keys into the type definition's own array (it reused
// the FindArray result as the output array), and re-inserted stashed keys after
// their values. RB3 has no editor, so retail's body is the native one as well.
// Retail 0x82765F40: no EditMode type-check pass and no EditorDir key stashing.
// Only a data-dir owner (or a proxy load) filters its keys through the type
// def's save flags; keys the type def does not declare are dropped.
void TypeProps::Save(BinStream &bs) {
    Hmx::Object *owner = RefOwner();
    if (!mMap || owner->DataDir() != owner
        || owner == owner->Dir() && !gLoadingProxyFromDisk) {
        bs << mMap;
        return;
    }
    DataArray *typeDef = owner->TypeDef();
    if (!typeDef) {
        bs << (DataArray *)nullptr;
        return;
    }
    DataArray *arrToWrite = nullptr;
    int keyIdx = 0;
    for (int i = 0; i < mMap->Size(); i += 2) {
        Symbol key = mMap->Sym(i);
        DataArray *def = typeDef->FindArray(key, false);
        if (def) {
            bool proxy = false;
            bool none = false;
            GetSaveFlags(def, proxy, none);
            if (!none && proxy != gLoadingProxyFromDisk) {
                if (!arrToWrite) {
                    arrToWrite = new DataArray(mMap->Size());
                }
                arrToWrite->Node(keyIdx) = key;
                arrToWrite->Node(keyIdx + 1) = mMap->Node(i + 1);
                keyIdx += 2;
            }
        }
    }
    if (arrToWrite) {
        arrToWrite->Resize(keyIdx);
        bs << arrToWrite;
        arrToWrite->Release();
    } else {
        bs << (DataArray *)nullptr;
    }
}
