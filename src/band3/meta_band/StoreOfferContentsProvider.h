#pragma once
#include "obj/Object.h"
#include "ui/UIListProvider.h"
#include "utl/VectorSizeDefs.h"
#include <vector>

class DataArray;
class RndDir;
class StoreOffer;
class StorePackedSong;
class UIListLabel;
class UILabel;
class UIListMesh;
class UIListCustom;
class RndMat;

class StoreOfferContentsProvider : public Hmx::Object, public UIListProvider {
public:
    enum ListType {
        kListPurchase = 0,
        kListDownload = 1,
    };

    struct Element {
        StorePackedSong *mSong; // 0x0
        bool mChecked; // 0x4
    };

    StoreOfferContentsProvider();
    virtual ~StoreOfferContentsProvider();
    virtual DataNode Handle(DataArray *, bool);
    virtual void InitData(RndDir *);
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual void Custom(int, int, UIListCustom *, Hmx::Object *) const;
    virtual Symbol DataSymbol(int) const;
    virtual bool IsActive(int) const;
    virtual int NumData() const;

    void BuildList(StoreOffer *, ListType);
    void ClearList();
    void SetChecked(int, bool);
    void ToggleChecked(int);
    void ToggleAllChecked();
    void AcceptCurChecked();
    void RefreshBlocks();
    bool SpecifyFirstSongContents();
    bool SpecifyNextSongContents();
    bool AnyChecked();
    bool AllChecked();
    int NumChecked();

    StoreOffer *mOffer; // 0x20
    std::vector<Element * VECTOR_SIZE_SMALL> mElements; // 0x24 (data 0x24, size 0x28 u16, capacity 0x2a u16)
    int mCurrentSongIndex; // 0x2c
    int mSpecifiedCount; // 0x30
    ListType mListType; // 0x34
    int unk38; // 0x38
    bool unk3c; // 0x3c
};
