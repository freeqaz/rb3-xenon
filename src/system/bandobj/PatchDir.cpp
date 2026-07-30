#include "bandobj/PatchDir.h"
#include "decomp.h"
#include "math/Rand.h"
#include "math/Rot.h"
#include "obj/Object.h"
#include "rndobj/Mat.h"
#include "ui/UI.h"
#include "utl/Loader.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include <functional>

std::vector<Symbol> PatchLayer::sCategoryNames;
PatchDir *PatchLayer::sStickerOwner;
RndDir *PatchLayer::sResource;
RndMat *PatchLayer::sMat;
RndGroup *PatchLayer::sGrpAnim;
RndTransAnim *PatchLayer::sTransAnim;
ColorPalette *PatchLayer::sColorPalette;
INIT_REVS(PatchDir);

float hackyScaleValue = 1.0f;

BinStream &operator<<(BinStream &bs, const PatchDescriptor &d) {
    bs << d.patchType;
    bs << d.patchIndex;
    return bs;
}

BinStream &operator>>(BinStream &bs, PatchDescriptor &d) {
    int i;
    bs >> i;
    d.patchType = i;
    bs >> d.patchIndex;
    return bs;
}

PatchSticker::PatchSticker()
    : unk18(1.0f), unk1c(1.0f), unk20(0), unk24(1), mLoader(0), mTex(0), unk30(0) {}

PatchSticker::~PatchSticker() { Unload(); }

void PatchSticker::MakeLoader() {
    MILO_ASSERT(!mLoader, 0x52);
    mLoader = dynamic_cast<FileLoader *>(TheLoadMgr.AddLoader(unkc, kLoadFront));
}

void PatchSticker::FinishLoad() {
    MILO_ASSERT(mLoader, 0x5B);
    MILO_ASSERT(!mTex, 0x5C);
    mTex = Hmx::Object::New<RndTex>();
    unk30 = Hmx::Object::New<RndTex>();
    RndBitmap bmap;
    const char *buf = mLoader->GetBuffer(0);
    RELEASE(mLoader);
    if (buf) {
        RndBitmap other;
        other.Create((void *)buf);
        RndBitmap *prev = 0;
        RndBitmap *cur = &other;
        {
            RndBitmap *next;
            goto mip_check;
            do {
                prev = cur;
                cur = next;
            mip_check:
                next = cur->nextMip();
                if (!next)
                    break;
                int minDim = next->Width();
                if (next->Height() < minDim)
                    minDim = next->Height();
                if (minDim < 0x40)
                    break;
            } while (true);
        }
        if (prev)
            prev->DetachMip();
        RndBitmap *detached = cur->DetachMip();
        unk30->SetBitmap(*cur, 0, true);
        cur->SetMip(detached);
        if (prev)
            prev->SetMip(cur);
        other.SetMip(0);
        mTex->SetBitmap(other, 0, true);
    }
}

void PatchSticker::Unload() {
    RELEASE(mLoader);
    RELEASE(mTex);
    RELEASE(unk30);
}

void PatchSticker::SetOnMat(RndMat *mat) const {
    MILO_ASSERT(mat, 0x92);
    mat->SetDiffuseTex(mTex);
    mat->SetAlpha(mTex ? 1.0f : 0.0f);
    mat->SetBlend(RndMat::kPreMultAlpha);
}

void PatchSticker::SetIconOnMat(RndMat *mat) const {
    MILO_ASSERT(mat, 0x9c);
    mat->SetDiffuseTex(unk30);
    mat->SetAlpha(unk30 ? 1.0f : 0.0f);
    mat->SetBlend(RndMat::kPreMultAlpha);
}

void PatchLayer::Init() {
    DataArray *cfg = SystemConfig("art_maker", "stickers");
    for (int i = 1; i < cfg->Size(); i++) {
        sCategoryNames.push_back(cfg->Array(i)->ForceSym(0));
    }
    InitResources();
}

void PatchLayer::InitResources() {
    DataArray *cfg = SystemConfig();
    DataArray *artMakerArr = cfg->FindArray("art_maker", false);
    if (artMakerArr) {
        DataArray *patchLayerArr = artMakerArr->FindArray("patch_layer", false);
        if (patchLayerArr) {
            sResource = dynamic_cast<RndDir *>(DirLoader::LoadObjects(
                FilePath(FileGetPath(patchLayerArr->File()), patchLayerArr->Str(1)),
                0,
                0
            ));
            MILO_ASSERT(sResource, 0xBA);
            sMat = sResource->Find<RndMat>("patch.mat", true);
            sTransAnim = sResource->Find<RndTransAnim>("root.tnm", true);
            sGrpAnim = sResource->Find<RndGroup>("warp.grp", true);
            sColorPalette = sResource->Find<ColorPalette>("sticker.pal", true);
        }
    }
}

void PatchLayer::Terminate() {
    RELEASE(sResource);
    sMat = 0;
}

PatchLayer::PatchLayer() : mStickerCategory(gNullStr), mStickerIdx(0), unk28(0) {
    Reset();
}

BEGIN_COPYS(PatchLayer)
    CREATE_COPY(PatchLayer)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mColorIdx)
        COPY_MEMBER(mDeformFrame)
        COPY_MEMBER(mPosX)
        COPY_MEMBER(mPosZ)
        COPY_MEMBER(mRot)
        COPY_MEMBER(mScaleX)
        COPY_MEMBER(mScaleY)
        COPY_MEMBER(mStickerCategory)
        COPY_MEMBER(mStickerIdx)
    END_COPYING_MEMBERS
END_COPYS

void PatchLayer::Reset() {
    mColorIdx = 0;
    SetPosition(Vector3(0, 0, 0));
    SetRotation(0);
    SetScaleX(1.0f);
    SetScaleY(1.0f);
    SetDeformFrame(0);
    unk28 = 0;
}

PatchSticker *PatchLayer::GetSticker(bool b) const {
    if (mStickerCategory.Null())
        return 0;
    else
        return sStickerOwner->GetSticker(mStickerCategory, mStickerIdx, b);
}

inline bool PatchLayer::HasSticker() const { return mStickerCategory.Str() != gNullStr; }
DECOMP_FORCEBLOCK(PatchDir, (bool (PatchLayer::*fp)() const),
    fp = &PatchLayer::HasSticker;
)

void PatchLayer::SelectFX() { unk28 = TheTaskMgr.UISeconds() + 0.5f; }

void PatchLayer::ClearSticker() {
    Reset();
    Symbol s(0);
    mStickerCategory = s;
}

void PatchLayer::FlipX() {
    SetScaleX(-1.0f * (mScaleX * (1 / 1638.3f) - 5.0f));
    float r = fmod(360.0f - (360.0f * mRot / 511.0f), 360.0);
    if (r < 0.0f)
        r += 360.0f;
    SetRotation(r);
}

void PatchLayer::FlipY() {
    SetScaleX(-1.0f * (mScaleX * (1 / 1638.3f) - 5.0f));
    float rot = 360.0f * mRot / 511.0f;
    float r = fmod(90.0f + (360.0f - (rot - 90.0f)), 360.0);
    if (r < 0.0f)
        r += 360.0f;
    SetRotation(r);
}

void PatchLayer::SetScale(float x, float y) {
    SetScaleX(1.0f / x);
    SetScaleY(1.0f / y);
}

void PatchLayer::SetDefaultColor() {
    PatchSticker *sticker = GetSticker(false);
    if (sticker)
        mColorIdx = sticker->unk20;
}

bool PatchLayer::AllowColor() {
    PatchSticker *sticker = GetSticker(false);
    MILO_ASSERT(sticker, 0x127);
    return sticker->unk24;
}

void PatchLayer::Draw() {
    PatchSticker *sticker = GetSticker(LOADMGR_EDITMODE);
    if (sticker) {
        sticker->SetOnMat(sMat);
        sMat->SetColor(sColorPalette->GetColor(mColorIdx));
        Transform tf50;
        tf50.Reset();
        tf50.v.Set((float)mPosX, 0, (float)mPosZ);
        Vector3 vb4(0, ((float)mRot * 360.0f / 511.0f) * DEG2RAD, 0);
        MakeRotMatrix(vb4, tf50.m, true);
        float scale = (float)mScaleX * (1 / 1638.3f) - 5.0f;
        hackyScaleValue = scale;
        if (0 > scale) {
            scale = ((float)mScaleX * (1 / 1638.3f) - 5.0f) * -1.0f;
        }
        float scaleX = sticker->unk18 * scale * 7.5f;
        float scaleZ = sticker->unk1c * ((float)mScaleY * (1 / 1638.3f) - 5.0f) * 7.5f;
        Scale(Vector3(scaleX, 1.0f, scaleZ), tf50.m, tf50.m);
        float blend = 1.0f;
        Transform tfa8;
        tfa8.Reset();
        if (scale != (float)mScaleX * (1 / 1638.3f) - 5.0f) {
            Scale(Vector3(-1.0f, 1.0f, 1.0f), tfa8.m, tfa8.m);
            sMat->SetTexXfm(tfa8);
        } else
            sMat->SetTexXfm(tfa8);
        float uisec = TheTaskMgr.UISeconds();
        if (unk28 >= uisec)
            sTransAnim->SetFrame((unk28 - uisec) * 40.0f + 100.0f, blend);
        else
            sTransAnim->SetFrame(0, blend);
        float deform = (float)mDeformFrame * (1 / 20.46f);
        if (deform != sGrpAnim->GetFrame()) {
            sGrpAnim->SetFrame(deform, blend);
        }
        sResource->SetLocalXfm(tf50);
        sResource->DrawShowing();
    }
}

BEGIN_HANDLERS(PatchLayer)
    HANDLE_EXPR(color_palette, sColorPalette)
    HANDLE_EXPR(has_sticker, !mStickerCategory.Null())
    HANDLE_ACTION(set_scale, SetScale(_msg->Float(2), _msg->Float(3)))
    HANDLE_EXPR(allow_color, AllowColor())
    HANDLE_ACTION(set_default_color, SetDefaultColor())
    HANDLE_ACTION(select_fx, SelectFX())
    HANDLE_ACTION(flip_x, FlipX())
    HANDLE_ACTION(flip_y, FlipY())
    HANDLE_ACTION(clear_sticker, ClearSticker())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x185)
END_HANDLERS

BEGIN_PROPSYNCS(PatchLayer)
    static Symbol sticker_category("sticker_category");
    SYNC_PROP_MODIFY(sticker_category, mStickerCategory, mStickerIdx = 0)
    static Symbol sticker_idx("sticker_idx");
    SYNC_PROP(sticker_idx, mStickerIdx)
    static Symbol color_idx("color_idx");
    SYNC_PROP(color_idx, mColorIdx)
END_PROPSYNCS

void PatchDir::Init() {
    PatchLayer::Init();
    TheDebug.AddExitCallback(Terminate);
}

void PatchDir::Terminate() { PatchLayer::Terminate(); }

BEGIN_COPYS(PatchDir)
    CREATE_COPY(PatchDir)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mLayers)
        if (ty == kCopyDeep && c->HasLayers()) {
            CacheRenderedTex(c->mTex, false);
        }
    END_COPYING_MEMBERS
    unk1c0 = true;
END_COPYS

PatchDir::PatchDir() : unk1c0(0) {
    mSaveSizeMethod = &SaveSize;
    mLayers.resize(50);
    mTex = Hmx::Object::New<RndTex>();
    mTex->SetMipMapK(666.0f);
    if (LOADMGR_EDITMODE)
        LoadStickerData();
}

PatchDir::~PatchDir() {
    for (std::map<Symbol, std::vector<PatchSticker *> >::iterator it = mStickerMap.begin();
         it != mStickerMap.end();
         ++it) {
        DeleteAll(it->second);
    }
    PatchLayer::sStickerOwner = 0;
    mLayers.clear();
    delete mTex;
}

int PatchDir::SaveSize(int) {
    int bits = PatchLayer::PackedBitCount() * 50;
    int size = bits / 8;
    if (bits % 8 != 0) size++;
    // Retail RB3-360 ground truth: the folded constant is 0x10234 (lis 1 / ori 0x234),
    // i.e. 532 + 0x10020. rb3-Wii's DEV decomp carries 0x10021 (== 0x10235) -- one too
    // high for this binary. 0x10020 is also the exact bitmap payload size LoadFixed
    // reads in the no-layers branch, so the composition is layer-bits + bitmap.
    size += 0x10020;
    REPORT_SIZE("PatchDir", size);
}

void PatchDir::Clear() {
    std::for_each(mLayers.begin(), mLayers.end(), std::mem_fun_ref(&PatchLayer::Reset));
    unk1c0 = true;
}

void PatchDir::CacheRenderedTex(RndTex *tex, bool b) {
    MILO_ASSERT(tex->Width() > 0 && tex->Height() > 0, 0x1EF);
    RndBitmap bmap;
    tex->LockBitmap(bmap, true);
    mTex->SetBitmap(bmap, 0, true);
    tex->UnlockBitmap();
    if (b)
        mTex->Compress((RndTex::AlphaCompress)1);
}

#define kStickerCategoryBits_2 8
#define kStickerIdxBits_4 6
#define kColorIdxBits 6

void PatchLayer::SavePacked(IntPacker &packer) const {
    int stickerCategoryIndex = -1;
    if (!mStickerCategory.Null()) {
        std::vector<Symbol>::iterator it =
            std::find(sCategoryNames.begin(), sCategoryNames.end(), mStickerCategory);
        if (it != sCategoryNames.end()) {
            stickerCategoryIndex = it - sCategoryNames.begin();
        }
    }
    MILO_ASSERT(stickerCategoryIndex < (1 << kStickerCategoryBits_2), 0x279);
    packer.AddS(stickerCategoryIndex, 8);
    MILO_ASSERT(mStickerIdx < (1 << kStickerIdxBits_4), 0x27C);
    packer.AddU(mStickerIdx, 6);
    MILO_ASSERT(mColorIdx < (1 << kColorIdxBits), 0x27F);
    packer.AddU(mColorIdx, 6);
    packer.AddS(mPosX, 9);
    packer.AddS(mPosZ, 9);
    packer.AddU(mRot, 9);
    packer.AddU(mScaleX, 14);
    packer.AddU(mScaleY, 14);
    packer.AddU(mDeformFrame, 10);
}

void PatchLayer::LoadPacked(IntPacker &packer) {
    int count = PatchDir::gRev > 4 ? packer.ExtractS(8) : packer.ExtractS(6);
    if (count < 0 || count >= sCategoryNames.size()) {
        mStickerCategory = gNullStr;
    } else
        mStickerCategory = sCategoryNames[count];
    if (PatchDir::gRev <= 3)
        mStickerIdx = packer.ExtractU(5);
    else
        mStickerIdx = packer.ExtractU(6);
    mColorIdx = packer.ExtractU(6);
    if (PatchDir::gRev == 1) {
        Vector3 pos;
        pos.x = packer.ExtractS(9);
        pos.z = packer.ExtractS(9);
        pos.y = 0;
        SetPosition(pos);
        SetRotation((int)packer.ExtractU(9) * 360.0f * (1.0f / 512.0f));
        int x = packer.ExtractU(14);
        int y = packer.ExtractU(14);
        SetScaleX(x * 0.00030517578125f);
        SetScaleY(y * 0.00030517578125f);
        SetDeformFrame((int)packer.ExtractU(10) * 0.048828125f);
    } else {
        mPosX = packer.ExtractS(9);
        mPosZ = packer.ExtractS(9);
        mRot = packer.ExtractU(9);
        mScaleX = packer.ExtractU(14);
        mScaleY = packer.ExtractU(14);
        mDeformFrame = packer.ExtractU(10);
    }
}

int PatchLayer::PackedBitCount() { return 85; }

Vector3 PatchLayer::Position() const { return Vector3(mPosX, 0, mPosZ); }

void PatchLayer::SetPosition(const Vector3 &v) {
    mPosX = v.x;
    mPosZ = v.z;
}

float PatchLayer::Rotation() const { return (mRot * 360.0f) / 511.0f; }

void PatchLayer::SetRotation(float r) {
    for (; r > 360.0f; r -= 360.0f)
        ;
    for (; r < -360.0f; r += 360.0f)
        ;
    MILO_ASSERT(r >= -360.0f, 0x302);
    MILO_ASSERT(r <= 360.0f, 0x303);
    if (r < 0)
        r += 360.0f;
    mRot = r * 1.4194444f;
}

float PatchLayer::ScaleX() const { return mScaleX * (1 / 1638.3f) - 5.0f; }

void PatchLayer::SetScaleX(float scaleX) {
    MILO_ASSERT(scaleX >= -5.0f, 0x314);
    MILO_ASSERT(scaleX <= 5.0f, 0x315);
    mScaleX = (scaleX + 5.0f) * 1638.3f;
}

float PatchLayer::ScaleY() const { return mScaleY * (1 / 1638.3f) - 5.0f; }

void PatchLayer::SetScaleY(float scaleY) {
    MILO_ASSERT(scaleY >= -5.0f, 0x321);
    MILO_ASSERT(scaleY <= 5.0f, 0x322);
    mScaleY = (scaleY + 5.0f) * 1638.3f;
}

float PatchLayer::DeformFrame() const { return mDeformFrame * (1 / 20.46f); }

void PatchLayer::SetDeformFrame(float df) {
    MILO_ASSERT(df >= 0.0f, 0x32E);
    MILO_ASSERT(df <= 50.0f, 0x32F);
    mDeformFrame = df * 20.46f;
}

BinStream &operator>>(BinStream &bs, PatchLayer &layer) {
    MILO_ASSERT(PatchDir::GetCurrentRev() == 0, 0x337);
    bs >> layer.mStickerCategory;
    bs >> layer.mStickerIdx;
    bs >> layer.mColorIdx;
    Vector3 v;
    bs >> v;
    layer.SetPosition(v);
    float rot;
    bs >> rot;
    layer.SetRotation(rot);
    float x;
    bs >> x;
    layer.SetScaleX(x);
    float y;
    bs >> y;
    layer.SetScaleY(y);
    float frame;
    bs >> frame;
    layer.SetDeformFrame(frame);
    return bs;
}

void PatchDir::Save(BinStream &bs) {
    bs << 5;
    SaveRemote(bs);
}

BEGIN_LOADS(PatchDir)
    LOAD_REVS(bs)
    ASSERT_REVS(5, 0)
    if (gRev == 0)
        bs >> mLayers;
    else
        LoadRemote(bs);
END_LOADS

#define kPatchBufSize 0x830

void PatchDir::SaveRemote(BinStream &bs) {
    char buf[0x830];
    IntPacker packer(buf, 0x830);
    packer.AddU(0, 0x10);
    SaveRemote(packer);
    unsigned int size = packer.mPos >> 3 & 0xFFFF;
    if (packer.mPos & 7)
        size = size + 1 & 0xFFFF;
    packer.SetPos(0);
    packer.AddU(size, 0x10);
    MILO_ASSERT(size < kPatchBufSize, 0x37B);
    bs.Write(buf, size);
}

void PatchDir::LoadRemote(BinStream &bs) {
    gRev = 5;
    char buf[2];
    IntPacker packer(buf, 2);
    unsigned short read;
    if (gRev < 3) {
        bs.Read(buf, 1);
        read = 1;
    } else {
        bs.Read(buf, 2);
        read = 2;
    }
    unsigned int size = packer.ExtractU(read << 3);
    size &= 0xFFFF;
    MILO_ASSERT(size < kPatchBufSize, 0x394);
    char buf2[0x830];
    IntPacker packer2(buf2, size);
    bs.Read(buf2, size - read);
    LoadRemote(packer2);
}

void PatchDir::SaveFixed(FixedSizeSaveableStream &stream) const {
    char buf[0x830];
    IntPacker packer(buf, 0x830);
    for (unsigned int i = 0; i < mLayers.size(); i++) {
        mLayers[i].SavePacked(packer);
    }
    unsigned int size = packer.mPos >> 3 & 0xFFFF;
    if (packer.mPos & 7)
        size = size + 1 & 0xFFFF;
    stream.Write(buf, size);
    if (HasLayers()) {
        bool b = true;
        stream.Write(&b, 1);
        RndBitmap bmap;
        mTex->LockBitmap(bmap, 1);
        bmap.Save(stream);
        mTex->UnlockBitmap();
    } else {
        bool b = false;
        stream.Write(&b, 1);
        char *empty = new char[0x10020];
        memset(empty, 0, 0x10020);
        stream.Write(empty, 0x10020);
        delete[] empty;
    }
    unk1c0 = false;
}

void PatchDir::LoadFixed(FixedSizeSaveableStream &stream, int) {
    char buf[2];
    IntPacker packer(buf, 2);
    char buf2[0x830];
    IntPacker packer2(buf2, 0x830);
    int bits = PatchLayer::PackedBitCount() * 50;
    int size = bits / 8;
    if (bits % 8 != 0)
        size++;
    stream.Read(buf2, size);
    gRev = 5;
    for (unsigned int i = 0; i < mLayers.size(); i++) {
        mLayers[i].LoadPacked(packer2);
    }
    char hasLayers;
    stream.Read(&hasLayers, 1);
    if (hasLayers > 0) {
        RndBitmap bmap;
        bmap.Load(stream);
        mTex->SetBitmap(bmap, 0, true);
    } else {
        char *empty = new char[0x10020];
        stream.Read(empty, 0x10020);
        delete[] empty;
    }
    unk1c0 = false;
}

void PatchDir::SaveRemote(IntPacker &packer) {
    unsigned char size = mLayers.size();
    packer.AddU(size, 8);
    for (unsigned int i = 0; i < size; i++) {
        packer.AddBool(!mLayers[i].mStickerCategory.Null());
        if (!mLayers[i].mStickerCategory.Null()) {
            mLayers[i].SavePacked(packer);
        }
    }
}

void PatchDir::LoadStickerData() {
    MILO_ASSERT(mStickerMap.empty(), 0x4A9);
    DataArray *config = SystemConfig("art_maker", "stickers");
    for (int i = 1; i < config->Size(); i++) {
        DataArray *categoryArr = config->Array(i);
        Symbol category = categoryArr->ForceSym(0);
        MILO_ASSERT(mStickerMap.find(category) == mStickerMap.end(), 0x4B2);
        std::vector<PatchSticker *> stickers;
        for (int j = 2; j < categoryArr->Size(); j++) {
            PatchSticker *sticker = new PatchSticker();
            DataArray *stickerArr = categoryArr->Array(j);
            Symbol sizeX("size_x");
            sticker->unk18 = stickerArr->FindArray(sizeX, true)->Float(1);
            Symbol sizeY("size_y");
            sticker->unk1c = stickerArr->FindArray(sizeY, true)->Float(1);
            Symbol paletteIndex("palette_index");
            sticker->unk20 = stickerArr->FindArray(paletteIndex, true)->Int(1);
            Symbol allowColor("allow_color");
            sticker->unk24 = stickerArr->FindArray(allowColor, true)->Int(1) != 0;
            sticker->unk0 = stickerArr->Str(0);
            Symbol texPath("tex_path");
            sticker->unkc.Set(
                FileGetPath(stickerArr->File()),
                stickerArr->FindArray(texPath, true)->Str(1)
            );
            stickers.push_back(sticker);
        }
        MILO_ASSERT(!stickers.empty(), 0x4C5);
        mStickerMap[category] = stickers;
    }
    PatchLayer::sStickerOwner = this;
}

void PatchDir::FakeFill(RndTex *tex) {
    mLayers.clear();
    if (tex)
        CacheRenderedTex(tex, false);
    for (int i = 0; i < 50; i++) {
        PatchLayer layer;
        Symbol category =
            PatchLayer::sCategoryNames[RandomInt(0, PatchLayer::sCategoryNames.size())];
        layer.mStickerCategory = category;
        int numStickers = SystemConfig("art_maker", "stickers", category)->Size() - 2;
        layer.mStickerIdx = RandomInt(0, numStickers);
        MILO_ASSERT(layer.mStickerIdx < numStickers, 0x51E);
        layer.mColorIdx = RandomInt(0, PatchLayer::sColorPalette->NumColors());
        if (RandomInt(0, 2))
            layer.FlipX();
        if (RandomInt(0, 2))
            layer.FlipY();
        layer.SetDeformFrame(RandomFloat(0.0f, 50.0f));
        layer.SetRotation(RandomFloat(0.0f, 360.0f));
        layer.SetScaleX(RandomFloat(0.25f, 2.0f));
        layer.SetScaleY(RandomFloat(0.25f, 2.0f));
        Vector3 pos(RandomFloat(-250.0f, 250.0f), 0.0f, RandomFloat(-200.0f, 200.0f));
        layer.SetPosition(pos);
        mLayers.push_back(layer);
    }
}

void PatchDir::LoadRemote(IntPacker &packer) {
    unsigned char numLayers = packer.ExtractU(8);
    mLayers.resize(numLayers);
    for (unsigned int i = 0; i < numLayers; i++) {
        if (packer.ExtractBool()) {
            mLayers[i].LoadPacked(packer);
        }
    }
    int destIndex = 0;
    while (destIndex < numLayers && !mLayers[destIndex].mStickerCategory.Null()) {
        destIndex++;
    }
    for (int srcIndex = destIndex + 1; srcIndex < numLayers; srcIndex++) {
        if (!mLayers[srcIndex].mStickerCategory.Null()) {
            MILO_ASSERT(destIndex < numLayers, 0x3CD);
            MILO_ASSERT(!mLayers[destIndex].HasSticker(), 0x3CE);
            mLayers[destIndex] = mLayers[srcIndex];
            mLayers[srcIndex].Reset();
            mLayers[srcIndex].mStickerCategory = gNullStr;
            mLayers[srcIndex].mStickerIdx = 0;
            destIndex++;
        }
    }
}

void PatchDir::DrawShowing() {
    TheUI->GetCam()->Select();
    for (std::vector<PatchLayer>::iterator it = mLayers.begin(); it != mLayers.end();
         ++it) {
        (*it).Draw();
    }
}

RndCam *PatchDir::CamOverride() { return TheUI->GetCam(); }

bool PatchDir::HasLayers() const {
    for (std::vector<PatchLayer>::const_iterator it = mLayers.begin();
         it != mLayers.end();
         ++it) {
        if (!(*it).mStickerCategory.Null())
            return true;
    }
    return false;
}

int PatchDir::NumLayers() const { return mLayers.size(); }

int PatchDir::NumLayersUsed() const {
    int count = 0;
    for (std::vector<PatchLayer>::const_iterator it = mLayers.begin();
         it != mLayers.end();
         ++it) {
        if (!(*it).mStickerCategory.Null())
            count++;
    }
    return count;
}

bool PatchDir::UsesSticker(const PatchSticker *sticker) const {
    for (std::vector<PatchLayer>::const_iterator it = mLayers.begin();
         it != mLayers.end();
         ++it) {
        if ((*it).GetSticker(false) == sticker)
            return true;
    }
    return false;
}

bool PatchDir::IsLoadingStickers() const { return (int)mStickersLoading.size() > 0; }
int PatchDir::NumLoadingStickers() const { return mStickersLoading.size(); }
PatchLayer &PatchDir::Layer(int idx) { return mLayers[idx]; }

int PatchDir::FindEmptyLayer() {
    int idx = 0;
    for (std::vector<PatchLayer>::iterator it = mLayers.begin(); it != mLayers.end();
         ++it) {
        if ((*it).mStickerCategory.Null()) {
            return idx;
        } else
            idx++;
    }
    return -1;
}

void PatchDir::LoadLayerStickers() {
    for (std::vector<PatchLayer>::iterator it = mLayers.begin(); it != mLayers.end();
         ++it) {
        PatchSticker *sticker = (*it).GetSticker(false);
        if (sticker) {
            LoadStickerTex(sticker, true);
        }
    }
}

void PatchDir::CollapseEmptyLayers() {
    int destIndex = 0;
    int byteOffset = 0;
    for (std::vector<PatchLayer>::iterator it = mLayers.begin(); it != mLayers.end();
         ++it) {
        if (!(*it).mStickerCategory.Null()) {
            int empty = FindEmptyLayer();
            if (empty >= 0 && empty < destIndex) {
                mLayers[empty] = mLayers[destIndex];
                mLayers[destIndex].ClearSticker();
            }
        }
        destIndex++;
        byteOffset += 0x44;
    }
}

std::vector<PatchSticker *> *PatchDir::GetStickers(Symbol category) {
    std::map<Symbol, std::vector<PatchSticker *> >::iterator it =
        mStickerMap.find(category);
    MILO_ASSERT(it != mStickerMap.end(), 0x490);
    return &it->second;
}

PatchSticker *PatchDir::GetSticker(Symbol category, int ix, bool b) {
    MILO_ASSERT(!mStickerMap.empty(), 0x497);
    std::map<Symbol, std::vector<PatchSticker *> >::iterator it =
        mStickerMap.find(category);
    MILO_ASSERT(it != mStickerMap.end(), 0x49A);
    std::vector<PatchSticker *> *stickers = &it->second;
    MILO_ASSERT(ix >= 0 && ix < stickers->size(), 0x49D);
    PatchSticker *sticker = (*stickers)[ix];
    if (b && !sticker->mTex) {
        LoadStickerTex(sticker, false);
    }
    return sticker;
}

void PatchDir::Poll() {
    std::vector<PatchSticker *>::iterator it = mStickersLoading.begin();
    while (it != mStickersLoading.end()) {
        PatchSticker *sticker = *it;
        MILO_ASSERT(sticker->GetLoader(), 0x4DD);
        if (sticker->GetLoader()->IsLoaded()) {
            sticker->FinishLoad();
            it = mStickersLoading.erase(it);
        } else {
            ++it;
        }
    }
}
__declspec(noinline) void _outline_MakeLoader(PatchSticker *_obj) {
    _obj->MakeLoader();
}

void PatchDir::LoadStickerTex(PatchSticker *sticker, bool push) {
    if (sticker->mTex || sticker->mLoader)
        return;
    _outline_MakeLoader(sticker);
    MILO_ASSERT(sticker->GetLoader(), 0x4EE);
    if (push)
        mStickersLoading.push_back(sticker);
    else {
        TheLoadMgr.PollUntilLoaded(sticker->mLoader, 0);
        sticker->FinishLoad();
    }
}

void PatchDir::UnloadStickerTex(PatchSticker *sticker) {
    if (UsesSticker(sticker))
        return;
    if ((int)sticker->mLoader) {
        std::vector<PatchSticker *>::iterator it =
            std::find(mStickersLoading.begin(), mStickersLoading.end(), sticker);
        MILO_ASSERT(it != mStickersLoading.end(), 0x504);
        mStickersLoading.erase(it);
    }
    sticker->Unload();
}

BEGIN_HANDLERS(PatchDir)
    HANDLE_EXPR(has_layers, HasLayers())
    HANDLE_ACTION(clear, Clear())
    HANDLE_EXPR(is_loading_stickers, (int)mStickersLoading.size() > 0)
    HANDLE_EXPR(get_tex, mTex)
    HANDLE_SUPERCLASS(RndDir)
    HANDLE_CHECK(0x544)
END_HANDLERS

// sw3 scatter-include (default/PatchDir <- bandobj/BandCamShot.cpp) [ObjMacros owner]
// SW_SCATTER_OWNER_INCLUDE keeps BandCamShot's own scatter-includes inert here.
#define SW_SCATTER_OWNER_INCLUDE
#include "bandobj/BandCamShot.cpp"
#undef SW_SCATTER_OWNER_INCLUDE
