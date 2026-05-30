#pragma once
#include "Object.h"
#include "obj/Dir.h" /* IWYU pragma: keep */
#include "obj/Object.h"
#include "utl/BinStream.h" /* IWYU pragma: keep */
#include "os/Debug.h" /* IWYU pragma: keep */
#include "utl/PoolAlloc.h"
#include <algorithm> /* IWYU pragma: keep */
#include <cstddef> /* IWYU pragma: keep */

// DO NOT try to include this header directly!
// include obj/Object.h instead

#pragma region ObjRefConcrete
// ------------------------------------------------
// ObjRefConcrete
// ------------------------------------------------

#ifdef HX_NATIVE
template <class T1, class T2>
ObjRefConcrete<T1, T2>::ObjRefConcrete(T1 *obj) : mObject(obj) {
    if (mObject)
        mObject->AddRef(this);
}

template <class T1, class T2>
__forceinline ObjRefConcrete<T1, T2>::ObjRefConcrete(const ObjRefConcrete &o)
    : mObject(o.mObject) {
    if (mObject)
        mObject->AddRef(this);
}

template <class T1, class T2>
ObjRefConcrete<T1, T2>::~ObjRefConcrete() {
    if (mObject) {
        if (ObjectDir::InDeleteObjects() || Hmx::Object::sRingsDirty) {
            SafeReleaseFromRing(this);
            mObject = nullptr;
            return;
        }
        mObject->Release(this);
    }
}

template <class T1, class T2>
void ObjRefConcrete<T1, T2>::SetObjConcrete(T1 *obj) {
    if (mObject) {
        if (ObjectDir::InDeleteObjects() || Hmx::Object::sRingsDirty) {
            SafeReleaseFromRing(this);
            goto skip_ring_ops;
        }
        mObject->Release(this);
    }
    mObject = obj;
    if (mObject) {
        mObject->AddRef(this);
    }
    return;
skip_ring_ops:
    mObject = obj;
    if (mObject) {
        mObject->AddRef(this);
    }
}

template <class T1, class T2>
void ObjRefConcrete<T1, T2>::CopyRef(const ObjRefConcrete &o) {
    if (&o == this) return;
    if (mObject) this->ObjRef::Release(nullptr);
    mObject = o.mObject;
    if (!mObject) return;
    this->ObjRef::AddRef(const_cast<ObjRef *>(static_cast<const ObjRef *>(&o)));
}
#else
// ----------------------------------------------------------------------------
// Retail X360: separate-pool-node ring. The ObjRefConcrete is the smart-pointer
// {vtable@0, mOwner@4, mObject@8}. The ring-ref passed to Hmx::Object::AddRef /
// Release is `this` (an ObjRefOwner).
//
// The base ctor ONLY stores mOwner/mObject — it does NOT AddRef. The AddRef
// belongs in the most-derived ctor (ObjPtr/ObjOwnerPtr/ObjDirPtr), AFTER the
// derived vtable is set (matching retail fn_8270B9A8: store fields, store the
// ObjPtr vtable, THEN AddRef). Doing AddRef in the base ctor would (a) emit it
// against the ObjRefConcrete vtable instead of the derived one and (b) make the
// base ctor the out-of-line body instead of ObjPtr's, so SampleZone/Instance
// would call ??0ObjRefConcrete and store the vtable inline (the 88.9% miss).
// ----------------------------------------------------------------------------
template <class T1, class T2>
ObjRefConcrete<T1, T2>::ObjRefConcrete(Hmx::Object *owner, T1 *obj)
    : mOwner(owner), mObject(obj) {}

template <class T1, class T2>
ObjRefConcrete<T1, T2>::ObjRefConcrete(const ObjRefConcrete &o)
    : mOwner(o.mOwner), mObject(o.mObject) {}

template <class T1, class T2>
ObjRefConcrete<T1, T2>::~ObjRefConcrete() {
    if (mObject)
        mObject->Release(this);
}

template <class T1, class T2>
void ObjRefConcrete<T1, T2>::SetObjConcrete(T1 *obj) {
    if (obj != mObject) {
        if (mObject)
            mObject->Release(this);
        mObject = obj;
        if (mObject)
            mObject->AddRef(this);
    }
}

template <class T1, class T2>
void ObjRefConcrete<T1, T2>::CopyRef(const ObjRefConcrete &o) {
    SetObjConcrete(o.mObject);
}
#endif

template <class T1, class T2>
Hmx::Object *ObjRefConcrete<T1, T2>::SetObj(Hmx::Object *root_obj) {
    T1 *obj = root_obj ? dynamic_cast<T1 *>(root_obj) : nullptr;
    SetObjConcrete(obj);
    return mObject;
}

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjRefConcrete<T1, class ObjectDir> &f) {
    MILO_ASSERT(f.RefOwner(), 0x4D1);
    T1 *obj = f;
    const char *objName = obj ? obj->Name() : "";
    bs << objName;
    return bs;
}

template <class T1, class T2>
bool ObjRefConcrete<T1, T2>::Load(BinStream &bs, bool print, ObjectDir *dir) {
    char buf[128];
    bs.ReadString(buf, 128);
    Hmx::Object *refOwner = RefOwner();
    if (!dir && refOwner) {
        dir = refOwner->Dir();
    }
#ifdef HX_NATIVE
    // On native, allow dir-only lookup (no refOwner needed) so ObjPtrs
    // with null owners (e.g. Font3d CharInfo::mMesh) can resolve when
    // the caller passes an explicit dir.
    if (dir) {
#else
    if (refOwner && dir) {
#endif
        SetObj(dir->FindObject(buf, false, true));
#ifdef HX_NATIVE
        // Native fallback: walk up the parent dir chain when not found locally.
        // On Xbox, FileMerger flattens all objects into the same scope.
        // On native, the merge pipeline is incomplete so objects may live in
        // a parent dir that isn't reachable with parentDirs=false.
        if (!mObject && buf[0] != '\0') {
            ObjectDir *searchDir = dir;
            while (!mObject && searchDir) {
                ObjectDir *nextDir = nullptr;
                if (searchDir->Dir() && searchDir->Dir() != searchDir) {
                    nextDir = searchDir->Dir();
                } else if (searchDir->Loader() && searchDir->Loader()->ParentDir()) {
                    nextDir = searchDir->Loader()->ParentDir();
                }
                if (!nextDir || nextDir == searchDir) break;
                SetObj(nextDir->FindObject(buf, false, true));
                searchDir = nextDir;
            }
            if (!mObject) {
                SetObj(ObjectDir::Main()->FindObject(buf, false, true));
            }
        }
#endif
        if (!mObject && buf[0] != '\0') {
            if (print) {
                MILO_NOTIFY(
                    "%s couldn't find %s in %s", PathName(refOwner), buf, PathName(dir)
                );
            }
            return false;
        }
    } else {
        if (mObject) {
            mObject->Release(this);
        }
        mObject = nullptr;
        if (buf[0] != '\0') {
            if (print)
                MILO_NOTIFY("No dir to find %s", buf);
        }
    }
    return true;
}

#pragma endregion
#pragma region ObjPtr
// ------------------------------------------------
// ObjPtr
// ------------------------------------------------

#ifdef HX_NATIVE
template <class T>
ObjPtr<T>::ObjPtr(Hmx::Object *owner, T *ptr) : ObjRefConcrete<T>(ptr), mOwner(owner) {}

template <class T>
ObjPtr<T>::ObjPtr(const ObjPtr &p) : ObjRefConcrete<T>(p), mOwner(p.mOwner) {}

template <class T>
ObjPtr<T>::~ObjPtr() {}
#else
// Retail X360 (fn_8270B9A8): the base ObjRefConcrete ctor stores mOwner/mObject
// (and the vtable is set to ObjPtr's during this derived ctor); then AddRef(this)
// runs HERE so the ring-ref recorded is `this` with the ObjPtr vtable already in
// place. Keeping AddRef in the derived ctor makes ObjPtr's ctor the out-of-line
// body (callers do `bl fn_8270B9A8`, not an inline vtable store + base-ctor call).
template <class T>
ObjPtr<T>::ObjPtr(Hmx::Object *owner, T *ptr) : ObjRefConcrete<T>(owner, ptr) {
    if (this->mObject)
        this->mObject->AddRef(this);
}

template <class T>
ObjPtr<T>::ObjPtr(const ObjPtr &p) : ObjRefConcrete<T>(p) {
    if (this->mObject)
        this->mObject->AddRef(this);
}

template <class T>
ObjPtr<T>::~ObjPtr() {}
#endif

template <class T>
BinStream &operator>>(BinStream &bs, ObjPtr<T> &ptr) {
    ptr.Load(bs, true, nullptr);
    return bs;
}

#pragma endregion
#pragma region ObjOwnerPtr
// ------------------------------------------------
// ObjOwnerPtr
// ------------------------------------------------

#ifdef HX_NATIVE
template <class T>
ObjOwnerPtr<T>::ObjOwnerPtr(ObjRefOwner *owner, T *ptr)
    : ObjRefConcrete<T>(ptr), mOwner(owner) {
    MILO_ASSERT(owner, 0xC8);
}

template <class T>
ObjOwnerPtr<T>::ObjOwnerPtr(const ObjOwnerPtr &o)
    : ObjRefConcrete<T>(o.mObject), mOwner(o.mOwner) {
    MILO_ASSERT(mOwner, 0xCE);
}

template <class T>
ObjOwnerPtr<T>::~ObjOwnerPtr() {}

template <class T>
Hmx::Object *ObjOwnerPtr<T>::RefOwner() const {
    return mOwner->RefOwner();
}
#else
// Retail X360: the ring-ref is mOwner (an ObjRefOwner), NOT this. We pass a null
// object to the base ctor (so it does not AddRef(this)), then AddRef(mOwner).
// The base ctor stores mOwner (as Hmx::Object*, reinterpreted) and mObject.
template <class T>
ObjOwnerPtr<T>::ObjOwnerPtr(ObjRefOwner *owner, T *ptr)
    : ObjRefConcrete<T>(reinterpret_cast<Hmx::Object *>(owner), nullptr) {
    mObject = ptr;
    if (mObject)
        mObject->AddRef(owner);
}

template <class T>
ObjOwnerPtr<T>::ObjOwnerPtr(const ObjOwnerPtr &o)
    : ObjRefConcrete<T>(o.mOwner, nullptr) {
    mObject = o.mObject;
    if (mObject)
        mObject->AddRef(OwnerRef());
}

template <class T>
ObjOwnerPtr<T>::~ObjOwnerPtr() {
    if (mObject)
        mObject->Release(OwnerRef());
    // Prevent the base dtor from releasing again with the wrong (this) ring-ref.
    mObject = nullptr;
}

template <class T>
void ObjOwnerPtr<T>::SetOwnerObj(T *obj) {
    if (obj != mObject) {
        if (mObject)
            mObject->Release(OwnerRef());
        mObject = obj;
        if (mObject)
            mObject->AddRef(OwnerRef());
    }
}
#endif

// template <class T1>
// BinStream &operator<<(BinStream &bs, const ObjOwnerPtr<T1> &ptr);

template <class T1>
BinStream &operator>>(BinStream &bs, ObjOwnerPtr<T1> &ptr) {
    ptr.Load(bs, true, nullptr);
    return bs;
}

#pragma endregion
#pragma region ObjPtrVec
// ------------------------------------------------
// ObjPtrVec
// ------------------------------------------------

template <class T1, class T2>
ObjPtrVec<T1, T2>::ObjPtrVec(Hmx::Object *owner, EraseMode e, ObjListMode o)
    : mOwner(owner), mEraseMode(e), mListMode(o) {
    MILO_ASSERT(owner, 0x321);
}

template <class T1, class T2>
ObjPtrVec<T1, T2>::ObjPtrVec(const ObjPtrVec &other)
    : mOwner(other.mOwner), mEraseMode(other.mEraseMode), mListMode(other.mListMode) {
    *this = other;
}

template <class T1, class T2>
ObjPtrVec<T1, T2>::Node::Node(const Node &n)
    : ObjRefConcrete<T1, T2>(n), mVecOwner(n.mVecOwner) {
#ifndef HX_NATIVE
    // X360: the base copy ctor only stores fields (AddRef moved to derived ctors),
    // so register this Node as a ring-ref here to preserve ring membership.
    if (this->mObject)
        this->mObject->AddRef(this);
#endif
}

template <class T1, class T2>
ObjPtrVec<T1, T2>::~ObjPtrVec() {
    mNodes.clear();
}

template <class T1, class T2>
void ObjPtrVec<T1, T2>::ReplaceNode(Node *n, Hmx::Object *obj) {
    if (mListMode == kObjListOwnerControl) {
#ifdef HX_NATIVE
        mOwner->Replace(n, obj);
#else
        mOwner->Replace(reinterpret_cast<ObjRef *>(n), obj);
#endif
    } else {
        Hmx::Object *oldObj = n->SetObj(obj);
        if (!oldObj && mListMode == kObjListNoNull) {
#ifdef HX_NATIVE
            // During ReplaceList, erasing from the vector shifts subsequent
            // nodes via CopyRef, which modifies ring prev/next pointers and
            // corrupts the ring walk. Suppress the erase; the null entry is
            // cleaned up when the vector is destroyed or iterated.
            if (!gInReplaceList) {
                erase(iterator(mNodes.begin() + (n - mNodes.data())));
            } else {
                MILO_WARN("ReplaceNode: suppressed erase during ReplaceList (owner=%s)",
                    mOwner ? PathName(mOwner) : "<null>");
            }
#else
            erase(n);
#endif
        }
#ifdef HX_NATIVE
        else if (obj && mListMode == kObjListNoNull) {
            // MergeObject redirects refs from o1→o2. If vec had refs to both,
            // both Nodes now point to o2 (duplicate). Erase the duplicate to
            // prevent stale-pointer dereference during later destruction.
            T1 *typed = dynamic_cast<T1 *>(obj);
            if (typed) {
                for (size_t i = 0; i < mNodes.size(); i++) {
                    if (&mNodes[i] != n && mNodes[i].Obj() == typed) {
                        n->SetObj(nullptr);
                        if (!gInReplaceList)
                            erase(iterator(mNodes.begin() + (n - mNodes.data())));
                        break;
                    }
                }
            }
        }
#endif
    }
}

template <class T1, class T2>
void ObjPtrVec<T1, T2>::Set(iterator it, T1 *obj) {
    if (!obj && mListMode == 0) {
        erase(it);
    } else
        it->SetObjConcrete(obj);
}

// see Draw.cpp for this
template <class T1, class T2>
void ObjPtrVec<T1, T2>::operator=(const ObjPtrVec &other) {
    if (this == &other) return;
    mNodes.clear();
    mNodes.reserve(other.mNodes.size());
    for (const_iterator it = other.begin(); it != other.end(); ++it) {
        mNodes.push_back(Node(this));
        Set(begin() + (mNodes.size() - 1), *it);
    }
}

template <class T1, class T2>
void ObjPtrVec<T1, T2>::push_back(T1 *obj) {
    insert(end(), obj);
}

template <class T1, class T2>
typename ObjPtrVec<T1, T2>::iterator
ObjPtrVec<T1, T2>::insert(typename ObjPtrVec<T1, T2>::const_iterator it, T1 *obj) {
    if (obj != 0 || mListMode != kObjListNoNull) {
#ifdef HX_NATIVE
        int idx = it.it - mNodes.begin();
#else
        // MSVC iterators can be null (zero-initialized); GCC iterators cannot.
        int idx = it.it ? (it.it - mNodes.begin()) : 0;
#endif
        Node newNode(this);
        typename std::vector<Node>::iterator pos = mNodes.begin() + idx;
        mNodes.insert(pos, 1, newNode);
        Set(begin() + idx, obj);
    }
#ifdef HX_NATIVE
    return iterator(mNodes.begin() + (it.it - mNodes.cbegin()));
#else
    return iterator(const_cast<typename std::vector<Node>::iterator>(it.it));
#endif
}

template <class T1, class T2>
bool ObjPtrVec<T1, T2>::Load(BinStream &bs, bool print, ObjectDir *dir) {
    bool ret = true;
    mNodes.clear();
    int count;
    bs >> count;
    mNodes.reserve(count);
    if (!dir && mOwner) {
        dir = mOwner->Dir();
    }
    if (print) {
        MILO_ASSERT(dir, 0x488);
    }
    while (count != 0U) {
        char buf[0x80];
        bs.ReadString(buf, 0x80);
        if (dir) {
            T1 *casted = dynamic_cast<T1 *>(dir->FindObject(buf, false, true));
#ifdef HX_NATIVE
            // Native fallback: walk up the parent dir chain when not found locally.
            // On Xbox, FileMerger flattens all objects into the same scope.
            // On native, the merge pipeline is incomplete so objects may live in
            // a parent dir that isn't reachable with parentDirs=false.
            if (!casted && buf[0] != '\0') {
                ObjectDir *searchDir = dir;
                while (!casted && searchDir) {
                    ObjectDir *nextDir = nullptr;
                    if (searchDir->Dir() && searchDir->Dir() != searchDir) {
                        nextDir = searchDir->Dir();
                    } else if (searchDir->Loader() && searchDir->Loader()->ParentDir()) {
                        nextDir = searchDir->Loader()->ParentDir();
                    }
                    if (!nextDir || nextDir == searchDir) break;
                    casted = dynamic_cast<T1 *>(nextDir->FindObject(buf, false, true));
                    searchDir = nextDir;
                }
                if (!casted) {
                    casted = dynamic_cast<T1 *>(
                        ObjectDir::Main()->FindObject(buf, false, true));
                }
            }
#endif
            if (!casted && buf[0] != '\0') {
                if (print)
                    MILO_NOTIFY(
                        "%s couldn't find %s in %s", PathName(mOwner), buf, PathName(dir)
                    );
                ret = false;
            } else if (casted || mListMode != kObjListNoNull) {
                push_back(casted);
            }
        }
        count--;
    }
    return ret;
}

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjPtrVec<T1, ObjectDir> &c) {
    bs << c.size();
    MILO_ASSERT(c.Owner(), 0x525);
    for (auto it = c.begin(); it != c.end(); ++it) {
        if (*it) {
            bs << (*it)->Name();
        } else {
            bs << "";
        }
    }
    return bs;
}

template <class T1>
BinStream &operator>>(BinStream &bs, ObjPtrVec<T1, ObjectDir> &vec) {
    vec.Load(bs, true, nullptr);
    return bs;
}

// ObjPtrVec::find, ::swap, ::sort bodies are in ObjPtrVec_impl.h
// to keep them out of the PCH (inlining budget pollution).
// Include ObjPtrVec_impl.h in .cpp files that call these methods.
#ifdef HX_NATIVE
#include "obj/ObjPtrVec_impl.h"
#endif

#pragma endregion
#pragma region ObjPtrList
// ------------------------------------------------
// ObjPtrList
// ------------------------------------------------

template <class T1, class T2>
ObjPtrList<T1, T2>::ObjPtrList(ObjRefOwner *owner, ObjListMode mode)
    : mSize(0), mNodes(nullptr), mOwner(owner), mListMode(mode) {
    if (mode == kObjListOwnerControl) {
        MILO_ASSERT(owner, 0x103);
    }
}

template <class T1, class T2>
ObjPtrList<T1, T2>::ObjPtrList(const ObjPtrList &other)
    : mSize(0), mNodes(nullptr), mOwner(other.mOwner), mListMode(other.mListMode) {
    for (iterator it = other.begin(); it != other.end(); ++it) {
        push_back(*it);
    }
}

#ifdef HX_NATIVE
template <class T1, class T2>
void *ObjPtrList<T1, T2>::Node::operator new(size_t s) {
    return PoolAlloc(s, s, __FILE__, 0x122, "ObjPtrList_node");
}

template <class T1, class T2>
void ObjPtrList<T1, T2>::Node::operator delete(void *v) {
    PoolFree(sizeof(Node), v, __FILE__, 0x122, "ObjPtrList_node");
}
#else
// X360 retail: 2-arg pool form (no debug info), matching POOL_OVERLOAD's retail
// X360 spelling and the AddObject evidence (fn_824410F0: PoolAlloc(0xc,0xc)).
template <class T1, class T2>
void *ObjPtrList<T1, T2>::Node::operator new(unsigned int s) {
    return PoolAlloc(s, s);
}

template <class T1, class T2>
void ObjPtrList<T1, T2>::Node::operator delete(void *v) {
    PoolFree(sizeof(Node), v);
}
#endif

#ifdef HX_NATIVE
template <class T1, class T2>
void ObjPtrList<T1, T2>::ReplaceNode(struct ObjPtrList::Node *node, Hmx::Object *obj) {
    if (mListMode == kObjListOwnerControl) {
        mOwner->Replace(static_cast<ObjRef *>(static_cast<ObjRefConcrete<T1, T2> *>(node)), obj);
    } else {
        Hmx::Object *old = static_cast<ObjRefConcrete<T1, T2> *>(node)->SetObj(obj);
        if (!old && mListMode == kObjListNoNull) {
            // During ReplaceList, erasing frees the node while other ObjRefs in
            // the ring still hold prev/next pointers to it. Subsequent ring
            // operations write through dangling pointers and corrupt glibc's
            // heap metadata ("corrupted double-linked list"). Suppress the erase;
            // the null entry persists until the list is destroyed or cleaned up.
            // Matches the guard in ObjPtrVec::ReplaceNode.
            if (!gInReplaceList) {
                erase(node);
            } else {
                MILO_WARN("ObjPtrList::ReplaceNode: suppressed erase during ReplaceList (owner=%s)",
                    mOwner ? PathName(dynamic_cast<Hmx::Object*>(mOwner)) : "<null>");
            }
        }
    }
}
#else
// X360 retail: the thin node has no SetObj/vtable; the LIST is the ring-ref.
// Do the ring ops directly on `this` (the list), mirroring rb3-Wii's Replace.
template <class T1, class T2>
void ObjPtrList<T1, T2>::ReplaceNode(struct ObjPtrList::Node *node, Hmx::Object *obj) {
    if (mListMode == kObjListOwnerControl) {
        // List-as-ref: the owner's Replace expects `from` = the dying
        // Hmx::Object* (same convention as ObjPtr::Replace), not the node.
        // Pass the currently-held object reinterpreted into the `from` slot.
        mOwner->Replace(reinterpret_cast<ObjRef *>(node->mObject), obj);
    } else if (!obj && mListMode == kObjListNoNull) {
        erase(node);
    } else {
        if (node->mObject)
            node->mObject->Release(this);
        node->mObject = dynamic_cast<T1 *>(obj);
        if (node->mObject)
            node->mObject->AddRef(this);
    }
}
#endif

template <class T1, class T2>
void ObjPtrList<T1, T2>::operator=(const ObjPtrList &other) {
    if (this == &other)
        return;
    while (mSize > other.mSize)
        pop_back();
    Node *otherNodes = other.mNodes;
    for (Node *n = mNodes; n != nullptr; n = n->next, otherNodes = otherNodes->next) {
#ifdef HX_NATIVE
        *n = *otherNodes;
#else
        // Thin X360 node has no operator=; replace the held object in place,
        // keeping the existing links, with list-as-ref Release/AddRef on `this`
        // (mirrors rb3-Wii operator= calling Set()).
        if (n->mObject)
            n->mObject->Release(this);
        n->mObject = otherNodes->mObject;
        if (n->mObject)
            n->mObject->AddRef(this);
#endif
    }
    for (; otherNodes != nullptr; otherNodes = otherNodes->next) {
        push_back(otherNodes->Obj());
    }
}

#ifndef HX_NATIVE
template <class T1, class T2>
T1 *ObjPtrList<T1, T2>::front() const {
    MILO_ASSERT(mNodes != NULL, 0x189);
    return mNodes->Obj();
}

template <class T1, class T2>
T1 *ObjPtrList<T1, T2>::back() const {
    MILO_ASSERT(mNodes != NULL, 0x18A);
    return mNodes->prev->Obj();
}
#endif

template <class T1, class T2>
void ObjPtrList<T1, T2>::pop_back() {
    MILO_ASSERT(mNodes != NULL, 0x18B);
    erase(mNodes->prev);
}

template <class T1, class T2>
void ObjPtrList<T1, T2>::pop_front() {
    MILO_ASSERT(mNodes != NULL, 0x18C);
    erase(mNodes);
}

template <class T1, class T2>
void ObjPtrList<T1, T2>::push_back(T1 *obj) {
    insert(end(), obj);
}

template <class T1, class T2>
typename ObjPtrList<T1, T2>::iterator
ObjPtrList<T1, T2>::insert(typename ObjPtrList<T1, T2>::iterator it, T1 *obj) {
    if (mListMode == kObjListNoNull) {
#ifdef HX_NATIVE
        // NullifyAllRefs can nullify ObjPtrList entries during async unload
        // or cascade Phase 0. Skip inserting null into no-null lists.
        if (!obj)
            return end();
#endif
        MILO_ASSERT(obj, 0x177);
    }
    Node *node = new Node();
#ifdef HX_NATIVE
    // Native: Node derives ObjRefConcrete; use SetObjConcrete so AddRef fires
    // on the node (the node IS the ring-ref in native mode).
    node->SetObjConcrete(obj);
#else
    // Thin X360 node has no SetObjConcrete; just store the raw pointer. Link()
    // performs the ring AddRef(this) (binary fn_826E8098 / rb3-Wii link()).
    node->mObject = obj;
#endif
    Link(it, node);
    return node;
}

template <class T1, class T2>
void ObjPtrList<T1, T2>::Set(iterator it, T1 *obj) {
#ifdef HX_NATIVE
    // Native: Node derives ObjRefConcrete which owns the ring-ref; use
    // SetObjConcrete so AddRef/Release fire on the node (not the list).
    it.mNode->SetObjConcrete(obj);
#else
    // Retail X360: thin pool node has no ring machinery; the LIST is the
    // ring-ref. Release/AddRef `this` (ObjRefOwner) directly.
    // Mirrors rb3-Wii ObjPtrList::Set (fn_80453DC4).
    Node *n = it.mNode;
    if (n->mObject)
        n->mObject->Release(this);
    n->mObject = obj;
    if (n->mObject)
        n->mObject->AddRef(this);
#endif
}

template <class T1, class T2>
inline typename ObjPtrList<T1, T2>::iterator
ObjPtrList<T1, T2>::find(const Hmx::Object *target) const {
    for (iterator it = begin(); it != end(); ++it) {
        if (*it == target)
            return it;
    }
    return end();
}

// TODO: not 100%, work on this
// addr: 0x825C6868
template <class T1, class T2>
bool ObjPtrList<T1, T2>::remove(T1 *target) {
    for (iterator it = begin(); it != end();) {
        auto old = it++;
        if (*old == target) {
            erase(old);
            return true;
        }
    }
    return false;
}

// remove a particular item inside iterator otherIt, from list otherList,
// and insert it into this list at the position indicated by thisIt
template <class T1, class T2>
void ObjPtrList<T1, T2>::MoveItem(
    iterator thisIt, ObjPtrList<T1, T2> &otherList, iterator otherIt
) {
    if (otherIt != thisIt) {
        otherList.Unlink(otherIt.mNode);
        Link(thisIt, otherIt.mNode);
    }
}

template <class T1, class T2>
bool ObjPtrList<T1, T2>::Load(BinStream &bs, bool print, ObjectDir *dir, bool b4) {
    bool ret = true;
    clear();
    int count;
    bs >> count;
    Hmx::Object *refOwner = mOwner ? mOwner->RefOwner() : nullptr;
    if (!dir && refOwner)
        dir = refOwner->Dir();
    if (print) {
        MILO_ASSERT(dir, 0x210);
    }
    while (count != 0U) {
        char buf[0x80];
        bs.ReadString(buf, 0x80);
        if (dir) {
            T1 *casted = dynamic_cast<T1 *>(dir->FindObject(buf, false, b4));
#ifdef HX_NATIVE
            if (!casted && buf[0] != '\0') {
                ObjectDir *searchDir = dir;
                while (!casted && searchDir) {
                    ObjectDir *nextDir = nullptr;
                    if (searchDir->Dir() && searchDir->Dir() != searchDir) {
                        nextDir = searchDir->Dir();
                    } else if (searchDir->Loader() && searchDir->Loader()->ParentDir()) {
                        nextDir = searchDir->Loader()->ParentDir();
                    }
                    if (!nextDir || nextDir == searchDir) break;
                    casted = dynamic_cast<T1 *>(nextDir->FindObject(buf, false, b4));
                    searchDir = nextDir;
                }
                if (!casted) {
                    casted = dynamic_cast<T1 *>(
                        ObjectDir::Main()->FindObject(buf, false, b4));
                }
            }
#endif
            if (!casted && buf[0] != '\0') {
                if (print)
                    MILO_NOTIFY(
                        "%s couldn't find %s in %s", PathName(refOwner), buf, PathName(dir)
                    );
                ret = false;
            } else if (casted) {
                push_back(casted);
            }
        }
        count--;
    }
    return ret;
}

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjPtrList<T1, ObjectDir> &c) {
    bs << c.size();
    MILO_ASSERT(c.Owner(), 0x4E1);
    FOREACH (it, c) {
        if (*it) {
            bs << (*it)->Name();
        } else {
            bs << "";
        }
    }
    return bs;
}

template <class T1>
BinStream &operator>>(BinStream &bs, ObjPtrList<T1, ObjectDir> &list) {
    list.Load(bs, true, nullptr, true);
    return bs;
}

template <class T1, class T2>
template <class S>
void ObjPtrList<T1, T2>::sort(const S &cmp) {
    if (mNodes && mNodes->next) {
        Node *sentinel = mNodes;
#ifdef HX_NATIVE
        // Native Link() creates NULL-terminated forward links (last->next = nullptr).
        // PPC Link() creates circular links (last->next = sentinel).
        for (Node *outer = sentinel->next; outer != nullptr; outer = outer->next) {
#else
        for (Node *outer = sentinel->next; outer != sentinel; outer = outer->next) {
#endif
            for (Node *inner = outer; inner != sentinel; inner = inner->prev) {
                Node *prev = inner->prev;
                if (cmp(inner->Obj(), prev->Obj())) {
                    // Both nodes stay in the list; set membership is unchanged,
                    // so just swap the held objects — no ring Release/AddRef.
                    // Mirrors rb3-Wii ObjPtrList::sort.
                    T1 *tmp = inner->mObject;
                    inner->mObject = prev->mObject;
                    prev->mObject = tmp;
                } else {
                    break;
                }
            }
        }
    }
}

#ifndef HX_NATIVE
// Xbox template implementations.
// These were originally explicit specializations in link_glue.cpp for each type.
// Providing generic template versions so the compiler can instantiate them.

template <class T1, class T2>
void ObjPtrList<T1, T2>::Link(iterator it, Node *node) {
    // List-as-ref: the LIST is the ring-ref, so AddRef `this` (not the thin
    // node) up front, before splicing. Matches binary fn_826E8098 and
    // rb3-Wii ObjPtrList::link().
    if (node->mObject)
        node->mObject->AddRef(this);
    node->next = it.mNode;
    if (it.mNode == mNodes) {
        if (mNodes) {
            node->prev = mNodes->prev;
            mNodes->prev = node;
        } else {
            node->prev = node;
        }
        mNodes = node;
    } else if (it.mNode == nullptr) {
        // Insert at end
        node->prev = mNodes->prev;
        mNodes->prev->next = node;
        mNodes->prev = node;
    } else {
        // Insert before a middle node
        node->prev = it.mNode->prev;
        it.mNode->prev->next = node;
        it.mNode->prev = node;
    }
    mSize++;
}

template <class T1, class T2>
typename ObjPtrList<T1, T2>::Node *ObjPtrList<T1, T2>::Unlink(Node *node) {
    MILO_ASSERT(node != NULL && mNodes != NULL, 0x26B);
    // List-as-ref: Release `this` (the ring-ref), not the thin node. The thin
    // node has no dtor, so erase()'s `delete node` only pool-frees — no double
    // release. Matches binary fn_823A2538 and rb3-Wii ObjPtrList::unlink().
    if (node->mObject)
        node->mObject->Release(this);
    if (node == mNodes) {
        if (mNodes->next != nullptr) {
            mNodes->next->prev = mNodes->prev;
            mNodes = mNodes->next;
        } else {
            mNodes = nullptr;
            mSize--;
            return nullptr;
        }
    } else if (node == mNodes->prev) {
        // Removing tail
        mNodes->prev = node->prev;
        mNodes->prev->next = nullptr;
        mSize--;
        return mNodes->prev;
    } else {
        // Middle node
        node->prev->next = node->next;
        node->next->prev = node->prev;
        mSize--;
        return node->next;
    }
    mSize--;
    return mNodes;
}

template <class T1, class T2>
typename ObjPtrList<T1, T2>::iterator ObjPtrList<T1, T2>::erase(iterator it) {
    Node *node = it.mNode;
    Node *next = Unlink(node);
    delete node;
    return iterator(next);
}

template <class T1, class T2>
Hmx::Object *ObjPtrList<T1, T2>::RefOwner() const {
    return mOwner ? mOwner->RefOwner() : 0;
}

template <class T1, class T2>
bool ObjPtrList<T1, T2>::Replace(ObjRef *from, Hmx::Object *obj) {
    // List-as-ref: the LIST is the polymorphic ring-ref, so ~Object dispatches
    // this with `from` reinterpreted as the dying Hmx::Object* (same convention
    // as ObjPtr::Replace; from==nullptr means "replace every entry"). Match the
    // node(s) whose held mObject == the dying object. Mirrors rb3-Wii
    // ObjPtrList::Replace, which loops `if (it->obj == from)`.
    Hmx::Object *fromObj = reinterpret_cast<Hmx::Object *>(from);
    for (Node *node = mNodes; node != nullptr;) {
        // ReplaceNode may erase `node` (NoNull + null obj), freeing it; capture
        // the successor first so iteration stays valid.
        Node *next = node->next;
        if (fromObj == nullptr || (Hmx::Object *)node->mObject == fromObj) {
            ReplaceNode(node, obj);
        }
        node = next;
    }
    return false;
}

// The thin X360 ObjPtrList::Node is non-polymorphic and declares no RefOwner();
// the LIST is the ring-ref. (No ObjPtrList::Node::RefOwner definition here.)

// -- ObjPtrVec Xbox template implementations --

template <class T1, class T2>
typename ObjPtrVec<T1, T2>::iterator
ObjPtrVec<T1, T2>::erase(typename ObjPtrVec<T1, T2>::iterator it) {
    int idx = it.it ? (it.it - mNodes.begin()) : 0;
    if (mEraseMode == kEraseSwapLast) {
        unsigned int last = mNodes.size() - 1;
        if ((unsigned int)idx != last) {
            T1 *lastObj = mNodes.back().Obj();
            mNodes.pop_back();
            Set(begin() + idx, lastObj);
            return it;
        }
    }
    mNodes.erase(mNodes.begin() + idx);
    return it;
}

template <class T1, class T2>
typename ObjPtrVec<T1, T2>::iterator ObjPtrVec<T1, T2>::FindRef(ObjRef *ref) {
    for (iterator it = begin(); it != end(); ++it) {
        if (reinterpret_cast<ObjRef *>(&(*it)) == ref) return it;
    }
    return end();
}

#endif // !HX_NATIVE

#ifdef HX_NATIVE
// Generic template implementations for native port.
// On Xbox, these were explicit specializations in link_glue.cpp for each type.
// On native, we provide generic versions so all ObjPtrList types work.

// ObjPtrList uses a semi-circular doubly-linked list:
//   - mNodes->prev always points to the TAIL node (O(1) pop_back)
//   - Forward links (next) are NULL-terminated (last->next = nullptr)
//   - Non-head nodes have regular prev links to their predecessor
//   - Single node: prev = self (it IS the tail)

template <class T1, class T2>
void ObjPtrList<T1, T2>::Link(iterator it, Node *node) {
    node->mListOwner = this;
    Node *pos = it.mNode;
    if (mNodes == nullptr) {
        // First node: self-referencing prev (head->prev = tail = self)
        node->next = nullptr;
        node->prev = node;
        mNodes = node;
    } else if (pos == nullptr) {
        // Insert at end (push_back): append after current tail
        Node *tail = mNodes->prev;
        tail->next = node;
        node->prev = tail;
        node->next = nullptr;
        mNodes->prev = node; // update head's tail pointer
    } else if (pos == mNodes) {
        // Insert before head: new node becomes head
        node->next = mNodes;
        node->prev = mNodes->prev; // inherit tail pointer
        mNodes->prev = node; // old head's prev = new predecessor
        mNodes = node;
    } else {
        // Insert before a middle/tail node
        node->next = pos;
        node->prev = pos->prev;
        pos->prev->next = node;
        pos->prev = node;
    }
    mSize++;
}

template <class T1, class T2>
typename ObjPtrList<T1, T2>::Node *ObjPtrList<T1, T2>::Unlink(Node *node) {
    Node *next = node->next;
    Node *prev = node->prev;
    if (mNodes == node) {
        // Removing head
        mNodes = next;
        if (mNodes) {
            // New head inherits the tail pointer
            // (prev = old head's prev = tail, which is still valid)
            mNodes->prev = prev;
        }
    } else {
        // Removing non-head node
        prev->next = next;
        if (next) {
            next->prev = prev;
        }
        // If removing the tail, update head's tail pointer
        if (mNodes->prev == node) {
            mNodes->prev = prev;
        }
    }
    mSize--;
    return next;
}

template <class T1, class T2>
typename ObjPtrList<T1, T2>::iterator ObjPtrList<T1, T2>::erase(iterator it) {
    Node *node = it.mNode;
    Node *next = Unlink(node);
    delete node;
    return iterator(next);
}

template <class T1, class T2>
Hmx::Object *ObjPtrList<T1, T2>::RefOwner() const {
    return mOwner ? mOwner->RefOwner() : 0;
}

template <class T1, class T2>
bool ObjPtrList<T1, T2>::Replace(ObjRef *ref, Hmx::Object *obj) {
    for (iterator it = begin(); it != end(); ++it) {
        if (it.mNode == ref) {
            ReplaceNode(it.mNode, obj);
            return true;
        }
    }
    return false;
}

template <class T1, class T2>
Hmx::Object *ObjPtrList<T1, T2>::Node::RefOwner() const {
    ObjPtrList<T1, T2> *list = static_cast<ObjPtrList<T1, T2> *>(mListOwner);
    return list->Owner();
}

template <class T1, class T2>
T1 *ObjPtrList<T1, T2>::front() const {
    return mNodes ? mNodes->Obj() : 0;
}

template <class T1, class T2>
T1 *ObjPtrList<T1, T2>::back() const {
    return mNodes ? mNodes->prev->Obj() : 0;
}

// -- ObjPtrVec generic template implementations --
// NOTE: Node::RefOwner is inline-defined in Object.h (line 318) for both
// Xbox and native; the BinStream operator<<'s for ObjPtrList/ObjPtrVec are
// defined unconditionally earlier in this file. Do NOT redefine them here
// or native builds will fail with redefinition errors.

template <class T1, class T2>
typename ObjPtrVec<T1, T2>::iterator
ObjPtrVec<T1, T2>::erase(ObjPtrVec<T1, T2>::iterator it) {
    return iterator(mNodes.erase(it.it));
}

template <class T1, class T2>
typename ObjPtrVec<T1, T2>::iterator ObjPtrVec<T1, T2>::FindRef(ObjRef *ref) {
    for (iterator it = begin(); it != end(); ++it) {
        if (&(*it) == ref) return it;
    }
    return end();
}

// ObjPtrVec::merge, ::unique, ::remove bodies are in ObjPtrVec_impl.h
#include "obj/ObjPtrVec_impl.h"

#endif
