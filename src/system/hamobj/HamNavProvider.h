#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "ui/UIListProvider.h"
#include "utl/MemMgr.h"

class HamNavList;

/** "List of data for HamNavList" */
class HamNavProvider : public Hmx::Object, public UIListProvider {
public:
    enum CheckboxMode {
        kCheckbox_None = 0,
        kCheckbox_Disabled = 1,
        kCheckbox_Enabled = 2
    };
    struct NavItem {
        NavItem()
            : mLabel(gNullStr), mStarMode(0), mCheckboxState(kCheckbox_None), mEnabled(1),
              mHidden(0), mFormatArgs(0), mSubListProvider(0) {}
        NavItem(const NavItem &other)
            : mLabel(other.mLabel), mLabels(other.mLabels), mSongID(other.mSongID),
              mStarMode(other.mStarMode), mCheckboxState(other.mCheckboxState),
              mEnabled(other.mEnabled), mHidden(other.mHidden),
              mFormatArgs(other.mFormatArgs), mSubListProvider(other.mSubListProvider) {
            if (mFormatArgs)
                mFormatArgs->AddRef();
        }
        ~NavItem() {
            if (mFormatArgs) {
                mFormatArgs->Release();
                mFormatArgs = nullptr;
            }
        }

        // Retail RB3-360 order: mLabels sits immediately after mLabel (0x4) and
        // mCheckboxState is at 0x18 -- the exact inverse of the DC3 order we had.
        // Evidence: operator<<(BinStream&, const NavItem&) emits, in source order,
        // `lwz r11, 0x18(r31)` (mCheckboxState) then `addi r4, r31, 0x4`
        // (&mLabels); ours emitted 0x4 then 0x18. sizeof is 0x28 either way.
        /** "used for a list entry with a single label" */
        Symbol mLabel; // 0x0
        /** "used for a list entry with a list of labels" */
        std::vector<Symbol> mLabels; // 0x4
        int mSongID; // 0x10 - song ID
        int mStarMode; // 0x14
        CheckboxMode mCheckboxState; // 0x18
        bool mEnabled; // 0x1c
        bool mHidden; // 0x1d
        DataArray *mFormatArgs; // 0x20
        DataProvider *mSubListProvider; // 0x24
    };
    // Hmx::Object
    virtual ~HamNavProvider();
    OBJ_CLASSNAME(HamNavProvider);
    OBJ_SET_TYPE(HamNavProvider);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // UIListProvider
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const { return nullptr; }
    virtual UIListProvider *Provider(int, int, UIListSubList *) const;
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const { return mNavItems.size(); }
    virtual bool IsActive(int) const;
    virtual bool IsHidden(int) const;

    OBJ_MEM_OVERLOAD(0x17)
    NEW_OBJ(HamNavProvider)
    static void Init();

    void SetChecked(Symbol, bool, bool);
    void SelectRadioButton(Symbol);
    void SetStars(Symbol, int, bool);
    void SetLabel(int, Symbol);
    Symbol DataSymbol(int, int) const;
    void SetLabel(int, int, Symbol);
    void SetLabels(int, DataArray *);
    void ResetLabelProvider(int);
    void SetEnabled(int, bool);
    bool IsEnabled(int) const;
    void SetHidden(int, bool);
    void AppendNavItem();

    void SetNavList(HamNavList *l) { mNavList = l; }
    std::vector<NavItem> &Items() { return mNavItems; }

    DataNode OnSetHidden(const DataArray *);

    friend class HamNavList;

protected:
    HamNavProvider();

    void CreateSubListProvider(int);
    int FindLabel(Symbol);

    DataNode OnSetEnabled(const DataArray *);
    DataNode OnSetFormatArgs(const DataArray *);

    std::vector<NavItem> mNavItems; // 0x30
    HamNavList *mNavList; // 0x3c
};
