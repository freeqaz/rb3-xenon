#pragma once
#include "obj/Object.h"
#include "rndobj/Dir.h"
#include "stl/_vector.h"
#include "ui/UIListProvider.h"
#include "ui/UIListState.h"
#include "ui/UIListWidget.h"
#include "utl/MemMgr.h"

enum UIListOrientation {
    kUIListVertical,
    kUIListHorizontal
};

/** "Top-level resource object for UILists" */
class UIListDir : public RndDir, public UIListProvider, public UIListStateCallback {
public:
    UIListDir();
    // Hmx::Object
    virtual ~UIListDir();
    OBJ_CLASSNAME(UIListDir)
    OBJ_SET_TYPE(UIListDir)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // ObjectDir
    virtual void SyncObjects();
    // RndDrawable
    virtual void DrawShowing();
    // RndPollable
    virtual void Poll();
    // UIListProvider
    virtual int NumData() const;
    virtual float GapSize(int, int, int, int) const;
    virtual bool IsActive(int) const;
    // UIListStateCallback
    virtual void StartScroll(const UIListState &, int, bool);
    virtual void CompleteScroll(const UIListState &);

    UIListOrientation Orientation() const;
    float ElementSpacing() const;
    UIList *SubList(int, std::vector<UIListWidget *> &);
    void DrawWidgets(
        UIListWidgetDrawState &,
        UIListState const &,
        std::vector<UIListWidget *> &,
        class Transform const &,
        UIComponent::State,
        Box *,
        bool
    );
    void PollWidgets(std::vector<UIListWidget *> &);
    void FillElement(UIListState const &, std::vector<UIListWidget *> &, int);
    void StartScroll(UIListState const &, std::vector<UIListWidget *> &, int, bool);
    void CompleteScroll(UIListState const &, std::vector<UIListWidget *> &);
    void FillElements(UIListState const &, std::vector<UIListWidget *> &);
    void ListEntered();
    void BuildDrawState(
        UIListWidgetDrawState &, UIListState const &, UIComponent::State, float, bool
    ) const;
    void CreateElements(UIList *, std::vector<UIListWidget *> &, int);

    NEW_OBJ(UIListDir)
    OBJ_MEM_OVERLOAD(0x1b)

protected:
    /** "scroll direction of list" */
    UIListOrientation mOrientation; // 0x1e4
    /** "Number of elements to fade from beginning/end of list". Ranges from 0 to 10. */
    int mFadeOffset; // 0x1e8
    /** "spacing between elements". Ranges from 0 to 1000. */
    float mElementSpacing; // 0x1ec
    /** "point during scroll when highlight changes". Ranges from 0 to 1. */
    float mScrollHighlightChange; // 0x1f0
    /** "draw widgets in preview mode?" */
    bool mTestMode; // 0x1f4
    UIListState mTestState; // 0x1f8
    /** "total number of data elements" */
    int mTestNumData; // 0x240
    /** "test gaps between elements". Ranges from 0 to 1000. */
    float mTestGapSize; // 0x244
    UIComponent::State mTestComponentState; // 0x248
    /** "test disable every other element" */
    bool mTestDisableElements; // 0x24c
    std::vector<UIListWidget *> mTestWidgets; // 0x250
    int mDirection; // 0x25c

private:
    float SetElementPos(Vector3 &v, float position, int gridSpan, float primaryBase, float secondaryBase) const;
    void Reset();
};
