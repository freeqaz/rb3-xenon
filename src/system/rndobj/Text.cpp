// ---------------------------------------------------------------------------
// RB3-360 RETAIL RndText implementation.
//
// PORTED FROM: ../rb3/src/system/rndobj/Text.cpp (rb3-Wii DEV decomp) — retail
// RB3-360's RndText is that generation, not DC3's. See
// docs/decomp/rndtext-retail-layout.md for the measured member table.
//
// Wii -> 360 substitutions applied throughout:
//   Hmx::Color32            -> Hmx::Color   (col.a -> col.alpha, Opaque()->Pack())
//   Style::font/size/...    -> mFont/mSize/mItalics/mTextColor/nobreak/pre/mZOffset
//   Line::unk18/1c/28/58    -> mStart/mEnd/xfm/mWidth  (Wii's separate `color`
//                              member does NOT exist on retail; see Text.h)
//   RndText::unkbp4..7 etc. -> mUseAltStyle/mNeedsUpdate/mMeshDirty/mManualLines,
//                              mFramesSinceDraw, mRotateLineVerts, mMeshCallback,
//                              mCurHeight, mCurWidth
//   font->GetMat()          -> font->GetMat()  (kept: retail's load is NON-virtual)
//   std::vector<Line>       -> HX_VECTOR(Line)  (StlNodeAlloc flavour, retail-proven)
//   SYNC_PROP_MODIFY_ALT    -> SYNC_PROP_MODIFY (this tree's dialect)
//   INIT_REVS(RndText)      -> INIT_REVS(21, 0) / BEGIN_LOADS + d.rev
//
// FONT-CHAIN DIVERGENCE (deliberate, and the one place this port does NOT
// follow the Wii oracle): rb3-Wii's RndFont has a `mNextFont` fallback chain,
// and Wii's Mats()/Replace()/GetDefiningFont() walk it. Retail RB3-360's
// RndFont is measurably the *other* generation — it syncs `mats` (plural,
// ObjPtrVec<RndMat>) where Wii syncs `mat` (singular) + mNextFont; the retail
// binary contains the string "mats" and not "mat". Our RndFont accordingly has
// no chain, and growing it one would change RndFont's layout fleet-wide. So the
// three chain-walking bodies below are written in their chainless form. They
// are the known-uncertain trio of this port.
// ---------------------------------------------------------------------------
#include "rndobj/Text.h"
#include "decomp.h"
#include "math/Mtx.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Draw.h"
#include "rndobj/Font.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#include "utl/TextStream.h"
#include "utl/UTF8.h"
#include <algorithm>
#include <map>
#include <set>
#include <stdio.h>
#include <string.h>

// Defined in utl/UTF8.cpp but commented out of utl/UTF8.h; declared here rather
// than re-exposing them tree-wide (UTF8.h is very widely included).
void UTF8ToLower(unsigned short, char *);
void UTF8ToUpper(unsigned short, char *);

// Self-verifying layout gates. Every one of these is MEASURED in
// docs/decomp/rndtext-retail-layout.md; if a header edit ever moves them the
// build breaks here instead of silently scoring 0%.
// ...on the X360 (ILP32) side only. Every one of these three sizes counts
// pointers, so all three are wrong by construction under LP64 -- e.g. RndText
// grows past 0x1c8. Guarded exactly like rndobj/Cam.cpp:22-24's
// `static_assert(sizeof(Frustum) == 0x60)`. The gate keeps its full force where
// it means something (the match build); natively it is noise.
#ifndef HX_NATIVE
typedef char _rndtext_size_check[sizeof(RndText) == 0x1c8 ? 1 : -1];
typedef char _rndtext_style_size_check[sizeof(RndText::Style) == 0x24 ? 1 : -1];
typedef char _rndtext_line_size_check[sizeof(RndText::Line) == 0x78 ? 1 : -1];
#endif

std::set<RndText *> RndText::mTextMeshSet;

// RB3-only RndText::Init() config keys — all three are present in the retail
// binary (one string hit each), which is part of the evidence that retail's
// RndText is the rb3-Wii generation.
float gSuperscriptScale = 0.7f;
float gGuitarScale = 0.7f;
float gGuitarZOffset = 1.0f;

// RB3-360 retail rev dialect (rb3-Wii/ObjMacros shape), not DC3's Object.h
// BinStreamRev stack decorator.  DC3's form emits a ??0BinStream, a
// ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream destructor that retail
// has none of, and dispatches each read on `&d` instead of the raw `bs`.
//
// Adjudicated for THIS unit on retail bytes: the target obj carries NO symbol
// mangled with AAVBinStreamRev@@, i.e. retail instantiated no rev-decorated
// operator>> here, so forwarding the raw stream deletes nothing.
//
// Written longhand rather than by including obj/ObjMacros.h: that header also
// swaps the SYNC_PROP and HANDLE families, which are already byte-exact here.
// No `#define gRev` alias -- several of these TUs are scatter-INCLUDED into
// another unit whose own gRev macro such an alias would silently shadow.
// The pair MUST share ONE internal-linkage aggregate (two file statics get two
// `lis` pairs), altRev FIRST (MSVC lays .bss out in REVERSE), and the padding
// MUST be an explicit member -- __declspec(align(4)) is unreliable here.
static struct {
    unsigned short altRev;
    unsigned short pad;
    unsigned short rev;
} gRevs_Text;
void RndText::Init() {
    Register();
    SystemConfig("rnd")->FindData("text_superscript_scale", gSuperscriptScale, false);
    SystemConfig("rnd")->FindData("text_guitar_scale", gGuitarScale, false);
    SystemConfig("rnd")->FindData("text_guitar_z_offset", gGuitarZOffset, false);
}

void RndText::Mats(std::list<class RndMat *> &matList, bool) {
    // Chainless (see the FONT-CHAIN DIVERGENCE note at the top).
    RndFont *font = mFont;
    if (font && font->GetMat())
        matList.push_back(font->GetMat());
}

RndDrawable *RndText::CollideShowing(const Segment &s, float &f, Plane &p) {
    FOREACH (it, mMeshMap) {
        RndMesh *mesh = it->second.mesh;
        if (mesh && mesh->CollideShowing(s, f, p))
            return this;
    }
    return nullptr;
}

int RndText::CollidePlane(const Plane &p) {
    int ret = 0;
    FOREACH (it, mMeshMap) {
        RndMesh *mesh = it->second.mesh;
        if (mesh) {
            int meshCol = mesh->CollidePlane(p);
            if (meshCol == 0) {
                return 0;
            }
            if (meshCol > 0) {
                if (ret < 0) {
                    return 0;
                } else {
                    ret = meshCol;
                }
            } else if (ret > 0) {
                return 0;
            } else {
                ret = meshCol;
            }
        }
    }
    return ret;
}

void RndText::Replace(ObjRef *ref, Hmx::Object *to) {
    RndTransformable::Replace(ref, to);
    // Wii walks the font chain here; chainless, the only replaceable ref this
    // class owns is mFont, and the ObjOwnerPtr has already been repointed by
    // the base call — all that is left is to rebuild the text.
    if (ref == (ObjRef *)&mFont)
        UpdateText(true);
}

const char *RndText::FindPathName() {
    if (Name() && !*Name() && !Dir() && TransParent()) {
        return MakeString("%s inside %s", ClassName().Str(), TransParent()->FindPathName());
    } else
        return Hmx::Object::FindPathName();
}

// Retail RndText IS saveable (fn_82455928, writes rev 0x15) — unlike rb3-Wii,
// whose dev-build decomp has SAVE_OBJ(RndText, 171) i.e. an unsaveable stub.
// The item list below is reconstructed from Load's current-revision path: 3
// superclasses + 11 members = 14 items, which is exactly the count measured off
// the retail body. Nothing in [0x138,0x15c) (mAltStyle) or [0x178,0x190) (the
// runtime tail) is written, also as measured.
BEGIN_SAVES(RndText)
    SAVE_REVS(0x15, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    SAVE_SUPERCLASS(RndTransformable)
    bs << mFont;
    bs << mAlign;
    bs << mText;
    bs << mStyle.mTextColor;
    bs << mWrapWidth;
    bs << mLeading;
    bs << mFixedLength;
    bs << mStyle.mItalics;
    bs << mStyle.mSize;
    bs << mTextMarkup;
    bs << mCapsMode;
END_SAVES

BEGIN_LOADS(RndText)
    int rev;
    bs >> rev;
    gRevs_Text.rev = getHmxRev(rev);
    gRevs_Text.altRev = getAltRev(rev);
    if (gRevs_Text.rev > 15)
        Hmx::Object::Load(bs);
    RndDrawable::Load(bs);
    if (gRevs_Text.rev < 7) {
        ObjPtrList<Hmx::Object> dir(this);
        int dump;
        bs >> dump >> dir;
    }
    if (gRevs_Text.rev > 1)
        RndTransformable::Load(bs);
    bs >> mFont;
    if (gRevs_Text.rev < 3) {
        int idx;
        bs >> idx;
        Alignment align_choices[6] = { kTopLeft,    kTopCenter,    kTopRight,
                                       kBottomLeft, kBottomCenter, kBottomRight };
        mAlign = align_choices[idx];
    } else {
        int align;
        bs >> align;
        MILO_ASSERT(align < 255, 0xE7);
        mAlign = align;
    }
    if (gRevs_Text.rev < 2) {
        Vector2 v2;
        bs >> v2;
        SetLocalPos(v2.x, 0, -v2.y * 0.75f);
    }
    bs >> mText;
    if (gRevs_Text.rev < 0x14) {
        std::vector<unsigned short> vec;
        ASCIItoWideVector(vec, mText.c_str());
        WideVectorToUTF8(vec, mText);
    }
    if (gRevs_Text.rev != 0) {
        bs >> mStyle.mTextColor;
    }
    if (gRevs_Text.rev > 0xC)
        bs >> mWrapWidth;
    else if (gRevs_Text.rev > 3) {
        bool b;
        bs >> b;
        bs >> mWrapWidth;
        if (!b)
            mWrapWidth = 0.0f;
        if (gRevs_Text.rev < 5 && (mWrapWidth < 0.0f || mWrapWidth > 1000.0f))
            mWrapWidth = 0.0f;
    }
    if (gRevs_Text.rev == 5) {
        String str;
        bs >> str;
    }
    if (gRevs_Text.rev >= 5 && gRevs_Text.rev <= 10) {
        bool b;
        bs >> b;
        if (mFont && mFont->GetMat()) {
            int i = 0;
            if (b)
                i = 2;
            mFont->GetMat()->SetZMode((ZMode)i);
        }
    }
    if (gRevs_Text.rev > 7)
        bs >> mLeading;
    int fixedLength;
    if (gRevs_Text.rev > 0xB) {
        bs >> fixedLength;
    } else if (gRevs_Text.rev > 8) {
        bool b;
        bs >> b;
        if (b) {
            b = mText.length();
        } else
            b = false;
    }
    MILO_ASSERT(fixedLength < 65535, 0x13C);
    MILO_ASSERT(fixedLength >= 0, 0x13D);
    mFixedLength = fixedLength;
    if (mFixedLength != 0)
        ResizeText(mFixedLength);
    if (gRevs_Text.rev > 9)
        bs >> mStyle.mItalics;
    if (gRevs_Text.rev > 0xC)
        bs >> mStyle.mSize;
    else if (mFont) {
        mStyle.mSize = mFont->DeprecatedSize();
    }
    if (gRevs_Text.rev < 0xD) {
        mStyle.mItalics /= mStyle.mSize;
    }
    if (gRevs_Text.rev > 0xD) {
        // 360: mTextMarkup is a real bool member, so this is a plain read.
        // rb3-Wii needs LOAD_BITFIELD here because it lives in RndDrawable.
        bs >> mTextMarkup;
    }
    if (gRevs_Text.rev > 0xE) {
        int capsMode;
        bs >> capsMode;
        MILO_ASSERT(capsMode < 255, 0x158);
        mCapsMode = capsMode;
    } else
        mCapsMode = kCapsModeNone;
    if (gRevs_Text.rev >= 0x12 && gRevs_Text.rev <= 0x14) {
        bool b;
        bs >> b;
    }
    if (gRevs_Text.rev == 0x13 || gRevs_Text.rev == 0x14) {
        int i, j, k;
        bs >> i >> j >> k;
    }
    if (gRevs_Text.rev < 0x11 && mCapsMode != kCapsModeNone) {
        SetText(mText.c_str());
    }
    mAltStyle = mStyle;
    UpdateText(true);
}

void RndText::DeferUpdateText() {
    MILO_ASSERT(mDeferUpdate >= 0, 0x174);
    MILO_ASSERT(mDeferUpdate < 15, 0x175);
    mDeferUpdate++;
}

void RndText::ResolveUpdateText() {
    MILO_ASSERT(mDeferUpdate > 0, 0x17E);
    MILO_ASSERT(mDeferUpdate < 15, 0x17F);
    mDeferUpdate--;
    if (mNeedsUpdate && mDeferUpdate == 0) {
        mNeedsUpdate = false;
        UpdateText(true);
    }
}

void RndText::CollectGarbage() {
    std::set<RndText *>::iterator it;
    for (it = mTextMeshSet.begin(); it != mTextMeshSet.end();) {
        RndText *cur = *it;
        std::set<RndText *>::iterator toErase = it;
        it++;
        cur->mFramesSinceDraw++;
        if (!cur->mManualLines && cur->mFramesSinceDraw > 4) {
            if (cur->mMeshMap.size() != 0) {
                cur->mNeedsUpdate = true;
                for (std::map<FontKey, MeshInfo>::iterator mit = cur->mMeshMap.begin();
                     mit != cur->mMeshMap.end();
                     ++mit) {
                    RndMesh *mesh = mit->second.mesh;
                    delete mesh;
                }
                cur->mMeshMap.clear();
            }
            mTextMeshSet.erase(toErase);
        }
    }
}

void RndText::UpdateText(bool) {
    if (mDeferUpdate > 0) {
        mNeedsUpdate = true;
    } else {
        FOREACH (it, mMeshMap) {
            RndMesh *mesh = it->second.mesh;
            delete mesh;
        }
        mMeshMap.clear();
        std::set<RndText *>::iterator it = mTextMeshSet.find(this);
        if (it != mTextMeshSet.end()) {
            mTextMeshSet.erase(it);
        }
        mStyle.mFont = mFont;
        WrapText(mText.c_str(), mStyle, mLines);
        mMeshDirty = true;
        mCurWidth = 0;
        FOREACH (it, mLines) {
            MaxEq(mCurWidth, it->mWidth);
        }
        mCurHeight = mLines.front().xfm.v.z - mLines.back().xfm.v.z;
        if (mFont) {
            float diff = mFont->CellDiff();
            mCurHeight += mStyle.mSize * diff * mLeading;
        }
    }
}

void RndText::SetWrapWidth(float f) {
    if (mWrapWidth == f)
        return;
    mWrapWidth = f;
    UpdateText(true);
}

void RndText::SetFixedLength(int len) {
    if (mFixedLength != len) {
        MILO_ASSERT(len < 65535, 0x1F2);
        mFixedLength = len;
        if (len != 0)
            ResizeText(len);
        UpdateText(true);
    }
}

void RndText::SetSize(float f) {
    if (mStyle.mSize == f)
        return;
    mStyle.mSize = f;
    UpdateText(true);
}

void RndText::SetItalics(float f) {
    if (mStyle.mItalics == f)
        return;
    mStyle.mItalics = f;
    UpdateText(true);
}

void RndText::SetColor(const Hmx::Color &col) {
    if (mStyle.mTextColor == col)
        return;
    else {
        mStyle.mTextColor = col;
        bool b1 = false;
        if (!mTextMarkup) {
            FOREACH (it, mMeshMap) {
                RndMesh *mesh = it->second.mesh;
                if (mesh && mesh->Mutable()) {
                    RndMesh::VertVector &verts = mesh->Verts();
                    FOREACH (vit, verts) {
                        vit->color = col;
                    }
                    mesh->Sync(0x1F);
                    b1 = true;
                }
            }
        }
        if (!b1) {
            UpdateText(true);
        }
    }
}

void RndText::SetMarkup(bool b) {
    if (mTextMarkup == b)
        return;
    mTextMarkup = b;
    UpdateText(true);
}

void RndText::SetData(
    Alignment a,
    const char *text,
    RndFont *font,
    float leading,
    float wrapwidth,
    float size,
    float italics,
    const Hmx::Color &col,
    bool markup,
    CapsMode caps,
    int fixedLength
) {
    RndTextUpdateDeferrer deferrer(this);
    if (mAlign != a || mCapsMode != caps || mFont != font || mLeading != leading
        || mWrapWidth != wrapwidth || mTextMarkup != markup || mStyle.mSize != size
        || mStyle.mItalics != italics || !(mStyle.mTextColor == col)
        || mFixedLength != fixedLength) {
        SetFont(font);
        mAlign = a;
        mCapsMode = caps;
        mFont = font;
        mLeading = leading;
        mWrapWidth = wrapwidth;
        mTextMarkup = markup;
        mStyle.mSize = size;
        mStyle.mItalics = italics;
        mStyle.mTextColor = col;
        MILO_ASSERT(fixedLength < 65535, 0x256);
        mFixedLength = fixedLength;
        if (mFixedLength != 0)
            ResizeText(mFixedLength);
        UpdateText(true);
    }
    SetText(text);
}

void RndText::SetAltStyle(
    RndFont *font, float size, Hmx::Color *col, float z, float italics, bool b
) {
    RndTextUpdateDeferrer def(this);
    mAltStyle.mFont = font ? font : mFont.Ptr();
    mAltStyle.mSize = size ? size : mStyle.mSize;
    mAltStyle.mTextColor = col ? *col : mStyle.mTextColor;
    mAltStyle.mZOffset = z;
    mAltStyle.mItalics = italics;
    mUseAltStyle = b;
    UpdateText(true);
}

void RndText::SetAltSizeAndZOffset(float f1, float f2) {
    if (mAltStyle.mSize == f1 && mAltStyle.mZOffset == f2)
        return;
    mAltStyle.mSize = f1 ? f1 : mStyle.mSize;
    mAltStyle.mZOffset = f2;
    UpdateText(true);
}

BEGIN_COPYS(RndText)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(RndTransformable)
    if (ty == kCopyFromMax)
        return;
    CREATE_COPY(RndText)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mFont)
        COPY_MEMBER(mAlign)
        COPY_MEMBER(mCapsMode)
        COPY_MEMBER(mText)
        COPY_MEMBER(mWrapWidth)
        COPY_MEMBER(mLeading)
        COPY_MEMBER(mStyle)
        COPY_MEMBER(mTextMarkup)
        COPY_MEMBER(mFixedLength)
        if (mFixedLength != 0)
            ResizeText(mFixedLength);
    END_COPYING_MEMBERS
    UpdateText(true);
END_COPYS

void RndText::Print() {
    TextStream *ts = &TheDebug;
    *ts << "   font: " << mFont << "\n";
    *ts << "   align: " << mAlign << "\n";
    *ts << "   text: ";
    for (int i = 0; i < mText.length();) {
        unsigned short us;
        int next = DecodeUTF8(us, &mText.c_str()[i]);
        *ts << WideCharToChar(&us);
        i += next;
    }
    *ts << "\n";
    *ts << "   wrap width: " << mWrapWidth << "\n";
    *ts << "   leading: " << mLeading << "\n";
    *ts << "   size: " << mStyle.mSize << "\n";
    *ts << "   italics: " << mStyle.mItalics << "\n";
    *ts << "   color: " << mStyle.mTextColor << "\n";
    *ts << "   markup: " << mTextMarkup << "\n";
    *ts << "   capsMode: " << mCapsMode << "\n";
}

// The retail ctor's mem-init list is a 1:1 match for rb3-Wii's, including the
// opaque-white style colour (Wii Color32(-1) -> Hmx::Color(1,1,1,1)) and the
// four zeroed bools. MEASURED off fn_82456CB0.
RndText::RndText()
    : mFont(this), mWrapWidth(0), mAlign(kTopLeft), mCapsMode(kCapsModeNone),
      mLeading(1), mFixedLength(0),
      mStyle(mFont, 1, 0, Hmx::Color(1, 1, 1, 1), 0),
      mAltStyle(nullptr, 1, 0, Hmx::Color(1, 1, 1, 1), 0), mDeferUpdate(0),
      mMeshCallback(nullptr), mCurHeight(0), mCurWidth(0), mFramesSinceDraw(0) {
    mTextMarkup = false;
    mUseAltStyle = false;
    mNeedsUpdate = false;
    mMeshDirty = false;
    mManualLines = false;
    mRotateLineVerts = false;
}

RndText::~RndText() {
    MILO_ASSERT(mDeferUpdate == 0, 723);
    FOREACH (it, mMeshMap) {
        RndMesh *mesh = it->second.mesh;
        delete mesh;
    }
    std::set<RndText *>::iterator it = mTextMeshSet.find(this);
    if (it != mTextMeshSet.end()) {
        mTextMeshSet.erase(it);
    }
}

void RndText::SetFont(RndFont *f) {
    if (mFont != f) {
        mFont = f;
        FOREACH (it, mMeshMap) {
            RELEASE(it->second.mesh);
        }
        mMeshMap.clear();
        std::set<RndText *>::iterator it = mTextMeshSet.find(this);
        if (it != mTextMeshSet.end()) {
            mTextMeshSet.erase(it);
        }
        FontKey fontasInt = (FontKey)f;
        mMeshMap.insert(std::pair<FontKey, MeshInfo>(fontasInt, MeshInfo()));
        mMeshMap[fontasInt].displayableChars = 0;
        mMeshMap[fontasInt].syncFlags = 0;
        UpdateText(true);
    }
}

void RndText::SetAlignment(Alignment a) {
    if (mAlign == a)
        return;
    mAlign = a;
    UpdateText(true);
}

void RndText::SetLeading(float f) {
    if (mLeading == f)
        return;
    mLeading = f;
    UpdateText(true);
}

const char *
RndText::ParseMarkup(const char *cc, RndText::Style *style, float f3, float f4) const {
    const char *ptr = cc + 1;
    bool b1 = *ptr == '/';
    if (b1)
        ptr++;
    if (strnicmp(ptr, "sup", 3) == 0) {
        style->mSize = b1 ? f3 : gSuperscriptScale * f3;
        ptr += 3;
    } else if (strnicmp(ptr, "gtr", 3) == 0) {
        const float *gtrBase = &gSuperscriptScale + 1;
        style->mSize = b1 ? f3 : gtrBase[0] * f3;
        style->mZOffset = b1 ? f4 : gtrBase[1];
        ptr += 3;
    } else if (strnicmp(ptr, "it", 2) == 0) {
        style->mItalics = b1 ? 0 : 0.1f;
    } else if (strnicmp(ptr, "pre", 3) == 0) {
        style->pre = !b1;
    } else if (strnicmp(ptr, "color", 5) == 0) {
        ptr += 5;
        if (b1) {
            style->mTextColor = mStyle.mTextColor;
        } else {
            int colorR = 0, colorG = 0, colorB = 0;
            int colorA = (int)(style->mTextColor.alpha * 255.999f);
            ptr++;
            sscanf(ptr, "%d %d %d %d", &colorR, &colorG, &colorB, &colorA);
            style->mTextColor.Set(
                colorR / 255.0f, colorG / 255.0f, colorB / 255.0f, colorA / 255.0f
            );
        }
    } else if (strnicmp(ptr, "nobreak", 7) == 0) {
        if (b1)
            style->nobreak = mStyle.nobreak;
        else
            style->nobreak = false;
    } else if (strnicmp(ptr, "alt", 3) == 0) {
        if (b1 || !mUseAltStyle) {
            style->mTextColor = mStyle.mTextColor;
            style->mSize = f3;
            style->mFont = mFont;
            style->mZOffset = 0;
            style->mItalics = mStyle.mItalics;
        } else {
            style->mTextColor = mAltStyle.mTextColor;
            style->mSize = mAltStyle.mSize;
            style->mFont = mAltStyle.mFont;
            style->mZOffset = mAltStyle.mZOffset;
            style->mItalics = mAltStyle.mItalics;
        }
    }
    while (*ptr != '>' && *ptr != '\0')
        ptr++;
    if (*ptr != '\0')
        ptr++;
    return ptr;
}

bool canBreak(const char *cc, int i) {
    if (i < 0)
        return false;
    if (cc[i] == ' ')
        return true;
    return cc[i] == '\t';
}

float segmentLength(int i1, int i2, int i3, int i4, float *f5, const char *c6) {
    float lineLen = 0;
    for (; c6[i2 - 1] == ' ' && i1 < i2; i2--, i4--)
        ;
    for (int i = i3; i < i4; i++) {
        lineLen += f5[i];
    }
    return lineLen;
}

void RndText::ComputeCharWidths(float *fp, int i2, const char *cc, Style style) {
    unsigned short u7 = 0;
    float size = style.mSize;
    float f3 = style.mZOffset;
    for (int i = 0; i < i2; i++) {
        if (*cc == '<' && mTextMarkup) {
            const char *parsed = ParseMarkup(cc, &style, size, f3);
            while (cc != parsed) {
                fp[i++] = 0;
                cc++;
            }
            i--;
        } else {
            unsigned short us68;
            int i6 = DecodeUTF8(us68, cc);
            RndFont *i4 = SupportChar(us68, style.mFont);
            if (i4) {
                // 360: the 2-arg float CharAdvance(prev,cur) rb3-Wii uses is the
                // 3-arg bool out-param form here.
                float f9 = 0;
                i4->CharAdvance(u7, us68, f9);
                float fVal = style.mSize * f9;
                fp[i] = fVal;
                u7 = us68;
                if (fVal < 0)
                    fp[i] = 0;
                FontKey key = (FontKey)i4;
                mMeshMap[key].displayableChars++;
            } else
                fp[i] = 0;
            cc += i6;
        }
    }
}

DECOMP_FORCEACTIVE(Text, "lineLen >= bestLineLen", "bestWp != -1", "curStyle.brk == false")

struct WrapPoint {
    WrapPoint() : style() {}
    int byteIdx; // 0x00
    int charIdx; // 0x04
    int cost; // 0x08
    int bestPrevIdx; // 0x0C
    int nextIdx; // 0x10
    float bestLineLen; // 0x14
    RndText::Style style; // 0x18
    bool isLineEnd;
    bool isHardBreak;
};

void RndText::WrapText(const char *text, const Style &style, HX_VECTOR(Line) & lines) {
    lines.erase(lines.begin(), lines.end());

    int numChars = text ? UTF8StrLen(text) : 0;
    int textLen = text ? strlen(text) : 0;

    if (style.mFont == nullptr || textLen == 0) {
        Line emptyLine;
        if (lines.size() > 1) {
            lines.erase(lines.begin() + 1, lines.end());
        } else {
            lines.insert(lines.end(), 1 - lines.size(), emptyLine);
        }
        Line &line0 = lines[0];
        line0.lineStyle = style;
        line0.mStart = text;
        line0.mEnd = text + strlen(text);
        line0.xfm.v.x = 0.0f;
        line0.xfm.v.z = 0.0f;
        line0.mWidth = 0.0f;
        return;
    }

    float *charWidths = (float *)_alloca(numChars * sizeof(float));
    ComputeCharWidths(charWidths, numChars, text, style);

    if (mWrapWidth == 0.0f) {
        Line emptyLine;
        if (lines.size() > 1) {
            lines.erase(lines.begin() + 1, lines.end());
        } else {
            lines.insert(lines.end(), 1 - lines.size(), emptyLine);
        }
        Line &line0 = lines[0];
        line0.lineStyle = style;
        line0.mStart = text;
        line0.mEnd = text + strlen(text);
        line0.xfm.v.x = 0.0f;
        line0.xfm.v.z = 0.0f;
        line0.mWidth = segmentLength(0, textLen, 0, numChars, charWidths, text);
        return;
    }

    WrapPoint stackBuf[256];
    // Main DP wrap algorithm.
    WrapPoint *wps = stackBuf;
    if (numChars > 256) {
        wps = new WrapPoint[numChars + 1];
    }
    memset(wps, 0, numChars * sizeof(WrapPoint));

    Style curStyle = style;

    wps[0].byteIdx = 0;
    wps[0].charIdx = 0;
    wps[0].cost = 0;
    wps[0].bestPrevIdx = -1;
    wps[0].nextIdx = -1;
    wps[0].bestLineLen = 0.0f;
    wps[0].style = curStyle;
    wps[0].isLineEnd = true;
    wps[0].isHardBreak = true;

    int charCount = 0;
    const char *cur = text;

    float minW = mWrapWidth * 0.7f;
    float goodW = mWrapWidth * 0.95f;
    unsigned short curChar;
    int numWp = 1;
    bool activeMarkup = curStyle.nobreak;
    int curCharLen = DecodeUTF8(curChar, cur);

    while (curChar != 0) {
        while (curChar != 0 && curChar != '\n') {
        soft_loop_top:
            if (curChar == '<' && mTextMarkup) {
                const char *parsed =
                    ParseMarkup(cur, &curStyle, style.mSize, style.mZOffset);
                cur = parsed;
                curCharLen = DecodeUTF8(curChar, cur);
                if (curStyle.nobreak == true) {
                    activeMarkup = true;
                }
                if (curChar == 0 || curChar == '\n')
                    break;
                goto soft_loop_top;
            }

            int byteIdx = cur - text;
            if (activeMarkup) {
                if (canBreak(text, byteIdx - 1)) {
                    int prevWp = numWp - 1;
                    int bestWp = -1;
                    int bestCost = 100000;
                    float bestLineLen = 0.0f;
                    bool overflow = false;
                    WrapPoint *cand = &wps[prevWp];
                    while (prevWp >= 0) {
                        float lineLen = segmentLength(
                            cand->byteIdx, byteIdx, cand->charIdx, charCount, charWidths, text
                        );
                        MILO_ASSERT(lineLen >= bestLineLen, 0x4CC);
                        unsigned int pen = 10;
                        if (lineLen > mWrapWidth) {
                            if (prevWp != numWp - 1 && bestWp != -1) {
                                overflow = true;
                            }
                        } else {
                            if (lineLen < goodW) {
                                float fullLen = segmentLength(
                                    cand->byteIdx,
                                    textLen,
                                    cand->charIdx,
                                    numChars,
                                    charWidths,
                                    text
                                );
                                if (fullLen >= mWrapWidth) {
                                    pen = (unsigned int)(int)((1.0f - lineLen / mWrapWidth) * 60.0f);
                                    if (lineLen < minW)
                                        pen += 200;
                                }
                            }
                        }
                        int tc = (int)pen + cand->cost;
                        if (tc <= bestCost) {
                            bestCost = tc;
                            bestWp = prevWp;
                            bestLineLen = lineLen;
                        }
                        if (cand->isHardBreak || overflow)
                            break;
                        cand--;
                        prevWp--;
                    }
                    MILO_ASSERT(bestWp != -1, 0x4FD);
                    WrapPoint *nxt = &wps[numWp];
                    nxt->byteIdx = byteIdx;
                    nxt->charIdx = charCount;
                    nxt->cost = bestCost;
                    nxt->bestPrevIdx = bestWp;
                    nxt->nextIdx = -1;
                    nxt->bestLineLen = bestLineLen;
                    nxt->style = curStyle;
                    nxt->isLineEnd = true;
                    nxt->isHardBreak = false;
                    wps[bestWp].isLineEnd = false;
                    numWp++;
                }
                if (activeMarkup != curStyle.nobreak) {
                    MILO_ASSERT(curStyle.nobreak == false, 0x511);
                    activeMarkup = false;
                }
            }
            cur += curCharLen;
            curCharLen = DecodeUTF8(curChar, cur);
            charCount++;
        }

        // Hard break (newline or nul). Add a hard-break wrap point; if it was a
        // newline, advance past it and continue the outer loop.
        {
            int byteEnd = cur - text;
            int prevWp = numWp - 1;
            int bestWp = -1;
            int bestCost = 100000;
            float bestLineLen = 0.0f;
            bool overflow = false;
            WrapPoint *cand = &wps[prevWp];
            while (prevWp >= 0) {
                float lineLen = segmentLength(
                    cand->byteIdx, byteEnd, cand->charIdx, charCount, charWidths, text
                );
                unsigned int pen = 10;
                if (lineLen > mWrapWidth) {
                    if (prevWp != numWp - 1 && bestWp != -1) {
                        overflow = true;
                    }
                } else {
                    if (mAlign & 0x20) {
                        float fullLen = segmentLength(
                            cand->byteIdx, textLen, cand->charIdx, numChars, charWidths, text
                        );
                        if (fullLen >= mWrapWidth) {
                            pen = (unsigned int)(int)((1.0f - lineLen / mWrapWidth) * 30.0f);
                            if (lineLen < minW)
                                pen += 100;
                        }
                    } else {
                        if (charCount - cand->charIdx <= 4)
                            pen = 50;
                    }
                }
                int tc = (int)pen + cand->cost;
                if (tc < bestCost) {
                    bestCost = tc;
                    bestWp = prevWp;
                    bestLineLen = lineLen;
                }
                if (cand->isHardBreak || overflow)
                    break;
                cand--;
                prevWp--;
            }
            MILO_ASSERT(bestWp != -1, 0x55F);
            WrapPoint *nxt = &wps[numWp];
            nxt->byteIdx = byteEnd;
            nxt->charIdx = charCount;
            nxt->cost = bestCost;
            nxt->bestPrevIdx = bestWp;
            nxt->nextIdx = -1;
            nxt->bestLineLen = bestLineLen;
            nxt->style = curStyle;
            nxt->isLineEnd = true;
            nxt->isHardBreak = true;
            wps[bestWp].isLineEnd = false;
            numWp++;
        }
        if (curChar == 0)
            break;
        // '\n' — step past it and continue.
        cur += curCharLen;
        curCharLen = DecodeUTF8(curChar, cur);
        charCount++;
    }

    // Link bestPrevIdx -> nextIdx.
    {
        int idx = numWp - 1;
        while (idx != 0) {
            int prev = wps[idx].bestPrevIdx;
            wps[prev].nextIdx = idx;
            idx = prev;
        }
    }

    // Build Line entries by forward-walking nextIdx from wps[0]. The line start
    // comes from wp->byteIdx directly — wrap points already sit at post-space
    // positions. Trailing whitespace is trimmed off the end pointer.
    {
        WrapPoint *wp = &wps[0];
        while (wp->nextIdx != -1) {
            WrapPoint *ne = &wps[wp->nextIdx];
            Line tmpLine;
            tmpLine.lineStyle = wp->style;
            tmpLine.mStart = text + wp->byteIdx;
            tmpLine.mEnd = text + ne->byteIdx;
            tmpLine.startIdx = wp->charIdx;
            tmpLine.endIdx = ne->charIdx;
            tmpLine.mWidth = ne->bestLineLen;
            while (tmpLine.mEnd > tmpLine.mStart) {
                char p = tmpLine.mEnd[-1];
                if (p != ' ' && p != '\n' && p != '\t')
                    break;
                --tmpLine.mEnd;
            }
            lines.push_back(tmpLine);
            wp = ne;
        }
    }

    if (wps != stackBuf) {
        delete[] wps;
    }

    if (lines.size() == 0) {
        Line emptyLine;
        emptyLine.lineStyle = style;
        emptyLine.mStart = text;
        emptyLine.mEnd = text;
        emptyLine.mWidth = 0.0f;
        lines.push_back(emptyLine);
    }

    // Max font cell-diff across the fonts used in this text.
    float ratio = 0.0f;
    for (std::map<FontKey, MeshInfo>::iterator it = mMeshMap.begin();
         it != mMeshMap.end();
         ++it) {
        RndFont *font = (RndFont *)it->first;
        float diff = font->CellDiff();
        if (diff > ratio)
            ratio = diff;
    }
    ratio *= style.mSize;

    float topY = 0.0f;
    if (mAlign & 0x20) {
        topY = 0.5f * ratio * ((float)(int)(lines.size() - 1) * mLeading + 1.0f);
    } else if (mAlign & 0x40) {
        topY = ratio * ((float)(int)(lines.size() - 1) * mLeading + 1.0f);
    }

    for (unsigned int i = 0; i < lines.size(); i++) {
        Line &l = lines[i];
        l.xfm.v.x = GetHorizontalAlignOffset(l, (Alignment)mAlign);
        l.xfm.v.z = topY;
        topY -= mLeading * ratio;
    }
}

// WrapText is the only caller of vector<Line>::insert(pos, n, x). Force the
// instantiation so the out-of-line _M_fill_insert helpers get emitted.
DECOMP_FORCEBLOCK(
    Text,
    (HX_VECTOR(RndText::Line) & lines, const RndText::Line &line, int n),
    lines.insert(lines.end(), n, line);
)

void RndText::SetText(const char *text) {
    String tmp;
    tmp.reserve(mText.capacity());
    mText.swap(tmp);
    if (mFixedLength != 0) {
        MILO_ASSERT(tmp.capacity() >= mFixedLength, 0x5D6);
        int len = UTF8StrLen(text);
        int textLen;
        if (len > mFixedLength) {
            char *ptr = (char *)text;
            for (int i = 0; i < mFixedLength; i++) {
                unsigned short us;
                ptr += DecodeUTF8(us, ptr);
            }
            textLen = ptr - text;
        } else
            textLen = strlen(text);
        if (mText.capacity() < textLen) {
            mText.resize(textLen);
        }
        strncpy((char *)mText.c_str(), text, textLen);
        ((char *)mText.c_str())[textLen] = '\0';
    } else {
        mText = text;
    }
    if (!mText.empty()) {
        if (mCapsMode == kForceLower || mCapsMode == kForceUpper) {
            int i2 = 0;
            const char *casestr = "[noforcecase]";
            for (int i = 0; i < mText.length();) {
                unsigned short us;
                unsigned int ui = DecodeUTF8(us, &mText[i]);
                if (us != (unsigned short)*casestr)
                    break;
                if (i2 == 0xC) {
                    mCapsMode = kCapsModeNone;
                    mText = mText.replace(0, 0xD, "");
                    break;
                }
                i2++;
                casestr++;
                i += ui;
            }
        }
        if (mCapsMode == kForceUpper) {
            for (int i = 0; i < mText.length();) {
                unsigned short us;
                unsigned int ui = DecodeUTF8(us, &mText[i]);
                UTF8ToUpper(us, &mText[i]);
                i += ui;
            }
            const char *search = "\xC3\x9F";
            unsigned int ui = 0;
            while (true) {
                ui = mText.find(search, ui);
                if (ui == String::npos)
                    break;
                mText.replace(ui, 2, "SS");
            }
        } else if (mCapsMode == kForceLower) {
            for (int i = 0; i < mText.length();) {
                unsigned short us;
                unsigned int len = DecodeUTF8(us, &mText[i]);
                UTF8ToLower(us, &mText[i]);
                i += len;
            }
        }
    }
    if (mText != tmp) {
        UpdateText(true);
    }
}

float RndText::GetStringWidthUTF8(
    const char *cc1, const char *cc2, bool bbb, const RndText::Style *styleIn
) const {
    unsigned short us8 = 0;
    float ret = 0;
    Style myStyle;
    Style *style = (Style *)styleIn;
    if (!cc2) {
        cc2 = cc1 + strlen(cc1);
    }
    if (!style) {
        myStyle = mStyle;
        style = &myStyle;
    }
    if (!style->mFont) {
        style->mFont = (RndFont *)mFont.Ptr();
    }
    float size = style->mSize;
    float zoff = style->mZOffset;
    const char *ccIt = cc1;
    while (ccIt != cc2) {
        if (ccIt > cc2) {
            MILO_WARN("bad utf8 string in RndText::GetStringWidth \"%s\"", cc1);
            ccIt = cc2;
            break;
        }
        unsigned short us;
        int decoded = DecodeUTF8(us, ccIt);
        if (us == 0x3C && mTextMarkup) {
            ccIt = ParseMarkup(ccIt, style, size, zoff);
        } else {
            RndFont *font = GetDefiningFont(us, style->mFont);
            if (font) {
                float adv = 0;
                font->CharAdvance(us8, us, adv);
                ret += style->mSize * adv;
            }
            us8 = us;
            ccIt += decoded;
        }
    }
    if (bbb) {
        unsigned short us;
        DecodeUTF8(us, ccIt);
        RndFont *font = GetDefiningFont(us, style->mFont);
        if (font) {
            ret += style->mSize * font->Kerning(us8, us);
        }
    }
    return ret;
}

void RndText::ResetFaces(RndMesh *mesh, int new_size) {
    MILO_ASSERT(mesh, 0x689);
    mesh->Faces().resize(new_size);
    std::vector<RndMesh::Face>::iterator it = mesh->Faces().begin();
    std::vector<RndMesh::Face>::iterator itEnd = mesh->Faces().end();
    int num = 0;
    for (; it != itEnd; it += 2, num += 4) {
        RndMesh::Face *face = it;
        face->Set(num, num + 1, num + 2);
        face[1].Set(num, num + 2, num + 3);
    }
}

void RndText::UpdateMesh(RndFont *font) {
    MeshInfo *meshInfo = &mMeshMap[(FontKey)font];
    RndMesh *mesh = meshInfo->mesh;
    MILO_ASSERT(mesh, 0x6A6);
    if (!font) {
        mesh->SetShowing(false);
        return;
    }
    mesh->SetShowing(true);
    int i8 = 0x1F;
    if (mFixedLength == 0) {
        int i1 = meshInfo->displayableChars * 2;
        ResetFaces(mesh, i1);
        i8 |= 0xA0;
        mesh->Verts().resize(i1 * 2);
    } else if (!(mesh->Mutable() & 0x1F) || mesh->Verts().size() != mFixedLength * 4) {
        mesh->SetMutable(0x1F);
        ResetFaces(mesh, mFixedLength * 2);
        i8 |= 0xA0;
        mesh->Verts().resize(mFixedLength * 4);
    }
    int len = mFixedLength;
    if (len && meshInfo->displayableChars > len) {
        // Residue: retail splits `len`'s live range with a degenerate
        // `rlwinm r10,r10,0,0,31` (mr r10,r10) before the doubling, which flips
        // the r10/r11 assignment across this whole guard. Four semantically
        // identical shapes (member re-read, `len *= 2`, distinct temp, split
        // compound-assign statements) all fold to the same IR under /O1.
        ResizeText(len * 2 - meshInfo->displayableChars);
        meshInfo->displayableChars = mFixedLength;
    }
    MILO_ASSERT(mesh->Verts().size() >= meshInfo->displayableChars * 4, 0x6CF);
    // Retail reads the font's material with a single non-virtual field load,
    // `lwz r4, 0x30(font)`, at every SetMat site in this TU (0x82458E20,
    // 0x82458ED4, 0x8245911C, 0x824594C4) -- there is no vtable call anywhere.
    // RndFont::mMat now sits at 0x28 (mObject at 0x30), so the plain non-virtual
    // GetMat() accessor compiles to exactly that load. The raw
    // `*(RndMat *const *)((const char *)font + 0x30)` cast that used to stand in
    // here -- and its HX_NATIVE escape hatch -- are no longer needed.
    mesh->SetMat(font->GetMat());
    CreateLines(font);
    if (mMeshCallback) {
        struct _Callback {
            virtual void _0() = 0;
            virtual void Update(RndMesh *) = 0;
        };
        static_cast<_Callback *>(mMeshCallback)->Update(mesh);
    }
    mesh->Sync(i8);
    meshInfo->syncFlags = 0;
}

// 360 translation note: rb3-Wii calls font->GetTexCoords(c, uv0, uv2) plus
// separate CharWidth/CharAdvance. The 360 RndFont fuses all three into
// CharWidthAdvanceCoords(c, &charW, &advW, &uvMin, &uvMax) -> bool, and the Vert
// UV member is `tex`, not `uv`. Otherwise this is Wii's body unchanged.
void SetupCharVerts(
    unsigned short us1,
    RndMesh::Vert *&vert,
    float &fref,
    float f4,
    float f5,
    float f6,
    float f7,
    const RndText::Style &style,
    RndFont *font,
    unsigned short us10,
    bool b11
) {
    float charW, advW;
    if (!font->CharWidthAdvanceCoords(us1, charW, advW, vert[0].tex, vert[2].tex))
        return;
    if (!b11) {
        fref += style.mSize * font->Kerning(us10, us1);
    }
    float f1 = style.mSize * charW;
    if (f1 <= 0) {
        f1 = style.mSize * advW;
    }
    if (f1 <= 0)
        return;
    else {
        vert[1].tex.Set(vert[0].tex.x, vert[2].tex.y);
        vert[3].tex.Set(vert[2].tex.x, vert[0].tex.y);
        float topZ = f5;
        float botZ = f5 - f6;
        vert[0].pos.Set(fref + f7, f4, topZ);
        vert[1].pos.Set(fref - f7, f4, botZ);
        vert[2].pos.Set(f1 + (fref - f7), f4, botZ);
        vert[3].pos.Set(f1 + fref + f7, f4, topZ);
        vert[0].norm.Set(0, -1, 0);
        vert[1].norm = vert[2].norm = vert[3].norm = vert[0].norm;
        vert[0].color = vert[1].color = vert[2].color = vert[3].color = style.mTextColor;
        vert += 4;
        if (!b11) {
            fref += style.mSize * advW;
        }
    }
}

void RndText::CreateLines(RndFont *font) {
    RndMesh *mesh = mMeshMap[(FontKey)font].mesh;
    MILO_ASSERT(mesh, 0x709);
    RndMesh::Vert *vertIt = mesh->Verts().begin();
    Style style = mLines[0].lineStyle;
    float f4 = style.mItalics * style.mSize;
    font->CellDiff();
    float f1 = mStyle.mSize;
    float f2 = mStyle.mZOffset;
    for (int i = 0; i < mLines.size(); i++) {
        Line &curLine = mLines[i];
        unsigned short i14 = 0;
        RndMesh::Vert *vert = vertIt;
        float f3 = curLine.xfm.v.z;
        float f90 = curLine.xfm.v.x;
        const char *cc13 = curLine.mStart;
        while (cc13 != curLine.mEnd) {
            unsigned short us98;
            unsigned int ui = DecodeUTF8(us98, cc13);
            if (us98 == 0x3C && mTextMarkup) {
                cc13 = ParseMarkup(cc13, &style, f1, f2);
                f4 = style.mItalics * style.mSize;
            } else {
                RndFont *definingFont = GetDefiningFont(us98, style.mFont);
                if (definingFont) {
                    if (font == definingFont) {
                        SetupCharVerts(
                            us98,
                            vertIt,
                            f90,
                            curLine.xfm.v.y,
                            f3 + style.mZOffset,
                            style.mSize * definingFont->CellDiff(),
                            f4,
                            style,
                            definingFont,
                            i14,
                            false
                        );
                    } else {
                        float adv = 0;
                        definingFont->CharAdvance(i14, us98, adv);
                        f90 += style.mSize * adv;
                    }
                }
                i14 = us98;
                cc13 += ui;
            }
        }
        RotateLineVerts(curLine, vert, vertIt);
    }
    while (vertIt != mesh->Verts().end()) {
        vertIt++->pos.Set(0, 0, 0);
    }
}

int RndText::NumCharsInBytes(
    const String &str, const RndText::Style &style, float &fref, int i4
) {
    int len = strlen(str.c_str());
    int i5 = 0;
    float f8 = 0;
    int s4 = 0;
    while (s4 < len) {
        unsigned short us;
        int decoded = DecodeUTF8(us, str.c_str() + s4);
        if (i4 > -1 && s4 + decoded > i4) {
            len = s4;
            goto done;
        }
        RndFont *support = SupportChar(us, mFont);
        if ((us == 0x20 || us == 9 || us == 10) && len > 0) {
            i5++;
            if (support) {
                f8 += style.mSize * support->CharAdvance(us);
            }
        } else {
            i5 = 0;
            f8 = 0;
        }
        s4 += decoded;
    }
done:
    fref += f8;
    return len - i5;
}

void RndText::ApplyLineText(
    const String &utf8,
    const RndText::Style &style,
    float &fref,
    RndText::Line &line,
    int i5,
    int i6,
    bool *b7
) {
    if (!mMeshMap.empty()) {
        if (mText.length() < line.endIdx) {
            mText.resize(line.endIdx);
        }
        MILO_ASSERT((line.startIdx + utf8.length()) <= mFixedLength, 0x791);
        const char *theStrstr = utf8.c_str();
        const char *ptr = theStrstr;
        for (int i = 0; i < i5; i++) {
            char ptrChar = *ptr;
            mText[line.startIdx + i] = ptrChar;
            ptr++;
        }
        for (int i = i5; i < i6; i++) {
            mText[line.startIdx + i] = 0x20;
        }
        float f28 = 0;
        unsigned short i23 = 0;
        int i7 = 0;
        RndFont *key = (RndFont *)mMeshMap.begin()->first;
        Style localStyle(style);
        while (*theStrstr != '\0') {
            while ((*theStrstr == '<' && mTextMarkup)) {
                theStrstr = ParseMarkup(theStrstr, &localStyle, style.mSize, style.mZOffset);
            }
            if (*theStrstr != '\0') {
                unsigned short use6;
                int decoded = DecodeUTF8(use6, theStrstr);
                if (i7 < i6) {
                    RndFont *defining = GetDefiningFont(use6, key);
                    if (defining) {
                        f28 += localStyle.mSize * defining->Kerning(i23, use6);
                        f28 += localStyle.mSize * defining->CharAdvance(use6);
                    }
                    i7++;
                    i23 = use6;
                }
                theStrstr += decoded;
            }
        }
        line.mWidth = f28;

        float f3 = line.xfm.v.x;
        float f4 = line.xfm.v.y;
        Alignment align = GetAlignment();
        float f26 = GetHorizontalAlignOffset(line, align);
        i7 = 0;
        i23 = 0;
        FOREACH (it, mMeshMap) {
            Style mapStyle(style);
            RndFont *curFontKey = (RndFont *)it->first;
            MeshInfo &meshInfo = it->second;
            RndMesh *curMesh = meshInfo.mesh;
            int uvar8 = 0;
            float fd4 = f3 + f26;
            if (curMesh) {
                if (!(curMesh->Mutable() & 0x1F)
                    || mFixedLength * 4 != curMesh->Verts().size()) {
                    curMesh->SetMutable(0x1F);
                    ResetFaces(curMesh, mFixedLength * 2);
                    curMesh->Verts().resize(mFixedLength * 4);
                    uvar8 |= 0xBF;
                }
                curMesh->SetMat(curFontKey->GetMat());
            }
            RndMesh::Vert *theVert = curMesh->Verts().begin() + line.startIdx * 4;
            RndMesh::Vert *vertd8 = theVert;
            const char *curStrStr = utf8.c_str();
            while (*curStrStr != '\0') {
                while (*curStrStr == '<' && mTextMarkup) {
                    curStrStr =
                        ParseMarkup(curStrStr, &mapStyle, style.mSize, style.mZOffset);
                }
                if (*curStrStr != '\0') {
                    float f6 = mapStyle.mItalics * mapStyle.mSize;
                    unsigned short use8;
                    int decoded = DecodeUTF8(use8, curStrStr);
                    if (i7 < i6) {
                        RndFont *defining = GetDefiningFont(use8, curFontKey);
                        if (defining) {
                            if (defining == curFontKey) {
                                float advTmp = 0;
                                defining->Kerning(i23, use8);
                                defining->CharAdvance(use8);
                                float diff = defining->CellDiff();
                                float f5 = line.xfm.v.z;
                                float f1 = mapStyle.mSize * diff;
                                if (mAlign & 0x20) {
                                    f5 += f1 / 2.0f;
                                } else if (mAlign & 0x40) {
                                    f5 += f1;
                                }
                                SetupCharVerts(
                                    use8,
                                    theVert,
                                    fd4,
                                    f4,
                                    f5 + mapStyle.mZOffset,
                                    f1,
                                    f6,
                                    mapStyle,
                                    defining,
                                    i23,
                                    false
                                );
                                uvar8 |= 0x1F;
                                (void)advTmp;
                            } else {
                                fd4 += style.mSize * defining->Kerning(i23, use8);
                                fd4 += style.mSize * defining->CharAdvance(use8);
                            }
                            uvar8 |= 0x1F;
                        }
                        i7++;
                        i23 = use8;
                    }
                    curStrStr += decoded;
                }
            }
            RotateLineVerts(line, vertd8, theVert);
            meshInfo.syncFlags |= uvar8;
        }
        if (b7)
            *b7 = true;
        else
            SyncMeshes();
        fref += f28;
    }
}

int RndText::AddLineUTF8(
    const String &utf8,
    const Transform &tf,
    const RndText::Style &style,
    float *fp,
    bool *bp,
    int i6
) {
    mManualLines = true;
    float f98 = 0;
    int lineIdx;
    fp = fp ? fp : &f98;

    const String &_ref0 = mText;
    int _tmp0 = utf8.length();
    int _tmp1 = _ref0.length();
    if ((unsigned int)(_tmp1 + _tmp0) > (unsigned int)mFixedLength) {
        MILO_WARN(
            "Text %s%s exceeds fixed length of %d, truncating",
            utf8.c_str(),
            _ref0.c_str(),
            mFixedLength
        );
        return -1;
    } else {
        int newCharsInBytes = NumCharsInBytes(utf8, style, *fp, i6);
        if (newCharsInBytes != 0 || i6 != 0) {
            MILO_ASSERT(newCharsInBytes <= utf8.length(), 0x850);
            for (lineIdx = mLines.size();
                 lineIdx > 0
                 && (mLines[lineIdx - 1].endIdx
                     && mLines[lineIdx - 1].startIdx == mLines[lineIdx - 1].endIdx);
                 lineIdx--)
                ;
            if (lineIdx == mLines.size()) {
                if (mLines.size() == mLines.capacity()) {
                    MILO_WARN(
                        "RndText::AddLineUTF8() - reserve_lines %d is too low; reallocating",
                        mLines.capacity()
                    );
                }
                Line linetopush;
                mLines.push_back(linetopush);
            }
            Line &line = mLines[lineIdx];
            line.xfm = tf;
            line.lineStyle = style;
            line.startIdx = 0;
            if (lineIdx != 0) {
                line.startIdx = mLines[lineIdx - 1].endIdx;
            }
            if (i6 > -1) {
                line.endIdx = line.startIdx + i6;
            } else {
                line.endIdx = line.startIdx + newCharsInBytes;
                i6 = newCharsInBytes;
            }
            MILO_ASSERT(line.endIdx <= mFixedLength, 0x874);
            line.mStart = _ref0.c_str() + line.startIdx;
            line.mEnd = _ref0.c_str() + line.endIdx;
            ApplyLineText(utf8, style, *fp, line, newCharsInBytes, i6, bp);
            return lineIdx;
        } else
            return -1;
    }
}

// Retail's Line has no separate `color` member (rb3-Wii's duplicate was dead
// storage — it always held the same value as lineStyle.color). So both the
// early-out and the final store go through lineStyle.mTextColor.
void RndText::UpdateLineColor(unsigned int idx, const Hmx::Color &col, bool *bptr) {
    HX_VECTOR(Line) &_ref0 = mLines;
    MILO_ASSERT(idx < _ref0.size(), 0x883);
    Line &curLine = _ref0[idx];
    MILO_ASSERT(mMeshMap.size() < 10, 0x887);
    if (curLine.lineStyle.mTextColor == col)
        return;
    int mapInts[10];
    for (int i = 0; i < mMeshMap.size(); i++) {
        mapInts[i] = 0;
    }

    std::map<FontKey, MeshInfo>::iterator it = mMeshMap.begin();
    for (int i = 0; i < mMeshMap.size(); i++) {
        RndFont *curFont = (RndFont *)it->first;
        for (int j = 0; j < curLine.startIdx;) {
            unsigned short us86;
            int decoded = DecodeUTF8(us86, mText.c_str() + j);
            if (GetDefiningFont(us86, curLine.lineStyle.mFont) == curFont) {
                mapInts[i]++;
            }
            j += decoded;
        }
        ++it;
    }

    std::map<FontKey, MeshInfo>::iterator it2 = mMeshMap.begin();
    for (int i = 0; i < mMeshMap.size(); i++, ++it2) {
        RndFont *curFont = (RndFont *)it2->first;
        unsigned int cidx = curLine.startIdx;
        unsigned int min = std::min<unsigned int>(curLine.endIdx, mFixedLength);
        int i11 = mapInts[i] * 4;
        for (; cidx < min;) {
            unsigned short us88;
            int decoded = DecodeUTF8(us88, mText.c_str() + cidx);
            RndFont *defining = GetDefiningFont(us88, curLine.lineStyle.mFont);
            if (defining == curFont) {
                FontKey definingFontAsInt = (FontKey)defining;
                MeshInfo &curMeshInfo = mMeshMap[definingFontAsInt];
                RndMesh::Vert *vert10 = curMeshInfo.mesh->Verts().begin() + i11;
                vert10[3].color = col;
                vert10[2].color = col;
                vert10[1].color = col;
                vert10[0].color = col;
                i11 += 4;
                curMeshInfo.syncFlags |= 0x1F;
            }
            cidx += decoded;
        }
    }

    curLine.lineStyle.mTextColor = col;
    if (bptr)
        *bptr = true;
    else
        SyncMeshes();
}

void RndText::ReplaceLineText(
    unsigned int idx,
    const String &utf8,
    const Transform &xfm,
    const RndText::Style &style,
    float *fptr,
    bool *bptr,
    int fixedLineLength
) {
    MILO_ASSERT(idx < mLines.size(), 0x8E5);
    float f3c = 0;
    fptr = fptr ? fptr : &f3c;
    int newCharsInBytes = NumCharsInBytes(utf8, style, *fptr, fixedLineLength);
    MILO_ASSERT(newCharsInBytes <= utf8.length(), 0x8EC);
    Line &line = mLines[idx];
    line.xfm = xfm;
    line.lineStyle = style;
    MILO_ASSERT(line.endIdx <= mFixedLength, 0x8F2);
    MILO_ASSERT(line.endIdx - line.startIdx == fixedLineLength, 0x8F3);
    FOREACH (it, mMeshMap) {
        MeshInfo &curInfo = it->second;
        RndMesh *mesh = curInfo.mesh;
        if (mesh) {
            RndMesh::Vert *vertIt = &mesh->Verts().mVerts[line.startIdx * 4];
            RndMesh::Vert *vertEnd = &mesh->Verts().mVerts[line.endIdx * 4];
            for (RndMesh::Vert *v = vertIt; v < vertEnd; v++) {
                v->pos.Set(0, 0, 0);
                v->tex.Set(0, 0);
            }
            curInfo.syncFlags |= 0x1F;
        }
    }
    ApplyLineText(utf8, style, *fptr, line, newCharsInBytes, fixedLineLength, bptr);
}

void RndText::SyncMeshes() {
    FOREACH (it, mMeshMap) {
        MeshInfo &info = it->second;
        if (info.syncFlags) {
            info.mesh->Sync(info.syncFlags);
            info.syncFlags = 0;
        }
    }
}

void RndText::SetMeshForceNoUpdate() {
    if (mMeshDirty) {
        mTextMeshSet.insert(this);
        mMeshDirty = false;
    }
}

void RndText::ReserveLines(int i) { mLines.reserve(i); }

void RndText::GetVerticalBounds(float &f1, float &f2) const {
    if (!mFont) {
        f1 = 0;
        f2 = 0;
    } else {
        f1 = mLines.front().xfm.v.z;
        f2 = -(mLeading * mStyle.mSize - mLines.back().xfm.v.z);
    }
}

float RndText::MaxLineWidth() const {
    float width = 0;
    for (int i = 0; i < mLines.size(); i++) {
        MaxEq(width, mLines[i].mWidth);
    }
    return width;
}

// Retail carries no mBounds* members; these derive the same extents the DC3
// block used to cache, so LabelShrinkWrapper / UIListLabel / UIListProvider keep
// working. Computed, not stored -> layout-neutral.
float RndText::BoundsLeft() const { return 0.0f; }
float RndText::BoundsRight() const { return MaxLineWidth(); }

float RndText::BoundsTop() const {
    float top, bottom;
    GetVerticalBounds(top, bottom);
    return top;
}

float RndText::BoundsBottom() const {
    float top, bottom;
    GetVerticalBounds(top, bottom);
    return bottom;
}

void RndText::GetMeshes(std::vector<RndMesh *> &meshes) {
    meshes.clear();
    FOREACH (it, mMeshMap) {
        meshes.push_back(it->second.mesh);
    }
}

void RndText::GetStringDimensions(
    float &f1, float &f2, HX_VECTOR(Line) & lines, const char *cc, float size
) {
    lines.clear();
    Style theStyle = mStyle;
    theStyle.mSize = size;
    theStyle.mFont = mFont;
    WrapText(cc, theStyle, lines);
    f1 = 0;
    FOREACH (it, lines) {
        MaxEq(f1, (*it).mWidth);
    }
    f2 = lines.front().xfm.v.z - lines.back().xfm.v.z;
    if (mFont) {
        float diff = mFont->CellDiff();
        f2 += theStyle.mSize * diff * mLeading;
    }
}

void RndText::GetCurrentStringDimensions(float &f1, float &f2) {
    f1 = mCurWidth;
    f2 = mCurHeight;
}

void RndText::Draw() {
    RndDrawable::Draw();
    if (!mShowing && !mManualLines) {
        if (mMeshMap.size() != 0) {
            mNeedsUpdate = true;
            FOREACH (it, mMeshMap) {
                RndMesh *mesh = it->second.mesh;
                delete mesh;
            }
            mMeshMap.clear();
            std::set<RndText *>::iterator it = mTextMeshSet.find(this);
            if (it != mTextMeshSet.end()) {
                mTextMeshSet.erase(it);
            }
        }
    }
}

void RndText::DrawShowing() {
    mFramesSinceDraw = 0;
    if (mNeedsUpdate) {
        mNeedsUpdate = false;
        UpdateText(true);
    }
    if (mMeshDirty) {
        mTextMeshSet.insert(this);
        mMeshDirty = false;
        FOREACH (it, mMeshMap) {
            if (it->second.mesh) {
                UpdateMesh((RndFont *)it->first);
            }
        }
    }
    FOREACH (it, mMeshMap) {
        MeshInfo &meshInfo = it->second;
        if (meshInfo.mesh) {
            meshInfo.mesh->DrawShowing();
        }
    }
}

float RndText::GetDistanceToPlane(const Plane &p, Vector3 &v) {
    if (mMeshMap.empty())
        return 0;
    else {
        float ret = 0;
        bool first = true;
        FOREACH (it, mMeshMap) {
            RndMesh *mesh = it->second.mesh;
            if (mesh) {
                Vector3 vec;
                float dist = mesh->GetDistanceToPlane(p, vec);
                if (first || std::fabs(dist) < std::fabs(ret)) {
                    first = false;
                    v = vec;
                    ret = dist;
                }
            }
        }
        return ret;
    }
}

bool RndText::MakeWorldSphere(Sphere &s, bool b) {
    s.Zero();
    FOREACH (it, mMeshMap) {
        RndMesh *mesh = it->second.mesh;
        if (mesh) {
            Sphere localS;
            if (b) {
                mesh->MakeWorldSphere(localS, true);
            } else if (mSphere.GetRadius()) {
                Multiply(mSphere, WorldXfm(), localS);
            }
            s.GrowToContain(localS);
        }
    }
    return s.GetRadius();
}

void RndText::UpdateSphere() {
    Sphere s;
    s.Zero();
    FOREACH (it, mMeshMap) {
        RndMesh *mesh = it->second.mesh;
        if (mesh) {
            mesh->UpdateSphere();
            s.GrowToContain(mesh->GetSphere());
        }
    }
    SetSphere(s);
}

RndFont *RndText::SupportChar(unsigned short us, RndFont *font) {
    RndFont *defining = GetDefiningFont(us, font);
    if (defining) {
        std::map<FontKey, MeshInfo>::iterator it = mMeshMap.find((FontKey)defining);
        if (it == mMeshMap.end()) {
            it = mMeshMap
                     .insert(std::pair<FontKey, MeshInfo>((FontKey)defining, MeshInfo()))
                     .first;
        }
        MeshInfo &meshInfo = it->second;
        if (!meshInfo.mesh) {
            meshInfo.mesh = Hmx::Object::New<RndMesh>();
            meshInfo.mesh->SetTransParent(this, false);
            meshInfo.mesh->SetTransConstraint((Constraint)2, nullptr, false);
            meshInfo.syncFlags = 0;
            meshInfo.displayableChars = 0;
        }
    }
    return defining;
}

// Chainless: rb3-Wii walks font->NextFont() looking for a fallback font that
// defines the char. Retail's RndFont has no chain (see the top-of-file note), so
// the single authored font either defines the char or it does not.
RndFont *RndText::GetDefiningFont(unsigned short &us, RndFont *font) const {
    if (us == 10)
        return nullptr;
    if (font && font->CharDefined(us))
        return font;
    return nullptr;
}

void RndText::ResizeText(int size) {
    int len = UTF8StrLen(mText.c_str());
    if (len > size) {
        char *ptr = &mText[0];
        for (int i = 0; i < size; i++) {
            unsigned short us;
            ptr += DecodeUTF8(us, ptr);
        }
        mText.resize(ptr - &mText[0]);
    } else if (len == 0) {
        mText.resize(size);
    } else {
        mText.resize((size - len) + strlen(mText.c_str()));
    }
}

class String RndText::TextASCII() const {
    class String s;
    s.resize(UTF8StrLen(mText.c_str()) + 1);
    UTF8toASCIIs((char *)s.c_str(), s.capacity(), mText.c_str(), '*');
    return s;
}

void RndText::SetTextASCII(const char *cc) {
    class String s;
    std::vector<unsigned short> vec;
    ASCIItoWideVector(vec, cc);
    WideVectorToUTF8(vec, s);
    SetText(s.c_str());
}

float RndText::GetHorizontalAlignOffset(const Line &line, Alignment align) const {
    float ret = 0;
    if (align & 2) {
        return -(line.mWidth / 2.0f - ret);
    }
    if (!(align & 4))
        return ret;
    return ret - line.mWidth;
}

void RndText::RotateLineVerts(
    const RndText::Line &line, RndMesh::Vert *vert1, RndMesh::Vert *vert2
) {
    if (mRotateLineVerts) {
        const Transform &linexfm = line.xfm;
        Transform tf48;
        Invert(linexfm, tf48);
        for (RndMesh::Vert *it = vert1; it != vert2; ++it) {
            Vector3 v58(it->pos.x, it->pos.y, it->pos.z);
            Multiply(v58, tf48, v58);
            Multiply(v58, linexfm.m, v58);
            Multiply(v58, linexfm, v58);
            it->pos.x = v58.x;
            it->pos.y = v58.y;
            it->pos.z = v58.z;
        }
    }
}

BEGIN_HANDLERS(RndText)
    HANDLE(set_fixed_length, OnSetFixedLength)
    HANDLE(set_font, OnSetFont)
    HANDLE(set_align, OnSetAlign)
    HANDLE(set_text, OnSetText)
    HANDLE(set_size, OnSetSize)
    HANDLE(set_wrap_width, OnSetWrapWidth)
    HANDLE(set_color, OnSetColor)
    HANDLE_EXPR(get_text_size, Max<int>(mFixedLength, (int)mText.length()))
    HANDLE_EXPR(get_string_width, GetStringWidthUTF8(_msg->Str(2), NULL, false, NULL))
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

DataNode RndText::OnSetFixedLength(DataArray *da) {
    SetFixedLength(da->Int(2));
    return 0;
}

DataNode RndText::OnSetFont(DataArray *da) {
    SetFont(da->Obj<RndFont>(2));
    return 0;
}

DataNode RndText::OnSetAlign(DataArray *da) {
    SetAlignment((Alignment)da->Int(2));
    return 0;
}

DataNode RndText::OnSetText(DataArray *da) {
    SetText(da->Str(2));
    return 0;
}

DataNode RndText::OnSetWrapWidth(DataArray *da) {
    SetWrapWidth(da->Float(2));
    return 0;
}

DataNode RndText::OnSetSize(DataArray *da) {
    SetSize(da->Float(2));
    return 0;
}

DataNode RndText::OnSetColor(DataArray *da) {
    SetColor(Hmx::Color(da->Float(2), da->Float(3), da->Float(4), da->Float(5)));
    return 0;
}

// Retail's PROPSYNC makes exactly TWO SYNC_SUPERCLASS calls (rb3-Wii has 2, DC3
// has 3) — MEASURED off the retail body's tail.
BEGIN_PROPSYNCS(RndText)
    SYNC_PROP_SET(text, TextASCII(), SetTextASCII(_val.Str()));
    SYNC_PROP_MODIFY(font, mFont, UpdateText(true))
    SYNC_PROP_MODIFY(align, mAlign, UpdateText(true))
    SYNC_PROP_MODIFY(caps_mode, mCapsMode, SetText(mText.c_str()))
    SYNC_PROP_SET(color, mStyle.mTextColor.Pack(), Hmx::Color col(_val.Int());
                  col.alpha = mStyle.mTextColor.alpha;
                  SetColor(col))
    SYNC_PROP_SET(alpha, mStyle.mTextColor.alpha, Hmx::Color col(mStyle.mTextColor);
                  col.alpha = _val.Float();
                  SetColor(col))
    SYNC_PROP_SET(wrap_width, mWrapWidth, SetWrapWidth(_val.Float()))
    SYNC_PROP_SET(leading, mLeading, SetLeading(_val.Float()))
    SYNC_PROP_SET(italics, mStyle.mItalics, SetItalics(_val.Float()))
    SYNC_PROP_SET(fixed_length, mFixedLength, SetFixedLength(_val.Int()))
    SYNC_PROP_SET(size, mStyle.mSize, SetSize(_val.Float()))
    SYNC_PROP_SET(markup, mTextMarkup, SetMarkup(_val.Int()))
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndTransformable)
END_PROPSYNCS
