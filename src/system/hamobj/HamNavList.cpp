#include "hamobj/HamNavList.h"
#include "HamListRibbon.h"
#include "HamScrollBehavior.h"
#include "flow/PropertyEventProvider.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/GestureMgr.h"
#include "gesture/Skeleton.h"
#include "gesture/SkeletonUpdate.h"
#include "gesture/SkeletonViz.h"
#include "hamobj/HamNavProvider.h"
#include "math/Utl.h"
#include "hamobj/HamScrollSpeedIndicator.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/JoypadMsgs.h"
#include "os/System.h"
#include "rndobj/Anim.h"
#include "rndobj/Overlay.h"
#include "rndobj/Trans.h"
#include "synth/Sound.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "ui/UIList.h"
#include "ui/UIListProvider.h"
#include "ui/UIListState.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "meta/MetaMusicManager.h"
#include "rndobj/Cam.h"
#include "rndobj/Rnd.h"
#include "rndobj/Utl.h"
#include "ui/Utl.h"
#include "math/Color.h"
#include "math/Geo.h"
#include "world/Instance.h"
#include <stdio.h>

DECLARE_MESSAGE(LeftHandListEngagementMsg, "left_hand_list_engagement")
LeftHandListEngagementMsg(bool b);
END_MESSAGE

const int HamNavList::sListStateMaxDisplay = HamListRibbon::sNumListSelectable + 6;
bool HamNavList::sForceDisengage;
#ifdef HX_NATIVE
float HamNavList::sSlideTrendAmount;
float HamNavList::sSlideSmoothAmount;
bool HamNavList::sLastSelectInControllerMode;
#endif

NavSelectMsg::NavSelectMsg(Symbol sym, int index, HamNavList *list, bool selecting)
    : Message(Type(), sym, index, (Hmx::Object *)list, selecting) {}

NavHighlightMsg::NavHighlightMsg(Symbol sym, int index, HamNavList *list, bool canSelect)
    : Message(Type(), sym, index, (Hmx::Object *)list, canSelect) {}

NavHighlightSettledMsg::NavHighlightSettledMsg(
    Symbol sym, int index, HamNavList *list, bool canSelect
)
    : Message(Type(), sym, index, (Hmx::Object *)list, canSelect) {}

HamNavList::HamNavList()
    : mNavInputType(kNavInput_RightHand), mListState(this, this),
      mRibbonMode(HamListRibbon::kRibbonSlide), unkc8(0), mListRibbonResource(this),
      mHeaderRibbonResource(this), mListDirResource(this),
      mScrollSpeedIndicatorResource(this), mNavProvider(this), mScrollSpeedAnim(this),
      mPendingEnterAnim(0), mSkipEnterAnim(0), mSuppressAutomaticEnter(0), mTestEnteringOverride(0), mHandHeight(0),
      mSlideSmoother(0, 10, 10), mDisengageSmoother(0, 10, 0), mDirectionGestureFilter(NULL), mHandHeightFilter(NULL), mSkeletonTrackingID(0),
      mScrollBehavior(this, &mListState), mDisableSlideSound(0), mDisableSelectSound(0),
      mEnabled(1), mSelectionEnabled(1), mAlwaysUseActiveSkeleton(1), mOnlyUseWhenFocused(1),
      mScrollSettleTime(0), mRefreshPending(0), mSelectDoneIndex(-1), mWasInDoubleUserMode(0), mHighButtonMode(0) {
    mListState.SetSpeed(0);
    mListState.SetSelected(0, -1, true);
    SetRate(k30_fps_ui);
}

HamNavList::~HamNavList() {
#ifdef HX_NATIVE
    if (ObjectDir::InDeleteObjects()) {
        mListWidgets.clear();
        delete mDirectionGestureFilter;
        delete mHandHeightFilter;
        return;
    }
#endif
    DeleteAll(mListWidgets);
    SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();
    if (handle.HasCallback(this)) {
        handle.RemoveCallback(this);
    }
    delete mDirectionGestureFilter;
    delete mHandHeightFilter;
    if (mListRibbonResource) {
        Sound *slideSound = mListRibbonResource->SlideSound();
        if (slideSound)
            slideSound->Stop(nullptr, false);
    }
}

bool HamNavList::Replace(ObjRef *ref, Hmx::Object *obj) {
    return RndTransformable::Replace(ref, obj);
}

BEGIN_HANDLERS(HamNavList)
    HANDLE_ACTION(set_provider, SetProvider(_msg->Obj<UIListProvider>(2)))
    HANDLE_ACTION(set_highlight, SetHighlight(_msg->Int(2)))
    HANDLE_ACTION(set_selected, mListState.SetSelected(_msg->Int(2), -1, true))
    HANDLE_ACTION(set_swelling, SetSwelling())
    HANDLE_ACTION(set_sliding, SetSliding(_msg->Float(2)))
    HANDLE_ACTION(set_selecting, SetSelecting(false))
    HANDLE_EXPR(get_selected, mListState.Selected())
    HANDLE_EXPR(get_selected_sym, GetSelectedSym())
    HANDLE_EXPR(is_scrolling_settled, mScrollSettleTime <= 0)
    HANDLE_ACTION(scroll_to_index, ScrollToIndex(_msg->Int(2), _msg->Int(3)))
    HANDLE_EXPR(get_top_index, mListState.FirstShowing())
    HANDLE_ACTION(refresh, mRefreshPending = true)
    HANDLE_ACTION(set_controller_focus, SetControllerFocus(_msg->Int(2)))
    HANDLE_ACTION(play_enter_anim, PlayEnterAnim())
    HANDLE_ACTION(enable_navigation, mEnabled = true)
    HANDLE_ACTION(disable_navigation, mEnabled = false)
    HANDLE_ACTION(enable_selection, mSelectionEnabled = true)
    HANDLE_ACTION(disable_selection, mSelectionEnabled = false)
    HANDLE_ACTION(scroll_sublist, ScrollSubList(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(
        scroll_sublist_to_index, ScrollSubListToIndex(_msg->Int(2), _msg->Int(3))
    )
    HANDLE_ACTION(push_back_big_element, PushBackBigElement(_msg->Sym(2)))
    HANDLE_ACTION(pop_back_big_element, mBigElements.pop_back())
    HANDLE_ACTION(erase_big_element, EraseBigElement(_msg->Int(2)))
    HANDLE_ACTION(push_back_big_element_index, mBigElementIndices.push_back(_msg->Int(2)))
    HANDLE_ACTION(pop_back_big_element_index, mBigElementIndices.pop_back())
    HANDLE_EXPR(is_data_header, IsDataHeader(_msg->Int(2)))
    HANDLE_EXPR(get_num_display, mListState.NumDisplay())
    HANDLE_EXPR(data_index, mListState.Provider()->DataIndex(_msg->Sym(2)))
    HANDLE_EXPR(data_symbol, mListState.Provider()->DataSymbol(_msg->Int(2)))
    HANDLE_EXPR(index_enabled, mListState.Provider()->IsActive(_msg->Int(2)))
    HANDLE_MESSAGE(ButtonDownMsg)
#ifdef HX_NATIVE
    HANDLE_MESSAGE(UITransitionCompleteMsg)
#endif
    HANDLE_SUPERCLASS(UIComponent)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(HamNavList)
    SYNC_PROP_MODIFY(list_ribbon_resource, mListRibbonResource, Update())
    SYNC_PROP_MODIFY(header_ribbon_resource, mHeaderRibbonResource, Update())
    SYNC_PROP_MODIFY(list_dir_resource, mListDirResource, Update())
    SYNC_PROP_MODIFY(
        scroll_speed_indicator_resource, mScrollSpeedIndicatorResource, Update()
    )
    SYNC_PROP_SET(mode, mRibbonMode, SetRibbonMode((HamListRibbon::RibbonMode)_val.Int()))
    SYNC_PROP_SET(
        nav_provider, mNavProvider.Ptr(), SetNavProvider(_val.Obj<HamNavProvider>())
    )
    SYNC_PROP(disable_select_sound, mDisableSelectSound)
    SYNC_PROP(disable_slide_sound, mDisableSlideSound)
    SYNC_PROP(skeleton_tracking_id, mSkeletonTrackingID)
    SYNC_PROP(enabled, mEnabled)
    SYNC_PROP(always_use_active_skeleton, mAlwaysUseActiveSkeleton)
    SYNC_PROP(only_use_when_focused, mOnlyUseWhenFocused)
    SYNC_PROP_SET(nav_input_type, mNavInputType, mNavInputType = (NavInputType)_val.Int())
    SYNC_PROP(scroll_speed_anim, mScrollSpeedAnim)
    SYNC_PROP(suppress_automatic_enter, mSuppressAutomaticEnter)
    SYNC_PROP(big_elements, mBigElements)
    SYNC_PROP(skip_enter_anim, mSkipEnterAnim)
    SYNC_SUPERCLASS(UIComponent)
    SYNC_SUPERCLASS(RndAnimatable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(HamNavList)
    SAVE_REVS(10, 0)
    SAVE_SUPERCLASS(UIComponent)
    SAVE_SUPERCLASS(RndAnimatable)
    bs << mListRibbonResource;
    bs << mListDirResource;
    bs << mNavProvider;
    bs << mDisableSelectSound;
    bs << mDisableSlideSound;
    bs << mEnabled;
    bs << mAlwaysUseActiveSkeleton;
    bs << mNavInputType;
    bs << mOnlyUseWhenFocused;
    bs << mScrollSpeedAnim;
    bs << mSuppressAutomaticEnter;
    bs << mBigElements;
    bs << mHeaderRibbonResource;
    bs << mScrollSpeedIndicatorResource;
    bs << mSkipEnterAnim;
END_SAVES

BEGIN_COPYS(HamNavList)
    COPY_SUPERCLASS(UIComponent)
    CREATE_COPY(HamNavList)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mListDirResource)
        COPY_MEMBER(mNavProvider)
        COPY_MEMBER(mListRibbonResource)
        COPY_MEMBER(mDisableSelectSound)
        COPY_MEMBER(mDisableSlideSound)
        COPY_MEMBER(mEnabled)
        COPY_MEMBER(mAlwaysUseActiveSkeleton)
        COPY_MEMBER(mOnlyUseWhenFocused)
        COPY_MEMBER(mNavInputType)
        COPY_MEMBER(mScrollSpeedAnim)
        COPY_MEMBER(mSuppressAutomaticEnter)
        COPY_MEMBER(mBigElements)
        COPY_MEMBER(mHeaderRibbonResource)
        COPY_MEMBER(mScrollSpeedIndicatorResource)
        COPY_MEMBER(mSkipEnterAnim)
    END_COPYING_MEMBERS
    Update();
END_COPYS

BEGIN_LOADS(HamNavList)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

INIT_REVS(10, 0)

void HamNavList::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(10, 0)
    UIComponent::PreLoad(bs);
    if (d.rev >= 2) {
        LOAD_SUPERCLASS(RndAnimatable)
    }
    if (d.rev >= 1) {
        bs >> mListRibbonResource;
        bs >> mListDirResource;
    } else {
        char buf[0x100];
        bs.ReadString(buf, 0x100);
        mListDirResource.SetName(buf, true);
    }
    bs >> mNavProvider;
    SetNavProvider(mNavProvider);
    if (d.rev >= 3) {
        d >> mDisableSelectSound;
        d >> mDisableSlideSound;
        d >> mEnabled;
        d >> mAlwaysUseActiveSkeleton;
        d >> (BinStreamEnum<NavInputType> &)mNavInputType;
    }
    if (d.rev >= 5) {
        d >> mOnlyUseWhenFocused;
    }
    if (d.rev >= 4) {
        bs >> mScrollSpeedAnim;
    }
    if (d.rev >= 6) {
        d >> mSuppressAutomaticEnter;
    }
    if (d.rev >= 7) {
        d >> mBigElements;
    }
    if (d.rev >= 8) {
        bs >> mHeaderRibbonResource;
    }
    if (d.rev >= 9) {
        bs >> mScrollSpeedIndicatorResource;
    }
    if (d.rev >= 10) {
        d >> mSkipEnterAnim;
    }
    d.PushRev(this);
}

void HamNavList::PostLoad(BinStream &bs) {
    bs.PopRev(this);
    UIComponent::PostLoad(bs);
    mListDirResource.PostLoad(nullptr);
    mListRibbonResource.PostLoad(nullptr);
    mHeaderRibbonResource.PostLoad(nullptr);
    mScrollSpeedIndicatorResource.PostLoad(nullptr);
    Update();
}

void HamNavList::SetControllerFocus(int i1) {
    if (TheGestureMgr && TheGestureMgr->InControllerMode()) {
        SetHighlight(i1);
    }
}

void HamNavList::Init() {
    REGISTER_OBJ_FACTORY(HamNavList);
    DataArray *cfg = SystemConfig("ui");
    cfg->FindData("slide_smooth_amount", sSlideSmoothAmount, false);
    cfg->FindData("slide_trend_amount", sSlideTrendAmount, false);
    HamScrollBehavior::Init();
}

void HamNavList::PushBackBigElement(Symbol element) { mBigElements.push_back(element); }
void HamNavList::EraseBigElement(int idx) {
    mBigElements.erase(mBigElements.begin() + idx);
}

bool HamNavList::SkipPoll() const {
    float uiSeconds = (float)TheTaskMgr.UISeconds();
    if (mScrollSettleTime < uiSeconds - 0.5f) {
        return true;
    }
    return mNavInputType == kNavInput_RightHand && mOnlyUseWhenFocused
        && TheUI->FocusComponent() != this;
}

void HamNavList::Refresh() { mRefreshPending = true; }

void HamNavList::SetHighButtonMode(bool b) {
    mHighButtonMode = b;
    if (mDirectionGestureFilter) {
        mDirectionGestureFilter->SetHighButtonMode(b);
    }
}

int HamNavList::NumData() const { return 18; }

void HamNavList::SetSwelling() {
    if (mRefreshPending)
        RealRefresh();

    if (mRibbonMode != HamListRibbon::kRibbonSelect) {
        if (mRibbonMode == HamListRibbon::kRibbonDisengaged) {
            SetHighlight(mListState.Selected());
        }
        SetRibbonMode(HamListRibbon::kRibbonSwell);
        float uiSeconds = TheTaskMgr.DeltaUISeconds();
        mSlideSmoother.Smooth(0.0f, uiSeconds);
    }
}

bool HamNavList::CanHaveFocus() { return mNavInputType == kNavInput_RightHand; }

void HamNavList::Poll() {
    UIComponent::Poll();

    if (mRefreshPending) {
        RealRefresh();
    }

    // Poll list dir widgets if loaded
    if (mListDirResource) {
        mListDirResource->PollWidgets(mListWidgets);
    }

    if (SkipPoll()) {
        // When skipping poll but ribbon resource exists, reset slide sound anim
        if (mListRibbonResource) {
            RndAnimatable *slideSoundAnim = mListRibbonResource->SlideSoundAnim();
            if (slideSoundAnim) {
                slideSoundAnim->SetFrame(0.0f, 1.0f);
            }
        }
        mDirectionGestureFilter->ClearSwipe();
        return;
    }

    // Check if gesture mode changed and update if needed
    if (TheGestureMgr && !TheLoadMgr.EditMode()) {
        if (TheGestureMgr->InDoubleUserMode() != mWasInDoubleUserMode) {
            Update();
        }
    }

    // Update skeleton tracking ID if using active skeleton
    if (mAlwaysUseActiveSkeleton) {
        mSkeletonTrackingID = TheGestureMgr->GetActiveSkeletonTrackingID();
    }

    // Get skeleton and update gestures
    Skeleton *skeleton = TheGestureMgr->GetSkeletonByTrackingID(mSkeletonTrackingID);
    if (skeleton && skeleton->IsValid() && !skeleton->IsSideways() && !sForceDisengage) {
        UpdateGestures(skeleton);

        // Update scroll speed indicator if present
        if (mScrollSpeedIndicatorResource) {
            if (mRibbonMode != HamListRibbon::kRibbonDisengaged) {
                if (!mListState.ScrollPastMinDisplay() && mScrollSpeedIndicatorResource->IsShowing()) {
                    mScrollSpeedIndicatorResource->Show(false);
                } else if (mRibbonMode == HamListRibbon::kRibbonSwell
                           && !mScrollSpeedIndicatorResource->IsShowing()
                           && mListState.ScrollPastMinDisplay()) {
                    mScrollSpeedIndicatorResource->Show(true);
                } else {
                    float scrollSpeed = mHandHeightFilter->mHandHeight;
                    float scrollDownCap = HamScrollBehavior::mScrollDownCap;
                    float scrollUpCap = HamScrollBehavior::mScrollUpCap;
                    mScrollSpeedIndicatorResource->Update(scrollSpeed, scrollUpCap, scrollDownCap);
                }
            }
        }
    } else {
        // No valid skeleton - check if we should disengage
        bool inVoiceMode = TheGestureMgr ? TheGestureMgr->InVoiceMode() : false;
        if (!inVoiceMode) {
            Disengage();

            // Hide scroll speed indicator if present
            if (mScrollSpeedIndicatorResource) {
                if (mScrollSpeedIndicatorResource->IsShowing()) {
                    mScrollSpeedIndicatorResource->Show(false);
                }
            }
        }
    }

    // Update swipe direction debug overlay
    if (mRibbonMode != HamListRibbon::kRibbonDisengaged) {
        RndOverlay *swipeOverlay = RndOverlay::Find("swipe_direction", true);
        swipeOverlay->SetCallback(mDirectionGestureFilter);
    }

    // Play enter anim if pending
    if (mPendingEnterAnim) {
        mPendingEnterAnim = false;
        PlayEnterAnim();
    }

    // Determine highlighted item based on mode
    if (mRibbonMode == HamListRibbon::kRibbonSwell) {
        bool inControllerMode = TheGestureMgr ? TheGestureMgr->InControllerMode() : false;
        if (!inControllerMode) {
            bool inVoiceMode = TheGestureMgr ? TheGestureMgr->InVoiceMode() : false;
            if (!inVoiceMode && !TheLoadMgr.EditMode()) {
                DetermineHighlightedItem();
            }
        }
    }

    // Check if we should clear scroll tracking
    if (mRibbonMode == HamListRibbon::kRibbonDisengaged) {
        bool inControllerMode = TheGestureMgr ? TheGestureMgr->InControllerMode() : false;
        if (!inControllerMode) {
            bool inVoiceMode = TheGestureMgr ? TheGestureMgr->InVoiceMode() : false;
            if (inVoiceMode) {
                mScrollBehavior.mScrollDir = 0;
            }
        }
    }

    // If disengaged and in controller mode, switch to swell
    if (mRibbonMode == HamListRibbon::kRibbonDisengaged) {
        bool inControllerMode = TheGestureMgr ? TheGestureMgr->InControllerMode() : false;
        if (inControllerMode) {
            SetRibbonMode(HamListRibbon::kRibbonSwell);
        }
    }

    // Update scroll behavior if scrollable
    if (mListRibbonResource && mListState.Provider()
        && mListRibbonResource->IsScrollable(mListState.NumShowing()) && !sForceDisengage) {
        mScrollBehavior.Update(mHandHeightFilter->mHandHeight);
    }

    // Update slide sound anim based on mode
    if (mListRibbonResource) {
        if (mRibbonMode == HamListRibbon::kRibbonSlide
            && !mListRibbonResource->TestEntering()) {
            float level = mSlideSmoother.Level();
            RndAnimatable *slideSoundAnim = mListRibbonResource->SlideSoundAnim();
            if (slideSoundAnim) {
                slideSoundAnim->SetFrame(level, 1.0f);
            }
        } else {
            RndAnimatable *slideSoundAnim = mListRibbonResource->SlideSoundAnim();
            if (slideSoundAnim) {
                slideSoundAnim->SetFrame(0.0f, 1.0f);
            }
        }
    }

    // Update each ribbon draw state swell amount
    for (unsigned int i = 0; i < mRibbonDrawStates.size(); i++) {
        float deltaUI = TheTaskMgr.DeltaUISeconds();
        float targetSwell = GetTargetSwellAmount(i);
        mRibbonDrawStates[i].mSwellSmoother.Smooth(targetSwell, deltaUI);
    }

    // Smooth the secondary smoother based on mode
    if (mRibbonMode == HamListRibbon::kRibbonDisengaged) {
        mDisengageSmoother.Smooth(1.0f, TheTaskMgr.DeltaUISeconds());
    } else {
        mDisengageSmoother.Smooth(0.0f, TheTaskMgr.DeltaUISeconds());
    }

    // Handle select mode completion
    if (mRibbonMode == HamListRibbon::kRibbonSelect) {
#ifdef HX_NATIVE
        // On native, ribbon animations never settle without Kinect input.
        // Skip the IsAnimating() check to avoid soft-lock after selection.
        if (!TheUI->InTransition() && !TheLoadMgr.EditMode()) {
#else
        if (!RndAnimatable::IsAnimating() && !TheUI->InTransition()
            && !TheLoadMgr.EditMode()) {
#endif
            SetRibbonMode(HamListRibbon::kRibbonSwell);

            // Reset all draw state smoothers
            for (unsigned int i = 0; i < mRibbonDrawStates.size(); i++) {
                mRibbonDrawStates[i].mSwellSmoother.SetParams(0.0f, 0.0f, 0.0f);
            }

            // Send nav_select_done message
            if (mSelectDoneIndex != -1) {
                UIListProvider *provider = mListState.Provider();
                MILO_ASSERT(provider, 0x185);

                static Message navSelectDoneMsg(
                    Symbol("nav_select_done"), DataNode(0), DataNode(0), DataNode(0), DataNode(0)
                );
                navSelectDoneMsg->Node(2) = DataNode(mSelectDoneSymbol);
                navSelectDoneMsg->Node(3) = DataNode(mSelectDoneIndex);
                navSelectDoneMsg->Node(4) = DataNode(this);
                navSelectDoneMsg->Node(5) = DataNode(mSelectDoneSelecting);

                TheUI->Handle(navSelectDoneMsg.Data(), false);
                TheHamProvider->Handle(navSelectDoneMsg.Data(), false);

                mSelectDoneIndex = -1;
            }

            // Notify ribbon resources
            if (mListRibbonResource) {
                mListRibbonResource->OnSelectDone();
            }
            if (mHeaderRibbonResource) {
                mHeaderRibbonResource->OnSelectDone();
            }
        }
    }

    // Update ribbon test entering state
    if (mListRibbonResource) {
        if (mTestEnteringOverride) {
            mListRibbonResource->SetTestEntering(true);
            SetFrame(0.0f, 1.0f);
        } else {
            if (mListRibbonResource->TestEntering()) {
                if (!RndAnimatable::IsAnimating()) {
                    mListRibbonResource->SetTestEntering(false);
                    SetFrame(0.0f, 1.0f);
                }
            }
        }
    }

    // Same for header ribbon
    if (mHeaderRibbonResource) {
        if (mTestEnteringOverride) {
            mHeaderRibbonResource->SetTestEntering(true);
            SetFrame(0.0f, 1.0f);
        } else {
            if (mHeaderRibbonResource->TestEntering()) {
                if (!RndAnimatable::IsAnimating()) {
                    mHeaderRibbonResource->SetTestEntering(false);
                    SetFrame(0.0f, 1.0f);
                }
            }
        }
    }
}

bool HamNavList::ShouldSkipSelectAnim(DataNode &node) const {
    UIListProvider *provider = mListState.Provider();
    if (!provider || 1 < mListState.NumShowing()) {
        if (node.Type() != kDataSymbol)
            return false;

        static Symbol skip_select_anim("skip_select_anim");
        static Symbol skip_select_anim_and_sound("skip_select_anim_and_sound");
        if (node.Sym(0) != skip_select_anim) {
            if (node.Sym(0) != skip_select_anim_and_sound)
                return false;
        }
    }
    return true;
}

bool HamNavList::ShouldSkipSelectSound(DataNode &node) const {
    if (node.Type() != kDataSymbol) {
        return false;
    } else {
        static Symbol skip_select_sound("skip_select_sound");
        static Symbol skip_select_anim_and_sound("skip_select_anim_and_sound");
        if (node.Sym(0) != skip_select_sound) {
            if (node.Sym(0) != skip_select_anim_and_sound)
                return false;
        }
        return true;
    }
}

void HamNavList::AddRibbonSinks(Hmx::Object *o, Symbol s) {
    if (mListRibbonResource && o)
        o->AddSink(mListRibbonResource, s);
    if (mHeaderRibbonResource && o)
        o->AddSink(mHeaderRibbonResource, s);
}

void HamNavList::RemoveRibbonSinks(Hmx::Object *o, Symbol s) {
    if (mListRibbonResource && o)
        o->RemoveSink(mListRibbonResource, s);
    if (mHeaderRibbonResource && o)
        o->RemoveSink(mHeaderRibbonResource, s);
}

void HamNavList::DoSelectFor(int i) {
    if (mRefreshPending)
        RealRefresh();
    mListState.SetSelected(i, mListState.FirstShowing(), true);
    sLastSelectInControllerMode = true;
    SetSelecting(true);
}

void HamNavList::HandleHighlightChanged(int i) {
    if (0 <= i && i < mListState.NumShowing()) {
        SendHighlightMsg(i);
        bool shouldSend = mScrollBehavior.GetFirstVal() <= 0.0f;
        if (shouldSend) {
            SendHighlightSettledMsg(i);
        }
        if (TheGestureMgr->GetInShellMode() && mListRibbonResource) {
            mListRibbonResource->PlayHighlightSound(i);
        }
    }
}

void HamNavList::OldResourcePreload(BinStream &bs) {
    char name[256];
    bs.ReadString(name, 0x100);
    mListRibbonResource.SetName(name, true);
}

void HamNavList::HideItem(int index, bool b) {
    if (mRefreshPending)
        RealRefresh();
#ifdef HX_NATIVE
    // Resize ribbon draw states if provider grew since last Update()
    // (e.g. DTA append_nav_item doesn't trigger Update)
    int numShowing = mListState.Provider() ? mListState.NumShowing() : 0;
    if ((int)mRibbonDrawStates.size() < numShowing) {
        HamListRibbonDrawState defaultState;
        mRibbonDrawStates.resize(numShowing, defaultState);
    }
    if (index < 0 || index >= (int)mRibbonDrawStates.size()) {
        return;
    }
#endif
    MILO_ASSERT_RANGE(index, 0, mRibbonDrawStates.size(), 0x527);
    mRibbonDrawStates[index].mHidden = b;
    if (mNavProvider)
        mNavProvider->SetEnabled(index, b == false);
}

bool HamNavList::IsDataHeader(int i) {
    if (mListState.Provider()) {
        UIListProvider *p = mListState.Provider();
        return p->IsHeader(i);
    } else {
        return false;
    }
}

void HamNavList::ScrollSubList(int i, int j) {
    if (mRefreshPending)
        RealRefresh();

    UIList *list = mListDirResource->SubList(i, mListWidgets);
    if (list)
        list->Scroll(j);
}

void HamNavList::ScrollSubListToIndex(int i, int j) {
    if (mRefreshPending)
        RealRefresh();

    UIList *list = mListDirResource->SubList(i, mListWidgets);
    if (list)
        list->SetSelected(j, j);
}

int HamNavList::NumItems() const {
    int count;
    int i;
    if (mListState.ScrollPastMinDisplay()) {
        if (mScrollBehavior.AtTop() || mScrollBehavior.AtBottom()) {
            i = HamListRibbon::sNumListSelectable + 1;
        } else
            i = HamListRibbon::sNumListSelectable + 2;
    } else {
        count = GetDisabledCount(mListState.NumShowing());
        i = mListState.NumShowing();
        i -= count;
    }
    return i;
}

float HamNavList::StartFrame() {
    if (mListRibbonResource) {
        return mListRibbonResource->StartFrame();
    } else {
        return 0.0f;
    }
}

float HamNavList::EndFrame() {
    if (mListRibbonResource) {
        return mListRibbonResource->EndFrame();
    } else {
        return 0.0f;
    }
}

void HamNavList::SendHighlightSettledMsg(int i) {
    UIListProvider *provider = mListState.Provider();
    MILO_ASSERT(provider, 0x327);
    auto active = provider->IsActive(i);
    bool canSel = provider->CanSelect(i);
    if (active) {
        Symbol dataSym = provider->DataSymbol(i);
        NavHighlightSettledMsg msg(dataSym, i, this, canSel);
        TheUI->Handle(msg, false);
        Handle(msg, true);
        TheHamProvider->Handle(msg, false);
    }
}

void HamNavList::SetProvider(UIListProvider *p) {
    UIListProvider *provider = mListState.Provider();
    if (p == provider) {
        RealRefresh();
    } else {
        if (mListState.ScrollPastMinDisplay()) {
            mScrollBehavior.Exit();
        }
        mListState.SetProvider(p, mListDirResource);
        RealRefresh();
        mListState.SetSelected(0, -1, true);
        if (mListState.ScrollPastMinDisplay())
            mScrollBehavior.Enter();
    }
}

void HamNavList::SetProviderNavItemLabels(int i, DataArray *d) {
    mNavProvider->SetLabels(i, d);
}

void HamNavList::StartScroll(UIListState const &state, int i, bool b) {
    if (mListDirResource) {
        mListDirResource->StartScroll(state, mListWidgets, i, b);
    }
}

Symbol HamNavList::GetSelectedSym() const {
    UIListProvider *provider = mListState.Provider();
    if (provider) {
        Symbol s = provider->DataSymbol(mListState.SelectedData());
        if (s == gNullStr) {
            MILO_FAIL("DataSymbol() not implemented in UIList provider");
        }
        return s;
    } else {
        return gNullStr;
    }
}

void HamNavList::SendHighlightMsg(int i) {
    if (mRefreshPending)
        RealRefresh();
    UIListProvider *provider = mListState.Provider();
    MILO_ASSERT(provider, 0x339);
    bool canSel = provider->CanSelect(i);
    Symbol dataSym = provider->DataSymbol(i);
    NavHighlightMsg msg(dataSym, i, this, canSel);
    TheUI->Handle(msg, false);
    Handle(msg, true);
    TheHamProvider->Handle(msg, false);
}

int HamNavList::GetHighlightItem() const {
    if (mListRibbonResource) {
        int numShowing = mListState.NumShowing();
        if (mListRibbonResource->IsScrollable(numShowing)) {
            int selDisplay = mListState.SelectedDisplay();
            int minDisplay = mListState.MinDisplay();
            return selDisplay - minDisplay;
        } else {
            return mListState.SelectedDisplay()
                - GetDisabledCount(mListState.SelectedDisplay());
        }
    } else
        return 0;
}

void HamNavList::SetSliding(float f) {
    if (mRefreshPending)
        RealRefresh();

    if (mRibbonMode != HamListRibbon::kRibbonSelect) {
        if (mRibbonMode != HamListRibbon::kRibbonSlide) {
            mSlideSmoother.SetParams(0.0f, 0.0f, 0.0f);
            SetRibbonMode(HamListRibbon::kRibbonSlide);
        }
        if (sSlideSmoothAmount == 0.0f) {
            mSlideSmoother.SetParams(f, f, 0.0f);
        } else {
            mSlideSmoother.Smooth(f, TheTaskMgr.DeltaUISeconds());
        }
        SetFrame(mSlideSmoother.Level(), 1.0f);
    }
}

void HamNavList::Draw(const BaseSkeleton &baseSkeleton, SkeletonViz &skeletonViz) {
    const Skeleton *skeleton = dynamic_cast<const Skeleton *>(&baseSkeleton);
    MILO_ASSERT(skeleton, 0x5a3);
    mDirectionGestureFilter->Draw(*skeleton, skeletonViz);
}

void HamNavList::SetHighlight(int i) {
    if (mRefreshPending)
        RealRefresh();
    UIListProvider *provider = mListState.Provider();
    if (provider && (0 <= i) && (i < mListState.NumShowing())) {
        mDirectionGestureFilter->ResetHoverTimer();
        mListState.SetSelected(i, mListState.FirstShowing(), true);
        HandleHighlightChanged(i);
    }
}

HamListRibbonDrawState::HamListRibbonDrawState()
    : mSwellSmoother(0.0f, 10.0f, 1.0f), mSelected(false),
#ifdef HX_NATIVE
      mElemDrawState(nullptr),
#else
      mElemDrawState(0),
#endif
      mHidden(false), mBigScale(0.0f), mActive(false) {}

LeftHandListEngagementMsg::LeftHandListEngagementMsg(bool b) : Message(Type(), b) {}

void HamNavListGlitchCB(float elapsed, void *context) {
    unsigned int frameId = TheRnd.GetFrameID();
    HamNavList *list = (HamNavList *)context;
    const char *name = PathName(list);
    TheDebug << MakeString(
        "HamNavList::Refresh %s took %f ms on frame %d\n",
        name, elapsed, frameId
    );
}

void HamNavList::CompleteScroll(const UIListState &state) {
    if (mListDirResource) {
        mListDirResource->CompleteScroll(state, mListWidgets);
    }
}

void HamNavList::ClearBigElements() {
    mBigElements.clear();
    mBigElementIndices.clear();
}

void HamNavList::SetRibbonMode(HamListRibbon::RibbonMode mode) {
    if (mRibbonMode == mode)
        return;

    if ((mNavInputType != kNavInput_RightHand || !TheMetaMusicManager)
        && mNavInputType == kNavInput_LeftHand) {
        bool inControllerMode = TheGestureMgr && TheGestureMgr->InControllerMode();
        if (!inControllerMode) {
            if (mode == HamListRibbon::kRibbonDisengaged) {
                static LeftHandListEngagementMsg leftHandListDisengaged(false);
                TheUI->Handle(leftHandListDisengaged, false);
            }
            if (mRibbonMode == HamListRibbon::kRibbonDisengaged) {
                static LeftHandListEngagementMsg leftHandListEngaged(true);
                TheUI->Handle(leftHandListEngaged, false);
            }
        }
    }
    mRibbonMode = mode;
    if (mListRibbonResource) {
        mListRibbonResource->SetMode(mode);
    }
    if (mHeaderRibbonResource) {
        mHeaderRibbonResource->SetMode(mode);
    }
}

void HamNavList::SetNavProvider(HamNavProvider *provider) {
    mNavProvider = provider;
    UIListProvider *listProvider;
    if (provider) {
        provider->SetNavList(this);
        listProvider = static_cast<UIListProvider *>(provider);
    } else {
        listProvider = static_cast<UIListProvider *>(this);
    }
    SetProvider(listProvider);
}

void HamNavList::ScrollToIndex(int idx, int firstShowing) {
    bool gesturingWithVoice = TheGestureMgr && TheGestureMgr->GesturingWithVoice();
    if (gesturingWithVoice && mListState.IsScrolling()) {
        mScrollBehavior.Exit();
    }
    mListState.SetSelected(idx, firstShowing, true);
    mRefreshPending = true;
    SetHighlight(idx);
}

void HamNavList::Disengage() {
    mDirectionGestureFilter->ClearSwipe();
    bool inControllerMode = TheGestureMgr && TheGestureMgr->InControllerMode();
    if ((!inControllerMode || !CanHaveFocus()) && mRibbonMode != HamListRibbon::kRibbonSelect) {
        SetRibbonMode(HamListRibbon::kRibbonDisengaged);
    }
}

int HamNavList::GetDisabledCount(int count) const {
    int disabled = 0;
    for (int i = 0; i < count; i++) {
        UIListProvider *provider = mListState.Provider();
        if (!provider->IsActive(i))
            disabled++;
    }
    while (count < mListState.NumShowing()) {
        UIListProvider *provider = mListState.Provider();
        if (provider->IsActive(count))
            break;
        disabled++;
        count++;
    }
    bool scrollable = mListState.IsScrolling();
    MILO_ASSERT(!scrollable || disabled == 0, 0x313);
    return disabled;
}

bool HamNavList::IsElementBig(int display) const {
    int numShowing = mListState.NumShowing();
    if (mListRibbonResource->IsScrollable(numShowing)) {
        display = (mListState.FirstShowing() + display) - mListState.MinDisplay();
    }
    if (display >= 0 && display < mListState.NumShowing()) {
        for (unsigned int i = 0; i < mBigElements.size(); i++) {
            Symbol sym = mListState.Provider()->DataSymbol(display);
            if (sym == mBigElements[i])
                return true;
        }
        for (unsigned int i = 0; i < mBigElementIndices.size(); i++) {
            if (display == (int)mBigElementIndices[i])
                return true;
        }
    }
    return false;
}

float HamNavList::CalculateSwell(int pos) const {
    int numItems = NumItems();
    float slideVal = Max(0.0f, mHandHeight);
    float clamped = Min(1.0f, slideVal);
    float diff = clamped - (float)pos / (float)(numItems - 1);
    float swell = sqrtf(fabsf(diff)) * sqrtf((float)numItems) * 0.15f;
    swell = Clamp(0.0f, 1.0f, swell);
    return 1.0f - swell;
}

float HamNavList::GetTargetSwellAmount(int display) {
    if (TheLoadMgr.EditMode()) {
        int selDisplay = mListState.SelectedDisplay();
        if (display == selDisplay) {
            if (mRibbonMode != HamListRibbon::kRibbonSwell) {
                return 1.0f;
            }
            return GetFrame();
        }
        return 0.0f;
    }
    if (mListRibbonResource->TestEntering()) {
        return 0.0f;
    }
    bool isScrolling = mScrollBehavior.IsScrolling();
    if (!isScrolling) {
        bool inControllerMode = TheGestureMgr && TheGestureMgr->InControllerMode();
        if (inControllerMode) {
            int selDisplay = mListState.SelectedDisplay();
            if (display == selDisplay) {
                UIComponent *focus = TheUI->FocusComponent();
                if (focus == this) {
                    return 1.0f;
                }
            }
        } else {
            if (mRibbonMode != HamListRibbon::kRibbonDisengaged) {
                int selDisplay = mListState.SelectedDisplay();
                if (display == selDisplay && mScrollBehavior.mScrollDir == 0) {
                    return 1.0f;
                }
                if (mRibbonMode == HamListRibbon::kRibbonSwell
                    && display < mListState.NumShowing()) {
                    UIListProvider *provider = mListState.Provider();
#ifdef HX_NATIVE
                    bool displayActive = provider && provider->IsActive(display);
                    if (displayActive
                        && mSkeletonTrackingID != -1) {
#else
                    auto displayActive = provider->IsActive(display);
                    if (provider && displayActive
                        && mSkeletonTrackingID != -1) {
#endif
                        int disabledCount = GetDisabledCount(display);
                        int effectivePos = display - disabledCount;
                        if (mListState.IsScrolling()) {
                            effectivePos -= (int)mListState.MinDisplay();
                            bool atTop = mScrollBehavior.AtTop();
                            if (!atTop) {
                                effectivePos += 1;
                            }
                        }
                        return CalculateSwell(effectivePos);
                    }
                }
            }
        }
    }
    return 0.0f;
}

void HamNavList::RealRefresh() {
    AutoGlitchReport glitchReport(15.0, HamNavListGlitchCB, this);
    mRefreshPending = false;
    if (mListRibbonResource) {
        UIListProvider *provider = mListState.Provider();
        if (provider) {
            int numShowing = mListState.NumShowing();
            bool scrollable = mListRibbonResource->IsScrollable(numShowing);
            if (scrollable) {
                int maxDisplay = sListStateMaxDisplay - 4;
                int minDisplayVal = 3;
                if ((int)numShowing <= maxDisplay) {
                    minDisplayVal = (numShowing - maxDisplay) + 2;
                }
                mListState.SetMinDisplay(minDisplayVal);
                mListState.SetScrollPastMinDisplay(true);
                mListState.SetMaxDisplay(maxDisplay);
                mListState.SetCircular((unsigned long)maxDisplay >= numShowing);
            } else {
                mListState.SetScrollPastMinDisplay(false);
                int sel = mListState.Selected();
                mListState.SetSelected(sel, -1, true);
            }
        }
    }
    if (mListDirResource) {
#ifdef HX_NATIVE
        // Recreate elements if NumShowing changed since CreateElements was last called
        // (e.g. provider set after Update() which created 0 elements)
        int numSh = mListState.NumShowing();
        mListDirResource->CreateElements(nullptr, mListWidgets, numSh);
        mRibbonDrawStates.resize(numSh, HamListRibbonDrawState());
#endif
        mListState.Provider()->UnHighlightCurrent();
        mListState.Provider()->ClearIconLabels();
        mListDirResource->FillElements(mListState, mListWidgets);
    }
    if (mNavInputType == kNavInput_RightHand) {
        UIListProvider *provider = mListState.Provider();
        if (provider) {
            static Message eqShiftEvenMsg(Symbol("eq_shift_even"));
            static Message eqShiftOddMsg(Symbol("eq_shift_odd"));
            int numShowing = mListState.NumShowing();
            if ((numShowing % 2 == 0) || mListState.ScrollPastMinDisplay()) {
                TheHamProvider->Handle(eqShiftEvenMsg, false);
            } else {
                TheHamProvider->Handle(eqShiftOddMsg, false);
            }
        }
    }
}

void HamNavList::Enter() {
    UIComponent::Enter();
    SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();
    if (!handle.HasCallback(static_cast<SkeletonCallback *>(this))) {
        handle.AddCallback(static_cast<SkeletonCallback *>(this));
    }

    if (!mDisableSlideSound && mListRibbonResource) {
        Sound *slideSound = mListRibbonResource->SlideSound();
        if (slideSound) {
            slideSound->Play(0, 0, 0, nullptr, 0);
        }
    }
    unkc8 = false;
    if (mSuppressAutomaticEnter) {
        mTestEnteringOverride = true;
    } else {
        mPendingEnterAnim = true;
    }
    if (mListRibbonResource) {
        mListRibbonResource->HandleEnter();
    }
    if (mHeaderRibbonResource) {
        mHeaderRibbonResource->HandleEnter();
    }
    if (mScrollSpeedIndicatorResource) {
        mScrollSpeedIndicatorResource->HandleEnter();
    }
    mScrollSettleTime = (float)TheTaskMgr.UISeconds();
    RealRefresh();

    static Symbol cheatFocusRestart("cheat_focus_restart");
    static Symbol pausecommandRestart("pausecommand_restart");
#ifdef HX_NATIVE
    if (mNavProvider && DataVariable(cheatFocusRestart).Int() != 0) {
#else
    if (mNavProvider && *(int *)&DataVariable(cheatFocusRestart) != 0) {
#endif
        int idx = mNavProvider->DataIndex(pausecommandRestart);
        if (idx != -1) {
            SetHighlight(idx);
        }
    }
}

void HamNavList::Exit() {
    UIComponent::Exit();
    SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();
    if (handle.HasCallback(static_cast<SkeletonCallback *>(this))) {
        handle.RemoveCallback(static_cast<SkeletonCallback *>(this));
    }
    if (mListRibbonResource) {
        Sound *slideSound = mListRibbonResource->SlideSound();
        if (slideSound) {
            slideSound->Stop(nullptr, false);
        }
    }
    if (mScrollSpeedIndicatorResource) {
        mScrollSpeedIndicatorResource->HandleExit();
    }
    mScrollBehavior.Exit();
    mSelectDoneSymbol = gNullStr;
    mSelectDoneIndex = -1;
}

void HamNavList::PostUpdate(const SkeletonUpdateData *data) {
    if (data && !SkipPoll()) {
        unkc8 = true;
        int i = 0;
        do {
            Skeleton *skel = data->mSkeletonsRight[i];
            if (skel->TrackingID() == mSkeletonTrackingID) {
                int elapsed = skel->ElapsedMs();
                int ms = 0 < elapsed ? elapsed : 0;
                mDirectionGestureFilter->Update(*skel, ms);
                elapsed = skel->ElapsedMs();
                ms = 0 < elapsed ? elapsed : 0;
                mHandHeightFilter->Update(*skel, ms);
                return;
            }
            i++;
        } while (i < 6);
    }
}

void HamNavList::Update() {
    delete mDirectionGestureFilter;
    delete mHandHeightFilter;

    if (mNavInputType == kNavInput_RightHand) {
        if (!TheGestureMgr->InDoubleUserMode()) {
            mDirectionGestureFilter =
                new DirectionGestureFilterSingleUser(kSkeletonRight, kSkeletonLeft, 0.5f, -0.2f);
        } else {
            mDirectionGestureFilter =
                new DirectionGestureFilterDoubleUser(kSkeletonRight, kSkeletonLeft, 0.5f, -0.2f);
        }
        mHandHeightFilter = new HandHeightGestureFilter(kSkeletonRight);
    } else {
        if (!TheGestureMgr->InDoubleUserMode()) {
            mDirectionGestureFilter =
                new DirectionGestureFilterSingleUser(kSkeletonLeft, kSkeletonRight, 0.5f, -0.1f);
        } else {
            mDirectionGestureFilter =
                new DirectionGestureFilterDoubleUser(kSkeletonLeft, kSkeletonRight, 0.5f, -0.1f);
        }
        mHandHeightFilter = new HandHeightGestureFilter(kSkeletonLeft);
    }

    mWasInDoubleUserMode = TheGestureMgr->InDoubleUserMode();

    if (mDirectionGestureFilter) {
        mDirectionGestureFilter->SetHighButtonMode(mHighButtonMode);
    }

    int numDisplay = (mNavInputType != kNavInput_RightHand) ? 2 : 10;
    mListState.SetNumDisplay(numDisplay, true);

    HamListRibbonDrawState defaultState;
    mRibbonDrawStates.resize(mListState.NumShowing(), defaultState);

    if (mListDirResource) {
        int numSh = mListState.NumShowing();
        mListDirResource->CreateElements(nullptr, mListWidgets, numSh);
    }
    mRefreshPending = true;
}

void HamNavList::PlayEnterAnim() {
    mTestEnteringOverride = false;
    if (mListRibbonResource) {
        if (mListRibbonResource->EnterAnim()) {
            mListRibbonResource->SetTestEntering(true);
            if (mSkipEnterAnim) {
                mListRibbonResource->SetFrame(mListRibbonResource->EndFrame(), 1.0f);
                mListRibbonResource->SetTestEntering(false);
            }
        }
    }
    if (mHeaderRibbonResource) {
        if (mHeaderRibbonResource->EnterAnim()) {
            mHeaderRibbonResource->SetTestEntering(true);
            if (mSkipEnterAnim) {
                mHeaderRibbonResource->SetFrame(mHeaderRibbonResource->EndFrame(), 1.0f);
                mHeaderRibbonResource->SetTestEntering(false);
            }
        }
    }
    if ((mListRibbonResource && mListRibbonResource->TestEntering()) ||
        (mHeaderRibbonResource && mHeaderRibbonResource->TestEntering())) {
        RndAnimatable::Animate(0.0f, false, 0.0f, nullptr, kEaseLinear, 0.0f, false);
    }
}

void HamNavList::Clear() {
    mDirectionGestureFilter->Clear();
    mHandHeightFilter->Clear();
}

void HamNavList::SetSelecting(bool selecting) {
    if (!mSelectionEnabled)
        return;
    sLastSelectInControllerMode = selecting;
    auto& listState = mListState;
    UIListProvider *provider = listState.Provider();
    MILO_ASSERT(provider, 0x491);
    int selected = listState.Selected();
    Symbol sym;
    UIList *sublist = mListDirResource->SubList(selected, mListWidgets);
    if (!(sublist == nullptr)) {
        HamNavProvider *navProvider = mNavProvider;
        int subSelected = sublist->Selected();
        int subSelPlusOne = subSelected + 1;
        int wrapped = sublist->GetListState().WrapShowing(subSelPlusOne);
        sym = navProvider->DataSymbol(wrapped, selected);
    } else {
        Symbol dataSym = provider->DataSymbol(selected);
        sym = dataSym;
    }
#ifdef HX_NATIVE
    static int sSelectDiag = 0;
    if (sSelectDiag++ < 32) {
        MILO_LOG(
            "DC3 HamNavList: select name='%s' selected=%d sym='%s' focus=%d selecting=%d provider=%p\n",
            Name(),
            selected,
            sym.Str(),
            TheUI->FocusComponent() == this,
            selecting,
            (void *)provider
        );
    }
#endif
    SetRibbonMode(HamListRibbon::kRibbonSelect);
    if (TheGestureMgr && TheGestureMgr->GesturingWithVoice()) {
        TheGestureMgr->SetGesturingWithVoice(false);
        if (listState.IsScrolling()) {
            mScrollBehavior.Enter();
        }
    }
    SendSelect(nullptr);
    bool canSelect = provider->CanSelect(selected);
    mSelectDoneSelecting = canSelect;
    mSelectDoneSymbol = sym;
    mSelectDoneIndex = selected;
    NavSelectMsg navSelectMsg(sym, selected, this, canSelect);
    TheHamProvider->Handle(navSelectMsg, false);
    DataNode result = TheUI->Handle(navSelectMsg, false);
    Handle(navSelectMsg, true);
    if (!mDisableSelectSound) {
        bool skipSound = ShouldSkipSelectSound(result);
        if (!skipSound && mListRibbonResource) {
            mListRibbonResource->PlaySelectSound(selected);
        }
    }
    if (mListRibbonResource) {
        RndAnimatable *sla = mListRibbonResource->SlideSoundAnim();
        if (sla) {
            sla->SetFrame(1.0f, 1.0f);
        }
        bool skipSelectAnim = ShouldSkipSelectAnim(result);
        mListRibbonResource->SetSelectToggle(skipSelectAnim);
    }
    if (mHeaderRibbonResource) {
        RndAnimatable *sla = mHeaderRibbonResource->SlideSoundAnim();
        if (sla) {
            sla->SetFrame(1.0f, 1.0f);
        }
        mHeaderRibbonResource->SetSelectToggle(ShouldSkipSelectAnim(result));
    }
    RndAnimatable::Animate(0.0f, false, 0.0f, nullptr, kEaseLinear, 0.0f, false);
}

bool HamNavList::InControllerMode() const {
    return TheGestureMgr && TheGestureMgr->InControllerMode();
}

void HamNavList::DetermineHighlightedItem() {
    MILO_ASSERT(!InControllerMode(), 0x2b7);
    MILO_ASSERT(!TheLoadMgr.EditMode(), 0x2b8);

    int numItems = NumItems();
    int maxItem = numItems - 1;
    double maxItemD = (double)maxItem;
    float maxItemF = (float)maxItemD;
    float numItemsF = (float)(double)numItems;

    float threshold = (1.0f - maxItemF * 0.15f) / numItemsF;

    int highlightItem = GetHighlightItem();
    bool gathering = mListState.ScrollPastMinDisplay();
    if (gathering) {
        if (!mScrollBehavior.AtTop()) {
            if (0 != highlightItem || mScrollBehavior.mScrollDir != 1) {
                if (highlightItem == HamListRibbon::sNumListSelectable - 1
                    && mScrollBehavior.mScrollDir == 2) {
                    highlightItem = HamListRibbon::sNumListSelectable + 1;
                } else {
                    highlightItem = highlightItem + 1;
                }
            }
        }
    }

    unsigned int posU = (int)(maxItemF * mHandHeight + 0.5f);
    float targetPos = (float)highlightItem / maxItemF;
    if ((int)posU <= maxItem) {
        posU &= (posU >> 31) - 1;
    }

    unsigned int adjustedPos = posU;
    if (mListState.ScrollPastMinDisplay()) {
        if (!mScrollBehavior.AtTop()) {
            adjustedPos = posU - 1;
        }
        int iPos = (int)adjustedPos;
        if (iPos == -1) {
            mScrollBehavior.mScrollDir = 1;
        } else if (iPos == HamListRibbon::sNumListSelectable) {
            mScrollBehavior.mScrollDir = 2;
        } else {
            mScrollBehavior.mScrollDir = 0;
        }
        int maxSelectable = HamListRibbon::sNumListSelectable - 1;
        if (iPos > maxSelectable) {
            adjustedPos = maxSelectable;
        } else {
            adjustedPos &= (adjustedPos >> 31) - 1;
        }
    }

    float handDiff = fabsf(mHandHeight - targetPos);
    float halfThreshold = threshold * 0.5f + 0.15f;
    if (!(handDiff >= halfThreshold)) {
        if (mScrollBehavior.AtBottom()) {
            mScrollBehavior.mScrollDir = 0;
        }
    } else {
        int firstShowing = mListState.FirstShowing();
        int disabled = GetDisabledCount(adjustedPos);
        int newHighlight = firstShowing + disabled + (int)adjustedPos;
        int curSelected = mListState.Selected();
        if (newHighlight != curSelected) {
            SetHighlight(newHighlight);
        }
    }
}

void HamNavList::UpdateGestures(const Skeleton *skeleton) {
    if (mRefreshPending)
        RealRefresh();

    bool inControllerMode = TheGestureMgr && TheGestureMgr->InControllerMode();
    if (!inControllerMode) {
        bool inVoiceMode = TheGestureMgr && TheGestureMgr->GesturingWithVoice();
        if (!inVoiceMode && mEnabled) {
            if (mRibbonMode == HamListRibbon::kRibbonSelect) {
                if (RndAnimatable::IsAnimating()) {
                    return;
                }
                float curFrame = RndAnimatable::GetFrame();
                float endFrame = EndFrame();
                if (curFrame == endFrame) {
                    return;
                }
            }
            bool inVoiceMode = TheGestureMgr && TheGestureMgr->InVoiceMode();
            if (inVoiceMode) {
                TheGestureMgr->SetInVoiceMode(false);
            }

            if (mRibbonMode == HamListRibbon::kRibbonDisengaged) {
                mDirectionGestureFilter->Clear();
                mDirectionGestureFilter->SetEngaged(false);
            } else {
                mDirectionGestureFilter->SetEngaged(true);
            }

            if (mScrollBehavior.IsScrolling()) {
                mDirectionGestureFilter->Clear();
                mDirectionGestureFilter->ClearSwipe();
            }

            int numItems = NumItems();
            if (numItems == 1) {
                mDirectionGestureFilter->ClearSwipe();
                mDirectionGestureFilter->SetAllowAboveShoulder(false);
            } else {
                mDirectionGestureFilter->SetAllowAboveShoulder(true);
            }

            int firstShowing = mListState.FirstShowing();
            unsigned char gathering = mListState.ScrollPastMinDisplay();
            int selected = mListState.Selected();
            int scrollOffset = selected + (!!gathering - firstShowing);
            if (scrollOffset == NumItems() - 1) {
                mDirectionGestureFilter->ClearSwipe();
            }

            bool handValid = skeleton && skeleton->IsValid()
                && mDirectionGestureFilter->IsHandValid(*skeleton);
            bool posValid = mListState.ScrollPastMinDisplay() && mDirectionGestureFilter->IsValidScrollPos(*skeleton);

            if ((!handValid && !posValid) && unkc8 && !TheLoadMgr.EditMode()) {
                Disengage();
            }

            if (posValid && mRibbonMode == HamListRibbon::kRibbonDisengaged) {
                SetSwelling();
            }

            if (mListRibbonResource->TestEntering()) {
                return;
            }

            mHandHeight = mHandHeightFilter->GetHandHeight();

            if (mDirectionGestureFilter->HasDirection() && mSelectionEnabled) {
                mDirectionGestureFilter->ClearSwipe();
                static Message forceLetterboxOff(Symbol("force_letterbox_off"));
                TheUI->Handle(forceLetterboxOff, false);
                SetSelecting(false);
                return;
            }

            if (handValid) {
                if (mDirectionGestureFilter->IsLockedIn()) {
                    if (!mDirectionGestureFilter->HasDirection()
                        && mScrollBehavior.mScrollDir == 0) {
                        SetSwelling();
                        return;
                    }
                    if (!mSelectionEnabled) {
                        return;
                    }
                    float pct = mDirectionGestureFilter->GetPercentPulled();
                    SetSliding(pct);
                    return;
                }
            }
        }
    }

    mDirectionGestureFilter->ClearSwipe();
}

DataNode HamNavList::OnMsg(const ButtonDownMsg &msg) {
    if (mRefreshPending)
        RealRefresh();

    bool inControllerMode = InControllerMode();
    if ((inControllerMode || TheLoadMgr.EditMode())
#ifdef HX_NATIVE
        && mEnabled) {
#else
        && !RndAnimatable::IsAnimating() && mEnabled) {
#endif
        bool gesturing = TheGestureMgr && TheGestureMgr->GesturingWithVoice();
        if (!gesturing && TheUI->FocusComponent() == this) {
            int dir = ScrollDirection(msg, false, true, 1);
            if (dir != 0) {
                int selected = mListState.Selected();
                do {
                    selected += dir;
                    if (selected < 0)
                        return DataNode(0);
                    if (selected >= mListState.NumShowing())
                        return DataNode(0);
                } while (!mListState.Provider()->IsActive(selected));

                if (mListState.ScrollPastMinDisplay()) {
                    int firstShowing = mListState.FirstShowing();
                    if (selected < firstShowing) {
                        mScrollBehavior.ScrollUp(false);
                    } else if (selected >= firstShowing
                        + HamListRibbon::sNumListSelectable - 1) {
                        mScrollBehavior.ScrollDown(false);
                    } else {
                        SetHighlight(selected);
                    }
                } else {
                    SetHighlight(selected);
                }
                return DataNode(0);
            }

            if (msg.GetAction() == kAction_Confirm) {
                if (mSelectionEnabled) {
                    SetSelecting(true);
                }
                return DataNode(0);
            }
        }
    }
    return DataNode(kDataUnhandled, 0);
}

#ifdef HX_NATIVE
DataNode HamNavList::OnMsg(const UITransitionCompleteMsg &) {
    StopAnimation();
    return DataNode(kDataUnhandled, 0);
}
#endif

void HamNavList::DrawDebug() const {
#ifdef HX_NATIVE
    // Debug overlay — not used on native (gated by sDrawDebug=0)
#else
    static unsigned char sDrawDebug = 0;
    if (!sDrawDebug)
        return;

    float h = mHandHeightFilter->mHandHeight;
    Vector2 p1(1.0f, h);
    Vector2 p0(0.0f, h);
    UtilDrawLine(p0, p1, Hmx::Color(0.0f, 1.0f, 0.0f, 1.0f));

    static Hmx::Color sRectColor(0.2f, 0.2f, 0.2f, 0.7f);
    static Hmx::Color sTextColor(1.0f, 1.0f, 1.0f, 1.0f);

    static float sRectX = 0.0f;
    static float sRectY = 0.1f;
    static float sRectW = 0.95f;
    static float sRectH = 0.95f;
    Hmx::Rect rect(sRectX, sRectY - 0.05f, sRectW, sRectH + 0.05f);
    TheRnd.DrawRectScreen(rect, sRectColor, nullptr, nullptr, nullptr);

    char buf[50];
    float lineStep = 0.05f;
    float startX = 0.0f;
    float startY = sRectY;
    unsigned int i = 0;
    do {
        sprintf_s(buf, "");
        switch (i) {
        case 0:
            sprintf_s(buf, "Hand height %f", mHandHeight);
            break;
        case 1:
            sprintf_s(buf, "ListState SelectedDisplay: %d", mListState.SelectedDisplay());
            break;
        case 2:
            sprintf_s(buf, "ListState FirstShowing: %d", mListState.FirstShowing());
            break;
        case 3:
            sprintf_s(buf, "ListState Selected: %d", mListState.Selected());
            break;
        case 4:
            sprintf_s(buf, "Num selectable items: %d", NumItems());
            break;
        }
        Vector2 pos(startX, startY + i * lineStep);
        TheRnd.DrawStringScreen(buf, pos, sTextColor, true);
        i++;
    } while ((int)i < 5);
#endif
}

void HamNavList::LinkRibbonDrawState(
    std::vector<HamListRibbonDrawState> &ribbonStates,
    UIListWidgetDrawState &widgetState
) {
#ifdef HX_NATIVE
    // LP64-safe version: use proper struct access instead of raw pointer arithmetic
    int widgetElemCount = (int)widgetState.mElements.size();
    if (widgetElemCount != (int)ribbonStates.size()) {
        HamListRibbonDrawState defaultState;
        ribbonStates.resize(widgetElemCount, defaultState);
    }
    for (int i = 0; i < widgetElemCount; i++) {
        ribbonStates[i].mSelected = (mListState.SelectedDisplay() == i);

        int numShowing = mListState.NumShowing();
        bool scrollable = mListRibbonResource && mListRibbonResource->IsScrollable(numShowing);

        if (scrollable) {
            ribbonStates[i].mActive = mListState.Provider()->IsHeader(
                mListState.FirstShowing() + i - mListState.MinDisplay()
            );
        } else {
            ribbonStates[i].mActive = mListState.Provider()->IsHeader(i);
        }

        ribbonStates[i].mBigScale = (float)IsElementBig(i);

        UIListElementDrawState &elem = widgetState.mElements[i];
        ribbonStates[i].mElemDrawState = &elem;

        if (elem.mElementState == kUIListWidgetHighlight) {
            bool shouldSet = true;
            if (mListRibbonResource && !mListRibbonResource->TestEntering()
                && mRibbonMode != HamListRibbon::kRibbonDisengaged) {
                if (TheUI->FocusComponent() == this) {
                    shouldSet = false;
                } else {
                    bool controllerMode = TheGestureMgr && TheGestureMgr->InControllerMode();
                    if (!controllerMode) {
                        shouldSet = false;
                    }
                }
            }
            if (shouldSet) {
                elem.mElementState = kUIListWidgetActive;
            }
        }
    }
#else
    // ILP32 (Xbox 360) version
    unsigned int widgetElemCount = widgetState.mElements.size();
    if (widgetElemCount != ribbonStates.size()) {
        HamListRibbonDrawState defaultState;
        ribbonStates.resize(widgetElemCount, defaultState);
    }
    for (int i = 0; i < (int)widgetElemCount; i++) {
        ribbonStates[i].mSelected = (mListState.SelectedDisplay() == i);

        int numShowing = mListState.NumShowing();
        bool scrollable = mListRibbonResource->IsScrollable(numShowing);

                ribbonStates[i].mActive = scrollable ? mListState.Provider()->IsHeader(
                mListState.FirstShowing() + i - mListState.MinDisplay()
            ) : mListState.Provider()->IsHeader(i);

        ribbonStates[i].mBigScale = (float)IsElementBig(i);
        ribbonStates[i].mElemDrawState = (unsigned int)&widgetState.mElements[i];

        UIListElementDrawState *elemPtr =
            (UIListElementDrawState *)ribbonStates[i].mElemDrawState;
        *(int *)&elemPtr->mComponentState = *(int *)((char *)this + 8);

        elemPtr = (UIListElementDrawState *)ribbonStates[i].mElemDrawState;
        if (elemPtr->mElementState == kUIListWidgetHighlight) {
            if (mListRibbonResource->TestEntering()
                || mRibbonMode == HamListRibbon::kRibbonDisengaged) {
                elemPtr = (UIListElementDrawState *)ribbonStates[i].mElemDrawState;
                elemPtr->mElementState = kUIListWidgetActive;
            } else if (TheUI->FocusComponent() != this) {
                bool controllerMode = TheGestureMgr && TheGestureMgr->InControllerMode();
                if (controllerMode) {
                    elemPtr = (UIListElementDrawState *)ribbonStates[i].mElemDrawState;
                    elemPtr->mElementState = kUIListWidgetActive;
                }
            }
        }
    }
#endif
}

void HamNavList::DrawShowing() {
    if (!mListRibbonResource || unkc8)
        return;
#ifdef HX_NATIVE
    if (!mListState.Provider())
        return;
#endif

    mScrollSettleTime = TheTaskMgr.UISeconds();

#ifdef HX_NATIVE
    RndCam *savedCam = RndCam::Current();
    // Use the PanelDir's scene camera (e.g. turbo_shell.cam) which frames the
    // content correctly. Fall back to UI cam only if no scene camera is set.
    PanelDir *ownerPanel = dynamic_cast<PanelDir*>(DataDir());
    RndCam *drawCam = (ownerPanel && ownerPanel->Cam()) ? ownerPanel->Cam()
        : (TheUI ? TheUI->GetCam() : nullptr);
    if (drawCam && drawCam != savedCam) {
        drawCam->Select();
    }
#endif

    UIListWidgetDrawState widgetState;
    mListDirResource->BuildDrawState(
        widgetState, mListState, kFocused, 0.0f, !mScrollBehavior.IsScrolling()
    );

    LinkRibbonDrawState(mRibbonDrawStates, widgetState);

    if (mScrollBehavior.IsScrolling()) {
        int first = mListState.FirstShowing();
        for (unsigned int i = 0; i < mRibbonDrawStates.size(); i++) {
            if ((int)i < first || (int)i >= first + HamListRibbon::sNumListSelectable) {
                mRibbonDrawStates[i].mSwellSmoother.SetParams(0.0f, 0.0f, 0.0f);
            }
        }
    }

    if (mListRibbonResource) {
        mListRibbonResource->SetFrame(GetFrame(), 1.0f);
        mListRibbonResource->mScrollAnims.SetScrollFrame(mScrollBehavior.mScrollProgress);
        mListRibbonResource->SetDisengageFrame(mDisengageSmoother.Level());
        mListRibbonResource->mMode = mRibbonMode;
        mListRibbonResource->Draw(WorldXfm(), mRibbonDrawStates, false, false);
    }

    if (mHeaderRibbonResource) {
        mHeaderRibbonResource->SetFrame(GetFrame(), 1.0f);
        mHeaderRibbonResource->mScrollAnims.SetScrollFrame(mScrollBehavior.mScrollProgress);
        mHeaderRibbonResource->SetDisengageFrame(mDisengageSmoother.Level());
        mHeaderRibbonResource->mMode = mRibbonMode;
        mHeaderRibbonResource->Draw(WorldXfm(), mRibbonDrawStates, true, false);
    }

#ifdef HX_NATIVE
    if (drawCam && drawCam != RndCam::Current()) {
        drawCam->Select();
    }
#endif

    for (unsigned int i = 0; i < mRibbonDrawStates.size(); i++) {
        HamListRibbonDrawState &state = mRibbonDrawStates[i];
        if (state.mHidden && state.mElemDrawState) {
            UIListElementDrawState *elem = (UIListElementDrawState *)state.mElemDrawState;
            elem->mAlpha = 0.0f;
        }
    }

#ifdef __EMSCRIPTEN__
    // Web fix: re-fill widget text content every frame.
    // Xbox fills once on Enter() then uses StartScroll/CompleteScroll to rotate
    // elements one-by-one during scroll animation. On web, the scroll callback
    // path misses Fill updates — re-filling per-frame ensures text matches
    // selection state. Benchmarked at ~0.08ms for 16 widgets × 10 display
    // slots (<0.5% of 30fps budget).
    mListDirResource->FillElements(mListState, mListWidgets);
#endif

    mListDirResource->DrawWidgets(
        widgetState, mListState, mListWidgets, WorldXfm(),
        GetState(), nullptr, false
    );

    if (mScrollSpeedIndicatorResource) {
        mScrollSpeedIndicatorResource->Draw(WorldXfm());
    }

#ifdef HX_NATIVE
    if (savedCam && savedCam != RndCam::Current()) {
        savedCam->Select();
    }
#endif
}

void WorldInstance::Load(BinStream &bs) {
    PreLoad(bs);
    PostLoad(bs);
}
