#pragma once
#include "obj/Object.h"
#include "ui/UIListProvider.h"
#include "utl/Str.h"

class DataArray;
class UIListLabel;
class UILabel;

// Retail X360 rewrite: the 360 store menu is driven by plain DataArrays
// ("submenus" / "new_releases") instead of the Wii build's
// StorePackedMetadata pages. Layout verified against retail fns
// 0x82656AD8-0x8265760C (ctor 0x826574C0, ??_G 0x826575C0).
class StoreMenuProvider : public UIListProvider, public Hmx::Object {
public:
    StoreMenuProvider(DataArray *, const char *);
    virtual ~StoreMenuProvider();
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual int NumData() const;
    virtual bool IsActive(int) const;
    virtual DataNode Handle(DataArray *, bool);

    void SetData(DataArray *);
    const char *GetTitle();
    const char *GetFileName(int);
    bool IsRandomSelect(DataArray *) const;

    int mIxHighlight; // 0x2c
    String mPath; // 0x30
    DataArray *mData; // 0x3c - the raw menu DataArray (ref-counted)
    DataArray *mSubmenus; // 0x40 - mData->FindArray("submenus", true)
    DataArray *mBannerData; // 0x44 - mData->FindArray("new_releases", false)
};
