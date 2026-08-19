#pragma once
#include <list>
#include "obj/ObjMacros.h"
#include "rndobj/Dir.h"
#include "rndobj/Mat.h"

class LayerDir : public RndDir {
public:
    class Layer {
    public:
        Layer(Hmx::Object *o)
            : mName(""), mMat(o, 0), mActive(1), mColorIdx(0), mAlpha(1.0f), mBitmap(""),
              unk40(""), mLayerOptional(0), mAllowColor(1), mColorPalette(o, 0),
              mAllowAlpha(0), mAlphaMin(0.0f), mAlphaMax(1.0f), mProxy(o, 0) {}

        String mName; // 0x0
        ObjPtr<RndMat> mMat; // 0xc
        bool mActive; // 0x18
        Hmx::Color mColor; // 0x1c
        int mColorIdx; // 0x2c
        float mAlpha; // 0x30
        String mBitmap; // 0x34
        FilePath unk40; // 0x40
        bool mLayerOptional; // 0x4c
        bool mAllowColor; // 0x4d
        ObjPtr<Hmx::Object> mColorPalette; // 0x50
        bool mAllowAlpha; // 0x5c
        float mAlphaMin; // 0x60
        float mAlphaMax; // 0x64
        std::list<FilePath> mBitmapList; // 0x68
        ObjPtr<RndDir> mProxy; // 0x70 (obj ptr read at 0x78 in retail)
    };

    LayerDir();
    OBJ_CLASSNAME(LayerDir);
    OBJ_SET_TYPE(LayerDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual ~LayerDir() {}
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void DrawShowing();
    virtual RndCam *CamOverride();

    void RefreshLayer(Layer &, bool);

    DataNode GetBitmapList(DataArray *);
    DataNode RandomizeColors(DataArray *);

    static RndCam *sCam;
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(LayerDir); }
    NEW_OBJ(LayerDir);

    OBJ_MEM_OVERLOAD(0x39);

    ObjList<Layer> mLayers; // 0x1dc
    bool mUseFreeCam; // 0x1e8
};
