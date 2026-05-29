#pragma once
#include "obj/Data.h" /* IWYU pragma: keep */
#include "obj/DataUtl.h"
#include "obj/MessageTimer.h" /* IWYU pragma: keep */
#include "os/Debug.h"
#include "utl/BinStream.h" /* IWYU pragma: keep */
#include "utl/MemMgr.h" /* IWYU pragma: keep */
#include "utl/Symbol.h" /* IWYU pragma: keep */
#include <list> /* IWYU pragma: keep */
#include <map> /* IWYU pragma: keep */

// forward declarations
class MsgSinks;
namespace Hmx {
    class Object;
}
class ObjRef;
class ObjRefOwner;
class ObjectDir;

class MergeFilter;
void MergeObjectsRecurse(ObjectDir *, ObjectDir *, MergeFilter &, bool);

#ifdef HX_NATIVE
// Set during ReplaceList to prevent ObjPtrVec::erase from shifting vector
// elements (CopyRef during shift corrupts ring prev/next pointers).
// Checked in ObjPtrVec::ReplaceNode and Transitions::Replace to suppress
// structural mutations during ring walks.
extern bool gInReplaceList;

#endif

#pragma region ObjRef

// Object Ref/Ptr declarations
// ObjRefOwner size: 0x4
class ObjRefOwner {
public:
    ObjRefOwner() {}
    virtual ~ObjRefOwner() {}
    virtual Hmx::Object *RefOwner() const = 0;
    virtual bool Replace(ObjRef *from, Hmx::Object *to) = 0;
};

void ObjRefRelinkRing(ObjRef *ref);

// ObjRef size: 0x8 (retail X360) / 0xc (HX_NATIVE: extra sentinel + vtable).
// RB3 retail's ObjRef is NON-polymorphic: next@0x0, prev@0x4, no vtable. The
// ring's Replace dispatch goes through the *referenced* object's vtable, not
// the node's (verified: Hmx::Object dtor fn_82738050 ring-walk + ring-free
// fn_82451A48, which read next@0x0 and free 0xc-byte ObjRefConcrete nodes).
// dc3-decomp models ObjRef as polymorphic (0xc, vptr@0x0) — a genuine
// RB3-vs-DC3 divergence. We keep dc3's polymorphic ObjRef for the HX_NATIVE
// engine (its ring machinery + ASAN sentinel rely on it) but drop the vtable
// for the X360 match build so Hmx::Object lays out at 0x28.
#ifdef HX_NATIVE
#define OBJREF_VIRTUAL virtual
#else
#define OBJREF_VIRTUAL
#endif
/** A circular doubly linked list to track an Object's refs. */
class ObjRef {
    friend class Hmx::Object;
    friend void ::MergeObjectsRecurse(ObjectDir *, ObjectDir *, MergeFilter &, bool);
    friend void ::ObjRefRelinkRing(ObjRef *);

protected:
    ObjRef *next; // 0x0 (retail) / 0x4 (native, after vptr)
    ObjRef *prev; // 0x4 (retail) / 0x8 (native)
#ifdef HX_NATIVE
    // Sentinel to detect freed ObjRefs during snapshot-based ReplaceRefs.
    // Set to kAliveSentinel in constructor, cleared to 0 in destructor.
    // Checked before calling Replace on snapshot entries — if the sentinel
    // doesn't match, the ObjRef was freed by a cascading destruction during
    // an earlier Replace callback. No ABI impact (native-only field).
    static constexpr uint32_t kAliveSentinel = 0xCAFEBABE;
    uint32_t mAliveSentinel = kAliveSentinel;
#endif

    // i *think* this is good?
    void AddRef(ObjRef *ref) {
        next = ref;
        prev = ref->prev;
        ref->prev = this;
        prev->next = this;
    }

    void Release(ObjRef *ref) {
        prev->next = next;
        next->prev = prev;
    }

#ifdef HX_NATIVE
    /** Unlink from ring, suppressing ASAN for writes to potentially-freed
     *  neighbors. After cascade, ring neighbors may be in quarantined
     *  (freed) memory — the writes are harmless but ASAN would flag them. */
    __attribute__((no_sanitize("address")))
    static void SafeReleaseFromRing(ObjRef *ref) {
        ref->prev->next = ref->next;
        ref->next->prev = ref->prev;
        ref->next = ref;
        ref->prev = ref;
    }
#endif

public:
    ObjRef() {
#ifdef HX_NATIVE
        // Self-loop so AddRef can safely read next/prev on first insertion.
        // On PPC, MemAlloc zeros memory so this isn't needed.
        next = this;
        prev = this;
#endif
    }
    OBJREF_VIRTUAL ~ObjRef() {
#ifdef HX_NATIVE
        mAliveSentinel = 0;
#endif
    }
#ifdef HX_NATIVE
    bool IsAlive() const { return mAliveSentinel == kAliveSentinel; }
#endif
    OBJREF_VIRTUAL Hmx::Object *RefOwner() const { return nullptr; }
    OBJREF_VIRTUAL bool IsDirPtr() { return false; }
    OBJREF_VIRTUAL Hmx::Object *GetObj() const {
        MILO_FAIL("calling get obj on abstract ObjRef");
        return nullptr;
    }
    OBJREF_VIRTUAL void Replace(Hmx::Object *) {
        MILO_FAIL("calling get obj on abstract ObjRef");
    }
    OBJREF_VIRTUAL ObjRefOwner *Parent() const { return nullptr; }
#ifdef HX_NATIVE
    /** Null the ref's target pointer and self-loop ring pointers.
     *  No Replace callback fires — purely mechanical. Used by
     *  NullifyAllRefs during cascade Phase 0 to avoid delete-this. */
    virtual void NullifyObj() { next = this; prev = this; }
#endif

    class iterator {
    private:
        ObjRef *curRef;

    public:
        iterator() : curRef(nullptr) {}
        iterator(ObjRef *ref) : curRef(ref) {}
        operator ObjRef *() const { return curRef; }
        ObjRef *operator->() const { return curRef; }

        iterator operator++() {
            curRef = curRef->next;
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++*this;
            return tmp;
        }

#ifdef HX_NATIVE
        iterator operator--() {
            curRef = curRef->prev;
            return *this;
        }
#endif

        bool operator!=(iterator it) { return curRef != it.curRef; }
        bool operator==(iterator it) { return curRef == it.curRef; }
        bool operator!() { return curRef == nullptr; }
    };

    iterator begin() const { return iterator(next); }
    iterator end() const { return iterator((ObjRef *)this); }
    bool empty() const { return next == this; }

    /** Make `this` its own standalone single list node. */
    // Assignment order: next first, prev second (matches retail stw 0x20 then 0x24).
    void DetachSelf() { next = this; prev = this; }
    /** Alias for DetachSelf */
    void Clear() { next = this; prev = this; }

    // Splice this ref from its current ring and insert at end of targetRing.
    // Returns predecessor in the original ring for safe iteration.
    ObjRef *SpliceToRing(ObjRef *targetRing) {
        ObjRef *p = prev;
        prev->next = next;
        next->prev = prev;
        next = targetRing;
        prev = targetRing->prev;
        targetRing->prev = this;
        prev->next = this;
        return p;
    }

    void AddSelf() {
        prev->next = this;
        next->prev = this;
    }

    /** Reposition `this` so it's just before `ref`. */
    ObjRef *MoveBefore(ObjRef *ref) {
        ObjRef *oldPrev = prev;
        Release(nullptr);
        AddRef(ref);
        return oldPrev;
    }

    // per ObjectDir::HasDirPtrs, this is the way to iterate across refs
    // for (ObjRef *it = mRefs.next; it != &mRefs; it = it->next) {

#ifdef HX_NATIVE
    void ReplaceList(Hmx::Object *obj);
#else
    void ReplaceList(Hmx::Object *obj) {
        while (!empty()) {
            ObjRef *oldNext = next;
            next->Replace(obj);
            MILO_ASSERT_FMT(oldNext != next, "ReplaceList stuck in infinite loop");
        }
    }
#endif
};

#pragma endregion
#pragma region ObjRefConcrete

// ObjRefConcrete size:
//   Retail X360: 0xc (non-polymorphic: next@0x0, prev@0x4, mObject@0x8 — no vtable)
//   HX_NATIVE:   0x10 (polymorphic: vtable@0x0, next@0x4, prev@0x8, mObject@0xc)
// DC3 uses polymorphic 0x10; RB3 retail verifiably uses 0xc (ring-free fn_82451A48
// allocates and frees 0xc-byte nodes).
template <class T1, class T2 = class ObjectDir>
class ObjRefConcrete : public ObjRef {
protected:
    T1 *mObject; // 0x8 (retail X360) / 0xc (HX_NATIVE)
public:
    ObjRefConcrete(T1 *obj);
    ObjRefConcrete(const ObjRefConcrete &o);
#ifdef HX_NATIVE
    virtual ~ObjRefConcrete();
    virtual Hmx::Object *GetObj() const { return mObject; }
    virtual void Replace(Hmx::Object *obj) { SetObj(obj); }
    void NullifyObj() override { mObject = nullptr; ObjRef::NullifyObj(); }
#else
    // Retail X360: non-polymorphic (no vtable). Implementation in ObjPtr_p.h.
    ~ObjRefConcrete();
    Hmx::Object *GetObj() const { return mObject; }
    void Replace(Hmx::Object *obj) { SetObj(obj); }
#endif

    T1 *operator->() const { return mObject; }
    operator T1 *() const { return mObject; }
    void operator=(T1 *obj) { SetObjConcrete(obj); }
    void operator=(const ObjRefConcrete &o) { SetObjConcrete(o); }

    void SetObjConcrete(T1 *obj);
    void CopyRef(const ObjRefConcrete &);
    Hmx::Object *SetObj(Hmx::Object *root_obj);
    bool Load(BinStream &, bool, ObjectDir *);
};

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjRefConcrete<T1, class ObjectDir> &f);

#pragma endregion
#pragma region ObjPtr

// ObjPtr size:
//   Retail X360: 0xc (= ObjRefConcrete, no mOwner — owner recovered from ring/vtable)
//   HX_NATIVE:   0x14 (adds mOwner@0x10 for explicit owner tracking)
template <class T>
class ObjPtr : public ObjRefConcrete<T> {
protected:
    struct DeferOwner {};
    ObjPtr(DeferOwner, T *ptr) : ObjRefConcrete<T>(ptr) {}
public:
    ObjPtr(Hmx::Object *owner, T *ptr = nullptr);
    ObjPtr(const ObjPtr &p);
    ~ObjPtr();
#ifdef HX_NATIVE
    Hmx::Object *mOwner; // 0x10 (HX_NATIVE only)
    virtual Hmx::Object *RefOwner() const { return mOwner; }
    Hmx::Object *Owner() const { return mOwner; }
#else
    // Retail X360: no mOwner (ObjPtr = 0xc bytes = ObjRefConcrete size)
    Hmx::Object *RefOwner() const { return nullptr; }
    Hmx::Object *Owner() const { return nullptr; }
#endif

    void operator=(T *obj) { SetObjConcrete(obj); }
    void operator=(const ObjPtr &p) { CopyRef(p); }
    T *Ptr() const { return mObject; }
};

// template <class T1>
// BinStream &operator<<(BinStream &bs, const ObjPtr<T1> &ptr);

template <class T1>
BinStream &operator>>(BinStream &bs, ObjPtr<T1> &ptr);

#pragma endregion
#pragma region ObjOwnerPtr

// ObjOwnerPtr size:
//   Retail X360: 0xc (= ObjRefConcrete, no mOwner — same as ObjPtr in retail)
//   HX_NATIVE:   0x14 (adds mOwner@0x10 for explicit owner tracking + Replace dispatch)
template <class T>
class ObjOwnerPtr : public ObjRefConcrete<T> {
public:
    ObjOwnerPtr(ObjRefOwner *owner, T *ptr = nullptr);
    ObjOwnerPtr(const ObjOwnerPtr &o);
    ~ObjOwnerPtr();
#ifdef HX_NATIVE
    ObjRefOwner *mOwner; // 0x10 (HX_NATIVE only)
    virtual Hmx::Object *RefOwner() const;
    virtual void Replace(Hmx::Object *obj) { mOwner->Replace(this, obj); }
#else
    // Retail X360: no mOwner (ObjOwnerPtr = 0xc bytes = ObjRefConcrete size)
    Hmx::Object *RefOwner() const { return nullptr; }
    void Replace(Hmx::Object *obj) { SetObj(obj); }
#endif
    void operator=(T *obj) { SetObjConcrete(obj); }
    T *Ptr() const { return mObject; }
};

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjOwnerPtr<T1> &ptr);

template <class T1>
BinStream &operator>>(BinStream &bs, ObjOwnerPtr<T1> &ptr);

#pragma endregion
#pragma region ObjPtrVec

enum EraseMode {
    kEraseShift = 0,
    kEraseSwapLast = 1,
};

enum ObjListMode {
    kObjListNoNull,
    kObjListAllowNull,
    kObjListOwnerControl
};

// ObjPtrVec size: 0x1c
template <class T1, class T2 = class ObjectDir>
class ObjPtrVec : public ObjRefOwner {
private:
    // Node size: 0x14
    struct Node : public ObjRefConcrete<T1, T2> {
        Node(ObjRefOwner *owner) : ObjRefConcrete<T1>(nullptr), mOwner(owner) {}
        Node(const Node &n);
        virtual ~Node() {}
        virtual Hmx::Object *RefOwner() const {
            ObjPtrVec<T1, T2> *vec = static_cast<ObjPtrVec<T1, T2> *>(mOwner);
            return vec->Owner();
        }
        virtual void Replace(Hmx::Object *obj) {
            ObjPtrVec<T1, T2> *vec = static_cast<ObjPtrVec<T1, T2> *>(mOwner);
            vec->ReplaceNode(this, obj);
        }
        virtual ObjRefOwner *Parent() const { return mOwner; }
#ifdef HX_NATIVE
        void NullifyObj() override {
            ObjRefConcrete<T1, T2>::NullifyObj();
            ObjPtrVec<T1, T2> *vec = static_cast<ObjPtrVec<T1, T2> *>(mOwner);
            if (vec && vec->Mode() == kObjListNoNull && !gInReplaceList) {
                vec->erase(typename ObjPtrVec<T1, T2>::iterator(
                    vec->mNodes.begin() + (this - vec->mNodes.data())
                ));
            }
        }
#endif

        T1 *Obj() const { return mObject; }
        Node &operator=(const Node &n) {
            CopyRef(n);
            mOwner = n.mOwner;
            return *this;
        }

        /** The ObjPtrVec this Node belongs to. */
        ObjRefOwner *mOwner; // 0x10
    };

protected:
    virtual Hmx::Object *RefOwner() const {
        MILO_FAIL("should never be called");
        return nullptr;
    }
    virtual bool Replace(ObjRef *, Hmx::Object *) {
        MILO_FAIL("should never be called");
        return false;
    }

    void ReplaceNode(Node *, Hmx::Object *);

public:
    // this derives off of std::vector<Node>::iterator in some way
    class iterator {
        friend class const_iterator;
        friend class ObjPtrVec;

    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef Node value_type;
        typedef ptrdiff_t difference_type;
        typedef Node *pointer;
        typedef Node &reference;

    private:
        typedef typename std::vector<Node>::iterator Base;
        Base it;

    public:
        iterator() : it() {}
        iterator(Base base) : it(base) {}

        Node &operator*() const { return *it; }
        Node *operator->() const { return &(*it); }

        iterator operator+(int idx) const { return iterator(it + idx); }
        int operator-(const iterator &other) const { return it - other.it; }

        iterator operator++() {
            ++it;
            return *this;
        }

        iterator operator--() {
            --it;
            return *this;
        }

        bool operator!=(const iterator &other) const { return it != other.it; }
        bool operator==(const iterator &other) const { return it == other.it; }
    };
    // ditto
    class const_iterator {
        friend class ObjPtrVec;

    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef Node value_type;
        typedef ptrdiff_t difference_type;
        typedef const Node *pointer;
        typedef const Node &reference;

    private:
        typedef typename std::vector<Node>::const_iterator Base;
        Base it;

    public:
        const_iterator(Base base) : it(base) {}
        const_iterator(iterator non_const) : it(non_const.it) {}

        const Node &operator*() const { return *it; }
        const Node *operator->() const { return &(*it); }

        const_iterator operator+(int idx) const { return const_iterator(it + idx); }

        const_iterator operator++() {
            ++it;
            return *this;
        }

        const_iterator operator--() {
            --it;
            return *this;
        }

        bool operator!=(const const_iterator &other) const { return it != other.it; }
        bool operator==(const const_iterator &other) const { return it == other.it; }
    };

    ObjPtrVec(Hmx::Object *owner, EraseMode = (EraseMode)0, ObjListMode = kObjListNoNull);
    ObjPtrVec(const ObjPtrVec &);
    virtual ~ObjPtrVec();

#ifdef HX_NATIVE
    iterator begin() { return mNodes.begin(); }
    iterator end() { return mNodes.end(); }
    const_iterator begin() const { return mNodes.begin(); }
    const_iterator end() const { return mNodes.end(); }
#else
    iterator begin() { return empty() ? nullptr : mNodes.begin(); }
    // Register swap pattern: declaring size() before begin() affects PPC register allocation
    // Variable declaration order is critical for matching FlowNode::MiloPreRun codegen
    // See STYLEGUIDE.md §Variable Declaration Order
    iterator end() { return begin() + size(); }
    const_iterator begin() const { return empty() ? nullptr : mNodes.begin(); }
    const_iterator end() const { return begin() + size(); }
#endif
    iterator FindRef(ObjRef *);

    iterator erase(iterator);
    iterator insert(const_iterator, T1 *);
    const_iterator find(const Hmx::Object *) const;
    iterator find(const Hmx::Object *);
    int size() const { return mNodes.size(); }
    bool empty() const { return mNodes.empty(); }
    T1 *front() const { return *begin(); }
    T1 *operator[](int idx) { return mNodes[idx].Obj(); }
    const T1 *operator[](int idx) const { return mNodes[idx].Obj(); }

    template <class S>
    void sort(const S &);

    bool remove(T1 *);
    void push_back(T1 *);
    void swap(int, int);
    bool Load(BinStream &, bool, ObjectDir *);
    void clear() { mNodes.clear(); }
    void reserve(unsigned int n) { mNodes.reserve(n); }
    void unique();
    __declspec(noinline) void Set(iterator it, T1 *obj);
    void merge(const ObjPtrVec &);
    Hmx::Object *Owner() const { return mOwner; }
    ObjListMode Mode() const { return mListMode; }
    // see Draw.cpp for this
    void operator=(const ObjPtrVec &other);

private:
    std::vector<Node> mNodes; // 0x4
    Hmx::Object *mOwner; // 0x10
    EraseMode mEraseMode; // 0x14
    ObjListMode mListMode; // 0x18
};

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjPtrVec<T1, ObjectDir> &vec);

template <class T1>
BinStream &operator>>(BinStream &bs, ObjPtrVec<T1, ObjectDir> &vec);

#pragma endregion
#pragma region ObjPtrList

// ObjPtrList size: 0x14
template <class T1, class T2 = class ObjectDir>
class ObjPtrList : public ObjRefOwner {
    friend class RndGroup;
public:
    ObjPtrList(ObjRefOwner *, ObjListMode = kObjListNoNull);
    ObjPtrList(const ObjPtrList &);
    virtual ~ObjPtrList() { clear(); }

private:
    // Node size: 0x14
    struct Node : public ObjRefConcrete<T1, T2> {
#ifdef HX_NATIVE
        // Clang needs explicit using-declarations for dependent base class members
        using ObjRefConcrete<T1, T2>::mObject;
        using ObjRefConcrete<T1, T2>::SetObj;
        using ObjRefConcrete<T1, T2>::SetObjConcrete;
#endif
        Node() : ObjRefConcrete<T1, T2>(nullptr) {}
        virtual ~Node() {}
        virtual Hmx::Object *RefOwner() const;
        virtual void Replace(Hmx::Object *obj) {
            ObjPtrList<T1, T2> *list = static_cast<ObjPtrList<T1, T2> *>(mOwner);
            list->ReplaceNode(this, obj);
        }
        virtual ObjRefOwner *Parent() const { return mOwner; }
#ifdef HX_NATIVE
        void NullifyObj() override {
            ObjRefConcrete<T1, T2>::NullifyObj();
            ObjPtrList<T1, T2> *list =
                static_cast<ObjPtrList<T1, T2> *>(mOwner);
            if (list && list->Mode() == kObjListNoNull && !gInReplaceList) {
                list->Unlink(this);
                delete this;
            }
        }
#endif

#ifdef HX_NATIVE
        static void *operator new(size_t);
#else
        static void *operator new(unsigned int);
#endif
        static void operator delete(void *);

        T1 *Obj() const { return mObject; }
        void operator=(const Node &n) { SetObjConcrete(n.mObject); }

        ObjRefOwner *mOwner; // 0x10
        Node *next; // 0x14
        Node *prev; // 0x18
    };
    int mSize; // 0x4
    Node *mNodes; // 0x8
    ObjRefOwner *mOwner; // 0xc
    ObjListMode mListMode; // 0x10

    virtual Hmx::Object *RefOwner() const;
    virtual bool Replace(ObjRef *, Hmx::Object *);
    void ReplaceNode(Node *, Hmx::Object *);

public:
    class iterator {
    public:
        iterator() : mNode(0) {}
        iterator(Node *node) : mNode(node) {}
        T1 *operator*() { return mNode->Obj(); }

        iterator operator++() {
            mNode = mNode->next;
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++*this;
            return tmp;
        }

        // iterator &operator=(T1 *obj) {
        //     mNode->SetObjConcrete(obj);
        //     return *this;
        // }

        bool operator!=(iterator it) { return mNode != it.mNode; }
        bool operator==(iterator it) { return mNode == it.mNode; }
        bool operator!() { return mNode == 0; }

        struct Node *mNode; // 0x0
    };

    ObjListMode Mode() const { return mListMode; }
    int size() const { return mSize; }
    bool empty() const { return mSize == 0; }
    Hmx::Object *Owner() const { return mOwner ? mOwner->RefOwner() : nullptr; }

    void clear() {
        while (mSize != 0)
            pop_back();
    }

    void DeleteAll() {
        while (!empty()) {
            T1 *cur = front();
            pop_front();
            delete cur;
        }
    }

    T1 *front() const;
    T1 *back() const;
    void pop_front();
    void pop_back();
    void push_back(T1 *obj);
    iterator find(const Hmx::Object *target) const;
    iterator begin() const { return iterator(mNodes); }
    iterator end() const { return iterator(0); }
    iterator erase(iterator);
    iterator insert(iterator, T1 *);
    void Set(iterator it, T1 *obj);
    void MoveItem(iterator thisIt, ObjPtrList<T1, T2> &otherList, iterator otherIt);

    typedef bool SortFunc(T1 *, T1 *);
    template <typename S>
    void sort(const S &);

    void operator=(const ObjPtrList &list);
    bool remove(T1 *);
    bool Load(BinStream &bs, bool, ObjectDir *, bool);

private:
    void Link(iterator, Node *);
    Node *Unlink(Node *);
};

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjPtrList<T1, ObjectDir> &list);

template <class T1>
BinStream &operator>>(BinStream &bs, ObjPtrList<T1, ObjectDir> &list);

#pragma endregion
#pragma region DataNodeObjTrack

// DataNodeObjTrack
class DataNodeObjTrack {
public:
    DataNodeObjTrack(const DataNode &node) : mObj(nullptr, nullptr) {
        mNode = node.Evaluate();
        if (mNode.Type() == kDataObject) {
            mObj = mNode.GetObj();
        }
    }
    DataNode Node() const {
        if (mNode.Type() == kDataObject) {
            return mObj.Ptr();
        } else
            return mNode;
    }
    DataNodeObjTrack &operator=(const DataNode &node) {
        mNode = node.Evaluate();
        if (mNode.Type() == kDataObject) {
            mObj = mNode.GetObj();
        }
        return *this;
    }
    DataNodeObjTrack &operator=(const DataNodeObjTrack &other) {
        mNode = other.Node().Evaluate();
        if (mNode.Type() == kDataObject) {
            mObj = mNode.GetObj();
        }
        return *this;
    }

protected:
    ObjPtr<Hmx::Object> mObj; // 0x0
    DataNode mNode; // 0x14
};

#pragma endregion
#pragma region Object Macros

// Hmx::Object-centric macros
/** Get this Object's path name.
 * @param [in] obj The Object.
 * @returns The Object's path name, or "NULL Object" if it doesn't exist.
 */
const char *PathName(const class Hmx::Object *obj);

#define NULL_OBJ (Hmx::Object *)nullptr

// BEGIN CLASSNAME MACRO -----------------------------------------------------------------
#define OBJ_CLASSNAME(classname)                                                         \
    virtual Symbol ClassName() const { return StaticClassName(); }                       \
    static Symbol StaticClassName() {                                                    \
        static Symbol name(#classname);                                                  \
        return name;                                                                     \
    }
// END CLASSNAME MACRO -------------------------------------------------------------------

// BEGIN SET TYPE MACRO ------------------------------------------------------------------
extern DataArray *SystemConfig(Symbol, Symbol, Symbol);
#define OBJ_SET_TYPE(classname)                                                           \
    virtual void SetType(Symbol classname) {                                              \
        DataArray *def;                                                                   \
        if (!classname.Null()) {                                                          \
            static DataArray *types =                                                     \
                SystemConfig("objects", StaticClassName(), "types");                      \
            DataArray *found = types->FindArray(classname, false);                        \
            if (found) {                                                                  \
                SetTypeDef(found);                                                        \
            } else {                                                                      \
                MILO_NOTIFY(                                                              \
                    "%s:%s couldn't find type %s", ClassName(), PathName(this), classname \
                );                                                                        \
                SetTypeDef(nullptr);                                                      \
            }                                                                             \
        } else                                                                            \
            SetTypeDef(nullptr);                                                          \
    }
// END SET TYPE MACRO --------------------------------------------------------------------

// BEGIN HANDLE MACROS -------------------------------------------------------------------
#define BEGIN_HANDLERS(objType)                                                          \
    DataNode objType::Handle(DataArray *_msg, bool _warn) {                              \
        Symbol sym = _msg->Sym(1);                                                       \
        MessageTimer timer(                                                              \
            (MessageTimer::Active()) ? static_cast<Hmx::Object *>(this) : 0, sym         \
        );

// for handlers of objects that aren't directly Hmx::Objects (i.e. UIListProvider)
#define BEGIN_CUSTOM_HANDLERS(objType)                                                   \
    DataNode objType::Handle(DataArray *_msg, bool _warn) {                              \
        Symbol sym = _msg->Sym(1);                                          \
        MessageTimer timer(                                                              \
            (MessageTimer::Active()) ? dynamic_cast<Hmx::Object *>(this) : 0, sym        \
        );

#define _NEW_STATIC_SYMBOL(str) static Symbol _s(#str);

#define _HANDLE_CHECKED(expr)                                                            \
    {                                                                                    \
        DataNode result = expr;                                                          \
        if (result.Type() != kDataUnhandled)                                             \
            return result;                                                               \
    }

#define HANDLE(s, func)                                                                  \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s)                                                                   \
            _HANDLE_CHECKED(func(_msg))                                                  \
    }

#define HANDLE_EXPR(s, expr)                                                             \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s)                                                                   \
            return expr;                                                                 \
    }

#define HANDLE_ACTION(s, action)                                                         \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            /* for style, require any side-actions to be performed via comma operator */ \
            (action);                                                                    \
            return 0;                                                                    \
        }                                                                                \
    }

#define HANDLE_ACTION_IF(s, cond, action)                                                \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (cond) {                                                                  \
                /* for style, require any side-actions to be performed via comma         \
                 * operator */                                                           \
                (action);                                                                \
            }                                                                            \
            return 0;                                                                    \
        }                                                                                \
    }

#define HANDLE_ACTION_IF_ELSE(s, cond, action_true, action_false)                        \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (cond) {                                                                  \
                /* for style, require any side-actions to be performed via comma         \
                 * operator */                                                           \
                (action_true);                                                           \
            } else {                                                                     \
                (action_false);                                                          \
            }                                                                            \
            return 0;                                                                    \
        }                                                                                \
    }

#define HANDLE_ARRAY(array)                                                              \
    {                                                                                    \
        /* this needs to be placed up here to match Hmx::Object::Handle */               \
        DataArray *found;                                                                \
        if (array && (found = array->FindArray(sym, false))) {                           \
            _HANDLE_CHECKED(found->ExecuteScript(1, this, _msg, 2))                      \
        }                                                                                \
    }

#define HANDLE_MESSAGE(msg)                                                              \
    if (sym == msg::Type())                                                              \
    _HANDLE_CHECKED(OnMsg(msg(_msg)))

#define HANDLE_METHOD(func) _HANDLE_CHECKED(func(_msg))

#define HANDLE_FORWARD(func) _HANDLE_CHECKED(func(_msg, false))

#define HANDLE_MEMBER(member) HANDLE_FORWARD(member.Handle)

#define HANDLE_MEMBER_PTR(member)                                                        \
    if (member)                                                                          \
    HANDLE_FORWARD(member->Handle)

#define HANDLE_SUPERCLASS(parent) HANDLE_FORWARD(parent::Handle)

#define HANDLE_VIRTUAL_SUPERCLASS(parent)                                                \
    if (ClassName() == StaticClassName())                                                \
    HANDLE_SUPERCLASS(parent)

#define END_HANDLERS                                                                     \
    if (_warn)                                                                           \
        MILO_NOTIFY("%s unhandled msg: %s", PathName(this), sym);                        \
    return DATA_UNHANDLED;                                                               \
    }

// for handlers of objects that aren't directly Hmx::Objects (i.e. UIListProvider)
#define END_CUSTOM_HANDLERS                                                              \
    if (_warn)                                                                           \
        MILO_NOTIFY(                                                                     \
            "%s unhandled msg: %s", PathName(dynamic_cast<Hmx::Object *>(this)), sym     \
        );                                                                               \
    return DATA_UNHANDLED;                                                               \
    }
// END HANDLE MACROS ---------------------------------------------------------------------

// BEGIN SYNCPROPERTY MACROS -------------------------------------------------------------
#define BEGIN_PROPSYNCS(objType)                                                         \
    bool objType::SyncProperty(DataNode &_val, DataArray *_prop, int _i, PropOp _op) {   \
        if (_i == _prop->Size())                                                         \
            return true;                                                                 \
        else {                                                                           \
            Symbol sym = _prop->Sym(_i);

#define BEGIN_CUSTOM_PROPSYNC(objType)                                                   \
    bool PropSync(objType &o, DataNode &_val, DataArray *_prop, int _i, PropOp _op) {    \
        if (_i == _prop->Size())                                                         \
            return true;                                                                 \
        else {                                                                           \
            Symbol sym = _prop->Sym(_i);

#define SYNC_PROP(s, member)                                                             \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s)                                                                   \
            return PropSync(member, _val, _prop, _i + 1, _op);                           \
    }

// for propsyncs that do something extra if the prop op is specifically kPropSet
#define SYNC_PROP_SET(s, member, func)                                                   \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (_op == kPropSet) {                                                       \
                func;                                                                    \
            } else {                                                                     \
                if (_op == (PropOp)0x40)                                                 \
                    return false;                                                        \
                _val = member;                                                           \
            }                                                                            \
            return true;                                                                 \
        }                                                                                \
    }

// for propsyncs that do NOT use size or get - aka, any combo of set, insert, remove, and
// handle is used
#define SYNC_PROP_MODIFY(s, member, func)                                                \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (PropSync(member, _val, _prop, _i + 1, _op)) {                            \
                if (!(_op & (kPropSize | kPropGet))) {                                   \
                    func;                                                                \
                }                                                                        \
                return true;                                                             \
            } else {                                                                     \
                return false;                                                            \
            }                                                                            \
        }                                                                                \
    }

#define _SYNC_PROP_BITFIELD(symbol, mask_member, line_num)                                \
    if (sym == symbol) {                                                                  \
        _i++;                                                                             \
        if (_i < _prop->Size()) {                                                         \
            DataNode &node = _prop->Node(_i);                                             \
            int res = 0;                                                                  \
            switch (node.Type()) {                                                        \
            case kDataInt:                                                                \
                res = node.Int();                                                         \
                break;                                                                    \
            case kDataSymbol: {                                                           \
                Symbol bitsym = node.Sym();                                               \
                MILO_ASSERT_FMT(                                                          \
                    strneq("BIT_", bitsym.Str(), 4),                                      \
                    "%s does not begin with BIT_",                                        \
                    bitsym.Str()                                                          \
                );                                                                        \
                bitsym = bitsym.Str() + 4;                                                \
                DataArray *macro = DataGetMacro(bitsym);                                  \
                MILO_ASSERT_FMT(                                                          \
                    macro, "PROPERTY_BITFIELD %s could not find macro %s", symbol, bitsym \
                );                                                                        \
                res = macro->Int(0);                                                      \
                break;                                                                    \
            }                                                                             \
            default:                                                                      \
                MILO_ASSERT(0, line_num);                                                  \
                break;                                                                    \
            }                                                                             \
            MILO_ASSERT(_op <= kPropInsert, line_num);                                       \
            if (_op == kPropGet) {                                                        \
                int final = mask_member & res;                                            \
                _val = (final > 0);                                                       \
            } else {                                                                      \
                if (_val.Int() != 0)                                                      \
                    mask_member |= res;                                                   \
                else                                                                      \
                    mask_member &= ~res;                                                  \
            }                                                                             \
            return true;                                                                  \
        } else                                                                            \
            return PropSync(mask_member, _val, _prop, _i, _op);                           \
    }

#define SYNC_PROP_BITFIELD(symbol, mask_member, line_num)                                \
    { _NEW_STATIC_SYMBOL(symbol) _SYNC_PROP_BITFIELD(_s, mask_member, line_num) }

#define SYNC_MEMBER(s, member)                                                           \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s)                                                                   \
            return member.SyncProperty(_val, _prop, _i + 1, _op);                        \
    }

#define SYNC_SUPERCLASS(parent)                                                          \
    if (parent::SyncProperty(_val, _prop, _i, _op))                                      \
        return true;

#define SYNC_VIRTUAL_SUPERCLASS(parent)                                                  \
    if (ClassName() == StaticClassName())                                                \
    SYNC_SUPERCLASS(parent)

#define END_PROPSYNCS                                                                    \
    return false;                                                                        \
    }                                                                                    \
    }

#define END_CUSTOM_PROPSYNC                                                              \
    return false;                                                                        \
    }                                                                                    \
    }

// END SYNCPROPERTY MACROS ---------------------------------------------------------------

// BEGIN SAVE MACROS ---------------------------------------------------------------------
#define BEGIN_SAVES(objType) void objType::Save(BinStream &bs) {
#define SAVE_REVS(rev, alt) bs << packRevs(alt, rev);
#define SAVE_SUPERCLASS(parent) parent::Save(bs);

#define SAVE_VIRTUAL_SUPERCLASS(parent)                                                  \
    if (ClassName() == StaticClassName())                                                \
    SAVE_SUPERCLASS(parent)

#define END_SAVES }
// END SAVE MACRO ------------------------------------------------------------------------

// BEGIN COPY MACROS ---------------------------------------------------------------------
#define BEGIN_COPYS(objType)                                                             \
    void objType::Copy(const Hmx::Object *o, Hmx::Object::CopyType ty) {
#define COPY_SUPERCLASS(parent) parent::Copy(o, ty);

#define COPY_VIRTUAL_SUPERCLASS(parent)                                                  \
    if (ClassName() == StaticClassName())                                                \
    COPY_SUPERCLASS(Hmx::Object)

#define COPY_SUPERCLASS_FROM(parent, obj) parent::Copy(obj, ty);

#define CREATE_COPY(objType) const objType *c = dynamic_cast<const objType *>(o);

// copy macro where you specify the variable name (used in asserts in some copy methods)
#define CREATE_COPY_AS(objType, var_name)                                                \
    const objType *var_name = dynamic_cast<const objType *>(o);

#define BEGIN_COPYING_MEMBERS if (c) {
// copy macro where you specify the variable name (used in asserts in some copy methods)
#define BEGIN_COPYING_MEMBERS_FROM(copy_name) if (copy_name) {
#define COPY_MEMBER(mem) mem = c->mem;

// copy macro where you specify the variable name (used in asserts in some copy methods)
#define COPY_MEMBER_FROM(copy_name, member) member = copy_name->member;

#define END_COPYING_MEMBERS }

#define END_COPYS }
// END COPY MACROS -----------------------------------------------------------------------

// BEGIN LOAD MACROS  --------------------------------------------------------------------
#define INIT_REVS(rev, alt)                                                              \
    static const __declspec(align(4)) unsigned short gRev = rev;                         \
    static const __declspec(align(4)) unsigned short gAltRev = alt;

#define BEGIN_LOADS(objType) void objType::Load(BinStream &bs) {
#define LOAD_REVS(bs)                                                                    \
    int revs;                                                                            \
    bs >> revs;                                                                          \
    BinStreamRev d(bs, revs);

#ifdef HX_NATIVE
#define ASSERT_REVS(rev1, rev2)                                                          \
    if (d.rev > rev1 || d.altRev > rev2) {                                               \
        fprintf(                                                                         \
            stderr,                                                                      \
            "ASSERT_REVS WARNING: %s '%s' version %d > %d (or alt %d > %d)\n",           \
            ClassName(),                                                                 \
            Name(),                                                                      \
            d.rev,                                                                       \
            rev1,                                                                        \
            d.altRev,                                                                    \
            rev2                                                                         \
        );                                                                               \
    }
#else
#define ASSERT_REVS(rev1, rev2)                                                          \
    if (d.rev > rev1) {                                                                  \
        MILO_FAIL(                                                                       \
            "%s can't load new %s version %d > %d",                                      \
            PathName(this),                                                              \
            ClassName(),                                                                 \
            d.rev,                                                                       \
            gRev                                                                         \
        );                                                                               \
    }                                                                                    \
    if (d.altRev > rev2) {                                                               \
        MILO_FAIL(                                                                       \
            "%s can't load new %s alt version %d > %d",                                  \
            PathName(this),                                                              \
            ClassName(),                                                                 \
            d.altRev,                                                                    \
            gAltRev                                                                      \
        );                                                                               \
    }
#endif

#define LOAD_SUPERCLASS(parent) parent::Load(d.stream);

#define LOAD_VIRTUAL_SUPERCLASS(parent)                                                  \
    if (ClassName() == StaticClassName())                                                \
    LOAD_SUPERCLASS(parent)

#define LOAD_BITFIELD(type, name)                                                        \
    {                                                                                    \
        type bs_name;                                                                    \
        d >> bs_name;                                                                    \
        name = bs_name;                                                                  \
    }

#define LOAD_BITFIELD_ENUM(type, name, enum_name)                                        \
    {                                                                                    \
        type bs_name;                                                                    \
        d >> bs_name;                                                                    \
        name = (enum_name)bs_name;                                                       \
    }

#define END_LOADS }
// END LOAD MACROS
// -----------------------------------------------------------------------

#define NEW_OBJ(objType)                                                                 \
    static Hmx::Object *NewObject() { return new objType; }

#define REGISTER_OBJ_FACTORY(objType)                                                    \
    Hmx::Object::RegisterFactory(objType::StaticClassName(), objType::NewObject);

#pragma endregion
#pragma region TypeProps

// TypeProps implementation
// Retail X360: TypeProps is a 0xc-byte inline ObjRefOwner member of Hmx::Object
// (vtable@0, mMap@4, mOwner@8). No mObjects ring — object-ref tracking uses
// the simpler rb3-Wii approach (pass Hmx::Object* ref explicitly to each op).
// HX_NATIVE keeps the full dc3-style TypeProps (with mObjects) for
// correct ref-counting in the native engine.
class TypeProps : public ObjRefOwner {
    friend class Hmx::Object;
private:
    DataArray *mMap; // 0x4
    Hmx::Object *mOwner; // 0x8
#ifdef HX_NATIVE
    ObjPtrList<Hmx::Object> mObjects; // 0xc (native only: ref-counting for embedded objects)
#endif

    void ReplaceObject(DataNode &n, Hmx::Object *from, Hmx::Object *to);
    void ReleaseObjects();
    void AddRefObjects();
    void ClearAll();

public:
#ifdef HX_NATIVE
    TypeProps(Hmx::Object *o)
        : mOwner(o), mMap(nullptr), mObjects(this, kObjListOwnerControl) {}
    virtual ~TypeProps() { ClearAll(); }
#else
    // Retail X360: inline in Hmx::Object, constructed via mTypeProps(this) in Object ctor.
    TypeProps(Hmx::Object *o) : mMap(nullptr), mOwner(o) {}
    virtual ~TypeProps() { ClearAll(); }
#endif
    virtual Hmx::Object *RefOwner() const { return mOwner; }
    virtual bool Replace(ObjRef *from, Hmx::Object *to);

    void SetOwner(Hmx::Object *o) { mOwner = o; }

    DataNode *KeyValue(Symbol key, bool fail = true) const;
    int Size() const;
    void ClearKeyValue(Symbol key);
    void SetKeyValue(Symbol key, const DataNode &value, bool);
    DataArray *GetArray(Symbol prop);
    void SetArrayValue(Symbol prop, int i, const DataNode &value);
    void RemoveArrayValue(Symbol prop, int i);
    void InsertArrayValue(Symbol prop, int i, const DataNode &value);
    void Load(BinStreamRev &d);
    TypeProps &operator=(const TypeProps &);
    void Save(BinStream &d);
    DataArray *Map() const { return mMap; }
    bool HasProps() const { return mMap && mMap->Size() != 0; }

    MEM_OVERLOAD(TypeProps, 0x485);
};

typedef Hmx::Object *ObjectFunc(void);

#pragma endregion
#pragma region Hmx::Object
#include "obj/PropSync.h" /* IWYU pragma: keep */

// Hmx::Object implementation
namespace Hmx {

    /**
     * @brief: The base class from which all major Objects used in-game build upon.
     * Original _objects description:
     * "The Object class is the root of the class hierarchy. Every
     * class has Object as a superclass."
     */
    class Object : public ObjRefOwner {
#ifdef HX_NATIVE
        friend class ObjRef;
#endif
        friend void ::MergeObjectsRecurse(ObjectDir *, ObjectDir *, MergeFilter &, bool);
    private:
        /** Remove this Object from its associated ObjectDir. */
        void RemoveFromDir();

        /** Handler to execute dta for each of this Object's refs.
         * @param [in] arr The supplied DataArray.
         * Expected DataArray contents:
         *     Node 2: The variable representing the current ObjRef's owner.
         *     Node 3+: Any commands to execute.
         * Example usage: {$this iterate_refs $ref {$ref set 0}}
         */
        DataNode OnIterateRefs(const DataArray *arr);
        /** Handler to set this Object's properties.
         * @param [in] arr The supplied DataArray.
         * Expected DataArray contents:
         *     Node 2+: The property key to set. Must be either a Symbol or a DataArray.
         *     Node 3+: The corresponding property value to set.
         * Example usage: {$this set key1 val1 key2 val2 key3 val3}
         */
        DataNode OnSet(const DataArray *arr);
        DataNode OnPropertyAppend(const DataArray *);
        DataNode OnGetTypeList(const DataArray *);
        DataNode OnAddSink(DataArray *);
        DataNode OnRemoveSink(DataArray *);
        void ExportPropertyChange(DataArray *, Symbol);

        /** A collection of Object class names and their corresponding instantiators. */
        static std::map<Symbol, ObjectFunc *> sFactories;

    protected:
        // Retail X360 Object layout (0x28 bytes total, from ctor lbl_82737FE8):
        //   0x00: ObjRefOwner vtable
        //   0x04: TypeProps (ObjRefOwner-derived, 0xc bytes: vtable@4, mMap@8, mOwner@c)
        //   0x10: mTypeDef*
        //   0x14: mNote (const char*)
        //   0x18: mName (const char*)
        //   0x1c: mDir*
        //   0x20: mRefs.next (ring head, 8 bytes)
        //   0x24: mRefs.prev
        // HX_NATIVE uses a larger TypeProps (with mObjects + vtable = 0x20 bytes)
        // and replaces mNote with String, so layout diverges from retail there.
#ifdef HX_NATIVE
        /** A collection of object instances which reference this Object. */
        ObjRef mRefs; // 0x4 (native)
        /** An array of properties this Object can have. */
        TypeProps *mTypeProps; // 0xc+0x10 (native, pointer)
#else
        /** An array of properties this Object can have (inline, 0xc bytes). */
        TypeProps mTypeProps; // 0x4 (retail X360: vtable@4, mMap@8, mOwner@c)
#endif
    private: // these were marked private in RB2
        /** A collection of handler methods this Object can have.
         *  More specifically, this is an array of arrays, with each array
         *  housing a name, followed by a handler script.
         *  Formatted in the style of:
         *  ( (name1 {handler1}) (name2 {handler2}) (name3 {handler3}) )
         */
        DataArray *mTypeDef; // 0x14 (native) / 0x10 (X360)
        /** A note about this Object, useful for debugging. */
        /** "Just a note describing the object, stripped out of shipping assets,
            so don't make code rely on this" */
#ifdef HX_NATIVE
        String mNote; // 0x18 (native, String 0xc bytes)
#else
        const char *mNote; // 0x14 (retail X360, const char*)
#endif
        /** This Object's name. */
        /** "name of the object" */
        const char *mName; // 0x20 (native) / 0x18 (X360)
        /** The ObjectDir in which this Object resides. */
        ObjectDir *mDir; // 0x24 (native) / 0x1c (X360)
        /** "Sinks for messages sent to me" */
        // Retail X360: mSinks does not exist in the Object dtor (fn_82738050).
        // mSinks is absent from the retail binary layout; development/tool feature only.
#ifdef HX_NATIVE
        MsgSinks *mSinks; // native only
#endif
    protected:
#ifndef HX_NATIVE
        /** Retail X360: ring head at 0x20 (after mDir), non-polymorphic (8 bytes). */
        ObjRef mRefs; // 0x20 (X360)
#endif
        /** An Object in the process of being deleted. */
        static Object *sDeleting;
    public:
#ifdef HX_NATIVE
        /** True after FlushDeferredFrees — rings may have dead entries. */
        static bool sRingsDirty;
#endif
    protected:

        MsgSinks *GetOrAddSinks();
        /** Handler to get the value of a given Object property.
         * @param [in] arr The supplied DataArray.
         * @returns The property value.
         * Expected DataArray contents:
         *     Node 2: The property to search for, either as a Symbol or DataArray.
         *     Node 3: The fallback value if no property is found.
         * Example usage: {$this get some_value 69}
         */
        DataNode OnGet(const DataArray *arr);
        void BroadcastPropertyChange(DataArray *);
        void BroadcastPropertyChange(Symbol);

    public:
        enum CopyType {
            kCopyDeep = 0,
            kCopyShallow = 1,
            kCopyFromMax = 2
        };

        enum SinkMode {
            /** "does a Handle to the sink, this gets all c handlers, type handling,
             * and exporting." */
            kHandle = 0,
            /** "just Exports to the sink, so no c or type handling" */
            kExport = 1,
            /** "just calls HandleType, good if know that particular thing is only
             * ever type handled." */
            kType = 2,
            /** "do type handling and exporting using Export, no C handling" */
            kExportType = 3,
        };

        Object();
        virtual ~Object();
#ifdef HX_NATIVE
        /** True when inside ~Object() -> ReplaceRefs(NULL) chain. */
        static bool IsDeleting() { return sDeleting != nullptr; }
        static Object *GetDeleting() { return sDeleting; }
#endif
        virtual Object *RefOwner() const { return const_cast<Object *>(this); }
        virtual bool Replace(ObjRef *from, Hmx::Object *to);
        /** This Object's class name. */
        OBJ_CLASSNAME(Object);
        /** Set this Object's mTypeDef array based this Object's types entry in
         * SystemConfig. */
        OBJ_SET_TYPE(Object);
        /** Returns whether this ref is an ObjDirPtr (vtable slot 5 in retail). */
        virtual bool IsDirPtr() { return false; }
        /** Executes a message/command on the Object.
         * @param [in] _msg The received message.
         * @param [in] _warn If true, and the message goes unhandled, print to console.
         * @returns The return value of whatever code was executed.
         */
        virtual DataNode Handle(DataArray *_msg, bool _warn = true);
        /** Reads or modifies the object property at the given path.
         * @param [out] _val A DataNode to either place the property val into, or set the
         * property val with.
         * @param [in] _prop The DataArray containing the symbol representing the property
         * to sync.
         * @param [in] _i The index in _prop containing the symbol of the property to
         * sync.
         * @param [in] _op The operation to be performed with the property.
         * @returns Returns true if a property was synced, or if the desired index == the
         * prop array size.
         */
        virtual bool SyncProperty(DataNode &_val, DataArray *_prop, int _i, PropOp _op);
        void InitObject();
        /** Serializes the Object's state
         * @param [in] bs The BinStream to save into.
         */
        virtual void Save(BinStream &);
        /** Copies the state of the given Object into this one.
         * @param [in] o The other Object to copy from.
         * @param [in] ty The copy type.
         */
        virtual void Copy(const Hmx::Object *o, Hmx::Object::CopyType ty);
        /** Deserializes the Object's state.
         * @param [in] bs The BinStream to load from.
         */
        virtual void Load(BinStream &);
        /** Any routines to write relevant data to a BinStream before the main Save method
         * executes. */
        virtual void PreSave(BinStream &) {}
        /** Any routines to write relevant data to a BinStream after the main Save method
         * executes. */
        virtual void PostSave(BinStream &) {}
        /** Prints relevant info about this Object to the debug console. */
        virtual void Print() {}
        virtual void Export(DataArray *msg, bool);
        /** Set this Object's mTypeDef array.
         * @param [in] data The array to set.
         */
        virtual void SetTypeDef(DataArray *data);
        DataArray *ObjectDef(Symbol);
        /** Sets this Object's name and updates the ObjectDir this Object resides in.
         * @param [in] name The name to give this Object.
         * @param [in] dir The ObjectDir to place this Object in. If name is null, this
         * Object won't have a set ObjectDir.
         */
        virtual void SetName(const char *name, ObjectDir *dir);
        virtual ObjectDir *DataDir();
        /** Any routines to read relevant data from a BinStream before the main Load
         * method executes. */
        virtual void PreLoad(BinStream &bs) { Load(bs); }
        /** Any routines to read relevant data from a BinStream after the main Load method
         * executes. */
        virtual void PostLoad(BinStream &) {}
        /** Get this Object's path name. */
        virtual const char *FindPathName();

        /** "script type of the object" */
        Symbol Type() const {
            if (mTypeDef)
                return mTypeDef->Sym(0);
            else
                return Symbol();
        }
        const ObjRef &Refs() const { return mRefs; }
        void SetNote(const char *note);
        DataArray *TypeDef() const { return mTypeDef; }
        ObjectDir *Dir() const { return mDir; }
        const char *Name() const { return mName; }
#ifdef HX_NATIVE
        const String &Note() const { return mNote; }
#else
        const char *Note() const { return mNote; }
#endif
        const char *AllocHeapName() { return MemHeapName(MemFindAddrHeap(this)); }
        void AddRef(ObjRef *ref) {
#ifdef HX_NATIVE
            // During cascade, ~ObjRefConcrete skips Release, leaving dead
            // entries in the ring. If the last entry is dead (freed), the
            // ring is corrupted — reset to self-loop before insertion.
            // Only check after a cascade has completed (flag avoids
            // no_sanitize cache-miss reads during normal operation).
            if (sRingsDirty && mRefs.prev != &mRefs && !IsRingPrevAlive())
                mRefs.Clear();
#endif
            ref->AddRef(&mRefs);
        }
#ifdef HX_NATIVE
        __attribute__((no_sanitize("address")))
        bool IsRingPrevAlive() const {
            return mRefs.prev->mAliveSentinel == ObjRef::kAliveSentinel;
        }
        /** Check if this object's mRefs sentinel is still alive.
         *  False after ~Object (mRefs' ~ObjRef clears sentinel). */
        bool IsRefAlive() const { return mRefs.IsAlive(); }
#endif
        void Release(ObjRef *ref) {
#ifdef HX_NATIVE
            if (!ref) return;
#endif
            ref->Release(nullptr);
        }
#ifdef HX_NATIVE
        MsgSinks *Sinks() const { return mSinks; }
#else
        MsgSinks *Sinks() const { return nullptr; }
#endif

        void ReplaceRefs(Hmx::Object *);
        void ReplaceRefsFrom(Hmx::Object *from, Hmx::Object *);
#ifdef HX_NATIVE
        /** Nullify all refs in this object's ring via NullifyObj.
         *  No Replace callbacks fire — avoids delete-this in
         *  MessageTask/ScriptTask/PropertyTask/DirLoader. */
        void NullifyAllRefs();
        /** Detach from parent dir without modifying the dir's hash table.
         *  Used during ~ObjectDir cascade to prevent surviving objects from
         *  holding a stale mDir pointer to the dying dir. Unlike
         *  SetName(nullptr, nullptr), this does NOT call RemovingObject
         *  or touch the hash table — the dir is about to be freed anyway. */
        void DetachFromDir() { mDir = nullptr; mName = gNullStr; }
#endif
        /** How many other objects reference this Object? */
        int RefCount() const;

        void RemovePropertySink(Hmx::Object *, DataArray *);
        bool HasPropertySink(Hmx::Object *, DataArray *);
        void RemoveSink(Hmx::Object *, Symbol = Symbol());

        void SaveType(BinStream &);
        void SaveRest(BinStream &);
        void ClearAllTypeProps();
        bool HasTypeProps() const;
        void AddSink(
            Hmx::Object *,
            Symbol = Symbol(),
            Symbol = Symbol(),
            SinkMode = kHandle,
            bool = true
        );
        void AddPropertySink(Hmx::Object *, DataArray *, Symbol);
        void MergeSinks(Hmx::Object *);
        DataNode PropertyArray(Symbol);
        int PropertySize(DataArray *);
        void InsertProperty(DataArray *, const DataNode &);
        void RemoveProperty(DataArray *);
        void PropertyClear(DataArray *);

        /** Search for a key in this Object's properties, and return the corresponding
         * value.
         * @param [in] prop: The property to search for, in DataArray form. The first node
         * must be a Symbol.
         * @param [in] fail: If true, print a message to the console if no property value
         * was found.
         * @returns The corresponding property's value as a DataNode pointer.
         */
        const DataNode *Property(DataArray *prop, bool fail = true) const;

        /** Search for a key in this Object's properties, and return the corresponding
         * value.
         * @param [in] prop: The property to search for, in Symbol form.
         * @param [in] fail: If true, print a message to the console if no property value
         * was found.
         * @returns The corresponding property's value as a DataNode pointer.
         */
        const DataNode *Property(Symbol prop, bool fail = true) const;

        DataNode HandleProperty(DataArray *, DataArray *, bool);

        /** Execute script in this Object's TypeDef,
         * based on the contents of a received message.
         * @param [in] _msg The received message.
         * @returns The return value of whatever script was executed.
         */
        DataNode HandleType(DataArray *msg);

        void ChainSource(Hmx::Object *, Hmx::Object *);
        void LoadType(BinStream &);
        void LoadRest(BinStream &);

        /** Either adds or updates the key/value pair in the properties.
         * @param [in] prop The key to either add or update
         * @param [in] val The corresponding value associated with the key.
         */
        void SetProperty(Symbol prop, const DataNode &val);

        /** Either adds or updates the key/value pair in the properties.
         * @param [in] prop The key to either add or update. The first node must be a
         * Symbol.
         * @param [in] val The corresponding value associated with the key.
         */
        void SetProperty(DataArray *prop, const DataNode &val);

        NEW_OBJ(Hmx::Object);
        /** Given an Object derivative's class name, construct a new instance of the
         * Object. */
        static Object *NewObject(Symbol name);
        /** Given an Object derivative's class name, check if its ctor is in our factory
         * list. */
        static bool RegisteredFactory(Symbol name);
        /** Add a new Object derivative to the factory list via class name and factory
         * func. */
        static void RegisterFactory(Symbol name, ObjectFunc *func);

        /** Create a new Object derivative based on its entry in the factory list. */
        template <class T>
        static T *New() {
            T *obj = dynamic_cast<T *>(Hmx::Object::NewObject(T::StaticClassName()));
            if (!obj)
                MILO_FAIL("Couldn't instantiate class %s", T::StaticClassName());
            return obj;
        }
    };

}

extern bool gLoadingProxyFromDisk;
extern bool gMiloTool;

inline TextStream &operator<<(TextStream &ts, const Hmx::Object *obj) {
    if (obj)
        ts << obj->Name();
    else
        ts << "<null>";
    return ts;
}

struct ObjMatchPr {
    ObjMatchPr(Hmx::Object *o) : obj(o) {}
    bool operator()(const Hmx::Object *value) const { return obj == value; }
    Hmx::Object *obj;
};

struct ObjNameSort {
    bool operator()(Hmx::Object *c1, Hmx::Object *c2) const {
        return strcmp(c1->Name(), c2->Name()) < 0;
    }
};

#pragma endregion
#pragma region ObjVector

// ObjVector
template <class T>
class ObjVector : public std::vector<T> {
private:
    typedef typename std::vector<T> Base;
    Hmx::Object *mOwner;

public:
    ObjVector(Hmx::Object *o) : mOwner(o) {}
    Hmx::Object *Owner() { return mOwner; }

    void push_back() { resize(size() + 1); }

    void push_back(const T &t) {
        push_back();
        back() = t;
    }

    void resize(unsigned int size) { Base::resize(size, T(mOwner)); }

    void operator=(const ObjVector &vec) {
        if (this != &vec) {
            resize(vec.size());
            Base::operator=((Base &)vec);
        }
    }
};

// there are symbols for both BinStreamRev >> ObjVector
// and BinStream >> ObjVector
// hmx why??? pick one and stick with it pls

template <class T>
BinStream &operator>>(BinStreamRev &bs, ObjVector<T> &vec) {
    unsigned int length;
    bs >> length;
#ifdef HX_NATIVE
    if (length > 0x10000) {
        fprintf(stderr, "ObjVector: CORRUPT length=%u (0x%x) at stream pos=%d\n", length, length, bs.stream.Tell());
        abort();
    }
#endif
    vec.resize(length);

    for (ObjVector<T>::iterator it = vec.begin(); it != vec.end(); it++) {
        bs >> *it;
    }
    return bs.stream;
}

template <class T>
BinStream &operator>>(BinStream &bs, ObjVector<T> &vec) {
    unsigned int length;
    bs >> length;
    vec.resize(length);

    for (ObjVector<T>::iterator it = vec.begin(); it != vec.end(); it++) {
        bs >> *it;
    }
    return bs;
}

#pragma endregion
#pragma region ObjList

// ObjList
template <class T>
class ObjList : public std::list<T> {
private:
    typedef typename std::list<T> Base;
    Hmx::Object *mOwner;

public:
    ObjList(Hmx::Object *o) : mOwner(o) {}
    Hmx::Object *Owner() { return mOwner; }

    void resize(unsigned int ul) { Base::resize(ul, T(mOwner)); }

    void push_front() { insert(begin(), T(mOwner)); }

    void push_front(const T &t) {
        push_front();
        front() = t;
    }

    void push_back() { resize(size() + 1); }

    void push_back(const T &t) {
        push_back();
        back() = t;
    }

    void operator=(const ObjList &oList) {
        if (this != &oList) {
            resize(oList.size());
            Base::operator=((Base &)oList);
        }
    }
};

template <class T>
BinStream &operator>>(BinStreamRev &bs, ObjList<T> &oList) {
    unsigned int length;
    bs >> length;
    oList.resize(length);

    for (std::list<T>::iterator it = oList.begin(); it != oList.end(); ++it) {
        bs >> *it;
    }
    return bs.stream;
}

template <class T>
BinStream &operator>>(BinStream &bs, ObjList<T> &oList) {
    unsigned int length;
    bs >> length;
    oList.resize(length);

    for (std::list<T>::iterator it = oList.begin(); it != oList.end(); ++it) {
        bs >> *it;
    }
    return bs;
}

#pragma endregion
#pragma region ObjectStage

// ObjectStage
class ObjectStage : public ObjPtr<Hmx::Object> {
public:
    ObjectStage() : ObjPtr<Hmx::Object>(sOwner) {}
    ObjectStage(Hmx::Object *o) : ObjPtr<Hmx::Object>(sOwner, o) {}
    virtual ~ObjectStage() {}

    static Hmx::Object *sOwner;
};

inline void
Interp(const ObjectStage &stage1, const ObjectStage &stage2, float f, Hmx::Object *&obj) {
    const ObjectStage &out = f < 1 ? stage1 : stage2;
    obj = out.Ptr();
}

BinStream &operator<<(BinStream &, const ObjectStage &);
BinStreamRev &operator>>(BinStreamRev &, ObjectStage &);

#pragma endregion
#pragma region ObjVersion

// ObjVersion
struct ObjVersion {
    ObjVersion(int i, Hmx::Object *o) : revs(i), obj(nullptr, o) {}
    ~ObjVersion() {}

    ObjPtr<Hmx::Object> obj;
    int revs;
};

#pragma endregion

#include "ObjPtr_p.h"
