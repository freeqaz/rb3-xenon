#pragma once
#include "obj/Object.h"
#include "ui/UIListProvider.h"
#include "utl/Str.h"

class DataArray;
class StorePage;
class UIListLabel;
class UILabel;

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

    int mIxHighlight; // 0x20
    String mPath; // 0x24
    StorePage *mPage; // 0x30
    String mTitle; // 0x34
    int mUnk40; // 0x40
};

const char *StoreMenuProviderGetTitleFromData(DataArray *);
