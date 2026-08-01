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
    // Retail fn_826635D8 (0x188). See the deferred-work comment block in
    // StoreOfferProvider.cpp for the full retail body (needs FindSongOffer,
    // which is not implemented yet); this signature-only stub exists so
    // Handle()'s show_browser_purchased arm can emit a real `bl` (matching
    // retail's guard-bit-numbered local-static dispatch) instead of an
    // inline byte load against the global Symbol. noinline: retail's real
    // body is out-of-line (called via bl, not folded into Handle); our
    // trivial one-liner stub would otherwise get /Ob2-inlined right back
    // into the lbz Handle used to emit before this port.
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
