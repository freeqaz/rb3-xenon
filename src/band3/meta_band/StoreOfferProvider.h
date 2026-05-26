#pragma once
#include "obj/Object.h"
#include "ui/UIListProvider.h"
#include "utl/Symbol.h"
#include <vector>

class DataArray;
class StoreOffer;
class RndMat;
class RndDir;
class UIListLabel;
class UILabel;
class UIListMesh;

class StoreOfferProvider : public Hmx::Object, public UIListProvider {
public:
    class Element {
    public:
        Element()
            : mOffer(0),
              mGroupHeading(),
              mShortcut(),
              mLocalize(true),
              mIsCover(false),
              mActive(true) {}

        Element(
            StoreOffer *offer,
            Symbol groupHeading,
            bool localize,
            bool isCover,
            bool active
        )
            : mOffer(offer),
              mGroupHeading(groupHeading),
              mShortcut(gNullStr),
              mLocalize(localize),
              mIsCover(isCover),
              mActive(active) {}

        StoreOffer *mOffer; // 0x0
        Symbol mGroupHeading; // 0x4
        Symbol mShortcut; // 0x8
        bool mLocalize; // 0xc
        bool mIsCover; // 0xd
        bool mActive; // 0xe
    };

    StoreOfferProvider(
        std::vector<StoreOffer *> *offers, std::vector<StoreOffer *> *packs
    );
    virtual ~StoreOfferProvider();
    virtual DataNode Handle(DataArray *, bool);
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const;
    virtual bool IsActive(int) const;
    virtual void InitData(RndDir *);

    void BuildList(DataArray *);
    void ClearList();
    StoreOffer *FindOffer(Symbol) const;
    const StoreOffer *FindPack(const StoreOffer *) const;
    const StoreOffer *FindAlbum(const StoreOffer *) const;
    Symbol PosToShortcut(int);
    int ShortcutToPos(Symbol);
    int PosToNextGroupPos(int);
    int PosToPrevGroupPos(int);
    Element *GetElementAtIndex(int) const;

protected:
    DataArray *mShortcuts; // 0x20
    std::vector<StoreOffer *> *mOffers; // 0x24
    std::vector<StoreOffer *> *mPacks; // 0x28
    std::vector<Element *> mElements; // 0x2c
    RndMat *mAlbumBgMat; // 0x34
    RndMat *mGroupBgMat; // 0x38
    RndMat *mSongBgMat; // 0x3c
};
