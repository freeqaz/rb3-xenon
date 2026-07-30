#pragma once
#include "math/Color.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Draw.h"
#include "rndobj/Font.h"
#include "rndobj/Mesh.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"
#include "utl/StlAlloc.h"
#include "utl/Symbol.h"
#include <map>
#include <set>
#include <vector>

#ifndef HX_NATIVE
using stlpmtx_std::StlNodeAlloc;
// Retail's mLines is stlpmtx_std::vector<Line, StlNodeAlloc<Line> > — proven by
// the mangled symbol band3/bandtrack/GemManager.cpp force-instantiates:
//   ?erase@?$vector@VLine@RndText@@V?$StlNodeAlloc@VLine@RndText@@@stlpmtx_std@@@...
// so this macro is load-bearing, not a convenience.
#define HX_VECTOR(T) stlpmtx_std::vector<T, stlpmtx_std::StlNodeAlloc<T> >
#else
#define HX_VECTOR(T) std::vector<T>
#endif

// ---------------------------------------------------------------------------
// RB3-360 RETAIL RndText.
//
// Ground truth: docs/decomp/rndtext-retail-layout.md (lane BP-2b) — a
// compiler-and-retail-verified member table. Retail RndText is rb3-Wii-lineage
// in structure, member order, Load/Save order and ctor mem-init list, with
// 360-widened types. It is NOT the DC3-generation class this file used to hold
// (no mStyles / StyleState / FontMap / FontMap3d / BlacklightPacket / fit /
// scroll / mCircle block — zero string hits for any of those in the retail
// binary).
//
//   base region [0x000,0x0d8)  byte-identical to our tree, untouched
//   own members [0x0d8,0x190)  = 0xb8, fully attributed below
//   vtordisp word      @0x190
//   Hmx::Object vbase  @0x194
//   RndHighlightable   @0x1C0
//   sizeof(RndText)    = 0x1c8
// ---------------------------------------------------------------------------
class RndText : public RndDrawable, public RndTransformable {
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

    // sizeof 0x24 — UNCHANGED from the previous (DC3-shaped) header: retail's
    // Style has the identical layout, only rb3-Wii's *names* differ
    // (font/size/italics/color/brk/pre/zOffset). Keeping our field names avoids
    // churning Lyric.{h,cpp}, UIFontImporter, HamListRibbon, StarsDisplay and
    // UIListLabel for zero layout benefit.
    //   MEASURED offsets, from mStyle@0x110:
    //   font@0x110 size@0x114 italics@0x118 Hmx::Color@0x11c
    //   brk@0x12c pre@0x12d zOffset@0x130
    class Style {
    public:
        Style()
            : mFont(nullptr), mSize(0), mItalics(0), mTextColor(1, 1, 1, 1),
              nobreak(true), pre(false), mZOffset(0) {}
        // rb3-Wii-lineage ctor. Wii passes Color32(-1) (opaque white); the
        // 360 widening makes that Hmx::Color(1,1,1,1) — the ctor default the
        // retail RndText ctor was measured to store.
        Style(RndFont *f, float sz, float ital, const Hmx::Color &col, float z)
            : mFont(f), mSize(sz), mItalics(ital), mTextColor(col), nobreak(true),
              pre(false), mZOffset(z) {}
        // NO user-declared copy ctor / operator=. The previous header carried
        // explicit memcpy(this,&s,0x24) forms marked "codegen-load-bearing";
        // that is REFUTED. Two reasons:
        //   1. For a 0x24 POD, MSVC's *implicit* copy already emits exactly the
        //      measured `li r5,0x24` + memcpy — the explicit form buys nothing.
        //   2. Declaring them makes Style, and therefore the Line that embeds
        //      it, non-trivially-copyable. That silently rewrites
        //      vector<Line>::erase from the trivial pointer-subtraction memmove
        //      into an element-wise copy loop, and retail's
        //      ?erase@?$vector@VLine@RndText@@... is the memmove form (it
        //      matches at 100% regardless of sizeof(Line), because the byte
        //      count is `(char*)end - (char*)last` with no sizeof multiply --
        //      which is exactly why it matched even when our Line was 0x14 and
        //      retail's was 0x78). Keeping Style trivial is what holds that
        //      retail-required instantiation at 100%.
        float GetAlpha() const { return mTextColor.alpha; }
        void SetAlpha(float alpha) { mTextColor.alpha = alpha; }

        /** "Font to use for this style" (raw ptr in RB3-360 retail) */
        RndFont *mFont; // 0x00
        /** "Size of the text" */
        float mSize; // 0x04
        /** "Defines the slant of the text, changed by <it> tag" */
        float mItalics; // 0x08
        /** "Color of the text, put into mesh verts. Modified by <color=r,g,b,a>." */
        Hmx::Color mTextColor; // 0x0c
        /** "Prevent line breaks in a block" (rb3-Wii `brk`) */
        bool nobreak; // 0x1c
        /** "Super-script / pre" */
        bool pre; // 0x1d
        /** "vertical offset as fraction of size" */
        float mZOffset; // 0x20
    }; // sizeof 0x24

    // sizeof 0x78 — MEASURED four ways (divw by 0x78, two mulli 0x78, and the
    // dtor's deallocation arithmetic).
    //
    // CORRECTION to docs/decomp/rndtext-retail-layout.md: that doc's *table*
    // (which is the measurement) and its *prose decomposition* disagree. Wii's
    // Line is 0x60 = Style 0x18 + 2 ptrs + 2 uints + Transform 0x30 + float +
    // Color32. On 360 Style grows to 0x24 (+0xc) and Transform to 0x40 (+0x10),
    // so KEEPING Wii's separate `color` member would give 0x88, not 0x78. The
    // measured table attributes every byte of [0,0x78) with no room for it.
    // Conclusion: retail DROPS Wii's redundant Line::color — the colour lives in
    // lineStyle.mTextColor (on Wii both were written with the same value, so the
    // duplicate was dead storage). INFERRED from the measured table, but the
    // arithmetic only closes this one way.
    class Line {
    public:
        Line()
            : lineStyle(), mStart(nullptr), mEnd(nullptr), startIdx(0), endIdx(0),
              mWidth(0) {
            xfm.Reset();
        }
        Style lineStyle; // 0x00  (0x24)
        /** mText.c_str() + startIdx */
        const char *mStart; // 0x24
        /** mText.c_str() + endIdx, after trailing-whitespace trim */
        const char *mEnd; // 0x28
        unsigned int startIdx; // 0x2c
        unsigned int endIdx; // 0x30
        Transform xfm; // 0x34  (0x40)
        /** advance-summed width of this line */
        float mWidth; // 0x74
    }; // sizeof 0x78

    class MeshInfo {
    public:
        MeshInfo() : mesh(nullptr), syncFlags(0), displayableChars(0) {}
        RndMesh *mesh; // 0x0
        int syncFlags; // 0x4
        int displayableChars; // 0x8
    };

#ifdef HX_NATIVE
    // mMeshMap is keyed by the RndFont* cast to an integer. `unsigned int` is
    // pointer-width on the 32-bit console but TRUNCATES a 64-bit host pointer.
    // The map is runtime-only (never read byte-for-byte from disk), so widening
    // the key on native is layout-safe.
    typedef unsigned long FontKey;
#else
    typedef unsigned int FontKey;
#endif

    // ---- Hmx::Object ----
    virtual ~RndText();
    OBJ_CLASSNAME(Text);
    OBJ_SET_TYPE(Text);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    virtual void Replace(ObjRef *, Hmx::Object *);
    virtual const char *FindPathName();
    virtual void Print();
    // ---- RndDrawable ----
    virtual void UpdateSphere();
    virtual float GetDistanceToPlane(const Plane &, Vector3 &);
    virtual bool MakeWorldSphere(Sphere &, bool);
    virtual void Mats(std::list<class RndMat *> &, bool);
    // NOT `virtual` — Draw() is non-virtual in RB3-360 retail (see the smoking
    // gun in rndobj/Draw.h: every call site is a direct `bl` to the single
    // cull-wrapper body). Declaring it virtual would add a vtable slot.
    DRAW_DC3_VIRTUAL void Draw();
    virtual void DrawShowing();
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    virtual int CollidePlane(const Plane &);
    virtual void Highlight() { RndDrawable::Highlight(); }

    OBJ_MEM_OVERLOAD(0x19);
    NEW_OBJ(RndText);

    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(RndText); }
    static void CollectGarbage();
    static void ResetFaces(RndMesh *, int);
    static std::set<RndText *> mTextMeshSet;

    // ---- text ----
    const String &GetText() const { return mText; }
    const String &RawText() const { return mText; }
    String TextASCII() const;
    void SetTextASCII(const char *);
    void SetText(const char *);
    void ResizeText(int);
    void SetFixedLength(int);
    int GetTextSize() const { return Max<int>(mFixedLength, (int)mText.length()); }

    // ---- style ----
    RndFont *GetFont() const { return mFont; }
    void SetFont(RndFont *);
    float Size() const { return mStyle.mSize; }
    void SetSize(float);
    void SetItalics(float);
    void SetColor(const Hmx::Color &);
    void SetMarkup(bool);
    void SetLeading(float);
    void SetWrapWidth(float);
    float WrapWidth() const { return mWrapWidth; }
    float Width() const { return mWrapWidth; }
    const Hmx::Color &StyleColor() const { return mStyle.mTextColor; }
    const Style &GetSingleStyle() const { return mStyle; }
    unsigned int GetSingleStyleColor() const { return mStyle.mTextColor.PackAlpha(); }
    /** Retail carries exactly one authored style (+ one alt); the DC3-era
        ObjVector<Style> mStyles does not exist. */
    int NumStyles() const { return 1; }
    Alignment GetAlignment() const { return (Alignment)mAlign; }
    void SetAlignment(Alignment);
    void SetCapsMode(CapsMode c) { mCapsMode = c; }
    void SetAltStyle(RndFont *, float, Hmx::Color *, float, float, bool);
    void SetAltSizeAndZOffset(float, float);
    void SetData(
        Alignment,
        const char *,
        RndFont *,
        float,
        float,
        float,
        float,
        const Hmx::Color &,
        bool,
        CapsMode,
        int
    );

    // ---- layout / meshes ----
    void UpdateText(bool);
    void UpdateText() { UpdateText(true); }
    void DeferUpdateText();
    void ResolveUpdateText();
    void SyncMeshes();
    void SetMeshForceNoUpdate();
    void GetMeshes(std::vector<RndMesh *> &);
    void ReserveLines(int);
    int NumLines() const { return mLines.size(); }
    float MaxLineWidth() const;
    void GetVerticalBounds(float &, float &) const;
    void GetCurrentStringDimensions(float &, float &);
    void GetStringDimensions(float &, float &, HX_VECTOR(Line) &, const char *, float);
    float GetStringWidthUTF8(const char *, const char *, bool, const Style *) const;
    void WrapText(const char *, const Style &, HX_VECTOR(Line) &);

    // Retail has no mBounds* members (the DC3 block is absent). These are
    // computed from the line list so LabelShrinkWrapper / UIListLabel /
    // UIListProvider keep their semantics (extents, not absolute edges).
    float BoundsLeft() const;
    float BoundsTop() const;
    float BoundsRight() const;
    float BoundsBottom() const;

    // ---- single-style line API (rb3-Wii-lineage; used by band3 Lyric.cpp) ----
    int
    AddLineUTF8(const String &, const Transform &, const Style &, float *, bool *, int);
    void ReplaceLineText(
        unsigned int,
        const String &,
        const Transform &,
        const Style &,
        float *,
        bool *,
        int
    );
    void UpdateLineColor(unsigned int, const Hmx::Color &, bool *);
    void ApplyLineText(const String &, const Style &, float &, Line &, int, int, bool *);
    int NumCharsInBytes(const String &, const Style &, float &, int);

    // ---- internals (public, as on rb3-Wii) ----
    const char *ParseMarkup(const char *, Style *, float, float) const;
    float GetHorizontalAlignOffset(const Line &, Alignment) const;
    void RotateLineVerts(const Line &, RndMesh::Vert *, RndMesh::Vert *);
    RndFont *GetDefiningFont(unsigned short &, RndFont *) const;
    RndFont *SupportChar(unsigned short, RndFont *);
    void UpdateMesh(RndFont *);
    void CreateLines(RndFont *);
    void ComputeCharWidths(float *, int, const char *, Style);

    DataNode OnSetFixedLength(DataArray *);
    DataNode OnSetFont(DataArray *);
    DataNode OnSetAlign(DataArray *);
    DataNode OnSetText(DataArray *);
    DataNode OnSetSize(DataArray *);
    DataNode OnSetWrapWidth(DataArray *);
    DataNode OnSetColor(DataArray *);

    friend class UIFontImporter;
    friend class LabelShrinkWrapper;
    friend class UIListLabelElement;
    friend class UILabel;
    friend class HamLabel;

    // =======================================================================
    // OWN MEMBER BLOCK — [0xd8, 0x190), totals exactly 0xb8.
    // Every offset below is MEASURED (see docs/decomp/rndtext-retail-layout.md)
    // except where tagged INFERRED.
    // =======================================================================
    HX_VECTOR(Line) mLines; // 0x0d8  0x0c  (stride 0x78)
    ObjOwnerPtr<RndFont> mFont; // 0x0e4  0x0c  (payload @0x0ec)
    float mWrapWidth; // 0x0f0  (ctor 0.0)
    /** 360-widened: rb3-Wii packs mAlign/mCapsMode as a u8 pair. */
    int mAlign; // 0x0f4  (ctor 0x11 kTopLeft)
    int mCapsMode; // 0x0f8  (ctor 0)
    float mLeading; // 0x0fc  (ctor 1.0)
    String mText; // 0x100  0x0c
    /** 360-widened: rb3-Wii has `int mFixedLength : 16`. */
    int mFixedLength; // 0x10c
    Style mStyle; // 0x110  0x24
    /** 360: a real bool member of RndText. On rb3-Wii this is a bitfield in the
        RndDrawable base. Serialized (Load rev > 0xD). */
    bool mTextMarkup; // 0x134  (+3 pad)
    Style mAltStyle; // 0x138  0x24  (Load tail: memcpy 0x24 from mStyle)

    // --- runtime-only tail. Save touches NOTHING in [0x138,0x15c) or
    // [0x178,0x190), so none of these are serialized and their identity cannot
    // be pinned from the stream. Offsets are MEASURED; the mapping onto
    // rb3-Wii's flags is INFERRED from role + declaration adjacency. ---

    /** INFERRED = Wii `unkbp4`: enables mAltStyle for the <alt> markup tag.
        Sits immediately after mAltStyle, and SetAltStyle writes both. */
    bool mUseAltStyle; // 0x15c  (+3 pad)
    // the 0x18 _Rb_tree flavour — do NOT gate this TU with RB3_RBTREE_0x1C
    std::map<FontKey, MeshInfo> mMeshMap; // 0x160  0x18
    /** signed; DeferUpdateText/ResolveUpdateText nest on it (cmpwi). */
    int mDeferUpdate; // 0x178
    /** INFERRED = Wii `unkbp5`: an UpdateText was requested while deferred.
        Sits immediately after mDeferUpdate, which is the pair it is read with. */
    bool mNeedsUpdate; // 0x17c  (+3 pad)
    /** callback interface, virtual slot 1 = Update(RndMesh*) */
    void *mMeshCallback; // 0x180
    /** height of the current text block (GetCurrentStringDimensions out2) */
    float mCurHeight; // 0x184
    /** width of the current text block  (GetCurrentStringDimensions out1) */
    float mCurWidth; // 0x188
    /** INFERRED = Wii `unkbp6`: meshes need a rebuild on next DrawShowing. */
    bool mMeshDirty; // 0x18c
    // 0x18d-0x18f: EVIDENCE RAN OUT in the retail sweep (pad, or unreferenced
    // bools). Retail's ctor was measured zeroing exactly four bools and its
    // UpdateText omits Wii's `unkbp6 = true`, so the Draw/CollectGarbage flags
    // were never located. Placing them here is INFERRED; it is sizeof- and
    // offset-neutral either way (the bytes are padding otherwise), and it is
    // what lets the rb3-Wii bodies port without inventing new members.
    /** INFERRED = Wii `unkbp7`: lines were added manually via AddLineUTF8. */
    bool mManualLines; // 0x18d
    /** INFERRED = Wii `unk124b4p1`: RotateLineVerts is enabled. */
    bool mRotateLineVerts; // 0x18e
    /** INFERRED = Wii `unk124b4:3`: frames since last DrawShowing (compared
        `> 4`, so 3 bits on Wii — fits a byte here). */
    unsigned char mFramesSinceDraw; // 0x18f

protected:
    RndText();
};

class RndTextUpdateDeferrer {
public:
    RndTextUpdateDeferrer(RndText *text) : mText(text) { text->DeferUpdateText(); }
    ~RndTextUpdateDeferrer() { mText->ResolveUpdateText(); }

    RndText *mText; // 0x0
};
