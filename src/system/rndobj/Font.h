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
// Declaration order is preserved verbatim from the old base so that both the
// member layout and the vtable slot order are unchanged by the collapse.
class RndFont : public Hmx::Object {
    friend class UIFontImporter;
    friend class RndText;

public:
    class KernInfo {
    public:
        unsigned short mFirstChar, mSecondChar;
        float kerning; // 0x4
    };

    struct CharInfo {
        int mPage; // 0x0
        float mU;
        float mV;
        float mCharWidth; // 0xc
        float mAdvance;
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
    virtual float CharWidth(unsigned short) const;
    virtual float CharAdvance(unsigned short) const;
    virtual bool CharAdvance(unsigned short, unsigned short, float &) const;
    virtual float Kerning(unsigned short, unsigned short) const;
    virtual bool CharDefined(unsigned short) const;
    virtual float AspectRatio() const { return mCellSize.y / mCellSize.x; }
    virtual RndMat *Mat() const {
        if (mMats.size() > 0)
            return (RndMat *)mMats[0];
        else
            return nullptr;
    }
    virtual const RndFont *DataOwner() const { return mTextureOwner; }
    virtual float FontUnit() const { return mCellSize.x; }
    virtual float FontUnitInverse() const { return 1.0f / FontUnit(); }
    virtual void Print() const;
    virtual bool BitmapFont() const { return true; }

    OBJ_MEM_OVERLOAD(0x7C)
    NEW_OBJ(RndFont)
    static void Init() { REGISTER_OBJ_FACTORY(RndFont) }

    void SetBaseKerning(float);
    void SetKerning(const std::vector<KernInfo> &);
    void GetKerning(std::vector<KernInfo> &) const;
    bool IsMonospace() const { return mMonospace; }
    const std::vector<unsigned short> &Chars() const { return mChars; }
    float BaseKerning() const { return mBaseKerning; }

    RndMat *Mat(int) const;
    RndTex *ValidTexture(int) const;
    void SetCellSize(float, float);
    int CharPage(unsigned short) const;
    void BleedTest();
    bool
    CharWidthAdvanceCoords(unsigned short, float &, float &, Vector2 &, Vector2 &) const;
    int NumMats() const { return mMats.size(); }
    float DeprecatedSize() const { return mDeprecatedSize; }
    // RB3 retail API used by ui/UILabel.cpp (rb3-Wii oracle rndobj/Font.h).
    // DECLARATION-ONLY, non-virtual -> layout- and vtable-neutral.
    RndFont *TextureOwner() const;
    float CellDiff() const;

protected:
    RndFont();
    virtual bool HasChar(unsigned short) const;
    virtual void SetASCIIChars(String);

    String GetASCIIChars() const;

    void SetCharInfo(CharInfo *, RndBitmap &, const Vector2 &, int);
    void UpdateChars();
    void SetBitmapSize(const Vector2 &);

    // former RndFontBase members -- offsets unchanged by the collapse
    std::vector<unsigned short> mChars; // 0x28
    bool mMonospace; // 0x34
    float mBaseKerning; // 0x38
    KerningTable *mKerningTable; // 0x3c

    ObjPtrVec<RndMat> mMats; // 0x40
    ObjOwnerPtr<RndFont> mTextureOwner; // 0x5c
    std::map<unsigned short, CharInfo> mCharInfoMap; // 0x68
    Vector2 mCellSize; // 0x80
    float mDeprecatedSize; // 0x88
    std::vector<Vector2> mMaterialOffsets; // 0x8c
    bool mPacked; // 0x98
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

    ObjPtr<RndMat> mMat; // 0x44
    ObjOwnerPtr<RndFont3d> mTextureOwner; // 0x58
    Vector3 mCellSize; // 0x6c
    Vector3 mInvCellSize; // 0x7c
    Vector3 unk8c; // 0x8c
    std::map<unsigned short, CharInfo *> mCharInfoMap; // 0x9c
};

class BitmapLocker {
    friend class RndFont;

public:
    BitmapLocker(RndFont *, int);
    ~BitmapLocker();
    void LoadPage(int);

private:
    RndFont *mFont; // 0x0
    RndTex *mTex; // 0x4
    RndBitmap *mBitmapPtr; // 0x8
    RndBitmap mBitmap; // 0xc
};
