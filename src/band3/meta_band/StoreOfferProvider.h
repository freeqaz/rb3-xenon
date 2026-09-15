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

    StoreOfferProvider(std::vector<StoreOffer *> *offers);
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
    // Retail fn_82663328 (0xB4).  Implemented in W16-CD; const because its
    // only caller, ShowBrowserPurchased, is const and passes `this` straight
    // through in r3.
    StoreOffer *FindSongOffer(int) const;
    // Retail fn_826635D8 (0x188).  W16-CD implemented the real body (row is at
    // 100.0); it is no longer the signature-only stub this comment described.
    // noinline is RETAINED: retail's body is out-of-line (Text calls it via
    // `bl` at three sites and Handle at one), and it is small enough that /Ob2
    // would otherwise inline it back into those callers.
    __declspec(noinline) bool ShowBrowserPurchased(const StoreOffer *) const;

protected:
    // Retail X360 layout (from ctor/NumData/InitData asm, rel to UIListProvider
    // subobject at full 0x28): vtable@0x0, mShortcuts@0x4, mOffers@0x8,
    // mElements@0xc (12B), mAlbumBgMat@0x18, mGroupBgMat@0x1c, mSongBgMat@0x20.
    // NO mPacks member: sizeof(StoreOfferProvider) is 0x4c on retail (the ctor
    // `operator new` call site is `li r3, 0x4c`, verified against
    // BandStorePanel's ctor at fn_82605128), not 0x50. The rb3-Wii dev oracle's
    // `packs` ctor param + mPacks field are dev-only; retail's ctor takes a
    // single `offers` pointer (confirmed by the Ghidra decomp of the retail
    // ctor: only one extra arg, `this+0x3c`, is passed to the StoreOfferProvider
    // constructor call).
    DataArray *mShortcuts; // 0x2c
    std::vector<StoreOffer *> *mOffers; // 0x30
    std::vector<Element *> mElements; // 0x34
    RndMat *mAlbumBgMat; // 0x40
    RndMat *mGroupBgMat; // 0x44
    RndMat *mSongBgMat; // 0x48
};
