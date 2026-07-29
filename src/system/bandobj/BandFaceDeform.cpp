#include "bandobj/BandFaceDeform.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"
#include "utl/Symbols.h"
#include <cmath>

INIT_REVS(BandFaceDeform);

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

BandFaceDeform::DeltaArray::~DeltaArray() { _MemFree(mData); }
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

    float minClamp = -2.0f;
    float maxClamp = 2.0f;

    int start = 0;
    int end = 0;

    float sp24, sp28, sp2C;
    float sp18, sp1C, sp20;
    signed char sp8, sp9, spA;

    void *&_ref0 = mData;
    while ((unsigned short)end < pos.size()) {
        while ((unsigned short)start < pos.size()) {
            sp24 = pos[start].x - base[start].x;
            sp2C = pos[start].z - base[start].z;
            sp28 = pos[start].y - base[start].y;

            float dx = (float)sp24;
            if (dx > maxClamp)
                dx = maxClamp;
            else if (dx < minClamp)
                dx = minClamp;
            sp8 = (signed char)(int)(63.5 * (double)dx + 0.5);

            float dy = sp28;
            if (dy > maxClamp)
                dy = maxClamp;
            else if (dy < minClamp)
                dy = minClamp;
            sp9 = (signed char)(int)(63.5 * (double)dy + 0.5);

            float dz = sp2C;
            if (dz > maxClamp)
                dz = maxClamp;
            else if (dz < minClamp)
                dz = minClamp;
            spA = (signed char)(int)(63.5 * (double)dz + 0.5);

            int nonZero = 0;
            if (sp8 != 0 || sp9 != 0 || spA != 0) {
                nonZero = 1;
            }
            if (nonZero == 0) {
                start++;
                continue;
            }
            break;
        }

        end = start + 1;
        while ((unsigned short)end < pos.size()) {
            sp18 = pos[end].x - base[end].x;
            sp20 = pos[end].z - base[end].z;
            sp1C = pos[end].y - base[end].y;

            float dx = (float)sp18;
            if (dx > maxClamp)
                dx = maxClamp;
            else if (dx < minClamp)
                dx = minClamp;
            sp8 = (signed char)(int)(63.5 * (double)dx + 0.5);

            float dy = sp1C;
            if (dy > maxClamp)
                dy = maxClamp;
            else if (dy < minClamp)
                dy = minClamp;
            sp9 = (signed char)(int)(63.5 * (double)dy + 0.5);

            float dz = sp20;
            if (dz > maxClamp)
                dz = maxClamp;
            else if (dz < minClamp)
                dz = minClamp;
            spA = (signed char)(int)(63.5 * (double)dz + 0.5);

            int nonZero = 0;
            if (sp8 != 0 || sp9 != 0 || spA != 0) {
                nonZero = 1;
            }
            if (nonZero != 0) {
                end++;
                continue;
            }
            break;
        }

        if (start < pos.size()) {
            int count = end - start;
            char *rec = (char *)MemResizeElem(
                _ref0, mSize, (char *)_ref0 + mSize, 0, count * 3 + 4, "BandFaceDeform"
            );

            *(unsigned short *)(rec + 0) = start;
            int vi = start;
            float md = maxDelta;
            *(unsigned short *)(rec + 2) = count;
            if ((int)start < (int)end) {
                int ctr = count;
                do {
                    int recOff = (vi - start) * 3;
                    float spC = pos[vi].x - base[vi].x;
                    float sp14 = pos[vi].z - base[vi].z;
                    float sp10 = pos[vi].y - base[vi].y;

                    float dx = (float)spC;
                    if (dx > maxClamp)
                        dx = maxClamp;
                    else if (dx < minClamp)
                        dx = minClamp;
                    rec[recOff + 4] = (signed char)(int)(63.5 * (double)dx + 0.5);

                    float dy = sp10;
                    if (dy > maxClamp)
                        dy = maxClamp;
                    else if (dy < minClamp)
                        dy = minClamp;
                    rec[recOff + 5] = (signed char)(int)(63.5 * (double)dy + 0.5);

                    float dz = sp14;
                    if (dz > maxClamp)
                        dz = maxClamp;
                    else if (dz < minClamp)
                        dz = minClamp;
                    rec[recOff + 6] = (signed char)(int)(63.5 * (double)dz + 0.5);

                    float ddx = pos[vi].x - base[vi].x;
                    float ddz = pos[vi].z - base[vi].z;
                    float ddy = pos[vi].y - base[vi].y;
                    float absx = (float)fabs(ddx);
                    if (md < absx) {
                        md = absx;
                        maxDelta = absx;
                    }
                    float absy = (float)fabs(ddy);
                    if (md < absy) {
                        md = absy;
                        maxDelta = absy;
                    }
                    float absz = (float)fabs(ddz);
                    if (md < absz) {
                        maxDelta = absz;
                    }

                    vi++;
                } while (--ctr);
            }

            unsigned short recCount = *(unsigned short *)(rec + 2);
            TheDebug << MakeString(
                "   run from %d to %d waste %g \n",
                (int)start,
                (int)end,
                4.0f / (float)(recCount * 3 + 4)
            );

            totalRuns++;
            totalLength += count;
        }

        start = end;
    }

    int sz = mSize;
    total += sz;
    TheDebug << MakeString(
        "   is size %d total %d av runlength %g totalWaste %d md %g\n",
        sz,
        total,
        (float)totalLength / (float)totalRuns,
        totalRuns * 4,
        maxDelta
    );
}

void BandFaceDeform::DeltaArray::SetSize(int i) {
    if (mSize != i) {
        mSize = i;
        _MemFree(mData);
        mData = _MemAlloc(mSize, 0);
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

// Retail's BandFaceDeform::Save streams mFrames through the generic
// vector operator<<, which needs a per-element operator<<. The rb3-Wii dev
// tree has only operator>> (its Save is a MILO_ASSERT(0) stub), so the
// element saver is declared here and left to the (not-yet-located) definition.
BinStream &operator<<(BinStream &bs, const BandFaceDeform::DeltaArray &da);

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

// rb3-Wii dev has SAVE_OBJ(BandFaceDeform, 0x129) (an unconditional
// MILO_ASSERT(0)); RB3-360 retail ships a real saver at 0x822C7768: it writes
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
