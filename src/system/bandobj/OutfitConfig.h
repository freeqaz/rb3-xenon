#pragma once
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/TexBlender.h"
#include "bandobj/BandPatchMesh.h"
#include "bandobj/BandCharDesc.h"
#include "math/SHA1.h"
#include "world/ColorPalette.h"

class OutfitConfig : public RndDrawable {
public:
    class Piercing {
    public:
        class Piece {
        public:
            Piece(Hmx::Object *o) : mAttachment(o, 0), mHighlight(0), mVert(-1) {}

            ObjPtr<RndTransformable> mAttachment; // 0x0
            bool mHighlight; // 0xc
            int mVert; // 0x10
            std::vector<unsigned short> unk14; // 0x14
        };

        Piercing(Hmx::Object *);
        RndMesh *GetHeadMesh();
        void Deform(SyncMeshCB *);

        ObjPtr<RndTransformable> mPiercing; // 0x0
        Transform unkc; // 0xc
        bool mReskin; // 0x3c
        ObjVector<Piece> mPieces; // 0x40
    };

    class MatSwap {
    public:
        MatSwap(Hmx::Object *);
        void SyncTwoColor();
        bool MatchesPatchCategory(int, ObjVector<BandPatchMesh> &);
        void SwapResource();
        void UnSwapResource();
        void Compose(int *, ObjVector<BandPatchMesh> &, int);
        bool Compress(BandCharDesc *);

        ObjPtr<RndMat> mMat; // 0x0
        ObjPtr<RndMat> mResourceMat; // 0xc
        // DO NOT REORDER THESE THREE. Order confirmed twice over: the rb3-Wii
        // Bank 5 debug DWARF gives MatSwap byte_size 0x70 with mTwoColorDiffuse
        // @0x18 / mTwoColorInterp @0x24 / mTwoColorMask @0x30, and operator>>
        // gates the FIRST TWO slots behind `gRev < 5` while reading the third
        // ungated -- and it matches retail at 100%, which it could not if the
        // 360 had permuted these members.
        // Tried and REVERTED (lane X23): rotating the declarations to
        // Mask/Diffuse/Interp to explain Compose's reads. It knocks
        // MatSwap::Compress and operator>>(BinStream&, MatSwap&) off 100%
        // (-2 whole-binary) because both walk these slots in sequence, and it
        // buys Compose only +0.009pp. The rotation is NOT the explanation.
        //
        // Retail's Compose really does read them rotated -- it composites
        // slot 0x2c (interp), then 0x38 (mask), then 0x20 (diffuse), applying
        // mColor2Palette to INTERP where this source applies it to diffuse.
        // Confirmed independently on the Wii DOL, which is rotated the same
        // way, so this is a defect shared by both decomps, not a 360 quirk.
        // Left unfixed here deliberately: it is one of several co-located
        // semantic defects in Compose (see below) that want fixing together as
        // a deliberate re-derivation, not piecemeal for a percentage.
        //
        // NOTE, and do not re-derive the wrong version: the four per-layer
        // stores to sMat+0x28 are mBlend, NOT mColorModFlags. There is NO
        // BaseMaterial layout divergence -- mBlend@0x28, mZMode@0x3c and
        // mTexWrap@0x48 all agree between retail and our build. Retail
        // composites the layers by BLEND MODE, writing 1/3/3/6 =
        // kBlendSrc / kBlendSrcAlpha / kBlendSrcAlpha / kBlendMultiply before
        // the four DrawRects, i.e. it calls SetBlend where this source calls
        // SetColorModFlags (which is why our RT collapses to the last layer --
        // the near-white-eyeballs symptom the HX_NATIVE comment in Compose
        // describes). 6 is not even a legal ColorModFlags value; it is
        // kBlendMultiply. Lane X23 first mis-read +0x28 as mColorModFlags with
        // shifted enum values; that attribution is WRONG. Full evidence:
        // /home/free/tmp/laneX23-ghidra/COMPOSE_CHAIN_GHIDRA_AUDIT.md S2-S5.
        ObjPtr<RndTex> mTwoColorDiffuse; // 0x18
        ObjPtr<RndTex> mTwoColorInterp; // 0x24
        ObjPtr<RndTex> mTwoColorMask; // 0x30
        ObjPtr<ColorPalette> mColor1Palette; // 0x3c
        int mColor1Option; // 0x48
        ObjPtr<ColorPalette> mColor2Palette; // 0x4c
        int mColor2Option; // 0x58
        ObjVector<ObjPtr<RndTex> > mTextures; // 0x5c
        bool mTwoColor; // 0x68
    };

    class MeshAO {
    public:
        class Seam {
        public:
            int mIndex; // 0x0
            int mCoeff; // 0x4
        };

        void Apply(OutfitConfig *, SyncMeshCB *);

        String mMeshName; // 0x0
        String unkc; // 0xc
        std::vector<int> mCoeffs; // 0x18
        std::vector<Seam> mSeams; // 0x20
    };

    class Overlay {
    public:
        Overlay(Hmx::Object *);

        int mCategory; // 0x0
        ObjPtr<RndTex> mTexture; // 0x4
    };

    OutfitConfig();
    OBJ_CLASSNAME(OutfitConfig);
    OBJ_SET_TYPE(OutfitConfig);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Mats(std::list<class RndMat *> &, bool);
    virtual void ListDrawChildren(std::list<RndDrawable *> &);
    virtual void DrawPreClear();
    virtual void UpdatePreClearState();
    virtual ~OutfitConfig() {}
    virtual void PreSave(BinStream &);
    virtual void PostSave(BinStream &);

    unsigned int OverlayFlags() const;
    int NumColorOptions() const;
    void CompressTextures();
    void Recompose();
    void RecomposePatches(int);
    void Randomize();
    void SetColors(const int *);
    BandCharDesc *FindBandCharDesc();
    void ApplyAO(SyncMeshCB *);
    int NumIndices(int) const;
    void SetSkinTextures();
    bool InMilo();
    void PoseBones();

    static RndMat *sMat;
    static RndCam *sCam;
    static BandCharDesc *sBandCharDesc;
    static void SetSkinTextures(ObjectDir *, ObjectDir *, BandCharDesc *);
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(OutfitConfig); }
    NEW_OBJ(OutfitConfig);
    static void Terminate();

    static unsigned short gRev;
    static unsigned short gAltRev;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;

    int mColors[3]; // 0x20, 0x24, 0x28
    ObjVector<MatSwap> mMats; // 0x2c
    int unk38; // 0x38
    int unk3c; // 0x3c
    std::vector<MeshAO> mMeshAO; // 0x40
    bool mComputeAO; // 0x48
    ObjVector<BandPatchMesh> mPatches; // 0x4c
    bool mPermaProject; // 0x58
    ObjVector<Piercing> mPiercings; // 0x5c
    ObjPtr<RndTexBlender> mTexBlender; // 0x68
    ObjPtr<RndTexBlender> mWrinkleBlender; // 0x74
    ObjVector<Overlay> mOverlays; // 0x80
    ObjPtr<RndMat> mBandLogo; // 0x8c
    CSHA1::Digest mDigest; // 0x98
};

class OldMatOption {
public:
    OldMatOption(Hmx::Object *o)
        : mMat(o, 0), mPrimaryPalette(o, 0), mSecondaryPalette(o, 0), mTexs(o) {}

    ObjPtr<RndMat> mMat; // 0x0
    ObjPtr<ColorPalette> mPrimaryPalette; // 0xc
    ObjPtr<ColorPalette> mSecondaryPalette; // 0x18
    ObjVector<ObjPtr<RndTex> > mTexs; // 0x24
};

class OldColorOption {
public:
    OldColorOption(Hmx::Object *o) : mColorIndex(0), mMatOptions(o) {}

    int mColorIndex; // 0x0
    ObjList<OldMatOption> mMatOptions; // 0x4
};