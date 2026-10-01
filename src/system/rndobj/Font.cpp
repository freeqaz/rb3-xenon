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

// Retail's rev pair: RndFont::Load (0x82475A20) writes altRev+0 / rev+4 off
// one base register (lbl_82CC648C), and KerningTable::Load (0x82475778) reads
// rev as its own symbol (lis + lhz lbl_82CC6490@l) -- two separate
// internal-linkage statics, altRev declared first (.bss order). This is the
// file-static rev LOAD_REVS writes; the old sFontRev copy of a
// BinStreamRev's rev was DC3's. (BinStreamRev itself does not exist in
// retail: zero .?AVBinStreamRev@@ type descriptors in band.exe.)
static unsigned short gAltRev = 0;
static unsigned short gRev = 0;

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
            // in declaration order.
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

void KerningTable::Load(BinStream &bs, RndFont *f) {
    if (gRev < 7) {
        std::vector<RndFont::KernInfo> info;
        bs >> info;
        SetKerning(info, f);
    } else {
        int num;
        bs >> num;
        if (num != mNumEntries) {
            mNumEntries = num;
            delete mEntries;
            mEntries = new Entry[mNumEntries];
        }
        memset(&mTable, 0, 0x80);
        for (int i = 0; i < mNumEntries; i++) {
            Entry &curEntry = mEntries[i];
            bs >> curEntry.key;
            bs >> curEntry.kerning;
            unsigned short us4, us3;
            if (gRev < 0x11) {
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
#ifndef HX_NATIVE
    // Retail (0x82472870) always locks the texture's bitmap; the loose-.bmp
    // development path is absent.
    if (mTexture) {
        mTexture->LockBitmap(mBm, 3);
        if (mBm.Pixels()) {
            mPbm = &mBm;
        }
    }
#else
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
#endif
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
#ifndef HX_NATIVE
    // Retail (0x82472D78): only the texture owner is replaceable; a null
    // replacement makes the font its own owner, otherwise it adopts the
    // replacement's owner. There is no base-class forwarding.
    if (reinterpret_cast<void *>(static_cast<Hmx::Object *>(mTextureOwner.Ptr()))
        == reinterpret_cast<void *>(from)) {
        if (!to)
            mTextureOwner = this;
        else
            mTextureOwner = dynamic_cast<RndFont *>(to)->mTextureOwner;
    }
    return;
#endif
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

// Retail (0x82476350): the cast precedes the superclass copy, the char list
// and monospace flag are copied too, and a font that becomes its own texture
// owner takes the source owner's base kerning and kerning table.
BEGIN_COPYS(RndFont)
    CREATE_COPY_AS(RndFont, f)
    MILO_ASSERT(f, 0x451);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_MEMBER_FROM(f, mMat)
    COPY_MEMBER_FROM(f, mCellSize)
    COPY_MEMBER_FROM(f, mTexCellSize)
    COPY_MEMBER_FROM(f, mDeprecatedSize)
    COPY_MEMBER_FROM(f, mChars)
    COPY_MEMBER_FROM(f, mMonospace)
    COPY_MEMBER_FROM(f, mPacked)
    COPY_MEMBER_FROM(f, mCharInfoMap)
    if (ty == kCopyShallow || (ty == kCopyFromMax && f->mTextureOwner != f)) {
        mTextureOwner = f->mTextureOwner;
    } else {
        mTextureOwner = this;
        mBaseKerning = f->mTextureOwner->mBaseKerning;
        std::vector<KernInfo> kerning;
        f->mTextureOwner->GetKerning(kerning);
        SetKerning(kerning);
    }
END_COPYS

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

__forceinline BinStream &operator>>(BinStream &d, RndFont::KernInfo &info) {
    if (gRev < 0x11) {
        char x;
        d >> x;
        info.mFirstChar = x;
        d >> x;
        info.mSecondChar = x;
    } else {
        d >> info.mFirstChar >> info.mSecondChar;
    }
    if (gRev < 6) {
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


// Load order follows retail's Save (0x82472EC0) exactly -- they are the two
// halves of one serialiser and MUST agree. The former DC3 `altRev >= 2` path
// read mChars/mMonospace/mBaseKerning/kerning BEFORE the material, which does
// not correspond to anything retail writes; keeping it against the decoded Save
// above would have produced a genuinely unbalanced stream. The altRev branches
// are dropped accordingly (retail's Save emits packRevs(0, 0x11) -- altRev is
// always 0).
void RndFont::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    if (gRev > 7) {
        Hmx::Object::Load(bs);
    }
    if (gRev < 3) {
        String str;
        int a, b, c, e;
        bool dd;
        bs >> a >> b >> c >> dd >> e >> str;
    }
    if (gRev < 1) {
        std::map<char, MatChar> charMap;
        bs >> charMap;
    } else {
        mMat.Load(bs, true, NULL);
        if (gRev > 9 && gRev < 0xc) {
            char buf[0x80];
            bs.ReadString(buf, 0x80);
            if (!mMat && buf[0] != '\0') {
                mMat = LookupOrCreateMat(buf, Dir());
            }
        }
        if (gRev < 4) {
            float w, h;
            if (gRev < 2) {
                int wi, hi;
                bs >> wi >> hi;
                w = wi;
                h = hi;
            } else {
                bs >> w >> h;
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
            bs >> mCellSize;
        }
        bs >> mDeprecatedSize >> mBaseKerning;
        if (gRev < 4) {
            mBaseKerning /= mDeprecatedSize;
        }
    }
    if (gRev > 1) {
        if (gRev < 0x11) {
            String str;
            bs >> str;
            ASCIItoWideVector(mChars, str.c_str());
        } else {
            bs >> mChars;
        }
    } else {
        // An initialized local: retail memcpys the literal and then
        // LOADS charBuf[0] (lbz + beq); a static const table let MSVC fold
        // the first character to ' ' and drop the test.
        char charBuf[96] =
            " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
        const char *ptr = charBuf;
        if (*ptr != '\0') {
            do {
                mChars.push_back(*ptr);
                ptr++;
            } while (*ptr != '\0');
        }
    }
    if (gRev > 4) {
        bool hasKerning;
        bs >> hasKerning;
        if (hasKerning) {
            mKerningTable = new KerningTable();
            mKerningTable->Load(bs, this);
        }
    }
    if (gRev > 8) {
        mTextureOwner.Load(bs, true, NULL);
    }
    if (!mTextureOwner) {
        mTextureOwner = this;
    }
    if (gRev > 0xa) {
        bs >> mMonospace;
    }
    if (gRev > 0xe) {
        bs >> mPacked;
    }
    if (gRev > 0xc) {
        int bw, bh;
        bs >> bw >> bh;
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
    if (gRev > 0xd) {
        bs >> mTexCellSize;
        if (gRev < 0x11) {
            for (int i = 0; i < 0x100; i++) {
                CharInfo &info = mCharInfoMap[i];
                bs >> info.mU;
                bs >> info.mV;
                bs >> info.mCharWidth;
                if (info.mCharWidth < 0) {
                    info.mCharWidth = 0;
                }
                if (gRev > 0xe) {
                    bs >> info.mAdvance;
                } else {
                    info.mAdvance = info.mCharWidth;
                }
                if (info.mAdvance < 0) {
                    info.mAdvance = 0;
                }
            }
        } else {
            unsigned int count;
            bs >> count;
            for (unsigned int i = 0; i < count; i++) {
                unsigned short keyChar;
                bs >> keyChar;
                CharInfo &info = mCharInfoMap[keyChar];
                bs >> info.mU;
                bs >> info.mV;
                bs >> info.mCharWidth;
                bs >> info.mAdvance;
            }
        }
    } else {
        MILO_LOG("NOTIFY: %s is old version, please resave\n", PathName(this));
        UpdateChars();
    }
    mCharInfoMap[0x20];
    mCharInfoMap[0xa0];
    mCharInfoMap[0xa0] = mCharInfoMap[0x20];
    if (gRev < 0x10) {
        std::vector<KernInfo> kernInfos;
        GetKerning(kernInfos);
        SetKerning(kernInfos);
        MILO_LOG("NOTIFY: %s is old version, resave file\n", PathName(this));
    }
    if (gRev > 0x10) {
        mNextFont.Load(bs, true, NULL);
    }
}

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
    // Single-page: the locker and the wrap test hoist out of the loop.
    // The DC3 form re-locked a per-character page inside the loop.
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
#ifdef HX_NATIVE
        if (errStr.length() != 0) {
            MILO_NOTIFY("Bleeding in %s:\n%s", Name(), errStr);
        } else {
            MILO_NOTIFY("No bleeding over found.  ");
        }
#else
        // Retail copies errStr into a temporary and destroys it (the stripped
        // notify's by-value argument); the "no bleeding" branch emits nothing.
        if (errStr.length() != 0) {
            MiloStripEval("Bleeding in %s:\n%s", Name(), errStr);
        }
#endif
    }
}

float RndFont::CharWidth(unsigned short c) const {
    MILO_ASSERT(HasChar(c), 0x143);
    CharInfo &info = mTextureOwner->mCharInfoMap[c];
    float w = info.mCharWidth;
    MILO_ASSERT(w >= 0, 0x146);
    return w;
}

// 0x82474500: Kerning(prev, c) + CharAdvance(c). Both callees already route
// through the texture owner, so there is no delegation here.
float RndFont::CharAdvance(unsigned short prev, unsigned short c) const {
    return Kerning(prev, c) + CharAdvance(c);
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
#ifdef HX_NATIVE
    if (DataOwner() != this) {
        return DataOwner()->GetASCIIChars();
    }
#endif
    // Retail (0x82472690) converts this font's own char list; no owner redirect.
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

// Retail 0x82475a00 (24 B, no .pdata) reads this->mKerningTable directly and
// tail-branches to KerningTable::GetKerning or vector::clear -- no DataOwner()
// walk (that loop is DC3-era).
void RndFont::GetKerning(std::vector<KernInfo> &kernInfo) const {
    if (mKerningTable) {
        mKerningTable->GetKerning(kernInfo);
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
        // Retail (0x824723F8) re-tests the column on every step of both scans:
        // inward from the left edge, then inward from the right edge.
        int leftCol = left;
        while (leftCol != right) {
            if (bmap.ColumnNonTransparent(leftCol, top, bottom, &dummy))
                break;
            if (right > left) {
                leftCol++;
            } else {
                leftCol--;
            }
        }
        float leftColF = (float)(long long)leftCol;
        right--;
        left--;
        int rightCol = right;
        while (rightCol != left) {
            if (bmap.ColumnNonTransparent(rightCol, top, bottom, &dummy))
                break;
            if (left > right) {
                rightCol++;
            } else {
                rightCol--;
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
// vector (RndFont::SetBitmapSize).
void RndFont::SetBitmapSize(const Vector2 &cs, unsigned int w, unsigned int h) {
    mCellSize = cs;
    mTexCellSize.x = mCellSize.x / w;
    mTexCellSize.y = mCellSize.y / h;
}

void RndFont::SetCellSize(float x, float y) {
    mCellSize.Set(x, y);
    UpdateChars();
}

// 0x82473A18: follows the texture-owner chain, then reads the char's cell with
// no existence test (callers only ask for characters the font defines).
void RndFont::GetTexCoords(unsigned short c, Vector2 &tl, Vector2 &br) const {
    // Retail (0x82473a18) forwards to the texture owner by tail recursion, as Kerning does.
    if (DataOwner() != this) {
        DataOwner()->GetTexCoords(c, tl, br);
        return;
    }
    const CharInfo &info = mCharInfoMap.find(c)->second;
    tl.x = info.mU;
    br.x = mTexCellSize.x * info.mCharWidth + info.mU;
    tl.y = info.mV;
    br.y = mTexCellSize.y + info.mV;
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
