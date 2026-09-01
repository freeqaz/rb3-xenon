#pragma once
#include "HamListRibbon.h"
#include "HamNavProvider.h"
#include "HamScrollBehavior.h"
#include "gesture/DirectionGestureFilter.h"
#include "gesture/HandHeightGestureFilter.h"
#include "gesture/Skeleton.h"
#include "hamobj/HamScrollSpeedIndicator.h"
#include "math/DoubleExponentialSmoother.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/JoypadMsgs.h"
#include "rndobj/Anim.h"
#include "ui/ResourceDirPtr.h"
#include "ui/UIComponent.h"
#include "ui/UIListDir.h"
#include "ui/UIListProvider.h"
#include "ui/UIListState.h"
#include "ui/UIListWidget.h"
#include "ui/UIScreen.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"

class HamNavList;

DECLARE_MESSAGE(NavSelectMsg, "nav_select")
NavSelectMsg(Symbol, int, HamNavList *, bool);
END_MESSAGE

DECLARE_MESSAGE(NavHighlightMsg, "nav_highlight")
NavHighlightMsg(Symbol, int, HamNavList *, bool);
END_MESSAGE

DECLARE_MESSAGE(NavHighlightSettledMsg, "nav_highlight_settled")
NavHighlightSettledMsg(Symbol, int, HamNavList *, bool);
END_MESSAGE

/** "List of navigation actions controlled by a single hand with gestures" */
class HamNavList : public UIComponent,
                   public RndAnimatable,
                   public UIListProvider,
                   public UIListStateCallback,
                   public SkeletonCallback {
public:
    enum NavInputType {
        kNavInput_RightHand = 0,
        kNavInput_LeftHand = 1
    };

    // Hmx::Object
    virtual ~HamNavList();
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(HamNavList);
    OBJ_SET_TYPE(HamNavList);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndDrawable
    virtual void DrawShowing();
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    virtual bool CanHaveFocus();
    // RndAnimatable
    virtual float StartFrame();
    virtual float EndFrame();
    // UIListProvider
    virtual int NumData() const;
    // UIListStateCallback
    virtual void StartScroll(const UIListState &, int, bool);
    virtual void CompleteScroll(const UIListState &);
    // SkeletonCallback
    virtual void Clear();
    virtual void Update(const struct SkeletonUpdateData &) {}
    virtual void PostUpdate(const struct SkeletonUpdateData *);
    virtual void Draw(const BaseSkeleton &, class SkeletonViz &);

    OBJ_MEM_OVERLOAD(0x26)
    NEW_OBJ(HamNavList)
    void Refresh();
    void HandleHighlightChanged(int);
    void PlayScrollSound();
    void StopScrollSound();
    void SetScrollSoundFrame(float);
    void SetNavProvider(HamNavProvider *);
    Symbol GetSelectedSym() const;
    void ScrollToIndex(int, int);
    void PlayEnterAnim();
    void ScrollSubList(int, int);
    void ScrollSubListToIndex(int, int);
    bool IsDataHeader(int);
    void SetProvider(UIListProvider *);
    void PushBackBigElement(Symbol);
    void EraseBigElement(int);
    void SetHighButtonMode(bool);
    void AddRibbonSinks(Hmx::Object *, Symbol);
    void RemoveRibbonSinks(Hmx::Object *, Symbol);
    void DoSelectFor(int);
    void SendHighlightMsg(int);
    void SendHighlightSettledMsg(int);
    float CalculateSwell(int) const;
    void ClearBigElements();
    void HideItem(int, bool);
    void SetProviderNavItemLabels(int, DataArray *);
    void DrawDebug() const;

    void Enable() { mEnabled = true; }
    void Disable() { mEnabled = false; }
    bool Enabled() const { return mEnabled; }
    HamNavProvider *GetHelpbarProvider() { return mNavProvider; }
    void Disengage();
    void SetSkeletonTrackingID(int id) { mSkeletonTrackingID = id; }
    void UpdateGestures(const Skeleton *);

    static void Init();
    static bool sLastSelectInControllerMode;
    static bool sForceDisengage;

    friend class HamScrollBehavior;

private:
    void LinkRibbonDrawState(std::vector<HamListRibbonDrawState> &, UIListWidgetDrawState &);
    void SetRibbonMode(HamListRibbon::RibbonMode);
    void SetHighlight(int);
    void SetSliding(float);
    void SetSelecting(bool);
    bool SkipPoll() const;
    void RealRefresh();
    void SetSwelling();
    bool ShouldSkipSelectAnim(DataNode &) const;
    bool ShouldSkipSelectSound(DataNode &) const;
    int NumItems() const;
    int GetDisabledCount(int) const;
    int GetHighlightItem(void) const;
    bool IsElementBig(int) const;
    void DetermineHighlightedItem();
    bool InControllerMode() const;
    float GetTargetSwellAmount(int);

    static const int sListStateMaxDisplay;
    static float sSlideSmoothAmount;
    static float sSlideTrendAmount;

    DataNode OnMsg(const ButtonDownMsg &);
#ifdef HX_NATIVE
    DataNode OnMsg(const UITransitionCompleteMsg &);
#endif

protected:
    UICOMP_DC3_VIRTUAL void OldResourcePreload(BinStream &);

    HamNavList();

    void Update();
    void SetControllerFocus(int);

    NavInputType mNavInputType; // 0x15c
    std::vector<UIListWidget *> mListWidgets; // 0x160
    UIListState mListState; // 0x16c
    std::vector<HamListRibbonDrawState> mRibbonDrawStates; // 0x1b4
    /** "Mode for animations" */
    HamListRibbon::RibbonMode mRibbonMode; // 0x1c0
    bool unkc8; // 0x1c4
    /** "HamListRibbon resource file" */
    ResourceDirPtr<HamListRibbon> mListRibbonResource; // 0x1c8
    /** "HamListRibbon resource file" */
    ResourceDirPtr<HamListRibbon> mHeaderRibbonResource; // 0x1d8
    /** "UIListDir resource file" */
    ResourceDirPtr<UIListDir> mListDirResource; // 0x1e8
    /** "HamScrollSpeedIndicator resource file" */
    ResourceDirPtr<HamScrollSpeedIndicator> mScrollSpeedIndicatorResource; // 0x1f8
    ObjPtr<HamNavProvider> mNavProvider; // 0x208
    ObjPtr<RndAnimatable> mScrollSpeedAnim; // 0x214
    bool mPendingEnterAnim; // 0x220
    /** "Skip the enter anim altogether" */
    bool mSkipEnterAnim; // 0x221
    /** "Don't automatically play the enter anim when this component enters" */
    bool mSuppressAutomaticEnter; // 0x222
    bool mTestEnteringOverride; // 0x223
    float mHandHeight; // 0x224
    DoubleExponentialSmoother mSlideSmoother; // 0x228
    DoubleExponentialSmoother mDisengageSmoother; // 0x23c
    DirectionGestureFilter *mDirectionGestureFilter; // 0x250
    HandHeightGestureFilter *mHandHeightFilter; // 0x254
    int mSkeletonTrackingID; // 0x258
    HamScrollBehavior mScrollBehavior;
    bool mDisableSlideSound; // 0x2b0
    bool mDisableSelectSound; // 0x2b1
    bool mEnabled; // 0x2b2
    bool mSelectionEnabled; // 0x2b3
    /** "Automatically tie this navlist to the active skeleton" */
    bool mAlwaysUseActiveSkeleton; // 0x2b4
    /** "This list can only be used when it is focused" */
    bool mOnlyUseWhenFocused; // 0x2b5
    float mScrollSettleTime; // 0x2b8
    bool mRefreshPending; // 0x2bc
    Symbol mSelectDoneSymbol; // 0x2c0
    int mSelectDoneIndex;
    bool mSelectDoneSelecting;
    bool mWasInDoubleUserMode;
    bool mHighButtonMode;
    /** "Elements that match these will be bigger than the other elements" */
    std::vector<Symbol> mBigElements; // 0x2cc
    std::vector<unsigned int> mBigElementIndices; // 0x2d8
};
