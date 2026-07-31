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
    SYNC_PROP_SET(
        font_point_size,
        mLastGenWasNG ? ConvertPctHeightToHeightNG(mFontPctSize)
                      : ConvertPctHeightToHeightOG(mFontPctSize),
        mFontPctSize = mLastGenWasNG ? ConvertHeightNGToPctHeight(_val.Int())
                                     : ConvertHeightOGToPctHeight(_val.Int())
    )
    SYNC_PROP_SET(
        font_pixel_size,
        mLastGenWasNG ? std::abs(ConvertPctHeightToHeightNG(mFontPctSize))
                      : std::abs(ConvertPctHeightToHeightOG(mFontPctSize)),
        mFontPctSize = mLastGenWasNG ? ConvertHeightNGToPctHeight(_val.Int())
                                     : ConvertHeightOGToPctHeight(_val.Int())
    )
    // rb3-Wii oracle: RB3 has NO `weight` prop here -- it goes straight from
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
    // NOTE: the oracle spells these SYNC_PROP_MODIFY_ALT, but that macro lives only
    // in obj/ObjMacros.h (not included here).  obj/Object.h's live SYNC_PROP_MODIFY
    // ALREADY has the ALT shape (`if (PropSync(...)) {...} else return false;`), so
    // this is already the oracle's codegen -- no change needed.
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
// constexpr packRevs() gives us `lis 0x4 / ori 0xa` (= 0x4000A).  The rb3-Wii oracle
// agrees -- it carries a file-scope `int gREV` and asserts against it on load.
static int gSaveRev = (4 << 16) | 10; // packRevs(alt=4, rev=10)

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
        COPY_MEMBER(mItalics)
        COPY_MEMBER(mFontQuality)
        COPY_MEMBER(mPitchAndFamily)
        COPY_MEMBER(mFontQuality)
        COPY_MEMBER(mFontCharset)
        COPY_MEMBER(mBitmapSavePath)
        COPY_MEMBER(mBitMapSaveName)
        COPY_MEMBER(mFontSupersample)
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

INIT_REVS(10, 4)

BEGIN_LOADS(UIFontImporter)
    LOAD_REVS(bs)
    ASSERT_REVS(10, 4)
    d >> mLowerCaseAthroughZ;
    d >> mUpperCaseAthroughZ;
    d >> mNumbers0through9;
    d >> mPunctuation;
    d >> mUpperEuro;
    d >> mLowerEuro;
    d >> mPlus;
    d >> mMinus;
    d >> mFontName;
    if (d.rev <= 4) {
        int height;
        d >> height;
        mFontPctSize = ConvertHeightNGToPctHeight(height);
    } else {
        d >> mFontPctSize;
    }
    d >> mFontWeight;
    d >> mItalics;
    d >> mPitchAndFamily;
    d >> mFontQuality;
    d >> mFontCharset;
    if (d.rev > 1) {
        d >> mFontSupersample;
    }
    d >> mBitmapSavePath;
    d >> mBitMapSaveName;
    d >> mLeft;
    d >> mRight;
    d >> mTop;
    d >> mBottom;
    d >> mFillWithSafeWhite;
    if (d.rev < 8) {
        d >> mFontToImportFrom;
    }
    if (d.rev > 2) {
        d >> mGennedFonts;
        d >> mReferenceKerning;
    }
    if (d.rev == 3) {
        ObjPtr<RndMat> mat(this);
        d >> mat;
    }
    if (d.rev > 3) {
        d >> mMatVariations;
    }
    // Was `if (d.rev > 5 && d.rev < 10) { ObjPtr<RndMat> mat(this); d >> mat; }` --
    // a DC3-era guard that read mDefaultMat into a DISCARDED temporary and skipped it
    // entirely at rev 10.  Retail's Save provably WRITES mDefaultMat (adding
    // `bs << mDefaultMat` is what took Save from 95.8% to 100%), and the rb3-Wii
    // oracle reads it unconditionally at `rev > 5`, so the `< 10` cutoff is a DC3
    // artifact and the discard left Save/Load asymmetric.
    if (d.rev > 5) {
        d >> mDefaultMat;
    }
    if (d.rev > 6) {
        d >> mHandmadeFont;
    }
    if (d.rev > 7) {
        d >> mSyncResource;
    }
    if (d.rev > 8) {
        d >> mLastGenWasNG;
    }
    if (d.altRev == 1) {
        int x;
        d >> x;
    }
END_LOADS

// rb3-Wii oracle body.  The DC3-era version set "weight", "drop_shadow" and
// "drop_shadow_opacity"; retail band.exe contains ZERO "drop_shadow" /
// "drop_shadow_opacity" strings but DOES contain "bold" / "imported_font" /
// "font_name" / "font_size" / "italics" (positive controls all fire), so the
// oracle's property list is the RB3 one.
// NOTE ON CONTROL FLOW: the oracle hoists a `bool has_import_font` flag, but retail
// did NOT -- that form costs an extra local (measured: frame delta +0x10 structural,
// with inserted li/li/clrlwi. flag machinery).  Retail uses the direct condition, so
// only the PROPERTY LIST is taken from the oracle, not its control flow.
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
            mGennedFonts.clear();
            mMatVariations.clear();
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

// rb3-Wii oracle (src/system/ui/UIFontImporter.cpp): RB3 has a single RndFont type,
// so there is no Font3d arm here -- the RndFontBase/RndFont3d split is a DC3-era
// addition.  Retail band.exe contains zero "RndFont3d" strings.
RndFont *UIFontImporter::FindFontForMat(RndMat *mat) const {
    if (mat) {
        static Symbol Font("Font");
        // NOTE: the oracle uses FOREACH_OBJREF (a REVERSE walk of a
        // std::vector<ObjRef*>).  This tree's Hmx::Object::Refs() is a DC3-era
        // intrusive next/prev ring returning `const ObjRef &`, so neither the vector
        // type nor rbegin()/rend() exists here.  Keeping the forward FOREACH walk;
        // the container divergence is an Object-level issue, not a UIFontImporter one.
        FOREACH (it, mat->Refs()) {
#ifdef HX_NATIVE
            Hmx::Object *owner = it->RefOwner();
#else
            // X360: ring entries are pool nodes; the ring-ref carries RefOwner().
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

// rb3-Wii oracle: plain reverse ObjRef walk, no mStyle.mFont filter.
RndText *UIFontImporter::FindTextForFont(RndFont *font) const {
    if (font) {
        static Symbol Text("Text");
        // See the Refs()-container note in FindFontForMat above.
        FOREACH (it, font->Refs()) {
#ifdef HX_NATIVE
            Hmx::Object *owner = it->RefOwner();
#else
            // X360: ring entries are pool nodes; the ring-ref carries RefOwner().
            // See the note in FindFontForMat above.
            Hmx::Object *owner = RefPtrOf(it)->RefOwner();
#endif
            if (owner) {
#ifdef HX_NATIVE
                // RB3's 2010-era milos serialize text objects under the bare class
                // name "Text", but this decomp's class carries the "Rnd" prefix
                // (OBJ_CLASSNAME(RndText) => ClassName() == "RndText"), so the
                // matched `== Text` compare never fires for a natively-loaded
                // RndText.  Accept the prefixed name too. (Oracle does the same.)
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
        return FileGetBase((*it)->Name());
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
    auto it = mGennedFonts.begin();
    for (int i = 0; it != mGennedFonts.end(); i++) {
        RndFont *cur = *it;
        bool b4 = false;
        if (i == 0) {
            b4 = true;
        } else {
            FOREACH (mit, mMatVariations) {
                if (cur->Mat() == *mit) {
                    b4 = true;
                }
            }
        }
        if (!b4) {
            cur->Mat();
            RndText *text = FindTextForFont(cur);
            it = mGennedFonts.erase(it);
            delete cur;
            if (text) {
                delete text;
            }
        } else {
            ++it;
        }
    }
}

void UIFontImporter::HandmadeFontChanged() {
    if (mHandmadeFont) {
        if (mGennedFonts.size() > 0) {
            RndFont *font = *mGennedFonts.begin();
            if (font != mHandmadeFont) {
                RndText *text = FindTextForFont(font);
                delete font;
                delete text;
            }
            // <?>
            RndFont *next = *mGennedFonts.begin();
            next = mHandmadeFont;
            // </?>
            FOREACH (it, mGennedFonts) {
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
    if (mHandmadeFont) {
        RndFont3d::StaticClassName();
        mHandmadeFont->ClassName();
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
        if (font && font->Mat(0) && font->Mat(0)->GetDiffuseTex()) {
            RndTex *tex = font->Mat(0)->GetDiffuseTex();
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
        FilePath path;
        if (ResourceDirBase::MakeResourcePath(
                path, "UILabel", "UILabelDir", mSyncResource.c_str()
            )) {
            ObjDirPtr<UILabelDir> labelDir;
            labelDir.LoadFile(path, false, true, kLoadFront, false);
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
                mItalics = labelDir->mItalics;
                mFontQuality = labelDir->mFontQuality;
                mPitchAndFamily = labelDir->mPitchAndFamily;
                mFontQuality = labelDir->mFontQuality;
                mFontCharset = labelDir->mFontCharset;
                mBitmapSavePath = labelDir->mBitmapSavePath;
                mBitMapSaveName = labelDir->mBitMapSaveName;
                mFontSupersample = labelDir->mFontSupersample;
                mLeft = labelDir->mLeft;
                mRight = labelDir->mRight;
                mTop = labelDir->mTop;
                mBottom = labelDir->mBottom;
                mFillWithSafeWhite = labelDir->mFillWithSafeWhite;
                if (mReferenceKerning && labelDir->mReferenceKerning) {
                    std::vector<RndFontBase::KernInfo> kernInfo;
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
