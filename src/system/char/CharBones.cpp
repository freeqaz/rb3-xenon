#include "char/CharBones.h"
#include "char/CharClip.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Vec.h"
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "utl/MakeString.h"
#include "obj/Object.h"
#include "utl/MemMgr.h"

CharBones *gPropBones;

short ShortVector3::ToShort(float f) {
    // Scale float to short range: divide by 1300 scale factor, multiply by short max (32767),
    // add 0.5 for rounding, clamp to valid range, then floor to convert to integer
    float mult = f * (1.0f / 1300.0f);
    float scaled = mult * 32767.0f;
    float temp = scaled + 0.5f;
    float clamped = Clamp(-32767.0f, 32767.0f, temp);
    return floor(clamped);
}

void ShortVector3::Set(const Vector3 &vec) {
    x = ToShort(vec.x);
    y = ToShort(vec.y);
    z = ToShort(vec.z);
}

void ShortQuat::Set(const Hmx::Quat &quat) {
    x = (short)floor(Clamp(-32767.0f, 32767.0f, quat.x * 32767.0f + 0.5f));
    y = (short)floor(Clamp(-32767.0f, 32767.0f, quat.y * 32767.0f + 0.5f));
    z = (short)floor(Clamp(-32767.0f, 32767.0f, quat.z * 32767.0f + 0.5f));
    w = (short)floor(Clamp(-32767.0f, 32767.0f, quat.w * 32767.0f + 0.5f));
}

void ByteQuat::Set(const Hmx::Quat &quat) {
    x = (char)floor(Clamp(-127.0f, 127.0f, quat.x * 127.0f + 0.5f));
    y = (char)floor(Clamp(-127.0f, 127.0f, quat.y * 127.0f + 0.5f));
    z = (char)floor(Clamp(-127.0f, 127.0f, quat.z * 127.0f + 0.5f));
    w = (char)floor(Clamp(-127.0f, 127.0f, quat.w * 127.0f + 0.5f));
}

void CharBones::Zero() {
#ifdef HX_NATIVE
    if (!mStart) return;
#endif
    memset(mStart, 0, mTotalSize);
}

int CharBones::TypeSize(int i) const {
    switch (i) {
    case TYPE_POS:
    case TYPE_SCALE:
        if (mCompression >= kCompressVects)
            return 6;
        else
            return sizeof(Vector3);
    case TYPE_QUAT:
        if (mCompression >= kCompressQuats)
            return 4;
        else if (mCompression != kCompressNone)
            return 8;
        else
            return sizeof(Hmx::Quat);

    default:
        if (mCompression != kCompressNone)
            return 2;
        else
            return 4;
    }
}

void CharBones::RecomputeSizes() {
#ifdef HX_NATIVE
    // The original code uses offset[-7] to reach mCounts from mOffsets via
    // pointer arithmetic. On LP64, padding between mCounts and mOffsets may
    // break this assumption. Use direct member access instead.
    mOffsets[0] = 0;
    for (int i = 0; i < TYPE_END; i++) {
        int count_diff = mCounts[i + 1] - mCounts[i];
        mOffsets[i + 1] = mOffsets[i] + TypeSize(i) * count_diff;
    }
    mTotalSize = (mOffsets[TYPE_END] + 0xFU) & 0xFFFFFFF0;
#else
    int i = 0;
    int *offset = &mOffsets[0];
    *offset = 0;
    do {
        int cur_offset = *offset;
        // offset[-7] = mCounts[i], offset[-6] = mCounts[i+1]
        // (mCounts is 7 ints (0x1C bytes) before mOffsets)
        int curCount = offset[-7];
        int nextCount = offset[-6];
        int sz = TypeSize(i);
        *++offset = cur_offset + sz * (nextCount - curCount);
        i++;
    } while (i < NUM_TYPES);
    // Round up to nearest 0x10 for alignment
    mTotalSize = mOffsets[TYPE_END] + 0xFU & 0xFFFFFFF0;
#endif
}

void CharBones::SetCompression(CompressionType ty) {
    if (ty != mCompression) {
        mCompression = ty;
        RecomputeSizes();
    }
}

CharBones::Type CharBones::TypeOf(Symbol s) {
    const char *p = s.Str();
    while (*p != 0) {
        if (*p == '.') {
            p++;
            switch (*p) {
            case 'p':
                return TYPE_POS;
            case 's':
                return TYPE_SCALE;
            case 'q':
                return TYPE_QUAT;
            case 'r': {
                // check if rot is x, y, or z
                char next = p[3];
                if (next >= 'x' && next <= 'z')
                    return (Type)(next - 'u');
            }
            default:
                break;
            }
        }
        p++;
    }
    MILO_FAIL("Unknown bone suffix in %s", (String &)s);
    return NUM_TYPES;
}

const char *CharBones::SuffixOf(CharBones::Type t) {
    static const char *suffixes[NUM_TYPES] = { "pos",  "scale", "quat",
                                               "rotx", "roty",  "rotz" };
    MILO_ASSERT(t < TYPE_END, 0x66);
    return suffixes[t];
}

Symbol CharBones::ChannelName(const char *cc, CharBones::Type t) {
    MILO_ASSERT(t < TYPE_END, 0x6F);
    char buf[256];
    strcpy(buf, cc);
    char *chr = strchr(buf, '.');
    if (!chr) {
        chr = buf + strlen(buf);
        *chr = '.';
    }
    strcpy(chr + 1, SuffixOf(t));
    return Symbol(buf);
}

int CharBones::FindOffset(Symbol s) const {
    Type ty = TypeOf(s);
    int nextcount = mCounts[ty + 1];
    int size = TypeSize(ty);
    int count = mCounts[ty];
    int offset = mOffsets[ty];
    for (int i = count; i < nextcount; i++, offset += size) {
        if (mBones[i].name == s)
            return offset;
    }
    return -1;
}

void CharBones::SetWeights(float wt, std::vector<Bone> &bones) {
    for (int i = 0; i < bones.size(); i++) {
        bones[i].weight = wt;
    }
}

void *CharBones::FindPtr(Symbol s) const {
    int offset = FindOffset(s);
    if (offset == -1)
        return 0;
    else
        return (void *)&mStart[offset];
}

// RETAIL KEPT THIS EMISSION (lane MILOKEEP-1).  A whole-binary census of every
// reference to the `TheDebug` global (0x82cc9874 — the complete superset of all
// MILO_* emission sites, since every emitter funnels through it) found only FOUR
// surviving formatted emissions in the entire retail binary, and this is one of
// them: retail's ?Print@CharBones@@UAAXXZ (0x823adf08) materialises the format
// literal at .rdata 0x8204bca4 and calls MakeString + TextStream::operator<<.
// Our stripped MILO_LOG (`((void)(__VA_ARGS__))`) deletes all of that, which is
// why the row sat at 23.9% with `MakeString`/`TextStream::operator<<` showing as
// target-only calls.
//
// Fixed TU-LOCALLY via push_macro, the Mesh.cpp:1619 / UIComponent.cpp:495
// precedent — os/Debug.h is a PCH input cascading to ~281 TUs where the blanket
// control measured −21, so it is deliberately NOT touched.  `char/` is PCH-
// excluded, so the blast radius is this one TU.  The two sibling functions that
// retail also kept (CharClip::Print, CharBonesSamples::Print) already spell this
// same residue explicitly as `TheDebug << MakeString(...)`.
// HX_NATIVE is left alone so the native port keeps its real logging path.
#ifndef HX_NATIVE
#pragma push_macro("MILO_LOG")
#undef MILO_LOG
#define MILO_LOG(...) TheDebug << MakeString(__VA_ARGS__)
#endif
void CharBones::Print() {
    for (auto it = mBones.begin(); it != mBones.end(); ++it) {
        MILO_LOG("%s %.2f: %s\n", it->name, it->weight, StringVal(it->name));
    }
}
#ifndef HX_NATIVE
#pragma pop_macro("MILO_LOG")
#endif

BinStream &operator<<(BinStream &bs, const CharBones::Bone &bone) {
    bs << bone.name;
    bs << bone.weight;
    return bs;
}

BinStream &operator>>(BinStream &bs, CharBones::Bone &bone) {
    bs >> bone.name;
    bs >> bone.weight;
    return bs;
}

void CharBones::SetWeights(float f) { SetWeights(f, mBones); }

BEGIN_CUSTOM_PROPSYNC(CharBones::Bone)
    SYNC_PROP(name, o.name)
    SYNC_PROP(weight, o.weight)
    SYNC_PROP_SET(preview_val, gPropBones->StringVal(o.name), )
END_CUSTOM_PROPSYNC

void CharBones::ListBones(std::list<Bone> &bones) const {
    for (int i = 0; i < mBones.size(); i++) {
        bones.push_back(mBones[i]);
    }
}

void CharBones::AddBones(const std::vector<Bone> &vec) {
    for (std::vector<Bone>::const_iterator it = vec.begin(); it != vec.end(); ++it) {
        AddBoneInternal(*it);
    }
    ReallocateInternal();
}

void CharBones::AddBones(const std::list<Bone> &bones) {
    for (std::list<Bone>::const_iterator it = bones.begin(); it != bones.end(); ++it) {
        AddBoneInternal(*it);
    }
    ReallocateInternal();
}

void CharBones::ClearBones() {
    mBones.clear();
    for (int i = 0; i < NUM_TYPES; i++) {
        mCounts[i] = 0;
        mOffsets[i] = 0;
    }
    mTotalSize = 0;
    mCompression = kCompressNone;
    ReallocateInternal();
}

void TestDstComplain(Symbol s) {
    MILO_NOTIFY_ONCE("src %s not in dst, punting animation", s);
}

CharBones::CharBones() : mCompression(kCompressNone), mStart(0), mTotalSize(0) {
    for (int i = 0; i < NUM_TYPES; i++) {
        mCounts[i] = 0;
        mOffsets[i] = 0;
    }
}

BEGIN_PROPSYNCS(CharBonesObject)
    gPropBones = this;
    SYNC_PROP(bones, mBones)
    // Retail does not chain to Hmx::Object: an unmatched property returns false.
END_PROPSYNCS

void CharBones::ScaleAdd(CharClip *clip, float f1, float f2, float f3) {
    clip->ScaleAdd(*this, f1, f2, f3);
}

void CharBones::AddBoneInternal(const Bone &bone) {
    int type = TypeOf(bone.name);
    int pos = mCounts[type];
    int end = mCounts[type + 1];
    while (pos < end) {
        if (mBones[pos].name == bone.name)
            return;
        if (strcmp(mBones[pos].name.Str(), bone.name.Str()) >= 0)
            break;
        pos++;
    }
    mBones.insert(mBones.begin() + pos, 1, bone);
    int size = TypeSize(type);
    type++;
    while (type < NUM_TYPES) {
        mCounts[type]++;
        mOffsets[type] += size;
        type++;
    }
    mTotalSize = (mOffsets[TYPE_END] + 0xFU) & 0xFFFFFFF0;
}

const char *CharBones::StringVal(Symbol s) {
    void *ptr = FindPtr(s);
    CharBones::Type t = TypeOf(s);
    switch (t) {
    case TYPE_POS:
    case TYPE_SCALE:
        if (mCompression >= kCompressVects) {
            Vector3 vshort((short *)ptr);
            return MakeString("%g %g %g", vshort.x, vshort.y, vshort.z);
        } else {
            Vector3 *vptr = (Vector3 *)ptr;
            return MakeString("%g %g %g", vptr->x, vptr->y, vptr->z);
        }
    case TYPE_QUAT: {
        Hmx::Quat q;
        Hmx::Quat *qPtr = (Hmx::Quat *)ptr;
        if (mCompression >= kCompressQuats) {
            ByteQuat *bqPtr = (ByteQuat *)qPtr;
            bqPtr->ToQuat(q);
        } else if (mCompression != kCompressNone) {
            ShortQuat *sqPtr = (ShortQuat *)qPtr;
            sqPtr->ToQuat(q);
        } else
            q = *qPtr;
        Vector3 v40;
        MakeEuler(q, v40);
        v40 *= RAD2DEG;
        return MakeString(
            "quat(%g %g %g %g) euler(%g %g %g)", q.x, q.y, q.z, q.w, v40.x, v40.y, v40.z
        );
    }
    default: {
        float floatVal;
        if (mCompression != kCompressNone) {
            floatVal = *((short *)ptr) * 0.00061035156f;
        } else {
            floatVal = *((float *)ptr);
        }
        floatVal *= RAD2DEG;
        if (mCompression != kCompressNone) {
            return MakeString("deg %g raw %d", floatVal, *((short *)ptr));
        } else {
            return MakeString("deg %g rad %g", floatVal, *((float *)ptr));
        }
    }
    }
}

void CharBones::ScaleAddIdentity() {
    Hmx::Quat *qend = (Hmx::Quat *)(mStart + mOffsets[TYPE_ROTX]);
    Bone *bone = mBones.data() + mCounts[TYPE_QUAT];
    Hmx::Quat *qstart = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
    if (qstart == qend) return;
    do {
        float identity = 1.0f - bone->weight;
        float w = qstart->w;
        if (w < 0.0f) {
            w -= identity;
        } else {
            w += identity;
        }
        qstart->w = w;
        qstart++;
        bone++;
    } while (qstart != qend);
}

// MARK: ScaleDown
void CharBones::ScaleDown(CharBones &bones, float f2) const {
    if (!mBones.empty()) {
        Bone *myBonesItr = (Bone *)mBones.data();
        if (f2 == 0) {
            if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
                Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_POS]));
                Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
                Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_QUAT]));
                Vector3 *otherVecItr = (Vector3 *)bones.mStart;
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherVecItr++;
                    }
                    myBonesItr++;
                    otherVecItr->Zero();
                    otherBonesItr->weight = 0;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherVecItr++;
                }
            }
            if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
                Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
                Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
                Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_ROTX]));
                Hmx::Quat *otherQuatItr =
                    (Hmx::Quat *)(bones.mStart + bones.mOffsets[TYPE_QUAT]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherQuatItr++;
                    }
                    myBonesItr++;
                    otherQuatItr->Set(0, 0, 0, 0);
                    otherBonesItr->weight = 0;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                }
            }
            if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
                Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
                Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_END]));
                Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_END]));
                float *otherRotItr = (float *)(bones.mStart + bones.mOffsets[TYPE_ROTX]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherRotItr++;
                    }
                    myBonesItr++;
                    *otherRotItr = 0;
                    otherBonesItr->weight = 0;
                    if (myBonesItr == myBonesEnd) {
                        return;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherRotItr++;
                }
            }
        } else {
            if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
                Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_POS]));
                Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
                Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_QUAT]));
                Vector3 *otherVecItr = (Vector3 *)bones.mStart;
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherVecItr++;
                    }
                    myBonesItr++;
                    *otherVecItr *= f2;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherVecItr++;
                }
            }
            if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
                Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
                Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
                Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_ROTX]));
                Hmx::Quat *otherQuatItr =
                    (Hmx::Quat *)(bones.mStart + bones.mOffsets[TYPE_QUAT]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherQuatItr++;
                    }
                    myBonesItr++;
                    otherQuatItr->Set(
                        otherQuatItr->x * f2,
                        otherQuatItr->y * f2,
                        otherQuatItr->z * f2,
                        otherQuatItr->w * f2
                    );
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                }
            }
            if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
                Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
                Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_END]));
                Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_END]));
                float *otherRotItr = (float *)(bones.mStart + bones.mOffsets[TYPE_ROTX]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherRotItr++;
                    }
                    myBonesItr++;
                    *otherRotItr *= f2;
                    if (myBonesItr == myBonesEnd) {
                        return;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherRotItr++;
                }
            }
        }
    }
}

// MARK: Blend
void CharBones::Blend(CharBones &bones) const {
    MILO_ASSERT(!mCompression && !bones.mCompression, 0x311);
    if (!mBones.empty()) {
        Bone *myBonesItr = (Bone *)mBones.data();
        if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_POS]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_QUAT]));
            Vector3 *myVecItr = (Vector3 *)mStart;
            Vector3 *otherVecItr = (Vector3 *)bones.mStart;
            while (true) {
                while (otherBonesItr->name != myBonesItr->name) {
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherVecItr++;
                }
                *otherVecItr *= 1 - myBonesItr->weight;
                *otherVecItr += *myVecItr;
                myBonesItr++;
                if (myBonesItr == myBonesEnd) {
                    break;
                }
                otherBonesItr++;
                if (otherBonesItr >= otherBonesEnd) {
                    TestDstComplain(myBonesItr->name);
                    return;
                }
                otherVecItr++;
                myVecItr++;
            }
        }
        if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_ROTX]));
            Hmx::Quat *otherQuatItr = (Hmx::Quat *)(bones.mStart + bones.mOffsets[TYPE_QUAT]);
            Hmx::Quat *myQuatItr = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
            while (true) {
                while (otherBonesItr->name != myBonesItr->name) {
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                }
                float scalar = 1 - myBonesItr->weight;
                otherQuatItr->x *= scalar;
                otherQuatItr->y *= scalar;
                otherQuatItr->z *= scalar;
                otherQuatItr->w *= scalar;
                float abs = fabsf(myBonesItr->weight);
                Hmx::Quat q(
                    myQuatItr->x * abs,
                    myQuatItr->y * abs,
                    myQuatItr->z * abs,
                    myQuatItr->w * myBonesItr->weight
                );
                if (q * *otherQuatItr < 0) {
                    otherQuatItr->x -= q.x;
                    otherQuatItr->y -= q.y;
                    otherQuatItr->z -= q.z;
                    otherQuatItr->w -= q.w;
                } else {
                    otherQuatItr->x += q.x;
                    otherQuatItr->y += q.y;
                    otherQuatItr->z += q.z;
                    otherQuatItr->w += q.w;
                }
                myBonesItr++;
                if (myBonesItr == myBonesEnd) {
                    break;
                }
                otherBonesItr++;
                if (otherBonesItr >= otherBonesEnd) {
                    TestDstComplain(myBonesItr->name);
                    return;
                }
                otherQuatItr++;
                myQuatItr++;
            }
        }
        if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_END]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_END]));
            float *otherRotItr = (float *)(bones.mStart + bones.mOffsets[TYPE_ROTX]);
            float *myRotItr = (float *)(mStart + mOffsets[TYPE_ROTX]);
            while (true) {
                while (otherBonesItr->name != myBonesItr->name) {
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherRotItr++;
                }
                *otherRotItr *= 1 - myBonesItr->weight;
                *otherRotItr += *myRotItr * myBonesItr->weight;
                myBonesItr++;
                if (myBonesItr == myBonesEnd) {
                    return;
                }
                otherBonesItr++;
                if (otherBonesItr >= otherBonesEnd) {
                    TestDstComplain(myBonesItr->name);
                    return;
                }
                otherRotItr++;
                myRotItr++;
            }
        }
    }
}

// MARK: ScaleAdd (CharBones)
void CharBones::ScaleAdd(CharBones &bones, float f2) const {
    if (!mBones.empty()) {
        Bone *myBonesItr = (Bone *)mBones.data();
        if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_POS]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_QUAT]));
            Vector3 *otherVecItr = (Vector3 *)bones.mStart;
            if (mCompression >= kCompressVects) {
                ShortVector3 *myVecItr = (ShortVector3 *)mStart;
                while (true) {
                    Vector3 v;
                    myVecItr->ToVector3(v);
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherVecItr++;
                    }
                    ScaleAddEq(*otherVecItr, v, f2);
                    otherBonesItr->weight += myBonesItr->weight * f2;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherVecItr++;
                    myVecItr++;
                }
            } else {
                Vector3 *myVecItr = (Vector3 *)mStart;
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherVecItr++;
                    }
                    ScaleAddEq(*otherVecItr, *myVecItr, f2);
                    otherBonesItr->weight += myBonesItr->weight * f2;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherVecItr++;
                    myVecItr++;
                }
            }
        }
        if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
            float f2abs = fabsf(f2);
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_ROTX]));
            Hmx::Quat *otherQuatItr = (Hmx::Quat *)(bones.mStart + bones.mOffsets[TYPE_QUAT]);
            if (mCompression >= kCompressQuats) {
                float absConstant = f2abs * 0.007874016f;
                float notAbsConstant = f2 * 0.007874016f;
                ByteQuat *myQuatItr = (ByteQuat *)(mStart + mOffsets[TYPE_QUAT]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherQuatItr++;
                    }
                    Hmx::Quat q;
                    q.Set(
                        myQuatItr->x * absConstant,
                        myQuatItr->y * absConstant,
                        myQuatItr->z * absConstant,
                        myQuatItr->w * notAbsConstant
                    );
                    if (q * *otherQuatItr < 0) {
                        otherQuatItr->x -= q.x;
                        otherQuatItr->y -= q.y;
                        otherQuatItr->z -= q.z;
                        otherQuatItr->w -= q.w;
                    } else {
                        otherQuatItr->x += q.x;
                        otherQuatItr->y += q.y;
                        otherQuatItr->z += q.z;
                        otherQuatItr->w += q.w;
                    }
                    otherBonesItr->weight += myBonesItr->weight * f2;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                    myQuatItr++;
                }
            } else if (mCompression != kCompressNone) {
                float absConstant = f2abs * 0.000030518509f;
                float notAbsConstant = f2 * 0.000030518509f;
                ShortQuat *myQuatItr = (ShortQuat *)(mStart + mOffsets[TYPE_QUAT]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherQuatItr++;
                    }
                    Hmx::Quat q;
                    q.Set(
                        myQuatItr->x * absConstant,
                        myQuatItr->y * absConstant,
                        myQuatItr->z * absConstant,
                        myQuatItr->w * notAbsConstant
                    );
                    // Hmx::Quat::operator* spelled out with the image's term
                    // order.  Every component here arrives via lha + fcfid +
                    // frsp, so the dot's term order is what drives the whole
                    // block's schedule: the lha order, and which component the
                    // sign branch gets to store before the join.  The header's
                    // x,y,z,w expression gives us y,x,z,w here; the image
                    // accumulates z, y, w, x.  (The ByteQuat arm above and the
                    // uncompressed arm below both match with the header form --
                    // their components are ready in one instruction, so the
                    // scheduler has nothing to reorder around.)
                    //
                    // RESIDUAL (W16-RF fork F, 99.95 fuzzy / mpn 100): two operand-order
                    // rows, both on x. The image has q.x as the first operand of
                    // the dot's x term and of the else arm's x add; ours has
                    // otherQuatItr->x first. Byte-identical: swapping the operands in
                    // either place, assigning q.x after y/z/w, a Quat ctor in place of
                    // Set, and a `const Quat &o` alias for *otherQuatItr. Worse: one
                    // expression (98.15), z,y,w,x accumulation (98.17), paired sums
                    // (95.74), and `*otherQuatItr * q` (98.4).
                    float quatDot = q.y * otherQuatItr->y;
                    quatDot += q.z * otherQuatItr->z;
                    quatDot += q.w * otherQuatItr->w;
                    quatDot += q.x * otherQuatItr->x;
                    if (quatDot < 0) {
                        otherQuatItr->x -= q.x;
                        otherQuatItr->y -= q.y;
                        otherQuatItr->z -= q.z;
                        otherQuatItr->w -= q.w;
                    } else {
                        otherQuatItr->x += q.x;
                        otherQuatItr->y += q.y;
                        otherQuatItr->z += q.z;
                        otherQuatItr->w += q.w;
                    }
                    otherBonesItr->weight += myBonesItr->weight * f2;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                    myQuatItr++;
                }
            } else {
                Hmx::Quat *myQuatItr = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherQuatItr++;
                    }
                    Hmx::Quat q;
                    q.Set(
                        myQuatItr->x * f2abs,
                        myQuatItr->y * f2abs,
                        myQuatItr->z * f2abs,
                        myQuatItr->w * f2
                    );
                    if (q * *otherQuatItr < 0) {
                        otherQuatItr->x -= q.x;
                        otherQuatItr->y -= q.y;
                        otherQuatItr->z -= q.z;
                        otherQuatItr->w -= q.w;
                    } else {
                        otherQuatItr->x += q.x;
                        otherQuatItr->y += q.y;
                        otherQuatItr->z += q.z;
                        otherQuatItr->w += q.w;
                    }
                    otherBonesItr->weight += myBonesItr->weight * f2;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                    myQuatItr++;
                }
            }
        }
        if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_END]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_END]));
            float *otherRotItr = (float *)(bones.mStart + bones.mOffsets[TYPE_ROTX]);
            if (mCompression != kCompressNone) {
                float shortConstant = f2 * 0.00061035156f;
                short *myRotItr = (short *)(mStart + mOffsets[TYPE_ROTX]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherRotItr++;
                    }
                    *otherRotItr += *myRotItr * shortConstant;
                    otherBonesItr->weight += myBonesItr->weight * f2;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        return;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherRotItr++;
                    myRotItr++;
                }
            } else {
                float *myRotItr = (float *)(mStart + mOffsets[TYPE_ROTX]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherRotItr++;
                    }
                    *otherRotItr += *myRotItr * f2;
                    otherBonesItr->weight += myBonesItr->weight * f2;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        return;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherRotItr++;
                    myRotItr++;
                }
            }
        }
    }
}

// RotateBy inlines Mtx.h's Multiply(Quat, Quat, Quat) in its three quat arms
// exactly as RotateTo does, and under /fp:fast MSVC reassociates the header's
// nested Set() per call site.  Same accumulator lever as RotateToMultiply
// below, but RotateBy's product is q * other (RotateTo's is other * q) and
// the image's association here is its own: the two compressed arms emit the
// components w, z, y, x (0x823C6DB4..0x823C6DCC) seeded from a.x*b.x,
// a.z*b.w, a.z*b.x and a.w*b.x; the uncompressed arm emits y, z, w, x
// (0x823C6F44..0x823C6F5C) and seeds y from a.y*b.w and x from a.w*b.x
// with a.x*b.w as the second term.  Each accumulator's first two terms are
// written PRE-SWAPPED, as in the RotateTo helpers: MSVC seeds from the
// second written term.
//
// RESIDUAL (w7-ba, 99.66 canonical, 330/357 rows; was 93.99): the two
// compressed arms are row-for-row equal.  The uncompressed arm keeps the
// image's seeds, fmadds sequence and store order, but the image loads a.z
// before b.w and slots the hoisted `cmplw cr6, r26, r24` at 0x823C6EF8 before
// the first fmuls, where ours issues the y seed as soon as its two operands
// are loaded (25 register rows + the two-instruction cmplw/fmuls transposition).
// Measured on that arm: source operand order inside the seeds is normalised
// (byte-identical); declaring z before y re-orders the emission to y, w, x, z
// (99.08, worse).  Not a source-visible knob that was found.
static void RotateByMultiply(const Hmx::Quat &a, const Hmx::Quat &b, Hmx::Quat &out) {
    float rw = a.w * b.w - a.x * b.x;
    rw -= a.y * b.y;
    rw -= a.z * b.z;
    float rz = a.w * b.z;
    rz += a.z * b.w;
    rz += a.x * b.y;
    rz -= a.y * b.x;
    float ry = a.w * b.y;
    ry += a.z * b.x;
    ry += a.y * b.w;
    ry -= a.x * b.z;
    float rx = a.y * b.z;
    rx += a.w * b.x;
    rx += a.x * b.w;
    rx -= a.z * b.y;
    out.Set(rx, ry, rz, rw);
}

static void RotateByMultiplyUncompressed(
    const Hmx::Quat &a, const Hmx::Quat &b, Hmx::Quat &out
) {
    float rw = a.w * b.w - a.x * b.x;
    rw -= a.y * b.y;
    rw -= a.z * b.z;
    float rz = a.x * b.y;
    rz += a.z * b.w;
    rz += a.w * b.z;
    rz -= a.y * b.x;
    float ry = a.z * b.x;
    ry += a.y * b.w;
    ry += a.w * b.y;
    ry -= a.x * b.z;
    float rx = a.y * b.z;
    rx += a.x * b.w;
    rx += a.w * b.x;
    rx -= a.z * b.y;
    out.Set(rx, ry, rz, rw);
}

// MARK: RotateBy
// RESIDUAL (W16-RF fork F, 99.94 fuzzy / mpn 100): two `fadds` operand-order rows,
// the x add of the ShortVector3 arm and the z add of the Vector3 arm. Byte-identical:
// `Add(*otherVecItr, v, *otherVecItr)` and per-component `a = b + a` spellings. Worse:
// `Add(*myVecItr, *otherVecItr, ...)` (96.83), and ToVector3 after the name search
// or a Vector3(short *) temporary (87.72).
void CharBones::RotateBy(CharBones &bones) const {
    if (!mBones.empty()) {
        Bone *myBonesItr = (Bone *)mBones.data();
        if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_POS]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_QUAT]));
            Vector3 *otherVecItr = (Vector3 *)bones.mStart;
            if (mCompression >= kCompressVects) {
                ShortVector3 *myVecItr = (ShortVector3 *)mStart;
                while (true) {
                    Vector3 v;
                    myVecItr->ToVector3(v);
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (myBonesItr && otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherVecItr++;
                    }
                    *otherVecItr += v;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherVecItr++;
                    myVecItr++;
                }
            } else {
                Vector3 *myVecItr = (Vector3 *)mStart;
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherVecItr++;
                    }
                    *otherVecItr += *myVecItr;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherVecItr++;
                    myVecItr++;
                }
            }
        }
        if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_ROTX]));
            Hmx::Quat *otherQuatItr = (Hmx::Quat *)(bones.mStart + bones.mOffsets[TYPE_QUAT]);
            if (mCompression >= kCompressQuats) {
                ByteQuat *myQuatItr = (ByteQuat *)(mStart + mOffsets[TYPE_QUAT]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherQuatItr++;
                    }
                    Hmx::Quat q;
                    myQuatItr->ToQuat(q);
#ifdef HX_NATIVE
                    {
                        // Native association kept from the pre-DC3 body (same
                        // product, q * other).
                        float dx = otherQuatItr->x, dy = otherQuatItr->y;
                        float dz = otherQuatItr->z, dw = otherQuatItr->w;
                        float nw = q.w*dw - q.x*dx - q.y*dy - q.z*dz;
                        float nx = q.w*dx + q.x*dw + q.y*dz - q.z*dy;
                        float ny = q.w*dy - q.x*dz + q.y*dw + q.z*dx;
                        float nz = q.w*dz + q.x*dy - q.y*dx + q.z*dw;
                        otherQuatItr->x = nx; otherQuatItr->y = ny;
                        otherQuatItr->z = nz; otherQuatItr->w = nw;
                    }
#else
                    RotateByMultiply(q, *otherQuatItr, *otherQuatItr);
#endif
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                    myQuatItr++;
                }
            } else if (mCompression != kCompressNone) {
                ShortQuat *myQuatItr = (ShortQuat *)(mStart + mOffsets[TYPE_QUAT]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherQuatItr++;
                    }
                    Hmx::Quat q;
                    myQuatItr->ToQuat(q);
#ifdef HX_NATIVE
                    {
                        // Native association kept from the pre-DC3 body (same
                        // product, q * other).
                        float dx = otherQuatItr->x, dy = otherQuatItr->y;
                        float dz = otherQuatItr->z, dw = otherQuatItr->w;
                        float nw = q.w*dw - q.x*dx - q.y*dy - q.z*dz;
                        float nx = q.w*dx + q.x*dw + q.y*dz - q.z*dy;
                        float ny = q.w*dy - q.x*dz + q.y*dw + q.z*dx;
                        float nz = q.w*dz + q.x*dy - q.y*dx + q.z*dw;
                        otherQuatItr->x = nx; otherQuatItr->y = ny;
                        otherQuatItr->z = nz; otherQuatItr->w = nw;
                    }
#else
                    RotateByMultiply(q, *otherQuatItr, *otherQuatItr);
#endif
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                    myQuatItr++;
                }
            } else {
                Hmx::Quat *myQuatItr = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherQuatItr++;
                    }
                    RotateByMultiplyUncompressed(*myQuatItr, *otherQuatItr, *otherQuatItr);
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                    myQuatItr++;
                }
            }
        }
        if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_END]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_END]));
            float *otherRotItr = (float *)(bones.mStart + bones.mOffsets[TYPE_ROTX]);
            if (mCompression != kCompressNone) {
                short *myRotItr = (short *)(mStart + mOffsets[TYPE_ROTX]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherRotItr++;
                    }
                    *otherRotItr += *myRotItr * 0.00061035156f;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        return;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherRotItr++;
                    myRotItr++;
                }
            } else {
                float *myRotItr = (float *)(mStart + mOffsets[TYPE_ROTX]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherRotItr++;
                    }
                    *otherRotItr += *myRotItr;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        return;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherRotItr++;
                    myRotItr++;
                }
            }
        }
    }
}

// Hmx::Quat's Multiply(q1, q2, out) in math/Mtx.h is the right expression tree
// for every other caller in the binary, but under /fp:fast MSVC reassociates it
// per call site, and RotateTo's three quat arms all want a different order from
// the one the header's nested Set() produces (ours comes out z, x, w, y; the
// image emits w, z, y, x and interleaves the four accumulations).  Per-component
// accumulator statements pin the association -- a `+=` is a reassociation
// barrier -- so the order is spelled out here once and shared by all three arms.
// Same lever as Multiply(Vector3, Matrix3) at the CharIKHead/CharLookAt Poll
// call sites; see the note above Multiply(const Vector3 &, const Hmx::Matrix3 &,
// Vector3 &) in math/Mtx.h for the measurement that established it.
//
// Both orders below are PRE-SWAPPED: MSVC emits the first two of whatever order
// is written transposed, independently in each dimension (the same rule that
// closed CharIKHead::Poll and CharBones::ScaleAdd).  The image emits the
// components w, z, y, x and seeds z from a.x*b.y, y from a.z*b.x and x from
// a.x*b.w -- so the accumulators are declared w, y, z, x and each one's first
// two terms are written the other way round.  The w component is anchored
// first in both builds; it is a single expression, not an accumulator chain.
static void RotateToMultiply(const Hmx::Quat &a, const Hmx::Quat &b, Hmx::Quat &out) {
    float rw = a.w * b.w - a.x * b.x;
    rw -= a.y * b.y;
    rw -= a.z * b.z;
    float rz = a.z * b.w;
    rz += a.x * b.y;
    rz += a.w * b.z;
    rz -= a.y * b.x;
    float ry = a.y * b.w;
    ry += a.z * b.x;
    ry += a.w * b.y;
    ry -= a.x * b.z;
    float rx = a.y * b.z;
    rx += a.x * b.w;
    rx += a.w * b.x;
    rx -= a.z * b.y;
    out.Set(rx, ry, rz, rw);
}

// ...and the uncompressed arm needs its OWN order.  MSVC's reassociation of the
// header's Multiply(Quat, Quat, Quat) is per call site, not per function: in the
// two compressed arms `q` round-trips through the stack (ByteQuat/ShortQuat both
// build it component by component), while here it is already live in FPRs, so
// the scheduler has different slack and picks a different association.  The
// image's uncompressed arm emits the components z, w, y, x -- not w, z, y, x --
// and seeds each of z, y and x from what is the THIRD term in the arms above.
// Same pre-swap of each accumulator's first two terms as in RotateToMultiply.
static void RotateToMultiplyUncompressed(
    const Hmx::Quat &a, const Hmx::Quat &b, Hmx::Quat &out
) {
    float rz = a.x * b.y;
    rz += a.w * b.z;
    rz += a.z * b.w;
    rz -= a.y * b.x;
    float rw = a.w * b.w - a.x * b.x;
    rw -= a.y * b.y;
    rw -= a.z * b.z;
    float ry = a.z * b.x;
    ry += a.w * b.y;
    ry += a.y * b.w;
    ry -= a.x * b.z;
    float rx = a.y * b.z;
    rx += a.w * b.x;
    rx += a.x * b.w;
    rx -= a.z * b.y;
    out.Set(rx, ry, rz, rw);
}

// MARK: RotateTo
// RESIDUAL (W16-RF fork F, 99.76 fuzzy / mpn 100): FPR operand-order and f2<->f4 rows
// in the ShortQuat and uncompressed arms (RotateToMultiply's operands). Tried in the
// uncompressed arm's q.Set: `f2 * x` / `f2 * z` (byte-identical) and per-component
// assignments in w,x,y,z order (99.73).
void CharBones::RotateTo(CharBones &bones, float f2) const {
    if (!mBones.empty()) {
        Bone *myBonesItr = (Bone *)mBones.data();
        if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_POS]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_QUAT]));
            Vector3 *otherVecItr = (Vector3 *)bones.mStart;
            if (mCompression >= kCompressVects) {
                ShortVector3 *myVecItr = (ShortVector3 *)mStart;
                while (true) {
                    Vector3 v;
                    myVecItr->ToVector3(v);
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherVecItr++;
                    }
                    ScaleAddEq(*otherVecItr, v, f2);
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherVecItr++;
                    myVecItr++;
                }
            } else {
                Vector3 *myVecItr = (Vector3 *)mStart;
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherVecItr++;
                    }
                    ScaleAddEq(*otherVecItr, *myVecItr, f2);
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherVecItr++;
                    myVecItr++;
                }
            }
        }
        if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_QUAT]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_ROTX]));
            Hmx::Quat *otherQuatItr = (Hmx::Quat *)(bones.mStart + bones.mOffsets[TYPE_QUAT]);
            if (mCompression >= kCompressQuats) {
                ByteQuat *myQuatItr = (ByteQuat *)(mStart + mOffsets[TYPE_QUAT]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherQuatItr++;
                    }
                    Hmx::Quat q;
                    myQuatItr->ToQuat(q);
                    q.x *= f2;
                    q.y *= f2;
                    q.z *= f2;
                    if (q.w < 0) {
                        q.w = (q.w * f2) - (1 - f2);
                    } else {
                        q.w = (q.w * f2) + (1 - f2);
                    }
#ifdef HX_NATIVE
                    {
                        // Native fix (kept from the pre-DC3 body): the quaternion
                        // product is taken as q * other, (sw,sx,sy,sz)*(dw,dx,dy,dz).
                        float sw_ = q.w, sx_ = q.x, sy_ = q.y, sz_ = q.z;
                        float dx = otherQuatItr->x, dy = otherQuatItr->y;
                        float dz = otherQuatItr->z, dw = otherQuatItr->w;
                        float nw = sw_*dw - sx_*dx - sy_*dy - sz_*dz;
                        float nx = sw_*dx + sx_*dw + sy_*dz - sz_*dy;
                        float ny = sw_*dy - sx_*dz + sy_*dw + sz_*dx;
                        float nz = sw_*dz + sx_*dy - sy_*dx + sz_*dw;
                        otherQuatItr->x = nx; otherQuatItr->y = ny;
                        otherQuatItr->z = nz; otherQuatItr->w = nw;
                    }
#else
                    RotateToMultiply(*otherQuatItr, q, *otherQuatItr);
#endif
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                    myQuatItr++;
                }
            } else if (mCompression != kCompressNone) {
                ShortQuat *myQuatItr = (ShortQuat *)(mStart + mOffsets[TYPE_QUAT]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherQuatItr++;
                    }
                    Hmx::Quat q;
                    myQuatItr->ToQuat(q);
                    q.x *= f2;
                    q.y *= f2;
                    q.z *= f2;
                    if (q.w < 0) {
                        q.w = (q.w * f2) - (1 - f2);
                    } else {
                        q.w = (q.w * f2) + (1 - f2);
                    }
#ifdef HX_NATIVE
                    {
                        // Native fix (kept from the pre-DC3 body): the quaternion
                        // product is taken as q * other, (sw,sx,sy,sz)*(dw,dx,dy,dz).
                        float sw_ = q.w, sx_ = q.x, sy_ = q.y, sz_ = q.z;
                        float dx = otherQuatItr->x, dy = otherQuatItr->y;
                        float dz = otherQuatItr->z, dw = otherQuatItr->w;
                        float nw = sw_*dw - sx_*dx - sy_*dy - sz_*dz;
                        float nx = sw_*dx + sx_*dw + sy_*dz - sz_*dy;
                        float ny = sw_*dy - sx_*dz + sy_*dw + sz_*dx;
                        float nz = sw_*dz + sx_*dy - sy_*dx + sz_*dw;
                        otherQuatItr->x = nx; otherQuatItr->y = ny;
                        otherQuatItr->z = nz; otherQuatItr->w = nw;
                    }
#else
                    RotateToMultiply(*otherQuatItr, q, *otherQuatItr);
#endif
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                    myQuatItr++;
                }
            } else {
                Hmx::Quat *myQuatItr = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherQuatItr++;
                    }
                    Hmx::Quat q;
                    q.Set(
                        myQuatItr->x * f2,
                        myQuatItr->y * f2,
                        myQuatItr->z * f2,
                        myQuatItr->w * f2
                    );
                    if (myQuatItr->w < 0) {
                        q.w -= (1 - f2);
                    } else {
                        q.w += (1 - f2);
                    }
#ifdef HX_NATIVE
                    {
                        // Native fix (kept from the pre-DC3 body): the quaternion
                        // product is taken as q * other, (sw,sx,sy,sz)*(dw,dx,dy,dz).
                        float sw_ = q.w, sx_ = q.x, sy_ = q.y, sz_ = q.z;
                        float dx = otherQuatItr->x, dy = otherQuatItr->y;
                        float dz = otherQuatItr->z, dw = otherQuatItr->w;
                        float nw = sw_*dw - sx_*dx - sy_*dy - sz_*dz;
                        float nx = sw_*dx + sx_*dw + sy_*dz - sz_*dy;
                        float ny = sw_*dy - sx_*dz + sy_*dw + sz_*dx;
                        float nz = sw_*dz + sx_*dy - sy_*dx + sz_*dw;
                        otherQuatItr->x = nx; otherQuatItr->y = ny;
                        otherQuatItr->z = nz; otherQuatItr->w = nw;
                    }
#else
                    RotateToMultiplyUncompressed(*otherQuatItr, q, *otherQuatItr);
#endif
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        break;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherQuatItr++;
                    myQuatItr++;
                }
            }
        }
        if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
            Bone *otherBonesItr = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_ROTX]));
            Bone *otherBonesEnd = (Bone *)(bones.mBones.data() + (bones.mCounts[TYPE_END]));
            Bone *myBonesEnd = (Bone *)(mBones.data() + (mCounts[TYPE_END]));
            float *otherRotItr = (float *)(bones.mStart + bones.mOffsets[TYPE_ROTX]);
            if (mCompression != kCompressNone) {
                float shortConstant = f2 * 0.00061035156f;
                short *myRotItr = (short *)(mStart + mOffsets[TYPE_ROTX]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherRotItr++;
                    }
                    *otherRotItr += *myRotItr * shortConstant;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        return;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherRotItr++;
                    myRotItr++;
                }
            } else {
                float *myRotItr = (float *)(mStart + mOffsets[TYPE_ROTX]);
                while (true) {
                    while (otherBonesItr->name != myBonesItr->name) {
                        otherBonesItr++;
                        if (otherBonesItr >= otherBonesEnd) {
                            TestDstComplain(myBonesItr->name);
                            return;
                        }
                        otherRotItr++;
                    }
                    *otherRotItr += *myRotItr * f2;
                    myBonesItr++;
                    if (myBonesItr == myBonesEnd) {
                        return;
                    }
                    otherBonesItr++;
                    if (otherBonesItr >= otherBonesEnd) {
                        TestDstComplain(myBonesItr->name);
                        return;
                    }
                    otherRotItr++;
                    myRotItr++;
                }
            }
        }
    }
}

CharBonesAlloc::~CharBonesAlloc() {
    MemFree(mStart);
}

void CharBonesAlloc::ReallocateInternal() {
    MemFree(mStart);
    mStart = (char *)MemAlloc(mTotalSize, __FILE__, 0x6C0, "CharBones");
}
