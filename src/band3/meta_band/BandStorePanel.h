#pragma once
#include "meta/StorePanel.h"
#include "obj/Msg.h"
#include "ui/UIListProvider.h"
#include "utl/Str.h"

class UIList;
class DataNetLoader;
class LocalUserLeftMsg;
class MetadataLoadedMsg;
class StoreOfferProvider;
// RB3 store architecture uses StorePackedOfferBase (not DataArray *) in MakeNewOffer.
// Forward-declare here; full definition lives in meta/StorePackedMetadata.h (not yet ported).
class StorePackedOfferBase;

class BandStoreShortcutProvider : public DataProvider {
public:
    BandStoreShortcutProvider(DataArray *arr)
        : DataProvider(arr, 0, false, false, 0) {}
    virtual ~BandStoreShortcutProvider() {}
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    inline const char *RawTextAtData(int i) const;
};

class BandStorePanel : public StorePanel, public MsgSource {
public:
    BandStorePanel();
    OBJ_CLASSNAME(BandStorePanel);
    OBJ_SET_TYPE(BandStorePanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~BandStorePanel();

    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual void Unload();
    virtual bool IsLoaded() const;

    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual bool IsSongInLibrary(const int &) const;
    virtual void ExitStore(StoreError) const;
    virtual LocalUser *StoreUser() const;
    virtual StoreOffer *MakeNewOffer(const StorePackedOfferBase *, bool);
    virtual StoreOffer *FindOffer(Symbol) const;
    virtual bool EnumerateSubsetOfOfferIDs() const { return true; }
    virtual void LoadArt(const char *, UIPanel *);
    virtual int UpdateOffers(const std::list<EnumProduct> &, bool);
    virtual void StoreUserProfileSwappedToUser(LocalUser *) {}

    DataNode OnMsg(const MetadataLoadedMsg &);
    DataNode OnMsg(const LocalUserLeftMsg &);

    static BandStorePanel *Instance();
    StoreOffer *GetLoneOffer(bool) const;
    const char *ShortcutTextAtData(int);
    void SetShortcutData(DataArray *);
    void ApplyShortcutProvider(UIList *);
    Symbol SortName();
    const char *GetIndexFile() const;
    const char *GetRequestPrefix() const;
    void Request(const String &, bool);
    NEW_OBJ(BandStorePanel);
    static void Init() { REGISTER_OBJ_FACTORY(BandStorePanel); }

    String &MenuTitle() { return mMenuTitle; }
    const String &MenuTitle() const { return mMenuTitle; }

protected:
    // Layout: starts after StorePanel (non-virtual 0x8c) + MsgSource sub-object,
    // so BandStorePanel's own members begin at 0xA0 (retail-exact, verified via
    // offsetof probe after the StorePanel base shrank to retail size).
    //
    // The own-member block is 0xA0..0xE8 = 72 bytes, read directly out of the
    // retail constructor fn_82605128 (band.exe), whose init stores are, in
    // offset order: stw 0,0xA0 / String::String() 0xA4 / stb 0,0xB0 /
    // stw <new StoreOfferProvider>,0xB4 / String::String() 0xB8 /
    // String::String() 0xC4 / Symbol::Symbol(gNullStr) 0xD0 /
    // String::String() 0xD4 / stb 0,0xE0 / stw 0,0xE4, then vtordisp at 0xE8
    // and Object::Object(this+0xEC).
    DataNetLoader *mMetadataLoader; // 0xA0
    String mLastRequest; // 0xA4
    bool mLastRequestExtra; // 0xB0
    StoreOfferProvider *mOfferProvider; // 0xB4
    // NO mOfferContentsProvider on retail 360 (the rb3-Wii oracle carries one
    // at its 0xC4). Three independent proofs: (a) mapping the Wii member list
    // onto the retail ctor's stores, the offset delta steps from Wii-0x0C to
    // Wii-0x10 exactly at that member and nowhere else; (b) the retail ctor
    // performs exactly ONE operator new (0x4c bytes -> mOfferProvider), where
    // the Wii ctor performs two; (c) the token "offer_contents_provider" does
    // not occur anywhere in band.exe, while every other BandStorePanel handler
    // string does, contiguously at .rdata 0x820bfb90..0x820bfc90. Its absence
    // is what makes the block 72 rather than 76 bytes, putting the Object
    // vbase displacement at 0xEC instead of 0xF0.
    String mPrevChunkPath; // 0xB8 (request_prev_chunk path)
    String mNextChunkPath; // 0xC4 (request_next_chunk path)
    // mSort PRECEDES mMenuTitle (as in the rb3-Wii oracle): the retail ctor
    // calls Symbol::Symbol(gNullStr) on this+0xD0 and String::String() on
    // this+0xD4. This still satisfies the AppLabel::SetStoreCrumbText pin
    // (MenuTitle().c_str() reads mMenuTitle+8 at retail 0xDC = 0xD4+8); the
    // previous tree swapped these two to compensate for the extra pointer
    // above, which produced the right mMenuTitle offset for the wrong reason.
    Symbol mSort; // 0xD0
    String mMenuTitle; // 0xD4
    bool mStartBrowserAtBottom; // 0xE0
    bool mUserCanDoInput; // 0xE1
    BandStoreShortcutProvider *mShortcutProvider; // 0xE4
};
