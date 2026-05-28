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
    // Layout: starts after StorePanel (0x90) and MsgSource (sub-object 0x90..0xAC)
    DataNetLoader *mMetadataLoader; // 0xAC
    String mLastRequest; // 0xB0
    bool mLastRequestExtra; // 0xBC
    StoreOfferProvider *mOfferProvider; // 0xC0
    StoreOfferContentsProvider *mOfferContentsProvider; // 0xC4
    String mPrevChunkPath; // 0xC8 (request_prev_chunk path)
    String mNextChunkPath; // 0xD4 (request_next_chunk path)
    Symbol mSort; // 0xE0
    String mMenuTitle; // 0xE4
    bool mStartBrowserAtBottom; // 0xF0
    bool mUserCanDoInput; // 0xF1
    BandStoreShortcutProvider *mShortcutProvider; // 0xF4
};
