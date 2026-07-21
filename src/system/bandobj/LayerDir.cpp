#include "bandobj/LayerDir.h"
#include "rndobj/Cam.h"
#include "rndobj/Tex.h"
#include "rndobj/TexRenderer.h"
#include "obj/Data.h"
#include "math/Rand.h"
#include "os/File.h"
#include "decomp.h"
#include "utl/Symbols.h"
#include <string.h>

INIT_REVS(LayerDir)

DECOMP_FORCEACTIVE(
    LayerDir,
    "colors",
    "color",
    "alpha",
    ".png",
    ".bmp",
    "_norm.png",
    "_spec.png",
    "_norm.bmp",
    "_spec.bmp",
    "ObjPtr_p.h",
    "f.Owner()",
    ""
)

RndCam *LayerDir::sCam;
LayerDir *gLayerDirOwner;

void LayerDir::Init() {
    sCam = Hmx::Object::New<RndCam>();
    sCam->SetFrustum(0.01f, 5.0f, 0.0f, 1.0f);
    Register();
}

LayerDir::LayerDir() : mLayers(this), mUseFreeCam(0) {}

void LayerDir::DrawShowing() {
    if (!mUseFreeCam)
        sCam->Select();
    RndDir::DrawShowing();
}

RndCam *LayerDir::CamOverride() { return mUseFreeCam ? 0 : sCam; }

DECOMP_FORCEBLOCK(LayerDir, (BinStream &bs, const ObjPtr<Hmx::Object> &p),
    bs << p;
)

BinStream &operator>>(BinStream &bs, LayerDir::Layer &layer) {
    bs >> layer.mName;
    layer.mMat.Load(bs, false, 0);
    bs >> layer.mActive;
    bs >> layer.mColor;
    if (LayerDir::gRev > 3)
        bs >> layer.mColorPalette;
    bs >> layer.mAlpha;
    bs >> layer.mBitmap;
    bs >> layer.mLayerOptional;
    bs >> layer.mAllowColor;
    bs >> layer.mAllowAlpha;
    bs >> layer.mAlphaMin;
    bs >> layer.mAlphaMax;
    {
        unsigned int length;
        bs >> length;
        layer.mBitmapList.resize(length);
        for (std::list<FilePath>::iterator it = layer.mBitmapList.begin();
             it != layer.mBitmapList.end();
             it++) {
            bs >> *it;
        }
    }
    if (LayerDir::gRev == 1) {
        bool b;
        bs >> b;
    }
    if (LayerDir::gRev > 1)
        layer.mProxy.Load(bs, false, 0);
    if (LayerDir::gRev > 6)
        bs >> layer.mColorIdx;
    return bs;
}

SAVE_OBJ(LayerDir, 0xC3)

void LayerDir::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(7, 0);
    if (gRev < 5)
        bs >> mLayers;
    if (gRev != 0)
        bs >> mUseFreeCam;
    bs.PushRev(packRevs(gAltRev, gRev), this);
    RndDir::PreLoad(bs);
}

void LayerDir::PostLoad(BinStream &bs) {
    RndDir::PostLoad(bs);
    int revs = bs.PopRev(this);
    gRev = getHmxRev(revs);
    gAltRev = getAltRev(revs);
    if (gRev == 5)
        bs >> mLayers;
    if (gRev > 5 && !IsProxy())
        bs >> mLayers;
}

BEGIN_COPYS(LayerDir)
    COPY_SUPERCLASS(RndDir)
    CREATE_COPY(LayerDir)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mUseFreeCam)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_CUSTOM_PROPSYNC(LayerDir::Layer)
    SYNC_PROP(name, o.mName)
    SYNC_PROP_MODIFY_ALT(mat, o.mMat, gLayerDirOwner->RefreshLayer(o, false))
    SYNC_PROP_MODIFY_ALT(bitmap, o.mBitmap, gLayerDirOwner->RefreshLayer(o, false))
    SYNC_PROP_MODIFY_ALT(active, o.mActive, gLayerDirOwner->RefreshLayer(o, false))
    SYNC_PROP_MODIFY_ALT(color, o.mColor, gLayerDirOwner->RefreshLayer(o, false))
    SYNC_PROP_MODIFY_ALT(color_idx, o.mColorIdx, gLayerDirOwner->RefreshLayer(o, true))
    SYNC_PROP_MODIFY_ALT(alpha, o.mAlpha, gLayerDirOwner->RefreshLayer(o, false))
    SYNC_PROP_MODIFY_ALT(proxy, o.mProxy, gLayerDirOwner->RefreshLayer(o, false))
    SYNC_PROP(layer_optional, o.mLayerOptional)
    SYNC_PROP(allow_color, o.mAllowColor)
    SYNC_PROP_MODIFY_ALT(
        color_palette, o.mColorPalette, gLayerDirOwner->RefreshLayer(o, false)
    )
    SYNC_PROP(allow_alpha, o.mAllowAlpha)
    SYNC_PROP(alpha_min, o.mAlphaMin)
    SYNC_PROP(alpha_max, o.mAlphaMax)
    SYNC_PROP(bitmap_list, o.mBitmapList)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(LayerDir)
    gLayerDirOwner = this;
    SYNC_SUPERCLASS(RndDir)
    SYNC_PROP(layers, mLayers)
    SYNC_PROP(use_free_cam, mUseFreeCam)
END_PROPSYNCS

BEGIN_HANDLERS(LayerDir)
    HANDLE_SUPERCLASS(RndDir)
    HANDLE(get_bitmap_list, GetBitmapList)
    HANDLE(randomize_colors, RandomizeColors)
    HANDLE_CHECK(0x12E)
END_HANDLERS

DataNode LayerDir::RandomizeColors(DataArray *) {
    for (ObjList<Layer>::iterator it = mLayers.begin(); it != mLayers.end(); ++it) {
        Hmx::Object *palette = it->mColorPalette;
        if (palette && it->mAllowColor) {
            int idx = RandomInt(0, palette->Property(Symbol("colors"), true)->Array()->Size());
            const DataArray *arr = palette->Property(Symbol("colors"), true)->Array();
            int packed = arr->Node(idx).Int(arr);
            it->mColor.Unpack(packed);
            RefreshLayer(*it, false);
        }
    }
    return DataNode(0);
}

void LayerDir::RefreshLayer(Layer &layer, bool useColorIdx) {
    if (layer.mMat) {
        if (layer.mActive) {
            if (layer.mAllowColor) {
                if (useColorIdx) {
                    Hmx::Object *palette = layer.mColorPalette;
                    if (palette) {
                        DataArray *arr =
                            palette->Property(Symbol("colors"), true)->Array();
                        int colorIdx = layer.mColorIdx;
                        if (arr->Size() > colorIdx) {
                            const DataArray *arr2 = layer.mColorPalette
                                                        ->Property(Symbol("colors"), true)
                                                        ->Array();
                            int packed = arr2->Node(colorIdx).Int(arr2);
                            layer.mColor.Unpack(packed);
                        }
                    }
                }
                layer.mMat->SetProperty(Symbol("color"), DataNode(layer.mColor.Pack()));
            }
            if (layer.mAllowAlpha) {
                float alphaMin = layer.mAlphaMin;
                float alphaMax = layer.mAlphaMax;
                float a = layer.mAlpha * (alphaMax - alphaMin) + alphaMin;
                Hmx::Object *mat2 = layer.mMat;
                mat2->SetProperty(Symbol("alpha"), DataNode(a));
            } else {
                Hmx::Object *mat2 = layer.mMat;
                mat2->SetProperty(Symbol("alpha"), DataNode(1.0f));
            }
            if (!layer.mProxy) {
                const String& png = layer.mBitmap + ".png";
                String bmp = layer.mBitmap + ".bmp";
                String normPng = layer.mBitmap + "_norm.png";
                String specPng = layer.mBitmap + "_spec.png";
                String normBmp = layer.mBitmap + "_norm.bmp";
                String specBmp = layer.mBitmap + "_spec.bmp";
                for (std::list<FilePath>::iterator it = layer.mBitmapList.begin();
                     it != layer.mBitmapList.end();
                     ++it) {
                    const char *name = FileGetName(it->c_str());
                    if (strcmp(name, png.c_str()) == 0
                        || strcmp(name, bmp.c_str()) == 0
                        || strcmp(name, normPng.c_str()) == 0
                        || strcmp(name, specPng.c_str()) == 0
                        || strcmp(name, normBmp.c_str()) == 0
                        || strcmp(name, specBmp.c_str()) == 0) {
                        FilePath bitmap(it->c_str());
                        layer.mMat->GetDiffuseTex()->SetBitmap(bitmap);
                        break;
                    }
                }
            } else {
                {
                    FilePath fp(layer.unk40.c_str());
                    layer.mProxy->SetProxyFile(fp, false);
                }
                layer.mProxy->Enter();
                layer.mProxy->SetFrame(GetFrame(), 1.0f);
            }
        } else {
            layer.mMat->SetProperty(Symbol("alpha"), DataNode(0.0f));
        }
        const ObjRef &refs = Refs();
        for (ObjRef::iterator it = refs.end(); it != refs.begin();) {
            --it;
            RndTexRenderer *tr =
                dynamic_cast<RndTexRenderer *>(RefPtrOf(it)->RefOwner());
            if (tr)
                tr->SetFrame(tr->GetFrame(), 1.0f);
        }
    }
}

DataNode LayerDir::GetBitmapList(DataArray *arr) {
    char _slotpad[8]; (void)_slotpad;
    DataArray *propPath = DataVariable(Symbol("milo_prop_path")).Array(NULL);
    DataNode savedNode(propPath->Node(2));
    propPath->Node(2) = DataNode(Symbol("name"));
    const char *name = (*reinterpret_cast<Hmx::Object **>(arr))->Property(propPath, true)->Str(NULL);
    for (ObjList<Layer>::iterator it = mLayers.begin(); it != mLayers.end(); ++it) {
        if (strcmp(it->mName.c_str(), name) == 0) {
            DataArray *result = new DataArray(it->mBitmapList.size());
            if (!result)
                result = new DataArray(0);
            std::list<FilePath>::iterator fp = it->mBitmapList.begin();
            int i = 0;
            for (; fp != it->mBitmapList.end(); ++fp, i++) {
                String fileName(FileGetName(fp->c_str()));
                result->Node(i) =
                    DataNode(fileName.substr(0, fileName.length() - 4));
            }
            return DataNode(DataArrayPtr(result));
        }
    }
    return DataNode(DataArrayPtr(new DataArray(0)));
}
