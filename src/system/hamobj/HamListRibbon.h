#pragma once
#include "flow/Flow.h"
#include "hamobj/HamLabel.h"
#include "math/DoubleExponentialSmoother.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Dir.h"
#include "synth/Sound.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

struct UIListElementDrawState;

struct HamListRibbonDrawState {
    HamListRibbonDrawState();

    DoubleExponentialSmoother mSwellSmoother;
#ifdef HX_NATIVE
    UIListElementDrawState *mElemDrawState; // LP64: pointer, not int
#else
    unsigned int mElemDrawState; // ILP32: unsigned int == pointer size
#endif
    float mBigScale;
    bool mSelected;
    bool mHidden;
    bool mActive;
};

/** "Top-level resource object for UILists" */
class HamListRibbon : public RndDir {
    friend class HamNavList;
public:
    enum RibbonMode {
        kRibbonSwell = 0,
        kRibbonSlide = 1,
        kRibbonSelect = 2,
        kRibbonDisengaged = 3
    };
    class ScrollAnims {
    public:
        ScrollAnims(Hmx::Object *owner)
            : mScrollAnim(owner), mScrollActive(owner), mScrollFade(owner),
              mScrollFaded(owner) {}

        void SetScrollFrame(float);
        void SetAnims(int);
        void Save(BinStream &) const;
        void Load(BinStreamRev &);

        ObjPtr<RndAnimatable> mScrollAnim; // 0x0
        ObjPtr<RndAnimatable> mScrollActive; // 0xc
        ObjPtr<RndAnimatable> mScrollFade; // 0x18
        ObjPtr<RndAnimatable> mScrollFaded; // 0x24
    };
    HamListRibbon();
    // Hmx::Object
    virtual ~HamListRibbon() {}
    OBJ_CLASSNAME(HamListRibbon);
    OBJ_SET_TYPE(HamListRibbon);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // ObjectDir
    virtual void SyncObjects() { RndDir::SyncObjects(); }
    // RndDrawable
    virtual void DrawShowing();
    // RndAnimatable
    virtual void SetFrame(float frame, float blend) {
        RndAnimatable::SetFrame(frame, blend);
    }
    virtual float StartFrame();
    virtual float EndFrame();

    OBJ_MEM_OVERLOAD(0x2E)
    NEW_OBJ(HamListRibbon)
    static const int sNumListSelectable;

    void HandleEnter();
    void OnSelectDone();
    void PlayHighlightSound(int);
    void PlaySelectSound(int);
    bool IsScrollable(int) const;
    void Draw(const Transform &, const std::vector<HamListRibbonDrawState> &, bool, bool);
    void SetDisengageFrame(float);

    Sound *SlideSound() const { return mSlideSound; }
    RndAnimatable *SlideSoundAnim() const { return mSlideSoundAnim; }
    Sound *ScrollSound() const { return mScrollSound; }
    RndAnimatable *ScrollSoundAnim() const { return mScrollSoundAnim; }
    RndAnimatable *EnterAnim() const { return mEnterAnim; }
    bool TestEntering() const { return mTestEntering; }
    void SetTestEntering(bool b) { mTestEntering = b; }
    void SetMode(RibbonMode m) { mMode = m; }
    void SetSelectToggle(bool b) { mSelectToggle = b; }

private:
    void ResetAnims(bool);
    void SetAnims(bool, float);
    void DrawRibbon(int, const Transform &, const Transform &, const HamListRibbonDrawState &, int, int, int, bool);
    float GetLabelTotalAlpha() const;

    DataNode OnEnterBlacklightMode(const DataArray *);
    DataNode OnExitBlacklightMode(const DataArray *);

    ScrollAnims mScrollAnims; // 0x1dc
    /** "(Milo only) Draw as a test list?" */
    bool mTestMode; // 0x20c
    /** "(Milo only) If test_mode is on, how many to draw" */
    int mTestNumDisplay; // 0x210
    /** "(Milo only) If test_mode is on, which element is highlighted" */
    int mTestSelectedIndex; // 0x214
    /** "How far apart elements should be spaced" */
    float mSpacing; // 0x218
    /** "Mode for animations" */
    RibbonMode mMode; // 0x21c
    /** "(Milo only) Test enter anim?" */
    bool mTestEntering; // 0x220
    /** "Minimum number of ribbons to show" */
    int mPaddedSize; // 0x224
    /** "Spacing between padded ribbons" */
    float mPaddedSpacing; // 0x228
    bool mSelectToggle; // 0x22c
    ObjPtr<RndAnimatable> mSwellAnim; // 0x230
    ObjPtr<RndAnimatable> mSlideAnim; // 0x23c
    ObjPtr<RndAnimatable> mSelectAnim; // 0x248
    ObjPtr<RndAnimatable> mSelectToggleAnim; // 0x254
    ObjPtr<RndAnimatable> mSelectInactiveAnim; // 0x260
    ObjPtr<RndAnimatable> mSelectAllAnim; // 0x26c
    ObjPtr<RndAnimatable> mDisengageAnim; // 0x278
    ObjPtr<RndAnimatable> mEnterAnim; // 0x284
    /** "Where the label goes" */
    ObjPtr<HamLabel> mLabelPlaceholder; // 0x290
    ObjPtrVec<Flow> mHighlightSounds; // 0x29c
    ObjPtrVec<Flow> mSelectSounds; // 0x2b8
    /** "Flow to play on enter" */
    ObjPtr<Flow> mEnterFlow; // 0x2d4
    ObjPtr<Sound> mSlideSound; // 0x2e0
    ObjPtr<RndAnimatable> mSlideSoundAnim; // 0x2ec
    ObjPtr<Sound> mScrollSound; // 0x2f8
    ObjPtr<RndAnimatable> mScrollSoundAnim; // 0x304
};
