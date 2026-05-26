#pragma once
#include "char/CharBones.h"
#include "char/CharClip.h"
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "rndobj/Highlight.h"
#include "rndobj/Poll.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

/** "Class to play back clips, has a tree view of layers" */
class HamDriver : public RndHighlightable, public CharWeightable, public CharPollable {
public:
    struct Layer {
        Layer() : mBeat(-kHugeFloat) {}
        virtual ~Layer() {}
        virtual void Eval(float) = 0;
        virtual void Play(CharBones &) = 0;
        virtual bool Replace(ObjRef *, Hmx::Object *) = 0;
        virtual CharClip *FirstClip() = 0;
        virtual void OffsetSec(float);

        MEM_OVERLOAD(Layer, 0x27)

        float mBeat; // 0x4 - layer start beat position
        float mWeight; // 0x8 - layer weight/blend value
    };

    struct LayerArray : public Layer {
        LayerArray() { mName[0] = 0; }
        virtual ~LayerArray() { Clear(); }
        virtual void Eval(float);
        virtual void Play(CharBones &);
        virtual bool Replace(ObjRef *, Hmx::Object *);
        virtual CharClip *FirstClip();
        virtual void OffsetSec(float);

        void Clear();

        char mName[0x20]; // 0xc
        std::list<Layer *> mLayers; // 0x2c - child layers
    };

    struct LayerClip : public Layer {
        LayerClip(Hmx::Object *);
        virtual void Eval(float);
        virtual void Play(CharBones &);
        virtual bool Replace(ObjRef *, Hmx::Object *);
        virtual CharClip *FirstClip();
        virtual void OffsetSec(float);

        float mClipBeat; // 0xc - clip playback beat offset
        ObjOwnerPtr<CharClip> mClip; // 0x10 - character clip reference
    };

    // Hmx::Object
    virtual ~HamDriver();
    virtual bool Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(HamDriver);
    OBJ_SET_TYPE(HamDriver);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndHighlightable
    virtual void Highlight();
    // CharPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit() { RndPollable::Exit(); }
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    OBJ_MEM_OVERLOAD(0x18)
    NEW_OBJ(HamDriver)

    void Clear();
    LayerClip *NewLayerClip();
    void OffsetSec(float);
    CharClip *FirstClip();
    LayerArray &Layers() { return mLayers; }
    void SetClipWeightMap();

    friend class HamCharacter;

protected:
    HamDriver();

    float Display(float);
    float DisplayRecurse(Layer *, int, float);
    void SetClipMapRecurse(Layer *);

    /** "The CharBones object to add into." */
    ObjPtr<CharBonesObject> mBones; // 0x30
    LayerArray mLayers; // 0x44
    float mDisplayBeat; // 0x78 - current beat position for display
    std::map<CharClip *, float> mClipTimingMap; // 0x7c - clip timing cache
};
