#include "ui/UIListSlot.h"
#include "macros.h"
#include "math/Color.h"
#include "obj/Object.h"
#include "ui/UIList.h"
#include "ui/UIListState.h"
#include "ui/UIListWidget.h"
#include "utl/Std.h"
#ifdef HX_NATIVE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

#ifdef HX_NATIVE
namespace {
bool DebugChooseModeSlot(const UIListProvider *provider) {
    static int enabled = -1;
    if (enabled == -1) {
        const char *env = getenv("MILO_DEBUG_CHOOSE_MODE");
        enabled = (env && env[0] && strcmp(env, "0") != 0) ? 1 : 0;
    }
    if (!enabled || !provider) {
        return false;
    }
    const Hmx::Object *obj = dynamic_cast<const Hmx::Object *>(provider);
    if (!obj) {
        return false;
    }
    const char *path = PathName(obj);
    return (path && strstr(path, "choose_mode"))
        || strcmp(obj->ClassName().Str(), "ChooseModeProvider") == 0;
}
}
#endif

UIListSlot::UIListSlot() : mSlotDrawType(kUIListSlotDrawAlways), mNextElement(0) {}

BEGIN_HANDLERS(UIListSlot)
    HANDLE_SUPERCLASS(UIListWidget)
END_HANDLERS

BEGIN_PROPSYNCS(UIListSlot)
    SYNC_PROP_SET(
        slot_draw_type, (int)mSlotDrawType, mSlotDrawType = (UIListSlotDrawType)_val.Int()
    )
    SYNC_SUPERCLASS(UIListWidget)
END_PROPSYNCS

BEGIN_SAVES(UIListSlot)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(UIListWidget)
    bs << mSlotDrawType;
END_SAVES

BEGIN_COPYS(UIListSlot)
    COPY_SUPERCLASS(UIListWidget)
    CREATE_COPY_AS(UIListSlot, s)
    MILO_ASSERT(s, 0xe1);
    COPY_MEMBER_FROM(s, mSlotDrawType)
END_COPYS

INIT_REVS(0, 0)

BEGIN_LOADS(UIListSlot)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(UIListWidget)
    int ty;
    bs >> ty;
    mSlotDrawType = (UIListSlotDrawType)ty;
END_LOADS

void UIListSlot::ResourceCopy(const UIListWidget *w) {
    UIListWidget::ResourceCopy(w);
    mMatchName = w->Name();
}

void UIListSlot::CreateElements(UIList *uilist, int count) {
    if (RootTrans()) {
        ClearElements();
        for (int i = 0; i < count; i++) {
            mElements.push_back(CreateElement(uilist));
        }
        mNextElement = CreateElement(uilist);
    }
}

#ifdef HX_NATIVE
// Lazy element creation for async loading: if RootTrans() is now valid
// but CreateElements was skipped (assets weren't loaded yet), create them now.
void UIListSlot::EnsureElements() {
    if (!RootTrans() || !mElements.empty() || mNextElement)
        return;
    UIList *list = ParentList();
    if (!list)
        return;
    int count = list->NumDisplay();
    if (count <= 0)
        return;
    for (int i = 0; i < count; i++) {
        mElements.push_back(CreateElement(list));
    }
    mNextElement = CreateElement(list);
}
#endif

void UIListSlot::Draw(
    const UIListWidgetDrawState &drawstate,
    const UIListState &liststate,
    const Transform &ctf,
    UIComponent::State compstate,
    Box *box,
    DrawCommand cmd
) {
    RndTransformable *root = RootTrans();
    if (root) {
#ifdef HX_NATIVE
        EnsureElements();
#endif
        int thesize = drawstate.mElements.size();
        if (thesize > mElements.size()) {
            int numSlotElements = mElements.size();
#ifdef HX_NATIVE
            return;
#else
            MILO_FAIL("%i isn't enough elements (need %i)", numSlotElements, thesize);
#endif
        }
        const Transform &rootWorldXfm = root->WorldXfm();
        Transform tf78(rootWorldXfm);
        Transform tfa8;
        UIListProvider *prov = liststate.Provider();
        float d10;
        UIColor *uicolor;
        for (int i = 0; i < thesize; i++) {
            const UIListElementDrawState &curdrawstate = drawstate.mElements[i];
            if (curdrawstate.mActive) {
                uicolor = 0;
                d10 = 1.0f;
                if (!box) {
                    if ((mSlotDrawType == kUIListSlotDrawHighlight
                            || mSlotDrawType == kUIListSlotDrawHighlightNoAlpha)
                            && curdrawstate.mDisplay != drawstate.mHighlightDisplay
                        || (mSlotDrawType == kUIListSlotDrawNoHighlight
                            || mSlotDrawType == kUIListSlotDrawNoHighlightNoAlpha)
                            && curdrawstate.mDisplay == drawstate.mHighlightDisplay) {
                        continue;
                    }

                    UIListWidgetState slotoverride = prov->SlotElementStateOverride(
                        curdrawstate.mShowing,
                        curdrawstate.mData,
                        this,
                        curdrawstate.mElementState
                    );
                    UIComponent::State curcompstate = curdrawstate.mComponentState;
                    uicolor = DisplayColor(slotoverride, curcompstate);
                    uicolor = prov->SlotColorOverride(
                        curdrawstate.mShowing, curdrawstate.mData, this, uicolor
                    );
                    if (mSlotDrawType == kUIListSlotDrawAlwaysNoAlpha
                        || mSlotDrawType == kUIListSlotDrawHighlightNoAlpha
                        || mSlotDrawType == kUIListSlotDrawNoHighlightNoAlpha) {
                        d10 = 1.0f;
                    } else {
                        d10 = curdrawstate.mAlpha;
                    }
                    if (curcompstate == UIComponent::kDisabled)
                        d10 *= DisabledAlphaScale();
                    prov->PreDraw(curdrawstate.mShowing, curdrawstate.mData, this);
                }
#ifdef HX_NATIVE
                static int sChooseModeSlotDiag = 0;
                if (DebugChooseModeSlot(prov) && sChooseModeSlotDiag < 80) {
                    const Hmx::Object *providerObj = dynamic_cast<const Hmx::Object *>(prov);
                    const Hmx::Color &overlay =
                        *(const Hmx::Color *)&curdrawstate.mScaleX;
                    float overlayAlpha = *(const float *)&curdrawstate.mData;
                    printf(
                        "DC3 UIListSlot::Draw provider=%s widget=%s/%s idx=%d showing=%d data=%d alpha=%.3f overlayAlpha=%.3f elemState=%d compState=%d pos=(%.2f,%.2f,%.2f) scale=(%.2f,%.2f,%.2f) overlay=(%.2f,%.2f,%.2f,%.2f) color=%s\n",
                        providerObj ? PathName(providerObj) : "<null>",
                        ClassName().Str(),
                        Name(),
                        i,
                        curdrawstate.mShowing,
                        curdrawstate.mData,
                        d10,
                        overlayAlpha,
                        curdrawstate.mElementState,
                        curdrawstate.mComponentState,
                        curdrawstate.mPosX,
                        curdrawstate.mPosY,
                        curdrawstate.mPosZ,
                        curdrawstate.mScaleX,
                        curdrawstate.mScaleY,
                        curdrawstate.mScaleZ,
                        overlay.red,
                        overlay.green,
                        overlay.blue,
                        overlay.alpha,
                        uicolor ? PathName(uicolor) : "<null>"
                    );
                    sChooseModeSlotDiag++;
                }
#endif
                tfa8 = tf78;
                if (ParentList())
                    ParentList()->AdjustTrans(tfa8, curdrawstate);
                CalcXfm(ctf, *(Vector3 *)&curdrawstate.mPosX, tfa8);
                tfa8.m.x.x *= curdrawstate.mScaleX;
                tfa8.m.y.y *= curdrawstate.mScaleY;
                tfa8.m.z.z *= curdrawstate.mScaleZ;
                if (cmd != kExcludeFirst || i > 0) {
                    mElements[i]->Draw(tfa8, d10, uicolor, box);
                }
                if (cmd == kDrawFirst)
                    return;
            }
        }
    }
}

void UIListSlot::Fill(const UIListProvider &prov, int display, int j, int k) {
    if (RootTrans()) {
#ifdef HX_NATIVE
        EnsureElements();
        if ((size_t)display >= mElements.size())
            return;
#endif
        MILO_ASSERT(display < mElements.size(), 0x98);
        mElements[display]->Fill(prov, j, k);
    }
}

void UIListSlot::StartScroll(int i, bool b) {
    if (b && RootTrans()) {
#ifdef HX_NATIVE
        EnsureElements();
        if (!mNextElement)
            return;
#endif
        mElements.insert(i < 0 ? mElements.begin() : mElements.end(), mNextElement);
        mNextElement = 0;
    }
}

void UIListSlot::CompleteScroll(const UIListState &liststate, int i) {
    if (RootTrans()) {
        if (mElements.size() == (unsigned)(liststate.NumDisplay() + 1)) {
            int idx = i > 0 ? 0 : liststate.NumDisplay();
            UIListSlotElement *elem = mElements[idx];
            mElements.erase(std::find(mElements.begin(), mElements.end(), elem));
            mNextElement = elem;
        }
    }
}

void UIListSlot::Poll() {
    FOREACH (it, mElements) {
        (*it)->Poll();
    }
}

bool UIListSlot::Matches(const char *cc) const {
    return strcmp(mMatchName.c_str(), cc) == 0;
}

const char *UIListSlot::MatchName() const { return mMatchName.c_str(); }

void UIListSlot::ClearElements() {
    DeleteAll(mElements);
    RELEASE(mNextElement);
}
