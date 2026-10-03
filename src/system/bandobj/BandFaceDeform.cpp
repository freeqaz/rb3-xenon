#include "bandobj/BandFaceDeform.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"
#include "utl/Symbols.h"
#include "math/Utl.h"
#include <cmath>

INIT_REVS(BandFaceDeform);

// Quantizes (pos - base) to three signed bytes: clamped to +-2 units, 63.5 steps per unit.
inline void CompressDelta(signed char *out, const Vector3 &pos, const Vector3 &base) {
    Vector3 d;
    Subtract(pos, base, d);
    for (int i = 0; i < 3; i++) {
        out[i] = (unsigned char)(Clamp(-2.0f, 2.0f, d[i]) * 63.5 + 0.5);
    }
}

inline bool IsZeroDelta(const signed char *d) { return d[0] == 0 && d[1] == 0 && d[2] == 0; }

BandFaceDeform::DeltaArray::DeltaArray() : mSize(0), mData(0) {}
BandFaceDeform::DeltaArray::DeltaArray(const BandFaceDeform::DeltaArray &da)
    : mSize(0), mData(0) {
    *this = da;
}

BandFaceDeform::DeltaArray &
BandFaceDeform::DeltaArray::operator=(const BandFaceDeform::DeltaArray &da) {
    SetSize(da.mSize);
    memcpy(mData, da.mData, mSize);
    return *this;
}

BandFaceDeform::DeltaArray::~DeltaArray() { MemFree(mData); }
void BandFaceDeform::DeltaArray::Clear() { SetSize(0); }

int BandFaceDeform::DeltaArray::NumVerts() {
    void *p = begin();
    int num = 0;
    void *itend = end();
    while (p < itend) {
        num += ((Delta *)p)->num;
        p = ((Delta *)p)->next();
    }
    return num;
}

extern void *MemResizeElem(void *&, int &, void *, int, int, const char *);

// Appends one record per run of vertices whose compressed delta is nonzero:
// u16 first vertex, u16 vertex count, then three signed bytes per vertex.
void BandFaceDeform::DeltaArray::AppendDeltas(
    const std::vector<Vector3> &pos, const std::vector<Vector3> &base
) {
    if (pos.size() != base.size()) {
        MILO_FAIL(
            "AppendDeltas pos has %d points, base has %d", pos.size(), base.size()
        );
    }
    static int total;
    static int totalRuns;
    static int totalLength;
    static float maxDelta;
    int first = 0;
    while (first < pos.size()) {
        signed char d[3];
        for (; first < pos.size(); first++) {
            CompressDelta(d, pos[first], base[first]);
            if (!IsZeroDelta(d))
                break;
        }
        int last = first + 1;
        for (; last < pos.size(); last++) {
            CompressDelta(d, pos[last], base[last]);
            if (IsZeroDelta(d))
                break;
        }
        if (first < pos.size()) {
            int num = last - first;
            unsigned short *rec = (unsigned short *)MemResizeElem(
                mData, mSize, end(), 0, num * 3 + 4, "BandFaceDeform"
            );
            rec[0] = first;
            rec[1] = num;
            for (int i = first; i < last; i++) {
                CompressDelta((signed char *)(rec + 2) + (i - first) * 3, pos[i], base[i]);
                Vector3 v;
                Subtract(pos[i], base[i], v);
                MaxEq(maxDelta, (float)fabs(v.x));
                MaxEq(maxDelta, (float)fabs(v.y));
                MaxEq(maxDelta, (float)fabs(v.z));
            }
            totalRuns++;
            totalLength += num;
        }
        first = last;
    }
    total += mSize;
}

void BandFaceDeform::DeltaArray::SetSize(int i) {
    if (mSize != i) {
        mSize = i;
        MemFree(mData);
        mData = MemAlloc(mSize, __FILE__, 0xE9, "DeltaArray", 0);
    }
}

BandFaceDeform::BandFaceDeform() {}

BandFaceDeform::~BandFaceDeform() {}

void BandFaceDeform::SetFromMeshAnim(RndMeshAnim *a1, RndMeshAnim *a2, int i1, int i2) {
    if (i2 == -1) {
        i2 = a1->VertPointsKeys().size();
    }
    mFrames.resize(i2);
    for (int i = 0; i < i2; i++) {
        mFrames[i].Clear();
        mFrames[i].AppendDeltas(
            a1->VertPointsKeys()[i + i1].value, a2->VertPointsKeys()[0].value
        );
    }
}

int BandFaceDeform::TotalSize() {
    int size = 0;
    for (int i = 0; i < mFrames.size(); i++) {
        size += mFrames[i].mSize;
    }
    return size;
}

BEGIN_COPYS(BandFaceDeform)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(BandFaceDeform)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mFrames)
    END_COPYING_MEMBERS
END_COPYS

// Retail 0x822C71E8 is a member taking the array in r3 and the stream in r4 and
// returning nothing (the vector saver calls it per element): the byte size, then
// each run's two halfword fields and its packed 3-byte deltas.
void BandFaceDeform::DeltaArray::Save(BinStream &bs) const {
    bs << mSize;
    for (Delta *d = (Delta *)mData; d < (Delta *)((char *)mData + mSize);
         d = (Delta *)d->next()) {
        bs << *(const unsigned short *)d;
        bs << d->num;
        bs.Write(d + 1, d->num * 3);
    }
}

inline BinStream &operator<<(BinStream &bs, const BandFaceDeform::DeltaArray &da) {
    da.Save(bs);
    return bs;
}

BinStream &operator>>(BinStream &bs, BandFaceDeform::DeltaArray &da) {
    da.Load(bs);
    return bs;
}

void BandFaceDeform::DeltaArray::Load(BinStream &bs) {
    int size;
    bs >> size;
    SetSize(size);
    Delta *d = (Delta *)mData;
    while (size > 0) {
        bs >> (short &)d->unk0;
        bs >> d->num;
        bs.Read(d + 1, d->thisoffset() - 4);
        size -= d->thisoffset();
        d = (Delta *)d->next();
    }
}

// RB3-360 retail ships a real saver here, not a SAVE_OBJ(BandFaceDeform, 0x129)
// MILO_ASSERT(0) stub: the saver at 0x822C7768 writes
// the packed rev 0 through BinStream::WriteEndian, chains to Hmx::Object::Save,
// then streams mFrames from this+0x28 (Hmx::Object is 0x28 bytes on 360).
BEGIN_SAVES(BandFaceDeform)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mFrames;
END_SAVES

BEGIN_LOADS(BandFaceDeform)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    bs >> mFrames;
END_LOADS

BEGIN_HANDLERS(BandFaceDeform)
    HANDLE_ACTION(
        set_from_meshanim,
        SetFromMeshAnim(_msg->Obj<RndMeshAnim>(2), _msg->Obj<RndMeshAnim>(3), 0, -1)
    )
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x145)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(BandFaceDeform::DeltaArray)
    SYNC_PROP_SET(verts, o.NumVerts(), )
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(BandFaceDeform)
    SYNC_PROP(frames, mFrames)
    SYNC_PROP_SET(size, TotalSize(), )
END_PROPSYNCS
