#pragma once
#include "char/CharClip.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/PropAnim.h"
#include "synth/Sound.h"
#include "utl/MemMgr.h"
#include "utl/TextStream.h"

/** "A full lipsync animation, basically a changing set of weights
    for a set of named visemes.  Sampled at 30hz" */
class CharLipSync : public Hmx::Object {
public:
    class Generator {
    public:
        struct Weight {
            unsigned char mPrev;
            unsigned char mCur;
        };

        Generator() : mLipSync(nullptr), mLastCount(0) {}
        void Init(CharLipSync *);
        void AddWeight(int, float);
        void NextFrame();
        void Finish();

    protected:
        void RemoveViseme(int);

        CharLipSync *mLipSync; // 0x0
        int mLastCount; // 0x4
        std::vector<Weight> mWeights; // 0x8
    };

    class PlayBack {
    public:
        struct Weight {
            Weight() : mClip(nullptr), mPrevWeight(0), mNextWeight(0), mCurWeight(0) {}

            ObjPtr<CharClip> mClip;
            float mPrevWeight;
            float mNextWeight;
            float mCurWeight;
        };
        PlayBack();
        void Set(CharLipSync *, ObjPtr<ObjectDir>);
        void SetClips(ObjPtr<ObjectDir>);
        void Reset();
        void Poll(float);

        MEM_OVERLOAD(PlayBack, 0x3F)

        std::vector<Weight> mWeights; // 0x0
        // RAW pointer, not ObjPtr. Retail CharLipSyncDriver::Poll loads the
        // CharLipSync through `lwz r11, 12(r11)` = PlayBack+0xc directly; our
        // ObjPtr<CharLipSync> occupies [0xc,0x18) and stores its raw pointer at
        // +8, so we emitted `lwz r11, 20(r11)`. ObjPtr itself is proven correct
        // (ObjPtr<CharLipSync>::Replace and ??_G both match retail at 100%), so
        // the field at 0xc cannot be an ObjPtr interior. rb3-Wii declares it raw
        // too; DC3 (newer) upgraded it. Sibling Generator::mLipSync above is
        // still raw in DC3, which is the same asymmetry.
        CharLipSync *mLipSync; // 0xc
        ObjPtr<ObjectDir> mClips; // 0x10
        int mIndex; // 0x34
        int mOldIndex; // 0x38
        int mFrame; // 0x3c
    };

    // Hmx::Object
    virtual ~CharLipSync();
    OBJ_CLASSNAME(CharLipSync);
    OBJ_SET_TYPE_ENGINE(CharLipSync);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    OBJ_MEM_OVERLOAD(0x1E)
    NEW_OBJ(CharLipSync)

    float Duration() { return (float)(mFrames - 1) / 30.0f; }
    RndPropAnim *GetPropAnim() const { return mPropAnim; }
    void Print(TextStream &);
    void Parse(DataArray *);

    static void Init();
    static void Terminate();
    static void RegisterLipSync(CharLipSync *);
    static void UnregisterLipSync(CharLipSync *);
    static CharLipSync *FindLipSyncForSound(Sound *);

protected:
    CharLipSync();

    static std::map<Symbol, CharLipSync *> *sLipSyncMap;

    DataNode OnParse(DataArray *);
    DataNode OnParseArray(DataArray *);

    /** "PropAnim to control this lipsync" — retail RB3-360 member, present in
        rb3-Wii (src/system/char/CharLipSync.h: ObjPtr<RndPropAnim> mPropAnim)
        and in the DC3 binary (ham_xbox_r.map: ??_G?$ObjPtr@VRndPropAnim@@@@ in
        char:CharLipSync.obj), but dropped from dc3-decomp's header which we
        inherited. Proven against the retail ctor fn_823C1F80: ObjPtr vtable
        ??_7?$ObjPtr@VRndPropAnim@@VObjectDir@@@@6B@ stored at this+0x28.
        See docs/decomp/research/2026-06-11-bp4-vbase-deep.md. */
    ObjPtr<RndPropAnim> mPropAnim; // 0x28
    /** "viseme names" */
    std::vector<String> mVisemes; // 0x34
    /** "how many keyframes" */
    int mFrames; // 0x40
    std::vector<unsigned char> mData; // 0x44
};
