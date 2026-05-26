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

short MakeShortAng(float f) {
    f = f * 1638.4f + 0.5f;
    MILO_ASSERT(f < 32768 && f > -32767, 0x60);
    f = floor(f);
    return f;
}

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

void ShortQuat::ToQuat(Hmx::Quat &quat) const {
    quat.Set(
        (float)(long long)x * 3.051851e-05f,
        (float)(long long)y * 3.051851e-05f,
        (float)(long long)z * 3.051851e-05f,
        (float)(long long)w * 3.051851e-05f
    );
}

void ByteQuat::ToQuat(Hmx::Quat &quat) const {
    quat.Set(
        (float)(long long)x * 0.0078740157f,
        (float)(long long)y * 0.0078740157f,
        (float)(long long)z * 0.0078740157f,
        (float)(long long)w * 0.0078740157f
    );
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
        int count_diff = offset[-6] - offset[-7];
        *++offset = cur_offset + TypeSize(i) * count_diff;
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
    char c = *p;
    while (c != 0) {
        if (c == '.') {
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
        c = *++p;
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

void CharBones::Print() {
    for (auto it = mBones.begin(); it != mBones.end(); ++it) {
        MILO_LOG("%s %.2f: %s\n", it->name, it->weight, StringVal(it->name));
    }
}

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
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

void CharBones::ScaleAdd(CharClip *clip, float f1, float f2, float f3) {
    clip->ScaleAdd(*this, f1, f2, f3);
}

void CharBones::AddBoneInternal(const Bone &bone) {
    Type type = TypeOf(bone.name);
    int end = mCounts[type + 1];
    int start = mCounts[type];
    int pos = start;
    if (start < end) {
        const char *name = bone.name.Str();
        do {
            const char *existing = mBones[pos].name.Str();
            if (existing == name) return;
            if (strcmp(existing, name) >= 0) break;
            pos++;
        } while (pos < end);
    }
    mBones.insert(mBones.begin() + pos, 1, bone);
    int size = TypeSize(type);
    for (int i = type + 1; i < NUM_TYPES; i++) {
        mCounts[i]++;
        mOffsets[i] += size;
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
void CharBones::ScaleDown(CharBones &dst, float f) const {
    const Bone *src = mBones.begin();
    if (src == mBones.end())
        return;

    if (f == 0.0f) {
        if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
            Bone *db_begin = dst.mBones.begin();
            Vector3 *data = (Vector3 *)dst.mStart;
            Bone *db = db_begin + dst.mCounts[TYPE_POS];
            Bone *db_end = db_begin + dst.mCounts[TYPE_QUAT];
            const Bone *src_end = src + mCounts[TYPE_QUAT];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    data++;
                }
                src++;
                data->z = 0.0f;
                data->y = 0.0f;
                data->x = 0.0f;
                db->weight = 0.0f;
                if (src == src_end) goto zero_quat;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                data++;
            }
        }
    zero_quat:
        if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
            Bone *db_begin = dst.mBones.begin();
            Hmx::Quat *qdata = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
            Bone *db = db_begin + dst.mCounts[TYPE_QUAT];
            Bone *db_end = db_begin + dst.mCounts[TYPE_ROTX];
            const Bone *src_end = mBones.begin() + mCounts[TYPE_ROTX];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    qdata++;
                }
                src++;
                qdata->x = 0.0f;
                qdata->y = 0.0f;
                qdata->z = 0.0f;
                qdata->w = 0.0f;
                db->weight = 0.0f;
                if (src == src_end) goto zero_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                qdata++;
            }
        }
    zero_rot:
        if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
            Bone *db_begin = dst.mBones.begin();
            float *fdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
            Bone *db = db_begin + dst.mCounts[TYPE_ROTX];
            Bone *db_end = db_begin + dst.mCounts[TYPE_END];
            const Bone *src_end = mBones.begin() + mCounts[TYPE_END];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    fdata++;
                }
                src++;
                *fdata = 0.0f;
                db->weight = 0.0f;
                if (src == src_end) return;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                fdata++;
            }
        }
    } else {
        if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
            Bone *db_begin = dst.mBones.begin();
            Vector3 *data = (Vector3 *)dst.mStart;
            Bone *db = db_begin + dst.mCounts[TYPE_POS];
            Bone *db_end = db_begin + dst.mCounts[TYPE_QUAT];
            const Bone *src_end = src + mCounts[TYPE_QUAT];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    data++;
                }
                src++;
                data->x *= f;
                data->y *= f;
                data->z *= f;
                if (src == src_end) goto scale_quat;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                data++;
            }
        }
    scale_quat:
        if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
            Bone *db_begin = dst.mBones.begin();
            Hmx::Quat *qdata = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
            Bone *db = db_begin + dst.mCounts[TYPE_QUAT];
            Bone *db_end = db_begin + dst.mCounts[TYPE_ROTX];
            const Bone *src_end = mBones.begin() + mCounts[TYPE_ROTX];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    qdata++;
                }
                src++;
                qdata->x *= f;
                qdata->y *= f;
                qdata->z *= f;
                qdata->w *= f;
                if (src == src_end) goto scale_rot;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                qdata++;
            }
        }
    scale_rot:
        if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
            Bone *db_begin = dst.mBones.begin();
            float *fdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
            Bone *db = db_begin + dst.mCounts[TYPE_ROTX];
            Bone *db_end = db_begin + dst.mCounts[TYPE_END];
            const Bone *src_end = mBones.begin() + mCounts[TYPE_END];
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) {
                        TestDstComplain(src->name);
                        return;
                    }
                    fdata++;
                }
                src++;
                *fdata *= f;
                if (src == src_end) return;
                db++;
                if (db >= db_end) {
                    TestDstComplain(src->name);
                    return;
                }
                fdata++;
            }
        }
    }
}

// MARK: Blend
void CharBones::Blend(CharBones &dst) const {
    MILO_ASSERT(!mCompression && !dst.mCompression, 0x311);
    const Bone *src = mBones.begin();
    if (src == mBones.end()) return;


    auto& counts = mCounts;
    if (counts[TYPE_QUAT] > counts[TYPE_POS]) {
        Vector3 *sdata = (Vector3 *)mStart;
        Vector3 *ddata = (Vector3 *)dst.mStart;
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_POS];
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        const Bone *src_end = src + counts[TYPE_QUAT];
        while (true) {
            while (db->name != src->name) {
                db++;
                if (db >= db_end) goto complain;
                ddata++;
            }
            float wt = 1.0f - src->weight;
            ddata->x *= wt;
            ddata->y *= wt;
            ddata->z *= wt;
            ddata->x += sdata->x;
            ddata->y += sdata->y;
            ddata->z += sdata->z;
            src++;
            if (src >= src_end) goto blend_quat;
            db++;
            if (db >= db_end) goto complain;
            ddata++;
            sdata++;
        }
    }
blend_quat:
    if (counts[TYPE_ROTX] > counts[TYPE_QUAT]) {
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        Hmx::Quat *dquat = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
        Hmx::Quat *squat = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
        const Bone *src_end = mBones.data() + counts[TYPE_ROTX];
        while (true) {
            while (db->name != src->name) {
                db++;
                if (db >= db_end) goto complain;
                dquat++;
            }
            float wt = 1.0f - src->weight;
            dquat->w *= wt;
            dquat->x *= wt;
            dquat->y *= wt;
            dquat->z *= wt;
            float abs_wt = fabsf(src->weight);
            float sy = squat->y * abs_wt;
            float sx = squat->x * abs_wt;
            float sz = squat->z * abs_wt;
            float sw = src->weight * squat->w;
            if (((dquat->x * sx + (dquat->y * sy + (dquat->w * sw + dquat->z * sz)))) < 0.0f) {
                dquat->x -= sx;
                dquat->y -= sy;
                dquat->z -= sz;
                dquat->w -= sw;
            } else {
                dquat->x += sx;
                dquat->y += sy;
                dquat->z += sz;
                dquat->w += sw;
            }
            src++;
            if (src >= src_end) goto blend_rot;
            db++;
            if (db >= db_end) goto complain;
            dquat++;
            squat++;
        }
    }
blend_rot:
    if (counts[TYPE_END] > counts[TYPE_ROTX]) {
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_END];
        float *dfdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
        float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
        const Bone *src_end = mBones.data() + counts[TYPE_END];
        while (true) {
            while (db->name != src->name) {
                db++;
                if (db >= db_end) goto complain;
                dfdata++;
            }
            *dfdata *= (1.0f - src->weight);
            float wt = src->weight;
            src++;
            *dfdata += wt * *sfdata;
            if (src >= src_end) return;
            db++;
            if (db >= db_end) goto complain;
            dfdata++;
            sfdata++;
        }
    }
    return;

complain:
    TestDstComplain(src->name);
}

// MARK: ScaleAdd (CharBones)
void CharBones::ScaleAdd(CharBones &dst, float f) const {
    const Bone *src = mBones.begin();
    if (src == mBones.end()) return;

    if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
        Vector3 *ddata = (Vector3 *)dst.mStart;
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_POS];
        const Bone *src_end = src + mCounts[TYPE_QUAT];
        if (mCompression >= kCompressVects) {
            short *sdata = (short *)mStart;
            while (true) {
                short sz = sdata[2];
                short sy = sdata[1];
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    ddata++;
                }
                ddata->x += (float)sdata[0] * 0.039674062f * f;
                ddata->z += (float)sz * 0.039674062f * f;
                ddata->y += (float)sy * 0.039674062f * f;
                db->weight += src->weight * f;
                src++;
                if (src == src_end) goto add_quat;
                db++;
                if (db >= db_end) goto complain;
                ddata++;
                sdata += 3;
            }
        } else {
            Vector3 *sdata = (Vector3 *)mStart;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    ddata++;
                }
                ddata->x += sdata->x * f;
                ddata->y += sdata->y * f;
                ddata->z += sdata->z * f;
                db->weight += src->weight * f;
                src++;
                if (src == src_end) goto add_quat;
                db++;
                if (db >= db_end) goto complain;
                ddata++;
                sdata++;
            }
        }
    }
add_quat:
    if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        const Bone *src_end = mBones.begin() + mCounts[TYPE_ROTX];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Hmx::Quat *dquat = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
        float abs_f = fabs(f);
        if (mCompression >= kCompressQuats) {
            char *sdata = (char *)(mStart + mOffsets[TYPE_QUAT]);
            float scale = abs_f * 0.0078740157f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dquat++;
                }
                float dy = dquat->y;
                float dx = dquat->x;
                float dz = dquat->z;
                float dw = dquat->w;
                float sy = (float)(long long)sdata[1] * scale;
                float sx = (float)(long long)sdata[0] * scale;
                float sz = (float)(long long)sdata[2] * scale;
                float sw = (float)(long long)sdata[3] * (f * 0.0078740157f);
                if (dw * sw + dz * sz + dx * sx + dy * sy < 0.0f) {
                    dquat->y = dy - sy;
                    dquat->z = dz - sz;
                    dquat->x = dx - sx;
                    dquat->w = dw - sw;
                } else {
                    dquat->y = dy + sy;
                    dquat->z = dz + sz;
                    dquat->x = dx + sx;
                    dquat->w = dw + sw;
                }
                db->weight += src->weight * f;
                src++;
                if (src == src_end) goto add_rot;
                db++;
                if (db >= db_end) goto complain;
                dquat++;
                sdata += 4;
            }
        } else if (mCompression != kCompressNone) {
            short *sdata = (short *)(mStart + mOffsets[TYPE_QUAT]);
            float scale = abs_f * 3.051851e-05f;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dquat++;
                }
                float dz = dquat->z;
                float dy = dquat->y;
                float dw = dquat->w;
                float dx = dquat->x;
                float sx = (float)(long long)sdata[0] * scale;
                float sz = (float)(long long)sdata[2] * scale;
                float sy = (float)(long long)sdata[1] * scale;
                float sw = (float)(long long)sdata[3] * (f * 3.051851e-05f);
                if (dx * sx + dy * sy + dz * sz + dw * sw < 0.0f) {
                    dquat->z = dz - sz;
                    dquat->x = dx - sx;
                    dquat->y = dy - sy;
                    dquat->w = dw - sw;
                } else {
                    dquat->z = sz + dz;
                    dquat->x = dx + sx;
                    dquat->y = sy + dy;
                    dquat->w = sw + dw;
                }
                db->weight += src->weight * f;
                src++;
                if (src == src_end) goto add_rot;
                db++;
                if (db >= db_end) goto complain;
                dquat++;
                sdata += 4;
            }
        } else {
            Hmx::Quat *squat = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dquat++;
                }
                float sy = squat->y * abs_f;
                float dy = dquat->y;
                float sx = squat->x * abs_f;
                float dx = dquat->x;
                float sz = squat->z * abs_f;
                float dz = dquat->z;
                float sw = squat->w * f;
                float dw = dquat->w;
                if (sx * dx + sy * dy + sz * dz + sw * dw < 0.0f) {
                    dquat->y = dy - sy;
                    dquat->z = dz - sz;
                    dquat->x = dx - sx;
                    dquat->w = dw - sw;
                } else {
                    dquat->y = sy + dy;
                    dquat->z = sz + dz;
                    dquat->x = sx + dx;
                    dquat->w = sw + dw;
                }
                db->weight += src->weight * f;
                src++;
                if (src == src_end) goto add_rot;
                db++;
                if (db >= db_end) goto complain;
                dquat++;
                squat++;
            }
        }
    }
add_rot:
    if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_END];
        float *dfdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        const Bone *src_end = mBones.begin() + mCounts[TYPE_END];
        float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
        if (mCompression != kCompressNone) {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dfdata++;
                }
                *dfdata += (float)*(short *)sfdata * (f * 0.0006103515625f);
                db->weight += src->weight * f;
                src++;
                if (src == src_end) return;
                db++;
                if (db >= db_end) goto complain;
                dfdata++;
                sfdata = (float *)((char *)sfdata + 2);
            }
        } else {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dfdata++;
                }
                *dfdata += *sfdata * f;
                db->weight += src->weight * f;
                src++;
                if (src == src_end) return;
                db++;
                if (db >= db_end) goto complain;
                dfdata++;
                sfdata++;
            }
        }
    }
    return;

complain:
    TestDstComplain(src->name);
}

// MARK: RotateBy
void CharBones::RotateBy(CharBones &dst) const {
    const Bone *src = mBones.begin();
    if (src == mBones.end()) return;

    // Position section
    auto& _ref1 = mCounts;
    if (_ref1[TYPE_QUAT] > _ref1[TYPE_POS]) {
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Vector3 *ddata = (Vector3 *)dst.mStart;
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_POS];
        const Bone *src_end = src + _ref1[TYPE_QUAT];
        if (db != nullptr && mCompression >= kCompressVects) {
            short *sdata = (short *)mStart;
            while (true) {
                long long sz = (long long)sdata[2];
                short sy = sdata[1];
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    ddata++;
                }
                src++;
                ddata->x += (float)(long long)sdata[0] * 0.039674062f;
                ddata->y += (float)(long long)sy * 0.039674062f;
                ddata->z += (float)sz * 0.039674062f;
                if (src_end == src) goto rotate_quat;
                db++;
                if (db >= db_end) goto complain;
                ddata++;
                sdata += 3;
            }
        } else {
            Vector3 *sdata = (Vector3 *)mStart;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    ddata++;
                }
                src++;
                ddata->x += sdata->x;
                ddata->y += sdata->y;
                ddata->z += sdata->z;
                if (src == src_end) goto rotate_quat;
                db++;
                if (db >= db_end) goto complain;
                ddata++;
                sdata++;
            }
        }
    }
rotate_quat:
    if (_ref1[TYPE_ROTX] > _ref1[TYPE_QUAT]) {
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Hmx::Quat *dquat = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
        int src_quat_off = mOffsets[TYPE_QUAT];
        const Bone *src_end = mBones.begin() + _ref1[TYPE_ROTX];
        if (mCompression >= kCompressQuats) {
            char *sqdata = (char *)(src_quat_off + mStart);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dquat++;
                }
                Hmx::Quat sq;
                ((ByteQuat *)sqdata)->ToQuat(sq);
                float dw = dquat->w;
                float dx = dquat->x;
                src++;
                float dz = dquat->z;
                float dy = dquat->y;
#ifdef HX_NATIVE
                float nw = sq.w*dw - sq.x*dx - sq.y*dy - sq.z*dz;
                float nx = sq.w*dx + sq.x*dw + sq.y*dz - sq.z*dy;
                float ny = sq.w*dy - sq.x*dz + sq.y*dw + sq.z*dx;
                float nz = sq.w*dz + sq.x*dy - sq.y*dx + sq.z*dw;
                dquat->x = nx; dquat->y = ny; dquat->z = nz; dquat->w = nw;
#else
                dquat->w = -(-(dy * sq.y - (dw * sq.w - dx * sq.x)) - dz * sq.z);
                dquat->z = -(dx * sq.y - ((dy * sq.x + (dz * sq.w + dw * sq.z))));
                dquat->y = -(dz * sq.x - (dw * sq.y + dy * sq.w + dx * sq.z));
                dquat->x = -(dy * sq.z - (dw * sq.x + dz * sq.y + dx * sq.w));
#endif
                if (src == src_end) goto rotate_rot;
                db++;
                if (db >= db_end) goto complain;
                dquat++;
                sqdata += 4;
            }
        } else if (mCompression != kCompressNone) {
            char *sqdata = (char *)(src_quat_off + mStart);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dquat++;
                }
                Hmx::Quat sq;
                ((ShortQuat *)sqdata)->ToQuat(sq);
                float dw = dquat->w;
                float dx = dquat->x;
                src++;
                float dz = dquat->z;
                float dy = dquat->y;
#ifdef HX_NATIVE
                float nw = sq.w*dw - sq.x*dx - sq.y*dy - sq.z*dz;
                float nx = sq.w*dx + sq.x*dw + sq.y*dz - sq.z*dy;
                float ny = sq.w*dy - sq.x*dz + sq.y*dw + sq.z*dx;
                float nz = sq.w*dz + sq.x*dy - sq.y*dx + sq.z*dw;
                dquat->x = nx; dquat->y = ny; dquat->z = nz; dquat->w = nw;
#else
                dquat->w = -(-(dy * sq.y - (dw * sq.w - dx * sq.x)) - dz * sq.z);
                dquat->z = -(dx * sq.y - (dy * sq.x + dz * sq.w + dw * sq.z));
                dquat->y = -(dz * sq.x - (dw * sq.y + dy * sq.w + dx * sq.z));
                dquat->x = -(dy * sq.z - (dw * sq.x + dz * sq.y + dx * sq.w));
#endif
                if (src == src_end) goto rotate_rot;
                db++;
                if (db >= db_end) goto complain;
                dquat++;
                sqdata += 8;
            }
        } else {
            Hmx::Quat *squat = (Hmx::Quat *)(src_quat_off + mStart);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dquat++;
                }
                float sy = squat->y;
                src++;
                float sz = squat->z;
                float dw = dquat->w;
                float sx = squat->x;
                float dx = dquat->x;
                float sw = squat->w;
                float dz = dquat->z;
                float dy = dquat->y;
                // dst = src * dst (quaternion multiply)
                dquat->y = -(sx * dz - (dy * sw + dx * sz + sy * dw));
                dquat->z = (sy * dx - (dy * sx + sw * dz + dw * sz));
                dquat->z = -dquat->z;
                dquat->w = -(dz * sz - -(dy * sy - (dw * sw - sx * dx)));
                dquat->x = -(dy * sz - (sy * dz + sx * dw + dx * sw));
                if (src == src_end) goto rotate_rot;
                db++;
                if (db >= db_end) goto complain;
                dquat++;
                squat++;
            }
        }
    }
rotate_rot:
    if (_ref1[TYPE_END] > _ref1[TYPE_ROTX]) {
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_END];
        const Bone *src_end = mBones.begin() + _ref1[TYPE_END];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        float *dfdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
        float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
        if (mCompression != kCompressNone) {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dfdata++;
                }
                src++;
                *dfdata += (float)(long long)*(short *)sfdata * 0.00061035156f;
                if (src == src_end) return;
                db++;
                if (db >= db_end) goto complain;
                dfdata++;
                sfdata = (float *)((char *)sfdata + 2);
            }
        } else {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dfdata++;
                }
                src++;
                *dfdata += *sfdata;
                if (src == src_end) return;
                db++;
                if (db >= db_end) goto complain;
                dfdata++;
                sfdata++;
            }
        }
    }
    return;

complain:
    TestDstComplain(src->name);
}

// MARK: RotateTo
void CharBones::RotateTo(CharBones &dst, float f) const {
    const Bone *src = mBones.begin();
    if (src == mBones.end()) return;

    // Position section
    if (mCounts[TYPE_QUAT] > mCounts[TYPE_POS]) {
        const Bone *src_end = src + mCounts[TYPE_QUAT];
        Vector3 *ddata = (Vector3 *)dst.mStart;
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_POS];
        if (db != nullptr && mCompression >= kCompressVects) {
            short *sdata = (short *)mStart;
            while (true) {
                long long sz = (long long)sdata[2];
                short sy = sdata[1];
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    ddata++;
                }
                src++;
                ddata->x += (float)(long long)sdata[0] * 0.039674062f * f;
                ddata->y += (float)(long long)sy * 0.039674062f * f;
                ddata->z += (float)sz * 0.039674062f * f;
                if (src == src_end) goto rotateto_quat;
                db++;
                if (db >= db_end) goto complain;
                ddata++;
                sdata += 3;
            }
        } else {
            Vector3 *sdata = (Vector3 *)mStart;
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    ddata++;
                }
                src++;
                ddata->x += sdata->x * f;
                ddata->y += sdata->y * f;
                ddata->z += sdata->z * f;
                if (src == src_end) goto rotateto_quat;
                db++;
                if (db >= db_end) goto complain;
                ddata++;
                sdata++;
            }
        }
    }
rotateto_quat:
    if (mCounts[TYPE_ROTX] > mCounts[TYPE_QUAT]) {
        auto dstBonesBegin = dst.mBones.begin();
        Bone *db_end = dstBonesBegin + dst.mCounts[TYPE_ROTX];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_QUAT];
        Hmx::Quat *dquat = (Hmx::Quat *)(dst.mStart + dst.mOffsets[TYPE_QUAT]);
        int src_quat_off = mOffsets[TYPE_QUAT];
        const Bone *src_end = mBones.begin() + mCounts[TYPE_ROTX];
        if (mCompression >= kCompressQuats) {
            char *sqdata = (char *)(src_quat_off + mStart);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dquat++;
                }
                Hmx::Quat sq;
                ((ByteQuat *)sqdata)->ToQuat(sq);
                float sx = sq.x * f;
                float sy = sq.y * f;
                sq.z *= f;
                if (sq.w < 0.0f) {
                    sq.w = sq.w * f - (1.0f - f);
                } else {
                    sq.w = sq.w * f + (1.0f - f);
                }
                float dx = dquat->x;
                src++;
                float dz = dquat->z;
                float dw = dquat->w;
                float dy = dquat->y;
#ifdef HX_NATIVE
                { float sw_ = sq.w, sx_ = sx, sy_ = sy, sz_ = sq.z;
                float nw = sw_*dw - sx_*dx - sy_*dy - sz_*dz;
                float nx = sw_*dx + sx_*dw + sy_*dz - sz_*dy;
                float ny = sw_*dy - sx_*dz + sy_*dw + sz_*dx;
                float nz = sw_*dz + sx_*dy - sy_*dx + sz_*dw;
                dquat->x = nx; dquat->y = ny; dquat->z = nz; dquat->w = nw; }
#else
                dquat->w = -(dz * sq.z - -(dy * sy - (dw * sq.w - dx * sq.x)));
                dquat->z = -(dy * sx - (dw * sq.z + dz * sq.w + dx * sy));
                dquat->y = -(dx * sq.z - ((dz * sx + (dy * sq.w + dw * sy))));
                dquat->x = -(dz * sy - (dw * sx + dy * sq.z + dx * sq.w));
#endif
                if (src == src_end) goto rotateto_rot;
                db++;
                if (db >= db_end) goto complain;
                dquat++;
                sqdata += 4;
            }
        } else if (mCompression != kCompressNone) {
            char *sqdata = (char *)(src_quat_off + mStart);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dquat++;
                }
                Hmx::Quat sq;
                ((ShortQuat *)sqdata)->ToQuat(sq);
                float sx = sq.x * f;
                float sy = sq.y * f;
                sq.z *= f;
                if (sq.w < 0.0f) {
                    sq.w = sq.w * f - (1.0f - f);
                } else {
                    sq.w = sq.w * f + (1.0f - f);
                }
                float dx = dquat->x;
                src++;
                float dz = dquat->z;
                float dw = dquat->w;
                float dy = dquat->y;
#ifdef HX_NATIVE
                { float sw_ = sq.w, sx_ = sx, sy_ = sy, sz_ = sq.z;
                float nw = sw_*dw - sx_*dx - sy_*dy - sz_*dz;
                float nx = sw_*dx + sx_*dw + sy_*dz - sz_*dy;
                float ny = sw_*dy - sx_*dz + sy_*dw + sz_*dx;
                float nz = sw_*dz + sx_*dy - sy_*dx + sz_*dw;
                dquat->x = nx; dquat->y = ny; dquat->z = nz; dquat->w = nw; }
#else
                dquat->w = -(dz * sq.z - -(dy * sy - (dx * sq.x - sq.w * dw)));
                dquat->z = -(dy * sx - (sq.z * dw + dz * sq.w + dx * sy));
                dquat->y = -(dx * sq.z - (sy * dw + dy * sq.w + dz * sx));
                dquat->x = -(dz * sy - (dw * sx + dy * sq.z + dx * sq.w));
#endif
                if (src == src_end) goto rotateto_rot;
                db++;
                if (db >= db_end) goto complain;
                dquat++;
                sqdata += 8;
            }
        } else {
            Hmx::Quat *squat = (Hmx::Quat *)(src_quat_off + mStart);
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dquat++;
                }
                float sw = squat->w * f;
                float sx = f * squat->x;
                float sy = squat->y * f;
                float sz = f * squat->z;
                if (squat->w < 0.0f) {
                    sw = sw - (1.0f - f);
                } else {
                    sw = (1.0f - f) + sw;
                }
                float dx = dquat->x;
                src++;
                float dz = dquat->z;
                float dw = dquat->w;
                float dy = dquat->y;
#ifdef HX_NATIVE
                // Native fix: PPC decomp has cross-product terms negated in x/y/z
                // (same decompiler register swap class as compressed paths above).
                // Correct quaternion multiply: result = (sw,sx,sy,sz) * (dw,dx,dy,dz)
                { float nw = sw*dw - sx*dx - sy*dy - sz*dz;
                float nx = sw*dx + sx*dw + sy*dz - sz*dy;
                float ny = sw*dy - sx*dz + sy*dw + sz*dx;
                float nz = sw*dz + sx*dy - sy*dx + sz*dw;
                dquat->x = nx; dquat->y = ny; dquat->z = nz; dquat->w = nw; }
#else
                dquat->z = -(sx * dy - (sw * dz + sy * dx + sz * dw));
                dquat->w = -(sz * dz - -(sy * dy - (sw * dw - sx * dx)));
                dquat->y = -(sz * dx - (sw * dy + sx * dz + sy * dw));
                dquat->x = -(sy * dz - (sw * dx + sz * dy + sx * dw));
#endif
                if (src == src_end) goto rotateto_rot;
                db++;
                if (db >= db_end) goto complain;
                dquat++;
                squat++;
            }
        }
    }
rotateto_rot:
    if (mCounts[TYPE_END] > mCounts[TYPE_ROTX]) {
        Bone *db_end = dst.mBones.begin() + dst.mCounts[TYPE_END];
        const Bone *src_end = mBones.begin() + mCounts[TYPE_END];
        Bone *db = dst.mBones.begin() + dst.mCounts[TYPE_ROTX];
        float *dfdata = (float *)(dst.mStart + dst.mOffsets[TYPE_ROTX]);
        float *sfdata = (float *)(mStart + mOffsets[TYPE_ROTX]);
        if (mCompression != kCompressNone) {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dfdata++;
                }
                src++;
                *dfdata += (float)*(short *)sfdata * (f * 0.00061035156f);
                if (src == src_end) return;
                db++;
                if (db >= db_end) goto complain;
                dfdata++;
                sfdata = (float *)((char *)sfdata + 2);
            }
        } else {
            while (true) {
                while (db->name != src->name) {
                    db++;
                    if (db >= db_end) goto complain;
                    dfdata++;
                }
                src++;
                *dfdata += *sfdata * f;
                if (src == src_end) return;
                db++;
                if (db >= db_end) goto complain;
                dfdata++;
                sfdata++;
            }
        }
    }
    return;

complain:
    TestDstComplain(src->name);
}

CharBonesAlloc::~CharBonesAlloc() {
    MemFree(mStart);
}

void CharBonesAlloc::ReallocateInternal() {
    MemFree(mStart);
    mStart = (char *)MemAlloc(mTotalSize, __FILE__, 0x6C0, "CharBones");
}
