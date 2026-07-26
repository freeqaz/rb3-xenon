#pragma once
#include "meta/FixedSizeSaveable.h"
#include "obj/ObjMacros.h"
#include "rndobj/Dir.h"
#include "rndobj/Group.h"
#include "rndobj/Tex.h"
#include "rndobj/TransAnim.h"
#include "utl/BinStream.h"
#include "world/ColorPalette.h"
#include "utl/IntPacker.h"

class PatchDir; // forward dec

class PatchDescriptor {
public:
    PatchDescriptor() : patchType(0), patchIndex(0) {}
    int patchType; // 0x0 - enum Type, but we don't know the enum names for certain
    int patchIndex; // 0x4
};

class PatchSticker {
public:
    PatchSticker();
    ~PatchSticker();

    void Unload();
    void FinishLoad();
    void MakeLoader();
    void SetOnMat(RndMat *) const;
    void SetIconOnMat(RndMat *) const;
    FileLoader *GetLoader() const { return mLoader; }

    String unk0; // 0x0
    FilePath unkc; // 0xc
    float unk18; // 0x18
    float unk1c; // 0x1c
    int unk20; // 0x20
    bool unk24; // 0x24
    FileLoader *mLoader; // 0x28
    RndTex *mTex; // 0x2c
    RndTex *unk30; // 0x30
};

class PatchLayer : public Hmx::Object {
public:
    PatchLayer();
    virtual ~PatchLayer() {}
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);

    void SavePacked(IntPacker &) const;
    void LoadPacked(IntPacker &);

    void Reset();
    void SetScale(float, float);
    bool AllowColor();
    void SetDefaultColor();
    void SelectFX();
    void FlipX();
    void FlipY();
    void ClearSticker();
    void SetPosition(const Vector3 &);
    void SetRotation(float);
    void SetScaleX(float);
    void SetScaleY(float);
    void SetDeformFrame(float);
    bool HasSticker() const;
    Vector3 Position() const;
    float DeformFrame() const;
    float Rotation() const;
    float ScaleX() const;
    float ScaleY() const;
    PatchSticker *GetSticker(bool) const;
    void Draw();

    static std::vector<Symbol> sCategoryNames;
    static ColorPalette *sColorPalette;
    static RndDir *sResource;
    static RndTransAnim *sTransAnim;
    static RndGroup *sGrpAnim;
    static RndMat *sMat;
    static PatchDir *sStickerOwner;

    static void Init();
    static void InitResources();
    static void Terminate();
    static int PackedBitCount();

    Symbol mStickerCategory; // 0x1c
    int mStickerIdx; // 0x20
    int mColorIdx; // 0x24
    float unk28; // 0x28
    int mPosX; // 0x2c
    int mPosZ; // 0x30
    int mRot; // 0x34
    int mScaleX; // 0x38
    int mScaleY; // 0x3c
    int mDeformFrame; // 0x40
};

class PatchDir : public FixedSizeSaveable, public RndDir {
public:
    PatchDir();
    virtual ~PatchDir();
    virtual void SaveFixed(FixedSizeSaveableStream &) const;
    virtual void LoadFixed(FixedSizeSaveableStream &, int);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Save(BinStream &);
    virtual void Load(BinStream &);
    virtual void DrawShowing();
    virtual RndCam *CamOverride();
    virtual void Poll();

    void LoadStickerData();
    bool HasLayers() const;
    int NumLayers() const;
    int NumLayersUsed() const;
    bool IsLoadingStickers() const;
    int NumLoadingStickers() const;
    void Clear();
    void CacheRenderedTex(RndTex *, bool);
    bool UsesSticker(const PatchSticker *) const;
    PatchLayer &Layer(int);
    int FindEmptyLayer();
    PatchSticker *GetSticker(Symbol, int, bool);
    std::vector<PatchSticker *> *GetStickers(Symbol);
    void LoadStickerTex(PatchSticker *, bool);
    void UnloadStickerTex(PatchSticker *);
    void SaveRemote(BinStream &);
    void LoadRemote(BinStream &);
    void SaveRemote(IntPacker &);
    void LoadRemote(IntPacker &);
    void FakeFill(RndTex *);
    void LoadLayerStickers();
    void CollapseEmptyLayers();

    RndTex *GetTex() const { return mTex; }

    static void Init();
    static void Terminate();
    static int SaveSize(int);

    DECLARE_REVS;
    static int GetCurrentRev() { return gRev; }
    // PatchDir has no own operator new/delete in retail -- it inherits
    // RndDir's OBJ_MEM_OVERLOAD. The shared OBJ_MEM_OVERLOAD lever is
    // noinline (CacheMgr-verified), so retail's `new PatchDir()` call site
    // shows RndDir::StaticClassName()'s guarded-Symbol-static evaluation
    // (discarded) inlined directly ahead of a bare 2-arg MemAlloc(size, 0)
    // call -- same shape as the FXSEND360_NEW lever (synth_xbox/FxSend.h).
    // Confirmed via Ghidra: fn_82279660 constructs Symbol("RndDir") (guard
    // bit + ctor), fn_827BCD38 is the real 2-arg heap allocator.
#if HX_NATIVE
    // Native: operator new must take size_t (unsigned long on LP64).
    static void *operator new(size_t s) {
        (void)RndDir::StaticClassName().Str();
        return MemAlloc(s, __FILE__, __LINE__, "PatchDir", 0);
    }
    static void *operator new(size_t s, void *place) { return place; }
    static void operator delete(void *v) { (MemFree)(v); }
#else
    static void *operator new(unsigned int s) {
        (void)RndDir::StaticClassName().Str();
        return (MemAlloc)(s, 0);
    }
    static void *operator new(unsigned int s, void *place) { return place; }
    static void operator delete(void *v) { (MemFree)(v); }
#endif

    std::vector<PatchLayer> mLayers; // retail 0x1e4
    std::map<Symbol, std::vector<PatchSticker *> > mStickerMap; // retail 0x1f0
    // The retail PatchDir ctor (fn_82279BF0) spaces mStickerMap 0x1f0 ->
    // mStickersLoading 0x20c, i.e. sizeof(std::map) == 0x1c in that TU (the
    // documented per-TU _Rb_tree +4 ODR split), then mTex 0x218 and unk1c0
    // 0x21c.  Rather than turn on RB3_MAP_0x1C -- which would also have to be
    // set in every TU that inlines GetTex(), each with its OWN maps to shift --
    // the same 4 bytes are spent here as an explicit pad.  This is the word
    // that used to sit at the tail as `unk1c4`; sizeof is unchanged (0x258).
    int unk1f0mappad;
    std::vector<PatchSticker *> mStickersLoading; // retail 0x20c
    RndTex *mTex; // retail 0x218
    mutable bool unk1c0; // retail 0x21c
};

BinStream &operator<<(BinStream &, const PatchDescriptor &);
BinStream &operator>>(BinStream &, PatchDescriptor &);