#pragma once
#include "math/Geo.h"
#include "obj/Object.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

class KerningTable;

// RB3 retail has NO RndFontBase: the retail RndFont Complete Object Locator
// reports numBaseClasses == 3 (RndFont -> Hmx::Object -> ObjRef), the binary
// contains zero "FontBase" bytes, and retail's RndFont::Save (vtable slot 8,
// 0x82472EC0) calls ?Save@Object@Hmx@@ directly with a single SAVE_REVS.
// The RndFontBase split is a DC3-era addition; RndFont derives from
// Hmx::Object here and carries the former base's members/methods itself.
//
// MEMBER LAYOUT -- decoded instruction-by-instruction from retail bytes, not
// inherited from either sibling repo. Three retail bodies agree:
//   * Save       0x82472EC0: ObjPtr saves at +0x28/+0x34/+0x88 through ONE
//                shared helper; Vector2 saves at +0x60 and +0x7c through one
//                shared helper; lfs +0x68, lfs +0x5c; vector save at +0x6c;
//                lwz +0x58 (kerning table) then a conditional KerningTable::Save;
//                lbz +0x78, lbz +0x84; map node-count at +0x50 / leftmost +0x48
//                (=> map at +0x40); per-char loop emits `sth` + 4x `lfs` at node
//                +0x10/+0x14/+0x18/+0x1c/+0x20 and NEVER a `lwz`.
//   * Print      0x82472C18: lwz +0x30 (mMat.mObject), Vector2 +0x60, lfs +0x68,
//                lfs +0x5c, vector walk at +0x6c.
//   * CharDefined 0x82473A98: two finds on the map at +0x40, then tests CharInfo
//                +0x0/+0x4/+0xc.
// That is the rb3-Wii generation's shape: ONE ObjPtr<RndMat> mMat (no
// ObjPtrVec mMats), an mTexCellSize Vector2 (no mMaterialOffsets vector), an
// mNextFont fallback chain, and a FOUR-FLOAT CharInfo with no page index. The
// multi-page DC3 shape this file used to carry does not exist in RB3 retail.
//
// ⛔ CORRECTED 2026-08-20 (lane VTGRIND). This paragraph used to read: "The
// VTABLE is deliberately NOT changed to rb3-Wii's. Retail's CharDefined and
// Print are mangled `?...@RndFont@@UB...` -- public *virtual* const -- whereas
// rb3-Wii declares both non-virtual. RB3-360 is a hybrid: Wii-era members under
// a DC3-era vtable."  That is HALF RIGHT, and the half that is wrong was
// unknowable from the instrument it used.
//
// A mangled name in scripts/target_symbol_map.json is NOT retail evidence about
// virtuality -- the map is populated by OUR matching, so the `UB` (public
// virtual const) spelling is our own declaration reflected back. Retail's
// VTABLE is retail bytes, and it adjudicates each member separately:
//   * ?Print@RndFont@@ body 0x82472C18  IS  in retail's vtable -> virtual. ✓
//   * ?CharDefined@RndFont@@ body 0x82473A98 is NOT -> not virtual. ✗
//   * ?CharWidth@RndFont@@ body 0x82474478  is NOT -> not virtual. ✗
// Retail's RndFont vtable (0x8206D344) has exactly 21 slots -- Hmx::Object's
// count -- so RndFont adds NO new virtuals; Print occupies Object::Print's slot
// (13), i.e. it OVERRIDES, which is why it must be non-const here.
// The members still move as described above; it is the vtable claim that is
// corrected.
class RndFont : public Hmx::Object {
    friend class UIFontImporter;
    friend class RndText;

public:
    class KernInfo {
    public:
        unsigned short mFirstChar, mSecondChar;
        float kerning; // 0x4
    };

    // Retail CharInfo is 16 bytes: exactly four floats, no trailing word.
    //
    // CORRECTION (lane W13-CHARINFO). This block previously claimed 20 bytes and
    // carried a fifth `int mUnk10`, on the strength of an `_M_erase` that
    // deallocates `li r3, 0x28` (a 40-byte node). That instrument was
    // MISATTRIBUTED. The 0x28 body is `fn_826DC4C0`, and a whole-binary caller
    // scan of the split asm finds it reached only from `fn_826DC598`
    // (CharLipSync) and from itself -- NO RndFont body calls it. Its
    // `...UCharInfo@RndFont@@...` map name is a fold-alias survivor's name, not
    // evidence about this map. (0x28 is, however, exactly the node size OUR
    // 20-byte CharInfo compiled to, which is how the misreading survived: the
    // "retail" figure being cited was our own codegen.)
    //
    // The map-name-independent chain, followed from RndFont's own pinned bodies
    // outward, gives 16:
    //   * RndFont bodies 0x82473568, 0x82473B38 and 0x82474560 each do
    //     `addi r3, <this>, 0x40` then `bl fn_822FA110` -- i.e. they clear THE
    //     MAP AT +0x40, which is mCharInfoMap.
    //   * fn_822FA110 (_Rb_tree::clear) calls fn_822F8B40 (_M_erase), which
    //     deallocates `li r3, 0x24` -- a 36-byte node.
    //   * The same family's _M_create_node (fn_822F8F80, reached from Font.s via
    //     _M_copy fn_822FA500) allocates `li r3, 0x24` and copies the value as
    //     FIVE words from node+0x10 -- a 20-byte pair, independently.
    //   _Rb_tree_node_base is 16 and the value sits at node+0x10, so
    //   sizeof(pair<const u16, CharInfo>) is 20 and sizeof(CharInfo) is 16.
    //
    // The float evidence is unchanged and still holds: Save (0x82472EC0) and
    // CharDefined (0x82473A98) put FLOATS at +0x0, +0x4, +0x8, +0xc -- the
    // per-char FOREACH emits `sth` then 4x `lfs` and never a `lwz`, so DC3's
    // leading `int mPage` cannot be there. With 16 bytes the four floats fill
    // the struct exactly and there is no room for an unidentified fifth word.
    struct CharInfo {
        float mU; // 0x0
        float mV; // 0x4
        float mCharWidth; // 0x8
        float mAdvance; // 0xc
    };
    virtual ~RndFont();
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(Font);
    OBJ_SET_TYPE(Font);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // ★ NONE of the accessors below is virtual in retail.  RndFont's retail
    // vtable (0x8206d344) has exactly 21 slots -- byte-verified: slot[21] is
    // 0xffffffff, not a function VA, followed by EH state-table entries, and
    // the next enumerated vtable is 471 words away, so the bound is not what
    // stopped the read.  21 is exactly Hmx::Object's slot count, so retail's
    // RndFont declares NO new virtuals at all.  Ours declared 13 (12 accessors
    // + a non-overriding Print), giving 34.
    // RndFont has no subclasses in src/, so devirtualizing changes no dispatch.
    // Same reasoning a prior lane applied one-at-a-time to HasChar below
    // ("retail issues direct bl calls with no vtable load"); this is that
    // finding generalized by tools/vtable_order_sweep.py's slot COUNT.
    float CharWidth(unsigned short) const;
    float CharAdvance(unsigned short) const;
    bool CharAdvance(unsigned short, unsigned short, float &) const;
    float Kerning(unsigned short, unsigned short) const;
    bool CharDefined(unsigned short) const;
    float AspectRatio() const { return mCellSize.y / mCellSize.x; }
    RndMat *Mat() const { return mMat; }
    // rb3-Wii's accessor, and NON-virtual on purpose. Retail reads the font's
    // material with a plain `lwz r4, 0x30(font)` field load at every SetMat site
    // in rndobj/Text.cpp (0x82458E20, 0x82458ED4, 0x8245911C, 0x824594C4) -- no
    // vtable call appears anywhere. Callers that retail inlines must use this,
    // not the Mat() above.  (That parenthetical used to read "which stays,
    // since it occupies a vtable slot" -- REFUTED 2026-08-20: retail's RndFont
    // vtable has 21 slots, exactly Hmx::Object's count, so NO RndFont accessor
    // occupies a slot.  Mat() is now non-virtual too.)
    RndMat *GetMat() const { return mMat; }
    const RndFont *DataOwner() const { return mTextureOwner; }
    float FontUnit() const { return mCellSize.x; }
    float FontUnitInverse() const { return 1.0f / FontUnit(); }
    // NOT const: Hmx::Object::Print() is non-const, so a `const` here does NOT
    // override -- MSVC keeps Object::Print in its slot AND appends a new one.
    // dc3-decomp declares it const and we inherited that; rb3-Wii, the closer
    // oracle for RB3, declares it NON-const.  Retail agrees: a Print@RndFont
    // body sits in Object::Print's slot (13), and of ~40 retail Print
    // overrides every other one is `UAA` (non-const) -- the lone `UBA` is this
    // symbol, whose spelling comes from OUR declaration via the map.
    virtual void Print();
    bool BitmapFont() const { return true; }

    OBJ_MEM_OVERLOAD(0x7C)
    NEW_OBJ(RndFont)
    static void Init() { REGISTER_OBJ_FACTORY(RndFont) }

    void SetBaseKerning(float);
    void SetKerning(const std::vector<KernInfo> &);
    void GetKerning(std::vector<KernInfo> &) const;
    bool IsMonospace() const { return mMonospace; }
    const std::vector<unsigned short> &Chars() const { return mChars; }
    float BaseKerning() const { return mBaseKerning; }

    // Retail inlines this into Save (0x82472EC0): `lwz r11,0x30(font)` ->
    // `lwz r11,0x94(r11)` (RndMat::mDiffuseTex), with a null short-circuit.
    RndTex *ValidTexture() const {
        if (mMat)
            return mMat->GetDiffuseTex();
        else
            return nullptr;
    }
    // Vestigial page-indexed forms. RB3 retail fonts are SINGLE-page (one
    // ObjPtr<RndMat>), so the index is ignored; these exist only so the
    // out-of-unit callers in ui/UIFontImporter.cpp keep compiling unchanged.
    RndMat *Mat(int) const;
    RndTex *ValidTexture(int) const;
    void SetCellSize(float, float);
    void BleedTest();
    bool
    CharWidthAdvanceCoords(unsigned short, float &, float &, Vector2 &, Vector2 &) const;
    float DeprecatedSize() const { return mDeprecatedSize; }
    // RB3 retail API used by ui/UILabel.cpp (rb3-Wii oracle rndobj/Font.h).
    // DECLARATION-ONLY, non-virtual -> layout- and vtable-neutral.
    RndFont *TextureOwner() const;
    // Inline (rb3-Wii Font.h's non-HX_NATIVE branch: `return mCellSize.y /
    // mCellSize.x;`). Retail's UILabel::FitText (0x827f5550) inlines this
    // ratio directly -- two float loads at mCellSize's offsets (0x60/0x64)
    // and a single fdivs, no `bl` -- so leaving this out-of-line forces an
    // extra call retail never makes.
    float CellDiff() const { return mCellSize.y / mCellSize.x; }

protected:
    RndFont();
    // NON-virtual, and defined in-class on purpose: retail's CharDefined
    // (0x82473A98) inlines the lookup -- it issues two direct `bl` calls to
    // map::find with no vtable load -- so retail's HasChar cannot be virtual.
    bool HasChar(unsigned short c) const {
        return mCharInfoMap.find(c) != mCharInfoMap.end();
    }
    void SetASCIIChars(String);

    String GetASCIIChars() const;

    void SetCharInfo(CharInfo *, RndBitmap &, const Vector2 &);
    void UpdateChars();
    void SetBitmapSize(const Vector2 &, unsigned int, unsigned int);

    // Retail-decoded layout (see the class comment for the deriving bytes).
    // sizeof(RndFont) == 0x94.
    ObjPtr<RndMat> mMat; // 0x28  (mObject at 0x30 -- retail's `lwz rN,0x30(font)`)
    ObjOwnerPtr<RndFont> mTextureOwner; // 0x34
    std::map<unsigned short, CharInfo> mCharInfoMap; // 0x40
    KerningTable *mKerningTable; // 0x58
    float mBaseKerning; // 0x5c
    Vector2 mCellSize; // 0x60
    float mDeprecatedSize; // 0x68
    std::vector<unsigned short> mChars; // 0x6c
    bool mMonospace; // 0x78
    Vector2 mTexCellSize; // 0x7c
    bool mPacked; // 0x84
    ObjPtr<RndFont> mNextFont; // 0x88
};

class KerningTable {
public:
    class Entry {
    public:
        Entry *next; // 0x0
        int key; // 0x4
        float kerning; // 0x8
    };
    KerningTable();
    ~KerningTable();
    float Kerning(unsigned short, unsigned short);
    void GetKerning(std::vector<RndFont::KernInfo> &) const;
    void SetKerning(const std::vector<RndFont::KernInfo> &, RndFont *);
    Entry *Find(unsigned short, unsigned short);
    void Save(BinStream &);
    void Load(BinStreamRev &, RndFont *);
    bool Valid(const RndFont::KernInfo &, RndFont *);

    int Key(unsigned short us0, unsigned short us2) {
        return (us0 & 0xFFFF) | ((us2 << 0x10) & 0xFFFF0000);
    }
    int Size() const { return mNumEntries * sizeof(Entry) + 0x88; }
    int TableIndex(unsigned short us0, unsigned short us2) { return (us0 ^ us2) & 0x1F; }

    MEM_OVERLOAD(KerningTable, 0x162);

    int mNumEntries; // 0x0
    Entry *mEntries; // 0x4
    Entry *mTable[32]; // 0x8
};

// DC3-ONLY. Retail RB3 contains no ".?AVRndFontBase@@" type descriptor and no
// "FontBase" string at all; this class survives solely as the base of the
// (likewise DC3-only) RndFont3d. Nothing retail-facing may derive from it.
// A follow-on lane should remove RndFont3d + RndFontBase together, in
// coordination with the owner of splits.txt.
class RndFontBase : public Hmx::Object {
    friend class UIFontImporter;
    friend class RndText;

public:
    typedef RndFont::KernInfo KernInfo;

    OBJ_CLASSNAME(FontBase);
    OBJ_SET_TYPE(FontBase);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual float CharWidth(unsigned short) const { return 0; }
    virtual float CharAdvance(unsigned short) const { return 0; }
    virtual bool CharAdvance(unsigned short, unsigned short, float &) const {
        return false;
    }
    virtual float Kerning(unsigned short, unsigned short) const;
    virtual bool CharDefined(unsigned short) const;
    virtual float AspectRatio() const { return 0; }
    virtual RndMat *Mat() const { return nullptr; }
    virtual const RndFontBase *DataOwner() const { return this; }
    virtual float FontUnit() const { return 0; }
    virtual float FontUnitInverse() const { return 1.0f / FontUnit(); }
    virtual void Print() const {}
    virtual bool BitmapFont() const { return true; }

    OBJ_MEM_OVERLOAD(0x1C)
    NEW_OBJ(RndFontBase)
    static void Init() { REGISTER_OBJ_FACTORY(RndFontBase) }

    void SetBaseKerning(float);
    void SetKerning(const std::vector<KernInfo> &);
    void GetKerning(std::vector<KernInfo> &) const;
    bool IsMonospace() const { return mMonospace; }
    const std::vector<unsigned short> &Chars() const { return mChars; }
    float BaseKerning() const { return mBaseKerning; }

protected:
    RndFontBase();
    virtual bool HasChar(unsigned short) const;

    String GetASCIIChars() const;
    void SetASCIIChars(String);

    std::vector<unsigned short> mChars; // 0x28
    bool mMonospace; // 0x34
    float mBaseKerning; // 0x38
    KerningTable *mKerningTable; // 0x3c
};

class RndFont3d : public RndFontBase {
public:
    struct CharInfo {
        CharInfo() : mMesh(nullptr) {}
        ~CharInfo() {}

        Box unk0; // 0x0
        float advance; // 0x20
        ObjPtr<RndMesh> mMesh; // 0x24
        bool visible; // 0x38

        MEM_OVERLOAD(CharInfo, 0x12A);
    };
    virtual ~RndFont3d() { Clear(); }
    OBJ_CLASSNAME(Font3d);
    OBJ_SET_TYPE(Font3d);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual float CharWidth(unsigned short) const;
    virtual float CharAdvance(unsigned short) const;
    virtual bool CharAdvance(unsigned short, unsigned short, float &) const;
    virtual float Kerning(unsigned short, unsigned short) const;
    virtual float AspectRatio() const;
    virtual RndMat *Mat() const;
    virtual const RndFontBase *DataOwner() const;
    virtual float FontUnit() const { return mTextureOwner->mCellSize.x; }
    virtual float FontUnitInverse() const { return mTextureOwner->mInvCellSize.x; }

    OBJ_MEM_OVERLOAD(0x10A)
    NEW_OBJ(RndFont3d)
    static void Init() { REGISTER_OBJ_FACTORY(RndFont3d) }

    CharInfo *GetCharInfo(unsigned short) const;
    Vector3 CharOriginOffset() const;
    bool CharWidthAdvanceMesh(unsigned short, float &, float &, RndMesh **) const;

protected:
    RndFont3d();

    virtual bool HasChar(unsigned short) const;

    void Clear();

    ObjPtr<RndMat> mMat; // 0x40
    ObjOwnerPtr<RndFont3d> mTextureOwner; // 0x4c
    Vector3 mCellSize; // 0x58
    Vector3 mInvCellSize; // 0x68
    Vector3 unk8c; // 0x78
    std::map<unsigned short, CharInfo *> mCharInfoMap; // 0x88
};

// Single-page, per the rb3-Wii oracle. The DC3 page-indexed form
// (BitmapLocker(RndFont*, int) + LoadPage(int)) presupposed the multi-page
// ObjPtrVec<RndMat> font that RB3 retail does not have.
//
// NOTE for the map owner: scripts/target_symbol_map.json maps
// 0x823fe968 -> ??0BitmapLocker@@QAA@PAVRndFont@@H@Z, but fn_823FE968 is NOT
// this constructor -- it never reads r4 (so it is a ONE-argument function),
// it calls a base-class constructor and installs a vtable at +0x0 (BitmapLocker
// has neither), and it initialises fields out to +0x6c on a ~0x70-byte object.
// It lives at 0x823FE968, far outside Font's 0x8247xxxx cluster, in one of the
// scatter-included BandDirector.cpp / Tail.cpp ranges. That map row is a
// misattribution and no source change here can make it match.
class BitmapLocker {
    friend class RndFont;

public:
    BitmapLocker(RndFont *);
    ~BitmapLocker();

    RndBitmap *PtrToBitmap() const { return mPbm; }

    RndTex *mTexture; // 0x0
    RndBitmap *mPbm; // 0x4
    RndBitmap mBm; // 0x8
};
