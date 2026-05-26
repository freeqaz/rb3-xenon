#pragma once
#include "math/Color.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Draw.h"
#include "rndobj/FontBase.h"
#include "rndobj/Font.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include "utl/StlAlloc.h"

#ifndef HX_NATIVE
using stlpmtx_std::StlNodeAlloc;
// Convenience: on Xbox, stlpmtx_std::vector with StlNodeAlloc; on native, std::vector
#define HX_VECTOR(T) stlpmtx_std::vector<T, stlpmtx_std::StlNodeAlloc<T> >
#else
#define HX_VECTOR(T) std::vector<T>
#endif

class RndCam;

class TextHolder {
public:
    TextHolder() {}
    virtual ~TextHolder() {}
    virtual void SetTextToken(Symbol) = 0;
    virtual void SetInt(int, bool) = 0;
};

class RndText : public virtual RndDrawable, public virtual RndTransformable {
public:
    enum Alignment {
        kCenter = 0x2,
        kTopLeft = 0x11,
        kTopCenter = 0x12,
        kTopRight = 0x14,
        kMiddleLeft = 0x21,
        kMiddleCenter = 0x22,
        kMiddleRight = 0x24,
        kBottomLeft = 0x41,
        kBottomCenter = 0x42,
        kBottomRight = 0x44
    };

    enum CapsMode {
        /** "Leave the text as is" */
        kCapsModeNone = 0,
        /** "Force text to all lower case" */
        kForceLower = 1,
        /** "Force text to all upper case" */
        kForceUpper = 2,
    };

    enum FitType {
        /** "Performs normal line wrapping if [width] is set" */
        kFitWrap = 0,
        /** "Shrinks the text until it fits within [width] and [height].
            Note that this is a very expensive process, super slow,
            and so should never be used on dynamically changing text when in game" */
        kFitJust = 1,
        /** "Constrains the text to one line of [width] with ellipses" */
        kFitEllipsis = 2,
        /** "Stretch text to fit width" */
        kFitStretch = 3,
        /** "Right-to-left scroll - Reset to beginning after end scrolls off" */
        kFitScrollMarqueeReset = 4,
        /** "Reverse scroll direction whenever string end or beginning is reached" */
        kFitScrollPingPong = 5,
        /** "Continuous right-to-left scrolling. String start follows sring end" */
        kFitScrollMarqueeWrap = 6,
        /** "Continuous right-to-left scroll with wrapping and not care about string size.
            '\n' will be replaced with indentation." */
        kFitScrollMarqueeWrapAlways = 7
    };

    class Style {
    public:
        Style(Hmx::Object *owner);
        Style(const Style &s);
        Style &operator=(const Style &s) {
            mFont = s.mFont;
            mBlacklight = s.mBlacklight;
            memcpy(this, &s, 0x34);
            return *this;
        }
        float GetAlpha() const { return mFontColor.alpha; }
        void SetAlpha(float alpha) { mFontColor.alpha = alpha; }

        // perhaps the memory from 0x0 to 0x34 is another struct
        /** "Size of the text" */
        float mSize; // 0x0
        /** "Color of the text, put into mesh verts.
            Modifed by <color=r,g,b,a> markup .
            This will only work if the font mat has [prelit] set true
            and [use_environment] set false" */
        Hmx::Color mTextColor; // 0x4
        /** "If true, and if there's a font,
            you can change color and alpha during the draw" */
        bool mFontColorOverride; // 0x14
        /** "Color of the font during draw, can be changed dynamically" */
        Hmx::Color mFontColor; // 0x18
        /** "Defines the slant of the text, changed by <it> tag".
            Ranges from -5 to 5. */
        float mItalics; // 0x28
        /** "Extra kerning for the text" */
        float mKerning; // 0x2c
        /** "vertical offset as fraction of size" */
        float mZOffset; // 0x30
        /** "Font to use for this style" */
        ObjPtr<RndFontBase> mFont; // 0x34
        /** "draw in blacklight pass?" */
        bool mBlacklight; // 0x48
    };

    class StyleState {
    public:
        StyleState(RndText *text, float size);

        // First 0x34 bytes: copied from Style via memcpy
        float mSize; // 0x0 - Style::mSize, then scaled by size param
        Hmx::Color mTextColor; // 0x4
        bool mFontColorOverride; // 0x14
        Hmx::Color mFontColor; // 0x18
        float mItalics; // 0x28
        float mKerning; // 0x2c
        float mZOffset; // 0x30
        // End of memcpy'd Style data
        Style *mStyle; // 0x34
        int mFontMapIdx; // 0x38
        float mBaseSize; // 0x3c
        bool mActive; // 0x40
        bool brk; // 0x41
    };

    class BlacklightPacket {
    public:
#ifdef HX_NATIVE
        RndMesh *mMesh;
        Hmx::Color mSavedColor;
        float mSize;
        int mSyncFlags;
        RndCam *mCam;
#else
        int unk[8];
#endif
    };

    class FontMapBase {
    public:
        virtual ~FontMapBase() {}
        virtual Symbol ClassName() const = 0;
        virtual void SetFont(RndFontBase *) = 0;
        virtual RndFontBase *Font() const = 0;
        virtual int NumMeshes() const = 0;
        virtual RndMesh *Mesh(int) const = 0;
        virtual int NumMaterials() const = 0;
        virtual RndMat *Material(int) const = 0;
        virtual void ResetDisplayableChars() = 0;
        virtual void IncrementDisplayableChars(unsigned short) = 0;
        virtual void AllocateMeshes(RndText *, int) = 0;
        virtual void CleanupSyncMeshes() = 0;
        virtual void SetupCharacter(
            unsigned short,
            float &,
            float,
            const StyleState &,
            unsigned short,
            float,
            FitType,
            float
        ) = 0;
        virtual bool SupportsScrolling() const = 0;
        virtual void SetupScrolling() = 0;
        virtual void UpdateScrolling(float) = 0;

        MEM_OVERLOAD(FontMapBase, 0xD7);

        bool mBlacklight; // 0x4
    };

    // size 0x18
    class FontMap : public FontMapBase {
    public:
        // size 0x10
        class Page {
        public:
            Page() : mesh(nullptr) {}
            ~Page() {
                if (mesh) {
                    RELEASE(mesh);
                }
            }
            MEM_OVERLOAD(Page, 0x115);

            RndMesh *mesh; // 0x0
            int displayableChars; // 0x4
            RndMesh::Vert *mVertStart; // 0x8
            int mSyncFlags; // 0xc
        };

        virtual ~FontMap();
        virtual Symbol ClassName() const { return StaticClassName(); }
        virtual void SetFont(RndFontBase *);
        virtual RndFontBase *Font() const { return mFont; }
        virtual int NumMeshes() const { return mPages.size(); }
        virtual RndMesh *Mesh(int idx) const { return mPages[idx]->mesh; }
        virtual int NumMaterials() const { return mPages.size(); }
        virtual RndMat *Material(int idx) const { return mFont->Mat(idx); }
        virtual void ResetDisplayableChars();
        virtual void IncrementDisplayableChars(unsigned short);
        virtual void AllocateMeshes(RndText *, int);
        virtual void CleanupSyncMeshes();
        virtual void SetupCharacter(
            unsigned short,
            float &,
            float,
            const StyleState &,
            unsigned short,
            float,
            FitType,
            float
        );
        virtual bool SupportsScrolling() const { return true; }
        virtual void SetupScrolling();
        virtual void UpdateScrolling(float);

        static Symbol StaticClassName() {
            static Symbol name("FontMap");
            return name;
        }

        RndFont *mFont; // 0x8
        std::vector<Page *> mPages; // 0xc
    };

    // size 0x20
    class FontMap3d : public FontMapBase {
    public:
        virtual ~FontMap3d();
        virtual Symbol ClassName() const { return StaticClassName(); }
        virtual void SetFont(RndFontBase *);
        virtual RndFontBase *Font() const { return mFont; }
        virtual int NumMeshes() const { return mMeshes.size(); }
        virtual RndMesh *Mesh(int idx) const { return mMeshes[idx]; }
        virtual int NumMaterials() const { return mFont && mFont->Mat(); }
        virtual RndMat *Material(int i) const {
            MILO_ASSERT(i==0, 0x150);
            return mFont->Mat();
        }
        virtual void ResetDisplayableChars() { mDisplayableChars = 0; }
        virtual void IncrementDisplayableChars(unsigned short);
        virtual void AllocateMeshes(RndText *, int);
        virtual void CleanupSyncMeshes();
        virtual void SetupCharacter(
            unsigned short,
            float &,
            float,
            const StyleState &,
            unsigned short,
            float,
            FitType,
            float
        );
        virtual bool SupportsScrolling() const { return false; }
        virtual void SetupScrolling() {}
        virtual void UpdateScrolling(float) {}

        static Symbol StaticClassName() {
            static Symbol name("FontMap3d");
            return name;
        }

        RndFont3d *mFont; // 0x8
        int mDisplayableChars; // 0xc
        std::vector<RndMesh *> mMeshes; // 0x10
        RndMesh **mMeshCursor; // 0x1c - current position for SetupCharacter
    };

    // Hmx::Object
    virtual ~RndText();
    OBJ_CLASSNAME(Text);
    OBJ_SET_TYPE(Text);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    // RndDrawable
    virtual void UpdateSphere();
    virtual float GetDistanceToPlane(const Plane &, Vector3 &);
    virtual bool MakeWorldSphere(Sphere &, bool);
    virtual void Mats(std::list<class RndMat *> &, bool);
    virtual void DrawShowing();
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    virtual int CollidePlane(const Plane &);
    virtual void Highlight();
    // RndText
    virtual Symbol TextToken() { return gNullStr; }

    OBJ_MEM_OVERLOAD(0x19);
    NEW_OBJ(RndText);

    const String &GetText() const { return mText; }
    String TextASCII() const;
    void SetTextASCII(const char *);
    void SetFixedLength(int);
    void ReFitTextScroll(String);
    float ComputeCharWidthsForText(String);
    void SetAltStyle(Hmx::Object *obj) { mAltStyle = obj; }
    FitType GetFitType() const { return mFitType; }
    void SetFitType(FitType f) { mFitType = f; }
    float Indentation() const { return mIndentation; }

    static void Init();
    static void DrawBlacklight();
    static void ClearBlacklight();
    static void SetBlacklightModeEnabled(bool b) { sBlacklightModeEnabled = b; }
    static bool IsBlacklightModeEnabled() { return sBlacklightModeEnabled; }

    int GetTextSize() const { return Max<int>(mFixedLength, mText.length()); }
    void SetCapsMode(CapsMode c) { mCapsMode = c; }
    void UpdateText();
    void GetWidthHeightBox(Box &) const;
    void SetText(const char *);
    int FontMapIndex(RndFontBase *, bool);
    float ComputeHeight(int, float, float &);
    int NumStyles() const { return mStyles.size(); }
    ObjVector<Style> &Styles() { return mStyles; }
    const String &RawText() const { return mText; }
    float Width() const { return mWidth; }
#ifdef HX_NATIVE
    void SetWidth(float w) { mWidth = w; }
    Alignment GetAlignment() const { return mAlignment; }
    void SetAlignment(Alignment a) { mAlignment = a; }
#endif
    float BoundsLeft() const { return mBoundsLeft; }
    float BoundsTop() const { return mBoundsTop; }
    float BoundsRight() const { return mBoundsRight; }
    float BoundsBottom() const { return mBoundsBottom; }

    friend class UIFontImporter;
    friend class LabelShrinkWrapper;
    friend class UIListLabelElement;

    // Line class for text layout — size 0x14
    class Line {
    public:
        const unsigned short *mStart; // 0x0 - pointer to first char
        const unsigned short *mEnd; // 0x4 - pointer past last char
        float mWidth; // 0x8
        float mXStart; // 0xc - x starting position
        float mYPos; // 0x10 - y position
    };

    void WrapText(const unsigned short *, int, float *, HX_VECTOR(Line) &, Hmx::Rect &, float);
    void ConstructMeshes(const HX_VECTOR(Line) &, const Hmx::Rect &, float);

protected:
    RndText();

    void DoBasicMarkup();
    void BuildFontMaps(bool);
    void FitTextJust();
    void FitTextEllipsis();
    void FitTextScroll();
    void SizeCheck();
    void UpdateScrollOffsets();
    static void DrawMesh(RndMesh *, float, int);
    int ConvertTextToWide(const char *, HX_VECTOR(unsigned short) &);
    void ReplaceMissingCharacters(HX_VECTOR(unsigned short) &);
    int OnComputeCharWidths(const unsigned short *, float *, bool);
    const unsigned short *ParseMarkup(const unsigned short *, StyleState &, unsigned short &);

    static void QueueBlacklightPacket(RndMesh *, float, int);
    static FontMapBase *AcquireFontMap(RndFontBase *);
    static bool sBlacklightModeEnabled;
    static int sBlacklightPacketCount;
    static std::vector<BlacklightPacket> sBlacklightPacketPool;
    static std::list<FontMapBase *> sFontMapCache;

    /** "Text value" */
    String mText; // 0x8
    /** "Width of text until it wraps." Ranges from 0 to 10000. */
    float mWidth; // 0x10
    /** "Height of the text, used for [fit_type] kFitJust". Ranges from 0 to 1000. */
    float mHeight; // 0x14
    /** "Lay text around circle of this circumference. Negative values face other way." */
    float mCircle; // 0x18
    /** "Alignment option for the text" */
    Alignment mAlignment; // 0x1c
    FitType mFitType; // 0x20
    /** "Defines the CAPS mode for the text" */
    CapsMode mCapsMode; // 0x24
    /** "Vertical distance between lines". Ranges from -5 to 5. */
    float mLeading; // 0x28
    /** "Number of character maximum for the text,
        if non-zero makes underlying mesh mutable, so updates are faster" */
    int mFixedLength; // 0x2c
    /** "Support markup or not.
        In the text, use <alt>, <alt2>, <alt3>, etc to use the higher styles,
        <sup> to get a super script, <nobreak> for preventing linebreaks in a block,
        <it> for italics, <gtr> for Bryn's guitar chord formatting.
        Example: Hit <it>Back</it> <alt>B</alt> to continue<sup>TM</sup> " */
    bool mMarkup; // 0x30
    /** "Support basic markup or not. It converts \\p to double-quotes.
        Furthur support can be added." */
    bool mBasicMarkup; // 0x31
    /** "If scrolling oversized text - delay this many seconds before starting" */
    float mScrollDelay; // 0x34
    /** "If scrolling oversized text - scroll this many characters per second" */
    float mScrollRate; // 0x38
    /** "If scrolling oversized text - delay this many seconds between scrolls.
        When the fit type is kFitScrollMarqueeWrapAlways, this value will be ignored." */
    float mScrollPause; // 0x3c
    bool mWrapEnabled; // 0x40
    float mScrollTimer; // 0x44
    float mScrollState; // 0x48
    float mScrollSpeed; // 0x4c
    float mScrollPos; // 0x50
    float mTotalWidth; // 0x54
    float mLineHeight; // 0x58
    int mScrollCopies; // 0x5c
    int mNumLines; // 0x60
    /** "Space between continuous scrolling messages.
        This value is only considered when the fit type
        is set to kFitScrollMarqueeWrapAlways." */
    float mIndentation; // 0x64
    std::list<float> mLineWidths; // 0x68
    std::list<float> mLineOffsets; // 0x70
    ObjPtr<Hmx::Object> mAltStyle; // 0x78
    float mScrollOffset; // 0x8c
    int mCurScrollChars; // 0x90
    int mScrollOutIndex; // 0x94
    /** "The different styles this text can have" */
    ObjVector<Style> mStyles; // 0x98
    std::vector<FontMapBase *> mFontMaps; // 0xa8
    float mBoundsLeft;
    float mBoundsTop;
    float mBoundsRight;
    float mBoundsBottom;
    int mNumLinesRendered; // 0xc4
    float mConstructScale; // 0xc8
};
