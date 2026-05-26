#pragma once
#include "game/Defines.h"
#include "meta_band/SongSortMgr.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "ui/UIListProvider.h"
#include "utl/Symbol.h"
#include <map>
#include <vector>

class UIListLabel;
class UIListMesh;
class UIListCustom;
class UIListWidget;
class UIColor;
class UILabel;
class RndMat;
class RndDir;
class DataArray;
class DataNode;

// A single per-list-row setting in the song-select "view settings" panel.
//
// Subclasses (Sort, Filter, ScoreType, Header) provide the option list,
// status text, and SelectOption side effects.
class ViewSetting : public UIListProvider {
public:
    enum Type {
        kBool = 0,
        kSort = 1,
        kFilter = 2,
    };

    ViewSetting(Symbol name) : mName(name), mEvenMat(nullptr), mOddMat(nullptr) {}
    virtual ~ViewSetting() {}

    // UIListProvider overrides
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual void Custom(int, int, UIListCustom *, Hmx::Object *) const;
    virtual int NumData() const = 0;
    virtual bool IsActive(int) const;
    virtual void InitData(RndDir *);

    // ViewSetting interface
    virtual const char *GetCurrentStatus() const = 0;
    virtual bool CanSelectMultiple() const;
    virtual void Reset();
    virtual void Refresh();
    virtual bool IsValid() const;
    virtual bool IsHeader() const;
    virtual void SelectOption(int) = 0;
    virtual int StartingOption() const;

    Symbol GetName() const { return mName; }

protected:
    Symbol mName; // 0x4
    RndMat *mEvenMat; // 0x8
    RndMat *mOddMat; // 0xC
};

// "Header" row that just labels a section in the list.
class HeaderViewSetting : public ViewSetting {
public:
    HeaderViewSetting(Symbol name) : ViewSetting(name) {}
    virtual ~HeaderViewSetting() {}

    virtual int NumData() const { return 0; }
    virtual const char *GetCurrentStatus() const { return gNullStr; }
    virtual void Reset() {}
    virtual bool IsHeader() const { return true; }
    virtual void SelectOption(int) { MILO_ASSERT(false, 0x91); }
};

// Setting that toggles the active song-sort.
class SortViewSetting : public ViewSetting {
public:
    SortViewSetting() : ViewSetting("sort_setting") {}
    virtual ~SortViewSetting() {}

    virtual int NumData() const { return kNumSongSortTypes - 1; }
    virtual bool IsActive(int) const;
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual const char *GetCurrentStatus() const;
    virtual void SelectOption(int);
    virtual int StartingOption() const;
};

// Setting that switches between a primary score type and an alternate
// (e.g. solo vs. band, guitar vs. pro-guitar) for the score display.
class ScoreTypeViewSetting : public ViewSetting {
public:
    ScoreTypeViewSetting()
        : ViewSetting("visible_scores"), mScoreType(kScoreBand) {}
    virtual ~ScoreTypeViewSetting() {}

    virtual int NumData() const { return 2; }
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual const char *GetCurrentStatus() const;
    virtual void Refresh();
    virtual bool IsValid() const;
    virtual void SelectOption(int);
    virtual int StartingOption() const;

    ScoreType GetBaseScoreType() const;
    ScoreType GetAlternateScoreType() const;

protected:
    ScoreType mScoreType; // 0x10
};

// Setting that toggles individual entries within one filter category
// (genre, decade, difficulty, etc.).
class FilterViewSetting : public ViewSetting {
    friend class ViewSettingsProvider;
public:
    struct Filter {
        Symbol mSym; // 0x0
        int mCount; // 0x4
    };

    FilterViewSetting(FilterType ft)
        : ViewSetting(FilterTypeToSym(ft)), mFilterType(ft) {}
    virtual ~FilterViewSetting() {}

    virtual int NumData() const;
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual void Custom(int, int, UIListCustom *, Hmx::Object *) const;
    virtual const char *GetCurrentStatus() const;
    virtual bool CanSelectMultiple() const;
    virtual void Reset();
    virtual bool IsValid() const;
    virtual void SelectOption(int);

    void SetFilterData(const std::map<Symbol, int> &);

    static Symbol FilterTypeToSym(FilterType);
    static bool CompareFilters(const Filter &, const Filter &);

protected:
    FilterType mFilterType; // 0x10
    std::vector<Filter> mFilters; // 0x14
};

// Aggregate UI list provider that owns the per-list ViewSettings.
class ViewSettingsProvider : public UIListProvider, public Hmx::Object {
public:
    ViewSettingsProvider();
    virtual ~ViewSettingsProvider();

    // UIListProvider overrides
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual int NumData() const;
    virtual bool IsActive(int) const;
    virtual void InitData(RndDir *);
    virtual UIColor *SlotColorOverride(int, int, UIListWidget *, UIColor *) const;

    // Hmx::Object overrides
    virtual DataNode Handle(DataArray *, bool);

    void BuildFilters(Symbol);
    int SelectSetting(int);
    void RefreshAllSettings();
    void ResetAllSettings();
    void ResetActiveSetting();

protected:
    std::vector<ViewSetting *> mSettings; // 0x20
    ViewSetting *mActiveSetting; // 0x28
    UIColor *mDisabledColor; // 0x2C
    RndMat *mHeaderMat; // 0x30
    RndMat *mEvenMat; // 0x34
    RndMat *mOddMat; // 0x38
};
