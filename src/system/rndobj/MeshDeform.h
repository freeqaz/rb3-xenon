#pragma once
#include "math/Mtx.h"
#include "obj/Object.h"
#include "rndobj/Mesh.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

/** "Reskins target mesh according to exobones." */
class RndMeshDeform : public Hmx::Object {
public:
    class VertArray {
    public:
        enum {
            kMaxWeights = 0x40
        };
        VertArray(RndMeshDeform *md) : mSize(0), mData(nullptr), mParent(md) {}
        ~VertArray() { MemFree(mData); }
        void Clear() { SetSize(0); }
        void *FindVert(int);
        void CopyVert(int, int, VertArray &);
        int AppendWeights(int, int *const, float *const);
        void Copy(const VertArray &);
        void Save(BinStream &);
        void Load(BinStream &);
        int NumVerts() {
            u8 *buf = (u8 *)mData;
            void *end = (void *)((intptr_t)mData + mSize);
            int i = 0;
            for (; buf < end;) {
                i++;
                buf += (*buf * 2) + 1;
            }
            return i;
        }

        // probably overkill but idk we already had this so why not
        class iterator {
        private:
            void *data;

        public:
            iterator() : data(nullptr) {}
            iterator(void *d) : data(d) {}
            operator void *() const { return data; }
            void *Data() const { return data; }
            // void *operator->() const { return data; }

            iterator operator++() {
                char *cData = (char *)data;
                cData += (*cData * 2) + 1;
                data = cData;
                return *this;
            }

            iterator operator++(int) {
                iterator tmp = *this;
                ++*this;
                return tmp;
            }

            bool operator!=(iterator it) { return data != it.data; }
            bool operator==(iterator it) { return data == it.data; }
            bool operator!() { return data == nullptr; }
        };

        iterator begin() const { return iterator(mData); }
        iterator end() const { return iterator((void *)((intptr_t)mData + mSize)); }

    protected:
        void SetSize(int);

        int mSize;
        void *mData;
        RndMeshDeform *mParent;
    };

    // size 0x6c
    struct BoneDesc {
        BoneDesc(Hmx::Object *owner) : mBone(owner) {
            unk14.Reset();
            unk54.Reset();
        }
        ObjPtr<RndTransformable> mBone; // 0x0
        Transform unk14;
        Transform unk54;
    };

    virtual ~RndMeshDeform();
    OBJ_CLASSNAME(MeshDeform);
    OBJ_SET_TYPE(MeshDeform);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreSave(BinStream &);
    virtual void Print();

    OBJ_MEM_OVERLOAD(0x1A)
    NEW_OBJ(RndMeshDeform)
    static void Init() { REGISTER_OBJ_FACTORY(RndMeshDeform) }

protected:
    RndMeshDeform();

    /** "The mesh we will change, set you can make a zero vert meshdeform
        just to clean up mutable character meshes" */
    ObjPtr<RndMesh> mMesh; // 0x2c
    Transform mMeshInverse; // 0x40
    ObjVector<BoneDesc> mBones; // 0x80
    VertArray mVerts; // 0x94
    bool mSkipInverse;
    bool mDeformed;
};
