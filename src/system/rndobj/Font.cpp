#include "rndobj/Font.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Bitmap.h"
#include "rndobj/FontBase.h"
#include "obj/Object.h"
#include "obj/PropSync.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "utl/FilePath.h"
#include "utl/MakeString.h"
#include "utl/UTF8.h"
#include <cmath>

KerningTable::KerningTable() : mNumEntries(0), mEntries(0) { memset(mTable, 0, 0x80); }
KerningTable::~KerningTable() { delete mEntries; }

KerningTable::Entry *KerningTable::Find(unsigned short us1, unsigned short us2) {
    if (mNumEntries == 0) {
        return nullptr;
    }
    Entry *entry = mTable[TableIndex(us1, us2)];
    int key = Key(us1, us2);
    while (entry != nullptr && key != entry->key) {
        entry = entry->next;
    }
    return entry;
}

float KerningTable::Kerning(unsigned short us1, unsigned short us2) {
    Entry *kerningEntry = Find(us1, us2);
    if (kerningEntry)
        return kerningEntry->kerning;
    else
        return 0;
}

bool KerningTable::Valid(const RndFont::KernInfo &info, RndFontBase *font) {
    return !font || (font->CharDefined(info.mFirstChar) && font->CharDefined(info.mSecondChar));
}

void KerningTable::Save(BinStream &bs) {
    bs << mNumEntries;
    for (int i = 0; i < mNumEntries; i++) {
        bs << mEntries[i].key;
        bs << mEntries[i].kerning;
    }
}

void KerningTable::SetKerning(
    const std::vector<RndFont::KernInfo> &info, RndFontBase *font
) {
    int validcount = 0;
    for (int i = 0; i < info.size(); i++) {
        if (Valid(info[i], font)) {
            validcount++;
        }
    }
    if (validcount != mNumEntries) {
        mNumEntries = validcount;
        delete[] mEntries;
        mEntries = new Entry[mNumEntries];
    }
    memset(mTable, 0, 0x80);
    int entryIdx = 0;
    for (int i = 0; i < info.size(); i++) {
        const RndFont::KernInfo &curInfo = info[i];
        if (Valid(curInfo, font)) {
            Entry &curEntry = mEntries[entryIdx++];
            curEntry.key = Key(curInfo.mFirstChar, curInfo.mSecondChar);
            curEntry.kerning = curInfo.kerning;
            int index = TableIndex(curInfo.mSecondChar, curInfo.mFirstChar);
            curEntry.next = mTable[index];
            mTable[index] = &curEntry;
        }
    }
}

void KerningTable::GetKerning(std::vector<RndFontBase::KernInfo> &info) const {
    info.resize(mNumEntries);
    for (int i = 0; i < mNumEntries; i++) {
        info[i].mFirstChar = mEntries[i].key;
        info[i].mSecondChar = (unsigned int)(mEntries[i].key) >> 16;
        info[i].kerning = mEntries[i].kerning;
    }
}

void KerningTable::Load(BinStreamRev &d, RndFontBase *f) {
    if (d.rev < 7) {
        std::vector<RndFontBase::KernInfo> info;
        d >> info;
        SetKerning(info, f);
    } else {
        int num;
        d >> num;
        if (num != mNumEntries) {
            mNumEntries = num;
            delete mEntries;
            mEntries = new Entry[mNumEntries];
        }
        memset(&mTable, 0, 0x80);
        for (int i = 0; i < mNumEntries; i++) {
            Entry &curEntry = mEntries[i];
            d >> curEntry.key;
            d >> curEntry.kerning;
            unsigned short us4, us3;
            if (d.rev < 0x11) {
                us4 = curEntry.key & 0xFF;
                us3 = curEntry.key >> 8 & 0xFF;
                curEntry.key = Key(us4, us3);
            } else {
                us4 = curEntry.key;
                us3 = curEntry.key >> 16;
            }
            int idx = TableIndex(us4, us3);
            curEntry.next = mTable[idx];
            mTable[idx] = &curEntry;
        }
    }
}

BitmapLocker::BitmapLocker(RndFont *font, int pageIdx) : mFont(font), mTex(0), mBitmapPtr(0) {
    LoadPage(pageIdx);
}

BitmapLocker::~BitmapLocker() {
    if (mTex) {
        mTex->UnlockBitmap();
    }
}

void BitmapLocker::LoadPage(int pageIdx) {
    if (mTex) {
        mTex->UnlockBitmap();
    }
    mBitmapPtr = nullptr;
    mTex = mFont->ValidTexture(pageIdx);
    if (mTex) {
        const char *filename = mTex->File().c_str();
        int len = strlen(filename);
        if (UsingCD() || len < 4 || stricmp(filename + len - 4, ".bmp")) {
            mTex->LockBitmap(mBitmap, 3);
            if (mBitmap.Pixels()) {
                mBitmapPtr = &mBitmap;
            }
        } else {
            mBitmap.LoadBmp(filename, false, true);
            if (mBitmap.Pixels()) {
                mBitmapPtr = &mBitmap;
            }
            mTex = nullptr;
        }
    }
}

RndFont::RndFont()
    : mMats(this, (EraseMode)0, kObjListNoNull), mTextureOwner(this, this),
      mCellSize(1, 1), mDeprecatedSize(0), mPacked(0) {}

RndFont::~RndFont() { RELEASE(mKerningTable); }

bool RndFont::Replace(ObjRef *from, Hmx::Object *to) {
    if (RefIs(from, mTextureOwner)) {
        RndFont *replace;
        if (mTextureOwner == this) {
            replace = this;
        } else {
            RndFont *f = dynamic_cast<RndFont *>(to);
            if (f) {
                replace = f->mTextureOwner;
            } else {
                replace = this;
            }
        }
        mTextureOwner = replace;
        return true;
    } else
        return Hmx::Object::Replace(from, to);
}

BEGIN_HANDLERS(RndFont)
    HANDLE_EXPR(texture_owner, mTextureOwner.Ptr())
    HANDLE_ACTION(bleed_test, BleedTest())
    HANDLE_SUPERCLASS(RndFontBase)
END_HANDLERS

BEGIN_PROPSYNCS(RndFont)
    SYNC_PROP_MODIFY(texture_owner, mTextureOwner, UpdateChars())
    SYNC_PROP_MODIFY(mats, mMats, UpdateChars())
    SYNC_PROP_MODIFY(monospace, mMonospace, UpdateChars())
    SYNC_PROP_MODIFY(packed, mPacked, UpdateChars())
    SYNC_PROP_SET(cell_width, (int)mCellSize.x, SetCellSize(_val.Int(), mCellSize.y))
    SYNC_PROP_SET(cell_height, (int)mCellSize.y, SetCellSize(mCellSize.x, _val.Int()))
    SYNC_PROP_SET(chars_in_map, GetASCIIChars(), SetASCIIChars(_val.Str()))
    SYNC_PROP_MODIFY(base_kerning, mBaseKerning, UpdateChars())
    SYNC_SUPERCLASS(RndFontBase)
END_PROPSYNCS

BEGIN_SAVES(RndFont)
    SAVE_REVS(0x11, 2)
    SAVE_SUPERCLASS(RndFontBase)
    bs << mMats;
    bs << mCellSize << mDeprecatedSize;
    bs << mTextureOwner;
    bs << mPacked;
    RndTex *validTex = ValidTexture(0);
    if (validTex) {
        bs << validTex->Width() << validTex->Height();
    } else {
        bs << 0 << 0;
    }
    bs << mMaterialOffsets;
    bs << mCharInfoMap.size();
    FOREACH (it, mCharInfoMap) {
        bs << it->first;
        const CharInfo &info = it->second;
        bs << info.mPage;
        bs << info.mU;
        bs << info.mV;
        bs << info.mCharWidth;
        bs << info.mAdvance;
    }
END_SAVES

BEGIN_COPYS(RndFont)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY_AS(RndFont, f)
    MILO_ASSERT(f, 0x451);
    COPY_MEMBER_FROM(f, mMats)
    COPY_MEMBER_FROM(f, mCellSize)
    COPY_MEMBER_FROM(f, mMaterialOffsets)
    COPY_MEMBER_FROM(f, mDeprecatedSize)
    COPY_MEMBER_FROM(f, mPacked)
    COPY_MEMBER_FROM(f, mCharInfoMap)
    RndFont *obj;
    if (ty == kCopyShallow || (ty == kCopyFromMax && f->mTextureOwner != f)) {
        obj = f->mTextureOwner;
    } else {
        obj = this;
    }
    mTextureOwner = obj;
END_COPYS

static const char theChars[96] =
    " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

struct MatChar {
    float width;
    float height;
};

__forceinline BinStream &operator>>(BinStream &bs, MatChar &mc) {
    char x[0x80];
    bs.ReadString(x, 0x80);
    bs >> mc.width;
    bs >> mc.height;
    return bs;
}

__forceinline BinStreamRev &operator>>(BinStreamRev &d, RndFontBase::KernInfo &info) {
    if (d.rev < 0x11) {
        char x;
        d >> x;
        info.mFirstChar = x;
        d >> x;
        info.mSecondChar = x;
    } else {
        d >> info.mFirstChar >> info.mSecondChar;
    }
    if (d.rev < 6) {
        char x;
        d >> x >> x;
    }
    d >> info.kerning;
    return d;
}

template<>
BinStream &operator>>(BinStream &bs, std::map<char, MatChar> &m) {
    unsigned int count;
    bs >> count;
    while (count > 0) {
        char key;
        bs >> key;
        MatChar &mc = m[key];
        char x[0x80];
        bs.ReadString(x, 0x80);
        bs >> mc.width;
        bs >> mc.height;
        count--;
    }
    return bs;
}

INIT_REVS(0x11, 2)

BEGIN_LOADS(RndFont)
    LOAD_REVS(bs)
    ASSERT_REVS(0x11, 2)
    if (d.altRev < 2) {
        if (d.rev > 7) {
            Hmx::Object::Load(d.stream);
        }
    } else {
        RndFontBase::Load(d.stream);
    }
    if (d.rev < 3) {
        String str;
        int a, b, c, e;
        bool dd;
        d >> a >> b >> c >> dd >> e >> str;
    }
    if (d.rev < 1) {
        std::map<char, MatChar> charMap;
        d.stream >> charMap;
    } else {
        if (d.altRev < 1) {
            ObjPtr<RndMat> mat(this, NULL);
            mat.Load(d.stream, true, NULL);
            if (d.rev > 9 && d.rev < 0xc) {
                char buf[0x80];
                d.stream.ReadString(buf, 0x80);
                if (!mat && buf[0] != '\0') {
                    mat = LookupOrCreateMat(buf, Dir());
                }
            }
            mMats.clear();
            mMats.push_back(mat);
        } else {
            mMats.Load(d.stream, true, NULL);
        }
        if (d.rev < 4) {
            float w, h;
            if (d.rev < 2) {
                int w, h;
                d.stream >> w >> h;
                mCellSize.x = h;
                mCellSize.y = w;
            } else {
                d.stream >> mCellSize.y >> mCellSize.x;
            }
            RndTex *validTex = ValidTexture(0);
            if (validTex) {
                RndBitmap bmap;
                validTex->LockBitmap(bmap, 3);
                mCellSize.x = std::floor((float)bmap.Width() / w + 0.5f);
                mCellSize.y = std::floor((float)bmap.Height() / h + 0.5f);
                validTex->UnlockBitmap();
            }
        } else {
            d.stream >> mCellSize;
        }
        d.stream >> mDeprecatedSize;
        if (d.altRev < 2) {
            d.stream >> mBaseKerning;
        }
        if (d.rev < 4) {
            mBaseKerning /= mDeprecatedSize;
        }
    }
    if (d.rev > 1) {
        if (d.rev < 0x11) {
            String str;
            d.stream >> str;
            ASCIItoWideVector(mChars, str.c_str());
        } else if (d.altRev < 2) {
            d >> mChars;
        }
    } else {
        char charBuf[96];
        memcpy(charBuf, theChars, sizeof(theChars));
        const char *ptr = charBuf;
        if (*ptr != '\0') {
            do {
                mChars.push_back(*ptr);
                ptr++;
            } while (*ptr != '\0');
        }
    }
    if (d.rev > 4 && d.altRev < 2) {
        bool hasKerning;
        d >> hasKerning;
        if (hasKerning) {
            mKerningTable = new KerningTable();
            mKerningTable->Load(d, this);
        }
    }
    if (d.rev > 8) {
        mTextureOwner.Load(d.stream, true, NULL);
    }
    if (!mTextureOwner) {
        mTextureOwner = this;
    }
    if (d.rev > 0xa && d.altRev < 2) {
        d >> mMonospace;
    }
    if (d.rev > 0xe) {
        d >> mPacked;
    }
    if (d.rev > 0xc) {
        int bw, bh;
        d.stream >> bw >> bh;
        RndTex *validTex = ValidTexture(0);
        if (validTex) {
            if (bw && validTex->Width()) {
                mCellSize.x *= (float)validTex->Width() / (float)bw;
            }
            if (bh && validTex->Height()) {
                mCellSize.y *= (float)validTex->Height() / (float)bh;
            }
        }
    }
    mMaterialOffsets.resize(mMats.size());
    if (d.rev > 0xd) {
        if (d.altRev < 1) {
            d.stream >> mMaterialOffsets[0];
        } else {
            d >> mMaterialOffsets;
        }
        if (d.rev < 0x11) {
            for (int i = 0; i < 0x100; i++) {
                CharInfo &info = mCharInfoMap[i];
                info.mPage = 0;
                d.stream >> info.mU;
                d.stream >> info.mV;
                d.stream >> info.mCharWidth;
                if (info.mCharWidth < 0) {
                    info.mCharWidth = 0;
                }
                if (d.rev > 0xe) {
                    d.stream >> info.mAdvance;
                } else {
                    info.mAdvance = info.mCharWidth;
                }
                if (info.mAdvance < 0) {
                    info.mAdvance = 0;
                }
            }
        } else {
            unsigned int count;
            d.stream >> count;
            for (unsigned int i = 0; i < count; i++) {
                unsigned short keyChar;
                d.stream >> keyChar;
                CharInfo &info = mCharInfoMap[keyChar];
                if (d.altRev > 0) {
                    d.stream >> info.mPage;
                } else {
                    info.mPage = 0;
                }
                d.stream >> info.mU;
                d.stream >> info.mV;
                d.stream >> info.mCharWidth;
                d.stream >> info.mAdvance;
            }
        }
    } else {
        MILO_LOG("NOTIFY: %s is old version, please resave\n", PathName(this));
        UpdateChars();
    }
    mCharInfoMap[0x20];
    mCharInfoMap[0xa0];
    mCharInfoMap[0xa0] = mCharInfoMap[0x20];
    if (d.rev < 0x10) {
        std::vector<KernInfo> kernInfos;
        GetKerning(kernInfos);
        SetKerning(kernInfos);
        MILO_LOG("NOTIFY: %s is old version, resave file\n", PathName(this));
    }
    if (d.rev > 0x10 && d.altRev < 1) {
        ObjPtr<RndFont> nextFont(this, NULL);
        nextFont.Load(d.stream, true, NULL);
    }
END_LOADS

void RndFont::UpdateChars() {
    if (mPacked) {
        SetBitmapSize(mCellSize);
    } else {
        if (!mChars.empty() && mChars[0] == 160) {
            MILO_NOTIFY(
                "%s: first character is ascii 160, converting to the space character.",
                Name()
            );
            mChars[0] = ' ';
        }
        mCharInfoMap.clear();
        int pageIdx = 0;
        BitmapLocker locker(this, 0);
        RndBitmap *bmap = locker.mBitmapPtr;
        if (bmap) {
            if (mMaterialOffsets.size() != mMats.size()) {
                mMaterialOffsets.resize(mMats.size());
            }
            float posX = 0;
            float posY = 0;
            mMaterialOffsets[0].x = mCellSize.x / (float)bmap->Width();
            mMaterialOffsets[0].y = mCellSize.y / (float)bmap->Height();
            for (unsigned int i = 0; i < mChars.size(); i++) {
                unsigned short curChar = mChars[i];
                if (posX + mCellSize.x > (float)bmap->Width()) {
                    posY += mCellSize.y;
                    posX = 0;
                }
                if (posY + mCellSize.y > (float)bmap->Height()) {
                    pageIdx++;
                    if (pageIdx >= (int)mMats.size()) {
                        MILO_NOTIFY(
                            "%s: too many characters for bitmap, truncating.", Name()
                        );
                        mChars.resize(i);
                        break;
                    }
                    posX = 0;
                    posY = 0;
                    locker.LoadPage(pageIdx);
                    mMaterialOffsets[pageIdx].x =
                        mCellSize.x / (float)locker.mBitmapPtr->Width();
                    mMaterialOffsets[pageIdx].y =
                        mCellSize.y / (float)locker.mBitmapPtr->Height();
                    bmap = locker.mBitmapPtr;
                }
                Vector2 pos(posX, posY);
                SetCharInfo(&mCharInfoMap[curChar], *bmap, pos, pageIdx);
                posX += mCellSize.x;
                if (curChar == 0x20) {
                    mCharInfoMap[curChar].mCharWidth = 0;
                } else if (curChar == 9) {
                    MILO_ASSERT(HasChar(L' ' ), 0x284);
                    mCharInfoMap[curChar] = mCharInfoMap[0x20];
                    mCharInfoMap[curChar].mAdvance *= 3.0f;
                }
            }
        }
    }
}

void RndFont::BleedTest() {
    String errStr;
    for (unsigned int i = 0; i < mChars.size(); i++) {
        unsigned short curChar = mChars[i];
        CharInfo &curInfo = mCharInfoMap[curChar];
        BitmapLocker locker(this, curInfo.mPage);
        RndBitmap *bmap = locker.mBitmapPtr;
        if (bmap) {
            bool haswrap = ((RndMat *)mMats[curInfo.mPage])->GetTexWrap() == kTexWrapClamp;
            int row_y = Round(curInfo.mV * (float)bmap->Height());
            int col_left = Round(curInfo.mU * (float)bmap->Width());
            int col_right = Round(curInfo.mCharWidth * mCellSize.x) + col_left;
            int iptr;
            if (row_y != 0 || !haswrap) {
                unsigned char row = bmap->RowNonTransparent(col_left, col_right, row_y, &iptr);
                if (row) {
                    errStr += MakeString(
                        "Top bleeding in 0x%04x, alpha %d, pixel %d,%d\n",
                        curChar, row, iptr, row_y
                    );
                }
            }
            row_y += (int)mCellSize.y - 1;
            if (!haswrap && row_y >= bmap->Height() - 1) {
                unsigned char row = bmap->RowNonTransparent(col_left, col_right, row_y, &iptr);
                if (row) {
                    errStr += MakeString(
                        "Bottom bleeding in 0x%04x, alpha %d, pixel %d,%d\n",
                        curChar, row, iptr, row_y
                    );
                }
            }
            row_y = Round(curInfo.mV * (float)bmap->Height());
            int ia0 = col_left - 1;
            if (col_left != 0 || (!haswrap && ia0 <= 0)) {
                MaxEq(ia0, 0);
                unsigned char row =
                    bmap->ColumnNonTransparent(ia0, row_y, row_y + (int)mCellSize.y, &iptr);
                if (row) {
                    errStr += MakeString(
                        "Left bleeding in 0x%04x, alpha %d, pixel %d,%d\n",
                        curChar, row, ia0, iptr
                    );
                }
            }
            ia0 = col_right;
            if (!haswrap && ia0 >= bmap->Width() - 1) {
                MinEq(ia0, bmap->Width() - 1);
                unsigned char row =
                    bmap->ColumnNonTransparent(ia0, row_y, row_y + (int)mCellSize.y, &iptr);
                if (row) {
                    errStr += MakeString(
                        "Right bleeding in 0x%04x, alpha %d, pixel %d,%d\n",
                        curChar, row, ia0, iptr
                    );
                }
            }
        }
    }
    if (errStr.length() != 0) {
        MILO_NOTIFY("Bleeding in %s:\n%s", Name(), errStr);
    } else {
        MILO_NOTIFY("No bleeding over found.  ");
    }
}

float RndFont::CharWidth(unsigned short c) const {
    MILO_ASSERT(HasChar(c), 0x143);
    CharInfo &info = mTextureOwner->mCharInfoMap[c];
    float w = info.mCharWidth;
    MILO_ASSERT(w >= 0, 0x146);
    return w;
}

bool RndFont::CharAdvance(unsigned short u1, unsigned short c, float &f3) const {
    if (mTextureOwner != this) {
        return mTextureOwner->CharAdvance(u1, c, f3);
    } else {
        auto it = mCharInfoMap.find(c);
        if (it != mCharInfoMap.end()
            && (it->second.mU != 0 || it->second.mV != 0 || it->second.mAdvance != 0)) {
            f3 = mMonospace ? 1 : it->second.mAdvance;
            f3 += Kerning(u1, c);
            return true;
        }
    }
    return false;
}

float RndFont::CharAdvance(unsigned short c) const {
    MILO_ASSERT(HasChar(c), 0x14E);
    if (mMonospace) {
        return 1;
    } else {
        return mTextureOwner->mCharInfoMap[c].mAdvance;
    }
}

bool RndFont::CharDefined(unsigned short c) const {
    if (HasChar(c)) {
        auto it = mCharInfoMap.find(c);
        const CharInfo &info = it->second;
        return info.mU != 0 || info.mV != 0 || info.mAdvance != 0;
    } else {
        return false;
    }
}

void RndFont::Print() const {
    TheDebug << "   pages: " << mMats.size() << "\n";
    FOREACH (it, mMats) {
        TheDebug << "         " << *it << "\n";
    }
    TheDebug << "   cellSize: " << mCellSize << "\n";
    TheDebug << "   deprecated size: " << mDeprecatedSize << "\n";
    TheDebug << "   space: " << mBaseKerning << "\n";
    TheDebug << "   chars: ";
    for (size_t i = 0; i < mChars.size(); i++) {
        unsigned short us = mChars[i];
        TheDebug << WideCharToChar(&us);
    }
    TheDebug << "\n";
    TheDebug << "   kerning: TODO\n";
}

bool RndFont::HasChar(unsigned short c) const {
    return mCharInfoMap.find(c) != mCharInfoMap.end();
}

void RndFont::SetASCIIChars(String str) {
    RndFontBase::SetASCIIChars(str);
    UpdateChars();
}

RndMat *RndFont::Mat(int idx) const {
    if (idx >= 0 && idx < mMats.size()) {
        return (RndMat *)mMats[idx];
    } else
        return nullptr;
}

RndTex *RndFont::ValidTexture(int idx) const {
    if (Mat(idx)) {
        return Mat(idx)->GetDiffuseTex();
    } else
        return nullptr;
}

void RndFont::SetCharInfo(CharInfo *info, RndBitmap &bmap, const Vector2 &pos, int page) {
    info->mPage = page;
    if (!(!(!(!(mMonospace))))) {
        int width = bmap.Width();
        info->mAdvance = 1.0f;
        info->mCharWidth = 1.0f;
        info->mU = pos.x / (float)width;
    } else {
        int left = (int)pos.x;
        int top = (int)pos.y;
        int right = (int)(mCellSize.x + pos.x);
        int bottom = (int)(mCellSize.y + pos.y);
        int dummy;
        int leftCol = left;
        if (right != leftCol) {
            auto _tmp0 = bmap.ColumnNonTransparent(leftCol, top, bottom, &dummy);
            while (_tmp0 == 0) {
                if (right > left) {
                    leftCol++;
                } else {
                    leftCol--;
                }
                if (right == leftCol)
                    break;
            }
        }
        float leftColF = (float)(long long)leftCol;
        int rightCol = right - 1;
        if (left - 1 != rightCol) {
            auto _tmp1 = bmap.ColumnNonTransparent(rightCol, top, bottom, &dummy);
            while (_tmp1 == 0) {
                if (right - 1 < left - 1) {
                    rightCol++;
                } else {
                    rightCol--;
                }
                if (rightCol == left - 1)
                    break;
            }
        }
        int width = bmap.Width();
        float charW = (float)(long long)rightCol + 1.0f - leftColF;
        if (0.0f < charW) {
            info->mU = leftColF / (float)width;
            float widthFrac = charW / mCellSize.x;
            info->mAdvance = widthFrac;
            info->mCharWidth = widthFrac;
        } else {
            info->mU = pos.x / (float)width;
            info->mAdvance = 0.25f;
            info->mCharWidth = 0.25f;
        }
    }
    info->mV = pos.y / (float)bmap.Height();
    MILO_ASSERT(info->mCharWidth >= 0, 0x422);
}

void RndFont::SetBitmapSize(const Vector2 &cs) {
    mCellSize = cs;
    if (mMaterialOffsets.size() != mMats.size()) {
        mMaterialOffsets.resize(mMats.size());
    }
    for (int i = 0; i < (int)mMats.size(); i++) {
        RndMat *mat = mMats[i];
        RndTex *tex = mat ? mat->GetDiffuseTex() : nullptr;
        if (tex && tex->Width() != 0 && tex->Height() != 0) {
            mMaterialOffsets[i].x = mCellSize.x / (float)tex->Width();
            mMaterialOffsets[i].y = mCellSize.y / (float)tex->Height();
        }
    }
}

void RndFont::SetCellSize(float x, float y) {
    mCellSize.Set(x, y);
    UpdateChars();
}

int RndFont::CharPage(unsigned short c) const {
    if (HasChar(c)) {
        return mCharInfoMap.find(c)->second.mPage;
    } else {
        return -1;
    }
}

bool RndFont::CharWidthAdvanceCoords(
    unsigned short c, float &charW, float &advW, Vector2 &uvMin, Vector2 &uvMax
) const {
    const RndFont *owner = this;
    while (owner->mTextureOwner != owner) {
        owner = owner->mTextureOwner;
    }
    std::map<unsigned short, CharInfo>::const_iterator it = owner->mCharInfoMap.find(c);
    if (it != owner->mCharInfoMap.end()) {
        const CharInfo &info = it->second;
        if (info.mU != 0 || info.mV != 0 || info.mAdvance != 0) {
            charW = info.mCharWidth;
            advW = owner->mMonospace ? 1.0f : info.mAdvance;
            uvMin.x = info.mU;
            uvMax.x = owner->mMaterialOffsets[info.mPage].x * info.mCharWidth + info.mU;
            uvMin.y = info.mV;
            uvMax.y = owner->mMaterialOffsets[info.mPage].y + info.mV;
            return true;
        }
    }
    return false;
}
