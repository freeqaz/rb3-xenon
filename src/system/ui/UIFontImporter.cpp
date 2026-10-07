#include "ui/UIFontImporter.h"
#include "ui/UILabelDir.h"
#include "ui/UILabel.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/File.h"
#include "os/System.h"
#include "rndobj/Font.h"
#include "rndobj/FontBase.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include "rndobj/Text.h"
#include "ui/ResourceDirPtr.h"
#include "ui/UILabelDir.h"
#include "utl/Loader.h"
#include "utl/Std.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#include "utl/UTF8.h"
#include <vector>

#ifndef HX_NATIVE
// Retail inlines ObjPtrList::Set into HandmadeFontChanged (Release, store,
// AddRef with the list as ring owner); the shared template body is out-of-line.
// A TU-local forceinline specialization reproduces that without touching the
// header every TU includes.
template <>
__forceinline void ObjPtrList<RndFont, ObjectDir>::Set(iterator it, RndFont *obj) {
    Node *n = it.mNode;
    if (n->mObject)
        n->mObject->Release(this);
    n->mObject = obj;
    if (n->mObject)
        n->mObject->AddRef(this);
}

// The ObjPtrList PropSync instantiations (retail 0x8281A910 RndFont, 0x8281AB00
// RndMat) inline insert (PoolAlloc + Link) and Set (Release, store, AddRef).
template <>
__forceinline void ObjPtrList<RndMat, ObjectDir>::Set(iterator it, RndMat *obj) {
    Node *n = it.mNode;
    if (n->mObject)
        n->mObject->Release(this);
    n->mObject = obj;
    if (n->mObject)
        n->mObject->AddRef(this);
}

template <>
__forceinline ObjPtrList<RndFont, ObjectDir>::iterator
ObjPtrList<RndFont, ObjectDir>::insert(iterator it, RndFont *obj) {
    Node *node = new Node;
    node->mObject = obj;
    Link(it, node);
    return node;
}

template <>
__forceinline ObjPtrList<RndMat, ObjectDir>::iterator
ObjPtrList<RndMat, ObjectDir>::insert(iterator it, RndMat *obj) {
    Node *node = new Node;
    node->mObject = obj;
    Link(it, node);
    return node;
}
#endif

#define HEIGHT_SD 480.0f
#define HEIGHT_HD 720.0f

float ConvertHeightOGToPctHeight(int i) { return std::fabs(-i / HEIGHT_SD); }
float ConvertHeightNGToPctHeight(int i) { return std::fabs(-i / HEIGHT_HD); }
int ConvertPctHeightToHeightNG(float f) { return -Round(f * HEIGHT_HD); }
int ConvertPctHeightToHeightOG(float f) { return -Round(f * HEIGHT_SD); }

UIFontImporter::UIFontImporter()
    : mUpperCaseAthroughZ(1), mLowerCaseAthroughZ(1), mNumbers0through9(1),
      mPunctuation(1), mUpperEuro(1), mLowerEuro(1), mPlus(""), mMinus(""),
      mFontName("Arial"), mFontPctSize(ConvertHeightNGToPctHeight(12)), mItalics(false),
      mFontQuality(0), mFontWeight(400), mPitchAndFamily(34), mFontCharset(0),
      mFontSupersample(0), mLeft(0), mRight(0), mTop(0), mBottom(0),
      mFillWithSafeWhite(false), mFontToImportFrom(this), mBitmapSavePath("ui/image/"),
      mBitMapSaveName("temp.BMP"), mGennedFonts(this), mReferenceKerning(this),
      mMatVariations(this), mDefaultMat(this), mHandmadeFont(this), mCheckNG(false),
      mSyncResource(), mLastGenWasNG(true) {
    static Symbol objects("objects");
    static Symbol default_bitmap_path("default_bitmap_path");
    DataArray *cfgArr =
        SystemConfig(objects, StaticClassName())->FindArray(default_bitmap_path, false);
    if (cfgArr) {
        mBitmapSavePath = cfgArr->Str(1);
    }
    GenerateBitmapFilename();
}

BEGIN_HANDLERS(UIFontImporter)
    HANDLE(show_font_picker, OnShowFontPicker)
    HANDLE(generate, OnGenerate)
    HANDLE(generate_og, OnGenerateOG)
    HANDLE(forget_gened_fonts, OnForgetGened)
    // Retail Handle (0x8281CAE0 / 0x8281CB5C) tests attach_to_importfont first.
    HANDLE(attach_to_importfont, OnAttachToImportFont)
    HANDLE(import_from_importfont, OnImportSettings)
    HANDLE(sync_with_resource, OnSyncWithResourceFile)
    HANDLE(get_resources_path, OnGetResourcesPath)
    HANDLE(get_bitmap_path, OnGetGennedBitmapPath)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(UIFontImporter)
    SYNC_PROP(UPPER_CASE_A_Z, mUpperCaseAthroughZ)
    SYNC_PROP(lower_case_a_z, mLowerCaseAthroughZ)
    SYNC_PROP(numbers_0_9, mNumbers0through9)
    SYNC_PROP(punctuation, mPunctuation)
    SYNC_PROP(UPPER_EURO, mUpperEuro)
    SYNC_PROP(lower_euro, mLowerEuro)
    SYNC_PROP(plus, mPlus)
    SYNC_PROP(minus, mMinus)
    SYNC_PROP(font_name, mFontName)
    SYNC_PROP_MODIFY(font_pct_size, mFontPctSize, GenerateBitmapFilename())
    // Retail writes the conversion OUT IN FULL here rather than calling the
    // ConvertPctHeightToHeight{NG,OG} helpers above: the target open-codes
    // `mFontPctSize * HEIGHT`, Round()'s `fcmpu` vs 0.0 + `fadds`/`fsubs` 0.5
    // pair, `fctiwz`, and the `neg` for the unary minus. Our helper calls
    // emitted a bare `bl ?ConvertPctHeightToHeightNG@@YAHM@Z` instead.
    // (Marking the helpers `inline` was tried and measured INERT -- at /O1
    // MSVC declines to inline a .cpp-level helper whose body has a branch.)
    SYNC_PROP_SET(
        font_point_size,
        mLastGenWasNG ? -Round(mFontPctSize * HEIGHT_HD)
                      : -Round(mFontPctSize * HEIGHT_SD),
        mFontPctSize = mLastGenWasNG ? ConvertHeightNGToPctHeight(_val.Int())
                                     : ConvertHeightOGToPctHeight(_val.Int())
    )
    SYNC_PROP_SET(
        font_pixel_size,
        mLastGenWasNG ? std::abs(-Round(mFontPctSize * HEIGHT_HD))
                      : std::abs(-Round(mFontPctSize * HEIGHT_SD)),
        // NOT ConvertHeight*ToPctHeight here. font_point_size's getter returns a
        // NEGATIVE value (`-Round(...)`), so its setter negates on the way back
        // and does use the helpers. font_pixel_size's getter is abs()'d, i.e.
        // already positive, and retail's setter correspondingly has NO negation:
        // the target sign-extends DataNode::Int() straight into the int->float
        // conversion here, while the helper form emits a `neg r11, r3` first.
        // The two setters are genuinely different expressions -- spelling both
        // the same way leaves a stray `neg` on whichever one you got wrong.
        mFontPctSize = mLastGenWasNG ? std::fabs(_val.Int() / HEIGHT_HD)
                                     : std::fabs(_val.Int() / HEIGHT_SD)
    )
    // RB3 has NO `weight` prop here -- it goes straight from
    // font_pixel_size to `bold`, whose getter is (mFontWeight > 400).
    SYNC_PROP_SET(
        bold, (mFontWeight > 400), if (_val.Int()) mFontWeight = 800;
        else mFontWeight = 400;
        GenerateBitmapFilename()
    )
    SYNC_PROP_MODIFY(italics, mItalics, GenerateBitmapFilename())
    SYNC_PROP(font_quality, (int &)mFontQuality)
    SYNC_PROP(pitch_and_family, mPitchAndFamily)
    SYNC_PROP(font_charset, mFontCharset)
    SYNC_PROP_MODIFY(font_supersample, (int &)mFontSupersample, GenerateBitmapFilename())
    SYNC_PROP(left, mLeft)
    SYNC_PROP(right, mRight)
    SYNC_PROP(top, mTop)
    SYNC_PROP(bottom, mBottom)
    SYNC_PROP(fill_with_safe_white, mFillWithSafeWhite)
    SYNC_PROP(font_to_import_from, mFontToImportFrom)
    SYNC_PROP(bitmap_save_path, mBitmapSavePath)
    SYNC_PROP(bitmap_save_name, mBitMapSaveName)
    SYNC_PROP(gened_fonts, mGennedFonts)
    SYNC_PROP(reference_kerning, mReferenceKerning)
    // NOTE: the natural spelling is SYNC_PROP_MODIFY_ALT, but that macro lives only
    // in obj/ObjMacros.h (not included here).  obj/Object.h's live SYNC_PROP_MODIFY
    // ALREADY has the ALT shape (`if (PropSync(...)) {...} else return false;`), so
    // this is already that codegen -- no change needed.
    SYNC_PROP_MODIFY(mat_variations, mMatVariations, SyncWithGennedFonts())
    SYNC_PROP_MODIFY(handmade_font, mHandmadeFont, HandmadeFontChanged())
    SYNC_PROP(resource_name, mSyncResource)
    SYNC_PROP(last_genned_ng, mLastGenWasNG)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

// Retail writes the save revision by LOADING A GLOBAL, not by storing a folded
// immediate: the target emits `lis/lwz lbl_82C793C0` where SAVE_REVS(10,4)'s
// constexpr packRevs() gives us `lis 0x4 / ori 0xa` (= 0x4000A).  The RB3 source
// carries a file-scope `int gREV` and asserts against it on load.
// Retail .data 0x82C793C0 holds 9 (rev 9, alt 0), the RB3 gREV; DC3's alt=4 /
// rev=10 is newer. Load handles nothing above rev 9.
static int gSaveRev = 9;

BEGIN_SAVES(UIFontImporter)
    bs << gSaveRev;
    bs << mLowerCaseAthroughZ;
    bs << mUpperCaseAthroughZ;
    bs << mNumbers0through9;
    bs << mPunctuation;
    bs << mUpperEuro;
    bs << mLowerEuro;
    bs << mPlus;
    bs << mMinus;
    bs << mFontName;
    bs << mFontPctSize;
    bs << mFontWeight;
    bs << mItalics;
    bs << mPitchAndFamily;
    bs << mFontQuality;
    bs << mFontCharset;
    bs << mFontSupersample;
    bs << mBitmapSavePath;
    bs << mBitMapSaveName;
    bs << mLeft;
    bs << mRight;
    bs << mTop;
    bs << mBottom;
    bs << mFillWithSafeWhite;
    bs << mGennedFonts;
    bs << mReferenceKerning;
    bs << mMatVariations;
    bs << mDefaultMat;
    bs << mHandmadeFont;
    bs << mSyncResource;
    bs << mLastGenWasNG;
END_SAVES

BEGIN_COPYS(UIFontImporter)
    CREATE_COPY(UIFontImporter)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mLowerCaseAthroughZ)
        COPY_MEMBER(mUpperCaseAthroughZ)
        COPY_MEMBER(mNumbers0through9)
        COPY_MEMBER(mPunctuation)
        COPY_MEMBER(mUpperEuro)
        COPY_MEMBER(mLowerEuro)
        COPY_MEMBER(mPlus)
        COPY_MEMBER(mMinus)
        COPY_MEMBER(mFontName)
        COPY_MEMBER(mFontPctSize)
        COPY_MEMBER(mFontWeight)
        COPY_MEMBER(mFontQuality)
        COPY_MEMBER(mPitchAndFamily)
        COPY_MEMBER(mFontQuality)
        COPY_MEMBER(mFontCharset)
        COPY_MEMBER(mBitmapSavePath)
        COPY_MEMBER(mBitMapSaveName)
        COPY_MEMBER(mFontSupersample)
        COPY_MEMBER(mItalics)
        COPY_MEMBER(mLeft)
        COPY_MEMBER(mRight)
        COPY_MEMBER(mTop)
        COPY_MEMBER(mBottom)
        COPY_MEMBER(mFillWithSafeWhite)
        COPY_MEMBER(mGennedFonts)
        COPY_MEMBER(mReferenceKerning)
        COPY_MEMBER(mMatVariations)
        COPY_MEMBER(mDefaultMat)
        COPY_MEMBER(mHandmadeFont)
        COPY_MEMBER(mSyncResource)
        COPY_MEMBER(mLastGenWasNG)
    END_COPYING_MEMBERS
END_COPYS

// RB3-360 retail rev dialect (ObjMacros shape): the packed rev is split
// into two HALFWORDS stored four bytes apart onto ONE internal-linkage align(4)
// base, and the RAW incoming BinStream is forwarded to every read and to the
// superclass Load.  DC3's Object.h BinStreamRev stack decorator additionally
// emits ??0BinStream, a ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream
// destructor that retail has none of, and dispatches each read on `&d`.
//
// Written longhand rather than by including obj/ObjMacros.h: that header also
// swaps the SYNC_PROP and HANDLE families, which are already byte-exact here.
// Retail stores NO revision for this unit.  Adjudicated on retail bytes: the
// target reloads the revision from a stack slot with `lwz r11,0x50(r31)` and
// tests it with SIGNED `cmpwi`, and the body contains no `sth`, no packed-word
// split and no static base register.  (rndobj/MeshDeform.cpp's target really
// does keep the aligned(4) aggregate, so this is a per-TU reading, not a rule.)
BEGIN_LOADS(UIFontImporter)
    int rev;
    bs >> rev;
    bs >> mLowerCaseAthroughZ;
    bs >> mUpperCaseAthroughZ;
    bs >> mNumbers0through9;
    bs >> mPunctuation;
    bs >> mUpperEuro;
    bs >> mLowerEuro;
    bs >> mPlus;
    bs >> mMinus;
    bs >> mFontName;
    if (rev <= 4) {
        int height;
        bs >> height;
        mFontPctSize = ConvertHeightNGToPctHeight(height);
    } else {
        bs >> mFontPctSize;
    }
    bs >> mFontWeight;
    bs >> mItalics;
    bs >> mPitchAndFamily;
    bs >> mFontQuality;
    bs >> mFontCharset;
    if (rev > 1) {
        bs >> mFontSupersample;
    }
    bs >> mBitmapSavePath;
    bs >> mBitMapSaveName;
    bs >> mLeft;
    bs >> mRight;
    bs >> mTop;
    bs >> mBottom;
    bs >> mFillWithSafeWhite;
    if (rev < 8) {
        bs >> mFontToImportFrom;
    }
    if (rev > 2) {
        bs >> mGennedFonts;
        bs >> mReferenceKerning;
    }
    if (rev == 3) {
        ObjPtr<RndMat> mat(this);
        bs >> mat;
    }
    if (rev > 3) {
        bs >> mMatVariations;
    }
    // Was `if (rev > 5 && rev < 10) { ObjPtr<RndMat> mat(this); bs >> mat; }` --
    // a DC3-era guard that read mDefaultMat into a DISCARDED temporary and skipped it
    // entirely at rev 10.  Retail's Save provably WRITES mDefaultMat (adding
    // `bs << mDefaultMat` is what took Save from 95.8% to 100%), and RB3
    // reads it unconditionally at `rev > 5`, so the `< 10` cutoff is a DC3
    // artifact and the discard left Save/Load asymmetric.
    if (rev > 5) {
        bs >> mDefaultMat;
    }
    if (rev > 6) {
        bs >> mHandmadeFont;
    }
    if (rev > 7) {
        bs >> mSyncResource;
    }
    if (rev > 8) {
        bs >> mLastGenWasNG;
    }
    // NO alt-rev branch here.  DC3 reads a discarded int when altRev == 1; retail
    // does not -- the eight instructions that block compiles to (lwz/srwi/cmplwi/
    // bne + the four-byte ReadEndian) appear in our obj as a pure INSERT with no
    // target counterpart, and its discarded `int x` local is what made our frame
    // 0x10 larger than retail's.  This is also why retail stores no alt-rev word
    // for this unit: nothing in the body ever reads one.
END_LOADS

// RB3 property list.  The DC3-era version set "weight", "drop_shadow" and
// "drop_shadow_opacity"; retail band.exe contains ZERO "drop_shadow" /
// "drop_shadow_opacity" strings but DOES contain "bold" / "imported_font" /
// "font_name" / "font_size" / "italics" (positive controls all fire), so the
// RB3 list is the one below.
// NOTE ON CONTROL FLOW: no hoisted `bool has_import_font` flag; retail
// did NOT -- that form costs an extra local (measured: frame delta +0x10 structural,
// with inserted li/li/clrlwi. flag machinery).  Retail uses the direct condition, so
// only the PROPERTY LIST is RB3's, not a flag-based control flow.
void UIFontImporter::ImportSettingsFromFont(RndFont *font) {
    if (font && font->Type() == Symbol("imported_font")) {
        SetProperty("font_name", font->Property("font_name")->Str());
        SetProperty(
            "font_size", ConvertHeightNGToPctHeight(font->Property("font_size")->Int())
        );
        SetProperty("bold", font->Property("bold")->Int());
        SetProperty("italics", font->Property("italics")->Int());
        SetProperty("left", font->Property("left")->Int());
        SetProperty("right", font->Property("right")->Int());
        SetProperty("top", font->Property("top")->Int());
        SetProperty("bottom", font->Property("bottom")->Int());
    } else
        MILO_NOTIFY(
            "Can't import settings from Font because it doesnt have import_font type"
        );
}

int UIFontImporter::GetMatVariationIdx(Symbol s) const {
    int size = NumMatVariations();
    for (int ret = 0; ret < size; ret++) {
        Symbol name = GetMatVariationName(ret);
        if (name == s) {
            return ret;
        }
    }
    return -1;
}

void UIFontImporter::AttachImporterToFont(RndFont *font) {
    if (font) {
        if (font->Dir() != Dir())
            MILO_NOTIFY(
                "Cannot attach font %s to font resource %s because its in a different dir.  Notify a programmer!"
            );
        else {
            // Retail 0x8281A798 also makes the font's own material the default mat.
            mGennedFonts.clear();
            mMatVariations.clear();
            mDefaultMat = font->GetMat();
            mGennedFonts.push_back(font);
            mReferenceKerning = font;
            ImportSettingsFromFont(font);
        }
    }
}

void UIFontImporter::GenerateBitmapFilename() {
    const char *mult = "";
    if (mFontSupersample == kFontSuperSample_2x)
        mult = "2x";
    else if (mFontSupersample == kFontSuperSample_4x)
        mult = "4x";

    class String s28(MakeString("%.2f", mFontPctSize * 100.0f));
    s28.ReplaceAll('.', '_');
    const char *b = (mFontWeight > 400) ? "B" : "";
    const char *i = mItalics ? "I" : "";
    mBitMapSaveName =
        MakeString("%s(%s)%s%s%s.BMP", mFontName.c_str(), s28.c_str(), i, b, mult);
    mBitMapSaveName.ReplaceAll(' ', '_');
}

// RB3 has a single RndFont type,
// so there is no Font3d arm here -- the RndFontBase/RndFont3d split is a DC3-era
// addition.  Retail band.exe contains zero "RndFont3d" strings.
RndFont *UIFontImporter::FindFontForMat(RndMat *mat) const {
    if (mat) {
        static Symbol Font("Font");
        // NOTE: the natural form is FOREACH_OBJREF (a REVERSE walk of a
        // std::vector<ObjRef*>).  This tree's Hmx::Object::Refs() is a DC3-era
        // intrusive next/prev ring returning `const ObjRef &`, so neither the vector
        // type nor rbegin()/rend() exists here.  Keeping the forward FOREACH walk;
        // the container divergence is an Object-level issue, not a UIFontImporter one.
        FOREACH (it, mat->Refs()) {
#ifdef HX_NATIVE
            Hmx::Object *owner = it->RefOwner();
#else
            // X360: mRefs entries are the ring-refs; each carries RefOwner().
            // Calling ObjRef::RefOwner() directly inlines to a constant nullptr
            // here (OBJREF_VIRTUAL is empty off HX_NATIVE), which lets the
            // compiler delete this whole loop body.
            Hmx::Object *owner = RefPtrOf(it)->RefOwner();
#endif
            if (owner) {
#ifdef HX_NATIVE
                // Native-only: identify RndFont via Itanium-ABI typeinfo rather than
                // the virtual ClassName(), because some owners have broken vtables
                // (.bss zeros) when their GCC key function is undefined.  Retained
                // from the previous version; the Font3d arm is gone because RB3 has
                // no RndFont3d.
                void **vptr = *(void ***)owner;
                if (!vptr)
                    continue;
                void *typeinfo = vptr[-1];
                if (!typeinfo)
                    continue;
                const char *tname = *(const char **)((char *)typeinfo + sizeof(void *));
                if (!tname)
                    continue;
                if (strcmp(tname, "7RndFont") == 0) {
                    return static_cast<RndFont *>(owner);
                }
#else
                if (owner->ClassName() == Font) {
                    return dynamic_cast<RndFont *>(owner);
                }
#endif
            }
        }
    }
    return nullptr;
}

void UIFontImporter::OnSetCharsetUTF8(String const &s) {
    mLowerEuro = false;
    mUpperEuro = false;
    mPunctuation = false;
    mNumbers0through9 = false;
    mLowerCaseAthroughZ = false;
    mUpperCaseAthroughZ = false;
    mMinus = "";
    mPlus = s;
}

// RB3: plain reverse ObjRef walk, no mStyle.mFont filter.
RndText *UIFontImporter::FindTextForFont(RndFont *font) const {
    if (font) {
        static Symbol Text("Text");
        // See the Refs()-container note in FindFontForMat above.
        FOREACH (it, font->Refs()) {
#ifdef HX_NATIVE
            Hmx::Object *owner = it->RefOwner();
#else
            // X360: mRefs entries are the ring-refs; each carries RefOwner().
            // See the note in FindFontForMat above.
            Hmx::Object *owner = RefPtrOf(it)->RefOwner();
#endif
            if (owner) {
#ifdef HX_NATIVE
                // RB3's 2010-era milos serialize text objects under the bare class
                // name "Text", but this decomp's class carries the "Rnd" prefix
                // (OBJ_CLASSNAME(RndText) => ClassName() == "RndText"), so the
                // matched `== Text` compare never fires for a natively-loaded
                // RndText.  Accept the prefixed name too.
                if (owner->ClassName() == Text
                    || owner->ClassName() == RndText::StaticClassName()) {
                    return dynamic_cast<RndText *>(owner);
                }
#else
                if (owner->ClassName() == Text) {
                    return dynamic_cast<RndText *>(owner);
                }
#endif
            }
        }
    }
    return nullptr;
}

String UIFontImporter::GetASCIIPlusChars() {
    static String plusChars;
    plusChars = mPlus;
    return plusChars;
}

String UIFontImporter::GetASCIIMinusChars() {
    static String minusChars;
    minusChars = mMinus;
    return minusChars;
}

Symbol UIFontImporter::GetMatVariationName(unsigned int ui) const {
    if (ui >= mMatVariations.size()) {
        return Symbol();
    } else {
        auto it = mMatVariations.begin();
        for (int i = 0; i < ui; i++) {
            ++it;
        }
        // retail 0x82818D98: strip the extension with rfind/substr
        String name((*it)->Name());
        if (name.rfind(".") != String::npos) {
            name = name.substr(0, name.rfind("."));
        }
        return Symbol(name.c_str());
    }
}

const char *UIFontImporter::GetMatVariationName(RndFont *font) const {
    if (font && font->Mat()) {
        RndMat *mat = font->Mat();
        if (mGennedFonts.size() > 0) {
            RndFont *front =
                mGennedFonts.size() != 0 ? *mGennedFonts.begin() : nullptr;
            if (mat == front->Mat()) {
                return "";
            }
        }
        if (mMatVariations.size() != 0) {
            FOREACH (it, mMatVariations) {
                if (*it == mat) {
                    return FileGetBase(mat->Name());
                }
            }
            MILO_NOTIFY("%s not found in resource dir %s", PathName(font), PathName(this));
        }
    }
    return "";
}

// Retail 0x82817E90; its only callers are the two arms of
// FontImporterSyncObjects below.
String UIFontImporter::GetBaseName() const {
    if (HandMadeFontExists()) {
        String str(mHandmadeFont->Name());
        if (str.find(".") != String::npos) {
            str = str.substr(0, str.rfind("."));
        }
        return str;
    } else {
        return mBitMapSaveName.substr(0, mBitMapSaveName.rfind("."));
    }
}

// Retail 0x8281A1F8, called from UILabelDir::SyncObjects (0x8280FD24). Converts a
// pre-material-variation resource: the old variation list is emptied and each
// genned font's material is renamed (the first one becomes the default mat).
void UIFontImporter::FontImporterSyncObjects() {
    if (!mDefaultMat && mMatVariations.size() > 0 && mGennedFonts.size() > 0) {
        ObjPtrList<RndMat>::iterator mit;
        ObjPtrList<RndFont>::iterator it;
        for (mit = mMatVariations.begin(); mit != mMatVariations.end();) {
            RndMat *old = *mit;
            mit = mMatVariations.erase(mit);
            delete old;
        }
        int idx = 0;
        for (it = mGennedFonts.begin(); it != mGennedFonts.end(); it++, idx++) {
            RndFont *font = *it;
            RndMat *mat = font->GetMat();
            if (idx == 0) {
                String name = GetBaseName();
                String matname = name + ".mat";
                mat->SetName(matname.c_str(), Dir());
                mDefaultMat = mat;
                String fontname = name + ".font";
                font->SetName(fontname.c_str(), Dir());
                RndText *text = FindTextForFont(font);
                if (text) {
                    String textname = name + ".txt";
                    text->SetName(textname.c_str(), Dir());
                    String textstr(text->RawText().c_str());
                    if (textstr.find("_default") != String::npos) {
                        textstr = textstr.substr(0, textstr.find("_default"));
                        text->SetText(textstr.c_str());
                    }
                }
            } else {
                String name = GetBaseName();
                String matname = mat->Name();
                int pos = matname.find(name.c_str());
                if (pos == 0) {
                    int matLen = matname.length();
                    int nameLen = name.length();
                    matname = matname.substr(name.length() + 1, matLen - nameLen - 1);
                }
                mat->SetName(matname.c_str(), Dir());
                mMatVariations.push_back(mat);
            }
        }
        MILO_WARN(MakeString(
            "Upgraded font resource to new material variation setup.  Please resave %s",
            Dir()->GetPathName()
        ));
    }
}

// Retail 0x8281A8D0: TextObj's genned-font arm (UILabelDir::TextObj tail-calls it).
RndText *UIFontImporter::GetGennedText(Symbol s) const {
    return FindTextForFont(GetGennedFont(s));
}

RndFont *UIFontImporter::GetGennedFont(Symbol s) const {
    if (s.Null()) {
        return *mGennedFonts.begin();
    } else {
        int idx_raw = GetMatVariationIdx(s);
        if (idx_raw == -1) {
            return nullptr;
        } else {
            unsigned int idx = idx_raw;
            RndMat *mat;
            if (idx >= mMatVariations.size()) {
                mat = nullptr;
            } else {
                auto it = mMatVariations.begin();
                for (unsigned int i = 0; i != idx; i++) {
                    ++it;
                }
                mat = *it;
            }
            return FindFontForMat(mat);
        }
    }
}

void UIFontImporter::SyncWithGennedFonts() {
    for (ObjPtrList<RndFont>::iterator it = mGennedFonts.begin(); it != mGennedFonts.end();) {
        RndFont *font = *it;
        bool matfound = false;
        for (ObjPtrList<RndMat>::iterator mit = mMatVariations.begin();
             mit != mMatVariations.end();
             ++mit) {
            if (font->Mat() == *mit)
                matfound = true;
        }
        if (font->Mat() == mDefaultMat)
            matfound = true;
        if (!matfound) {
            RndText *text = FindTextForFont(font);
            it = mGennedFonts.erase(it);
            delete font;
            if (text)
                delete text;
        } else
            it++;
    }
}

void UIFontImporter::HandmadeFontChanged() {
    if (mHandmadeFont) {
        if (mGennedFonts.size() > 0) {
            RndFont *frontfont = *mGennedFonts.begin();
            if (frontfont != mHandmadeFont) {
                RndText *text = FindTextForFont(frontfont);
                delete frontfont;
                delete text;
            }
            mGennedFonts.Set(mGennedFonts.begin(), mHandmadeFont);
            for (ObjPtrList<RndFont>::iterator it = ++mGennedFonts.begin();
                 it != mGennedFonts.end();
                 it++) {
                if (*it == mHandmadeFont) {
                    mGennedFonts.erase(it);
                    break;
                }
            }
        } else {
            mGennedFonts.push_back(mHandmadeFont);
        }
        mReferenceKerning = mHandmadeFont;
        mLowerEuro = false;
        mUpperEuro = false;
        mPunctuation = false;
        mNumbers0through9 = false;
        mLowerCaseAthroughZ = false;
        mUpperCaseAthroughZ = false;
        mMinus = "";
        std::vector<unsigned short> thechars(mHandmadeFont->Chars());
        mPlus = WideVectorToASCII(thechars);
    }
}

const char *UIFontImporter::GetResourcesPath() {
    static Symbol objects("objects");
    static Symbol resources_path("resources_path");
    DataArray *arr =
        SystemConfig(objects, UILabel::StaticClassName())->FindArray(resources_path, false);
    if (!arr)
        return 0;
    else {
        const char *str = arr->Str(1);
        if (*str == '\0')
            return 0;
        else {
            return FileMakePath(MakeString("%s/%s", FileRoot(), "ui/"), str);
        }
    }
}

DataNode UIFontImporter::OnGetResourcesPath(DataArray *da) {
    const char *path = GetResourcesPath();
    if (path)
        return DataNode(FileRelativePath(FileRoot(), path));
    else
        return DataNode("");
}

DataNode UIFontImporter::OnGetGennedBitmapPath(DataArray *da) {
    if ((unsigned int)mGennedFonts.size() > 0) {
        RndFont *font = static_cast<RndFont *>(*mGennedFonts.begin());
        // Retail reads the font's single material (RndFont +0x30), not Mat(0).
        if (font && font->GetMat() && font->GetMat()->GetDiffuseTex()) {
            RndTex *tex = font->GetMat()->GetDiffuseTex();
            if (tex) {
                return tex->File().c_str();
            }
        }
    }
    return "";
}

DataNode UIFontImporter::OnImportSettings(DataArray *da) {
    ImportSettingsFromFont(mFontToImportFrom);
    return 0;
}

DataNode UIFontImporter::OnForgetGened(DataArray *) {
    mGennedFonts.clear();
    return 0;
}

// Retail calls ImportSettingsFromFont DIRECTLY here, not the AttachImporterToFont
// wrapper: the charged `bl` at index 10 of this row
// names ?ImportSettingsFromFont@UIFontImporter@@QAAXPAVRndFont@@@Z, and our body of
// THAT name is byte-exact against retail 0x82818840 (1036 B, fuzzy 100).  Retail
// bytes decide.  AttachImporterToFont is left defined (now uncalled) so
// the native link is unaffected.  Lane W16-EC.
// Retail 0x8281B018 (the attach_to_importfont handler) attaches, it does not
// only import settings.
DataNode UIFontImporter::OnAttachToImportFont(DataArray *) {
    AttachImporterToFont(mFontToImportFrom);
    return 0;
}

DataNode UIFontImporter::OnGenerate(DataArray *a) { return 0; }

DataNode UIFontImporter::OnGenerate3d(DataArray *a) {
    if (a->Size() >= 3) {
        a->Int(2);
    }
    return 0;
}

DataNode UIFontImporter::OnGenerateOG(DataArray *a) { return 0; }

DataNode UIFontImporter::OnShowFontPicker(DataArray *) { return 0; }

DataNode UIFontImporter::OnSyncWithResourceFile(DataArray *a) {
    if (!mSyncResource.empty()) {
        // Retail 0x8281C5B0: "%s/%s.milo" under GetResourcesPath(), loaded
        // through an ObjDirPtr built from null, then PostLoad(0).
        const char *milopath = MakeString("%s/%s.milo", GetResourcesPath(), mSyncResource);
        {
            ObjDirPtr<UILabelDir> labelDir(0);
            labelDir.LoadFile(FilePath(FileRoot(), milopath), false, true, kLoadFront, false);
            labelDir.PostLoad(0);
            if (labelDir.IsLoaded()) {
                mLowerCaseAthroughZ = labelDir->mLowerCaseAthroughZ;
                mUpperCaseAthroughZ = labelDir->mUpperCaseAthroughZ;
                mNumbers0through9 = labelDir->mNumbers0through9;
                mPunctuation = labelDir->mPunctuation;
                mUpperEuro = labelDir->mUpperEuro;
                mLowerEuro = labelDir->mLowerEuro;
                mPlus = labelDir->mPlus;
                mMinus = labelDir->mMinus;
                mFontName = labelDir->mFontName;
                mFontPctSize = labelDir->mFontPctSize;
                mFontWeight = labelDir->mFontWeight;
                mFontQuality = labelDir->mFontQuality;
                mPitchAndFamily = labelDir->mPitchAndFamily;
                mFontQuality = labelDir->mFontQuality;
                mFontCharset = labelDir->mFontCharset;
                mBitmapSavePath = labelDir->mBitmapSavePath;
                mBitMapSaveName = labelDir->mBitMapSaveName;
                mFontSupersample = labelDir->mFontSupersample;
                mItalics = labelDir->mItalics;
                mLeft = labelDir->mLeft;
                mRight = labelDir->mRight;
                mTop = labelDir->mTop;
                mBottom = labelDir->mBottom;
                mFillWithSafeWhite = labelDir->mFillWithSafeWhite;
                if (mReferenceKerning && labelDir->mReferenceKerning) {
                    std::vector<RndFont::KernInfo> kernInfo;
                    labelDir->mReferenceKerning->GetKerning(kernInfo);
                    mReferenceKerning->SetKerning(kernInfo);
                    mReferenceKerning->SetBaseKerning(
                        labelDir->mReferenceKerning->BaseKerning()
                    );
                }
            }
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// lane-AE batch-3 (sw3) force-emit: retail placed the COMDAT for
//   ?LoadFile@?$ObjDirPtr@VUIListDir@@@@QAAXABVFilePath@@_N1W4LoaderPos@@1@Z
// inside the .text span pinned to default/UIFontImporter. ObjDirPtr<C>::LoadFile
// is defined inline in obj/Dir.h, so it is only emitted in a TU that odr-uses
// it -- nothing in this TU did, so objdiff had no base symbol to pair with.
// Explicit instantiation forces the COMDAT without adding a call site.
#include "ui/UIListDir.h"
template void
ObjDirPtr<UIListDir>::LoadFile(const FilePath &, bool, bool, LoaderPos, bool);
