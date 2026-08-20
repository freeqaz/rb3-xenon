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

static unsigned short sFontRev;

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

bool KerningTable::Valid(const RndFont::KernInfo &info, RndFont *font) {
    return !font
        || (font->RndFont::CharDefined(info.mFirstChar)
            && font->RndFont::CharDefined(info.mSecondChar));
}

void KerningTable::Save(BinStream &bs) {
    bs << mNumEntries;
    for (int i = 0; i < mNumEntries; i++) {
        bs << mEntries[i].key;
        bs << mEntries[i].kerning;
    }
}

void KerningTable::SetKerning(
    const std::vector<RndFont::KernInfo> &info, RndFont *font
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
            // (first, second) -- NOT swapped. TableIndex is symmetric so the
            // swap was semantically invisible, but retail loads the two shorts
            // in declaration order (rb3-Wii oracle agrees).
            int index = TableIndex(curInfo.mFirstChar, curInfo.mSecondChar);
            curEntry.next = mTable[index];
            mTable[index] = &curEntry;
        }
    }
}

void KerningTable::GetKerning(std::vector<RndFont::KernInfo> &info) const {
    info.resize(mNumEntries);
    for (int i = 0; i < mNumEntries; i++) {
        info[i].mFirstChar = mEntries[i].key;
        info[i].mSecondChar = (unsigned int)(mEntries[i].key) >> 16;
        info[i].kerning = mEntries[i].kerning;
    }
}

void KerningTable::Load(BinStreamRev &d, RndFont *f) {
    if (sFontRev < 7) {
        std::vector<RndFont::KernInfo> info;
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
            if (sFontRev < 0x11) {
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

BitmapLocker::BitmapLocker(RndFont *font) : mTexture(0), mPbm(0) {
    mTexture = font->ValidTexture();
    if (mTexture) {
        const char *filename = mTexture->File().c_str();
        int len = strlen(filename);
        if (UsingCD() || len < 4 || stricmp(filename + len - 4, ".bmp")) {
            mTexture->LockBitmap(mBm, 3);
            if (mBm.Pixels()) {
                mPbm = &mBm;
            }
        } else {
            mBm.LoadBmp(filename, false, true);
            if (mBm.Pixels()) {
                mPbm = &mBm;
            }
            mTexture = nullptr;
        }
    }
}

BitmapLocker::~BitmapLocker() {
    if (mTexture) {
        mTexture->UnlockBitmap();
    }
}

RndFont::RndFont()
    : mMat(this), mTextureOwner(this, this), mKerningTable(0), mBaseKerning(0.0f),
      mCellSize(1.0f, 1.0f), mDeprecatedSize(0.0f), mMonospace(0),
      mTexCellSize(0.0f, 0.0f), mPacked(0), mNextFont(this) {}

RndFont::~RndFont() { RELEASE(mKerningTable); }

void RndFont::Replace(ObjRef *from, Hmx::Object *to) {
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
        return;
    } else
        Hmx::Object::Replace(from, to);
}

BEGIN_HANDLERS(RndFont)
    HANDLE_EXPR(mat, Mat())
    HANDLE_EXPR(texture_owner, mTextureOwner.Ptr())
    HANDLE_ACTION(bleed_test, BleedTest())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndFont)
    SYNC_PROP_MODIFY(texture_owner, mTextureOwner, UpdateChars())
    SYNC_PROP_MODIFY(mat, mMat, UpdateChars())
    SYNC_PROP_MODIFY(monospace, mMonospace, UpdateChars())
    SYNC_PROP_MODIFY(packed, mPacked, UpdateChars())
    SYNC_PROP_SET(cell_width, (int)mCellSize.x, SetCellSize(_val.Int(), mCellSize.y))
    SYNC_PROP_SET(cell_height, (int)mCellSize.y, SetCellSize(mCellSize.x, _val.Int()))
    SYNC_PROP_SET(chars_in_map, GetASCIIChars(), SetASCIIChars(_val.Str()))
    SYNC_PROP_MODIFY(base_kerning, mBaseKerning, UpdateChars())
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

// Transcribed from retail 0x82472EC0 (548 B). The write order below is the
// instruction order of that function, one-for-one:
//   packRevs(0,0x11) -> Hmx::Object::Save -> ObjPtr@0x28 -> Vector2@0x60 ->
//   f32@0x68 -> f32@0x5c -> vector@0x6c -> bool(ptr@0x58) -> [KerningTable::Save]
//   -> ObjPtr@0x34 -> u8@0x78 -> u8@0x84 -> tex w/h via mMat@0x30 ->
//   Vector2@0x7c -> map count@0x50 -> per-char {u16, f32 x4} -> ObjPtr@0x88.
BEGIN_SAVES(RndFont)
    SAVE_REVS(0x11, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mMat;
    // One chained expression: retail keeps the BinStream& returned by the
    // out-of-line Vector2 operator<< in r29 and threads it through the next
    // three (inlined) writes. Splitting these into separate statements restarts
    // each one from `bs` (r31) and costs three register mismatches.
    bs << mCellSize << mDeprecatedSize << mBaseKerning << mChars;
    bs << (mKerningTable != nullptr);
    if (mKerningTable) {
        mKerningTable->Save(bs);
    }
    bs << mTextureOwner;
    bs << mMonospace;
    bs << mPacked;
    RndTex *validTex = ValidTexture();
    if (validTex) {
        bs << validTex->Width() << validTex->Height();
    } else {
        bs << 0 << 0;
    }
    bs << mTexCellSize;
    bs << mCharInfoMap.size();
    FOREACH (it, mCharInfoMap) {
        bs << it->first;
        const CharInfo &info = it->second;
        bs << info.mU;
        bs << info.mV;
        bs << info.mCharWidth;
        bs << info.mAdvance;
    }
    bs << mNextFont;
END_SAVES

BEGIN_COPYS(RndFont)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY_AS(RndFont, f)
    MILO_ASSERT(f, 0x451);
    COPY_MEMBER_FROM(f, mMat)
    COPY_MEMBER_FROM(f, mCellSize)
    COPY_MEMBER_FROM(f, mTexCellSize)
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

__forceinline BinStreamRev &operator>>(BinStreamRev &d, RndFont::KernInfo &info) {
    if (sFontRev < 0x11) {
        char x;
        d >> x;
        info.mFirstChar = x;
        d >> x;
        info.mSecondChar = x;
    } else {
        d >> info.mFirstChar >> info.mSecondChar;
    }
    if (sFontRev < 6) {
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

// Load order follows retail's Save (0x82472EC0) exactly -- they are the two
// halves of one serialiser and MUST agree. The former DC3 `altRev >= 2` path
// read mChars/mMonospace/mBaseKerning/kerning BEFORE the material, which does
// not correspond to anything retail writes; keeping it against the decoded Save
// above would have produced a genuinely unbalanced stream. The altRev branches
// are dropped accordingly (retail's Save emits packRevs(0, 0x11) -- altRev is
// always 0).
BEGIN_LOADS(RndFont)
    LOAD_REVS(bs)
    ASSERT_REVS(0x11, 2)
    sFontRev = d.rev;
    if (d.rev > 7) {
        Hmx::Object::Load(d.stream);
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
        mMat.Load(d.stream, true, NULL);
        if (d.rev > 9 && d.rev < 0xc) {
            char buf[0x80];
            d.stream.ReadString(buf, 0x80);
            if (!mMat && buf[0] != '\0') {
                mMat = LookupOrCreateMat(buf, Dir());
            }
        }
        if (d.rev < 4) {
            float w, h;
            if (d.rev < 2) {
                int wi, hi;
                d.stream >> wi >> hi;
                w = wi;
                h = hi;
            } else {
                d.stream >> w >> h;
            }
            RndTex *validTex = ValidTexture();
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
        d.stream >> mDeprecatedSize >> mBaseKerning;
        if (d.rev < 4) {
            mBaseKerning /= mDeprecatedSize;
        }
    }
    if (d.rev > 1) {
        if (d.rev < 0x11) {
            String str;
            d.stream >> str;
            ASCIItoWideVector(mChars, str.c_str());
        } else {
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
    if (d.rev > 4) {
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
    if (d.rev > 0xa) {
        d >> mMonospace;
    }
    if (d.rev > 0xe) {
        d >> mPacked;
    }
    if (d.rev > 0xc) {
        int bw, bh;
        d.stream >> bw >> bh;
        RndTex *validTex = ValidTexture();
        if (validTex) {
            if (bw && validTex->Width()) {
                mCellSize.x *= (float)validTex->Width() / (float)bw;
            }
            if (bh && validTex->Height()) {
                mCellSize.y *= (float)validTex->Height() / (float)bh;
            }
        }
    }
    if (d.rev > 0xd) {
        d.stream >> mTexCellSize;
        if (d.rev < 0x11) {
            for (int i = 0; i < 0x100; i++) {
                CharInfo &info = mCharInfoMap[i];
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
    if (d.rev > 0x10) {
        mNextFont.Load(d.stream, true, NULL);
    }
END_LOADS

void RndFont::UpdateChars() {
    if (mPacked) {
        RndTex *tex = ValidTexture();
        if (tex) {
            SetBitmapSize(mCellSize, tex->Width(), tex->Height());
        }
    } else {
        if (!mChars.empty() && mChars[0] == 160) {
            MILO_NOTIFY(
                "%s: first character is ascii 160, converting to the space character.",
                Name()
            );
            mChars[0] = ' ';
        }
        mCharInfoMap.clear();
        BitmapLocker locker(this);
        RndBitmap *bmap = locker.PtrToBitmap();
        if (bmap) {
            mTexCellSize.x = mCellSize.x / (float)bmap->Width();
            mTexCellSize.y = mCellSize.y / (float)bmap->Height();
            Vector2 pos(0, 0);
            for (int i = 0; i < mChars.size(); i++) {
                unsigned short curChar = mChars[i];
                if (pos.x + mCellSize.x > (float)bmap->Width()) {
                    pos.x = 0;
                    pos.y += mCellSize.y;
                }
                // Single-page: retail has one ObjPtr<RndMat>, so overflowing the
                // bitmap truncates instead of advancing to the next page.
                if (pos.y + mCellSize.y > (float)bmap->Height()) {
                    MILO_NOTIFY("%s: too many characters for bitmap, truncating.", Name());
                    mChars.resize(i);
                    break;
                }
                SetCharInfo(&mCharInfoMap[curChar], *bmap, pos);
                pos.x += mCellSize.x;
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
    // Single-page: the locker and the wrap test hoist out of the loop (rb3-Wii
    // shape). The DC3 form re-locked a per-character page inside the loop.
    BitmapLocker locker(this);
    RndBitmap *bmap = locker.PtrToBitmap();
    if (bmap) {
        bool haswrap = mMat->GetTexWrap() == kTexWrapClamp;
        String errStr;
        for (int i = 0; i < mChars.size(); i++) {
            unsigned short curChar = mChars[i];
            CharInfo &curInfo = mCharInfoMap[curChar];
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
        if (errStr.length() != 0) {
            MILO_NOTIFY("Bleeding in %s:\n%s", Name(), errStr);
        } else {
            MILO_NOTIFY("No bleeding over found.  ");
        }
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

// Transcribed from retail 0x82472C18 (352 B): it loads mMat.mObject from +0x30
// and prints the material's name, then cellSize@0x60, deprecated size@0x68,
// space@0x5c, then walks mChars@0x6c. There is no "pages:" line and no material
// list -- that was the DC3 multi-page form.
void RndFont::Print() {
    TheDebug << "   mat: " << mMat << "\n";
    TheDebug << "   cellSize: " << mCellSize << "\n";
    TheDebug << "   deprecated size: " << mDeprecatedSize << "\n";
    TheDebug << "   space: " << mBaseKerning << "\n";
    TheDebug << "   chars: ";
    // No cast on size(): retail compares UNSIGNED (`cmplw`) against a signed
    // pointer-difference size (`srawi.`). An (int) cast here flips both.
    for (int i = 0; i < mChars.size(); i++) {
        unsigned short us = mChars[i];
        TheDebug << WideCharToChar(&us);
    }
    TheDebug << "\n";
    TheDebug << "   kerning: TODO\n";
}

// HasChar is now a non-virtual in-class inline (see Font.h) -- retail's
// CharDefined inlines it.

// Former RndFontBase::SetASCIIChars, inlined.
void RndFont::SetASCIIChars(String str) {
    if (DataOwner() != this) {
        MILO_ASSERT(0, 0x167);
    } else {
        ASCIItoWideVector(mChars, str.c_str());
    }
    UpdateChars();
}

// ---- former RndFontBase members, now RndFont's own ----

float RndFont::Kerning(unsigned short us1, unsigned short us2) const {
    if (DataOwner() != this) {
        return DataOwner()->Kerning(us1, us2);
    } else if (us1 == 0 || us2 == 0)
        return 0;
    else if (!mMonospace && mKerningTable) {
        return mBaseKerning + mKerningTable->Kerning(us1, us2);
    } else
        return mBaseKerning;
}

String RndFont::GetASCIIChars() const {
    if (DataOwner() != this) {
        return DataOwner()->GetASCIIChars();
    } else
        return WideVectorToASCII(mChars);
}

void RndFont::SetBaseKerning(float f1) {
    MILO_ASSERT(DataOwner() == this, 0x65);
    mBaseKerning = f1;
}

void RndFont::SetKerning(const std::vector<KernInfo> &kernInfo) {
    MILO_ASSERT(DataOwner() == this, 0x7C);
    if (kernInfo.empty()) {
        RELEASE(mKerningTable);
    } else {
        if (!mKerningTable) {
            mKerningTable = new KerningTable();
        }
        mKerningTable->SetKerning(kernInfo, this);
    }
}

void RndFont::GetKerning(std::vector<KernInfo> &kernInfo) const {
    const RndFont *owner;
    for (owner = this; owner->DataOwner() != owner; owner = owner->DataOwner())
        ;
    if (owner->mKerningTable) {
        owner->mKerningTable->GetKerning(kernInfo);
    } else {
        kernInfo.clear();
    }
}

// Vestigial page-indexed forms -- RB3 retail fonts hold a single material, so
// the index is ignored. Kept only for the out-of-unit ui/UIFontImporter.cpp
// callers; nothing in this TU uses them.
RndMat *RndFont::Mat(int) const { return mMat; }

RndTex *RndFont::ValidTexture(int) const { return ValidTexture(); }

void RndFont::SetCharInfo(CharInfo *info, RndBitmap &bmap, const Vector2 &pos) {
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

// Single-page: the atlas cell fraction is one Vector2 member, not a per-material
// vector (rb3-Wii RndFont::SetBitmapSize).
void RndFont::SetBitmapSize(const Vector2 &cs, unsigned int w, unsigned int h) {
    mCellSize = cs;
    mTexCellSize.x = mCellSize.x / w;
    mTexCellSize.y = mCellSize.y / h;
}

void RndFont::SetCellSize(float x, float y) {
    mCellSize.Set(x, y);
    UpdateChars();
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
            uvMax.x = owner->mTexCellSize.x * info.mCharWidth + info.mU;
            uvMin.y = info.mV;
            uvMax.y = owner->mTexCellSize.y + info.mV;
            return true;
        }
    }
    return false;
}

// sw2 scatter-include (default/Font <- bandobj/BandDirector.cpp)
#define gRev gRev_BandDirector
#define gAltRev gAltRev_BandDirector
#include "bandobj/BandDirector.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/Font <- band3/bandtrack/Tail.cpp)
#define gRev gRev_Tail
#define gAltRev gAltRev_Tail
#include "band3/bandtrack/Tail.cpp"
#undef gRev
#undef gAltRev
