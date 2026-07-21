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
class StoreOfferContentsProvider;
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
    virtual int StoreUser() const;
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
    DataNetLoader *mMetadataLoader; // 0xA0
    String mLastRequest; // 0xA4
    bool mLastRequestExtra; // 0xB0
    StoreOfferProvider *mOfferProvider; // 0xB4
    StoreOfferContentsProvider *mOfferContentsProvider; // 0xB8
    // Retail order pinned by AppLabel::SetStoreCrumbText: its MenuTitle().c_str()
    // load reads mMenuTitle+8 at retail offset 0xDC — i.e. retail mMenuTitle
    // (0xD4) FOLLOWS both chunk-path strings (verified via objdiff: with the
    // shrunk base, mMenuTitle must sit +0x18 above the two chunk Strings).
    String mPrevChunkPath; // 0xBC (request_prev_chunk path)
    String mNextChunkPath; // 0xC8 (request_next_chunk path)
    String mMenuTitle; // 0xD4
    Symbol mSort; // 0xE0
    bool mStartBrowserAtBottom; // 0xE4
    bool mUserCanDoInput; // 0xE5
    BandStoreShortcutProvider *mShortcutProvider; // 0xE8 (unverified vs retail)
};
