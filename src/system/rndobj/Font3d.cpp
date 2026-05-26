#include "rndobj/Font.h"
#include "math/Utl.h"
#include "os/Debug.h"

bool RndFont3d::HasChar(unsigned short us) const {
    return mCharInfoMap.find(us) != mCharInfoMap.end();
}

RndFont3d::RndFont3d()
    : mMat(this), mTextureOwner(this, this), mCellSize(0, 0, 0), mInvCellSize(0, 0, 0),
      unk8c(0, 0, 0) {}

void RndFont3d::Clear() {
    FOREACH (it, mCharInfoMap) {
        delete it->second;
    }
    mCharInfoMap.clear();
    mChars.clear();
    RELEASE(mKerningTable);
}

BEGIN_HANDLERS(RndFont3d)
    HANDLE_SUPERCLASS(RndFontBase)
END_HANDLERS

BEGIN_PROPSYNCS(RndFont3d)
    SYNC_SUPERCLASS(RndFontBase)
END_PROPSYNCS

void RndFont3d::Save(BinStream &bs) {
    bs << 0;
    RndFontBase::Save(bs);
    bs << mMat;
    bs << mTextureOwner;
    bs << mCellSize;
    bs << mInvCellSize;
    bs << unk8c;
    int size = mCharInfoMap.size();
    bs << size;
    FOREACH (it, mCharInfoMap) {
        bs << it->first;
        CharInfo *info = it->second;
        bs << info->unk0;
        bs << info->advance;
        bs << info->mMesh;
        bs << info->visible;
    }
}

BEGIN_COPYS(RndFont3d)
    COPY_SUPERCLASS(RndFontBase)
    CREATE_COPY_AS(RndFont3d, f)
    MILO_ASSERT(f, 0xEB);
    mMat.CopyRef(f->mMat);
    mCellSize = f->mCellSize;
    mInvCellSize = f->mInvCellSize;
    unk8c = f->unk8c;
    FOREACH (it, mCharInfoMap) {
        delete it->second;
    }
    mCharInfoMap.clear();
    FOREACH (it2, f->mCharInfoMap) {
        CharInfo *info = new CharInfo();
        *info = *it2->second;
        mCharInfoMap[it2->first] = info;
    }
    if (ty == kCopyShallow || (ty == kCopyFromMax && f->mTextureOwner != f)) {
        COPY_MEMBER_FROM(f, mTextureOwner)
    } else {
        mTextureOwner = this;
    }
END_COPYS

INIT_REVS(0, 0)
BEGIN_LOADS(RndFont3d)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(RndFontBase)
    d >> mMat;
    d >> mTextureOwner;
    d >> mCellSize;
    d >> mInvCellSize;
    d >> unk8c;
    int size;
    d >> size;
    for (unsigned int i = 0; i < (unsigned int)size; i++) {
        unsigned short key;
        d >> key;
        CharInfo *info = new CharInfo();
        d >> info->unk0;
        d >> info->advance;
#ifdef HX_NATIVE
        // On native, CharInfo::mMesh has no owner (constructed with nullptr),
        // so ObjPtr::Load cannot derive a directory from RefOwner().
        // Pass the font's Dir() so char meshes (e.g. char_u0041.mesh)
        // resolve within the font's .milo ObjectDir.
        info->mMesh.Load(d.stream, true, Dir());
#else
        info->mMesh.Load(d.stream, true, nullptr);
#endif
        d >> info->visible;
        mCharInfoMap[key] = info;
    }
END_LOADS

RndFont3d::CharInfo *RndFont3d::GetCharInfo(unsigned short c) const {
    if (mTextureOwner != this) {
        for (;;)
            ;
    }
    std::map<unsigned short, CharInfo *>::const_iterator it = mCharInfoMap.find(c);
    if (it != mCharInfoMap.end()) {
        return it->second;
    }
    return nullptr;
}

Vector3 RndFont3d::CharOriginOffset() const {
    if (mTextureOwner != this) {
        return mTextureOwner->CharOriginOffset();
    }
    float unit = FontUnitInverse();
    Vector3 result;
    result.x = unk8c.x * unit;
    result.y = unk8c.y * unit;
    result.z = unk8c.z * unit;
    return result;
}

bool RndFont3d::CharWidthAdvanceMesh(
    unsigned short c, float &width, float &advance, RndMesh **mesh
) const {
    if (mTextureOwner != this) {
        return mTextureOwner->CharWidthAdvanceMesh(c, width, advance, mesh);
    }
    std::map<unsigned short, CharInfo *>::const_iterator it = mCharInfoMap.find(c);
    if (it != mCharInfoMap.end()) {
        CharInfo *info = it->second;
        if (info->unk0.Volume() > 0.0f || info->advance > 0.0f) {
            width = FontUnitInverse() * Max(info->unk0.mMax.x, 0.f);
            if (mMonospace) {
                advance = 1.0f;
            } else {
                advance = FontUnitInverse() * info->advance;
            }
            *mesh = info->mMesh;
            return true;
        }
    }
    return false;
}

float RndFont3d::CharWidth(unsigned short c) const {
    if (mTextureOwner != this) {
        return mTextureOwner->CharWidth(c);
    }
    MILO_ASSERT(HasChar(c), 0xCA);
    CharInfo *info = mTextureOwner->mCharInfoMap.find(c)->second;
    float w = Max(info->unk0.mMax.x, 0.f);
    MILO_ASSERT(w >= 0.f, 0xCC);
    return FontUnitInverse() * w;
}

bool RndFont3d::CharAdvance(unsigned short us1, unsigned short us2, float &f) const {
    if (mTextureOwner != this) {
        return mTextureOwner->CharAdvance(us1, us2, f);
    }
    std::map<unsigned short, CharInfo *>::const_iterator it = mCharInfoMap.find(us2);
    if (it != mCharInfoMap.end()) {
        CharInfo *info = it->second;
        if (info->unk0.Volume() > 0.0f || info->advance > 0.0f) {
            if (mMonospace) {
                f = 1.0f;
            } else {
                f = FontUnitInverse() * info->advance;
            }
            f += Kerning(us1, us2);
            return true;
        }
    }
    return false;
}

float RndFont3d::CharAdvance(unsigned short c) const {
    if (mTextureOwner != this) {
        return mTextureOwner->CharAdvance(c);
    }
    MILO_ASSERT(HasChar(c), 0xD5);
    if (mMonospace)
        return 1.0f;
    CharInfo *info = mCharInfoMap.find(c)->second;
    float a = info->advance;
    MILO_ASSERT(a >= 0.f, 0xDB);
    return FontUnitInverse() * a;
}

float RndFont3d::Kerning(unsigned short us1, unsigned short us2) const {
    if (mTextureOwner != this) {
        return mTextureOwner->Kerning(us1, us2);
    }
    return RndFontBase::Kerning(us1, us2) * mInvCellSize.x;
}

float RndFont3d::AspectRatio() const {
    return mTextureOwner->mCellSize.z / mTextureOwner->mCellSize.x;
}

RndMat *RndFont3d::Mat() const { return mMat; }

const RndFontBase *RndFont3d::DataOwner() const { return mTextureOwner; }
