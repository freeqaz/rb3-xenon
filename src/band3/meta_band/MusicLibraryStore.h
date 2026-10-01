#pragma once
#include "meta/StoreOffer.h"
#include "obj/Object.h"
#include "stl/_vector.h"

class DataArray;
class DataNetLoader;
class LocalUser;
class NetCacheLoader;
class RndTex;
class StorePreviewMgr;
class StorePurchaser;
class ContentInstalledMsg;

/** Retail-only (Xbox 360) in-game DLC store behind the music library: it
    downloads the upsell offer list, buys single songs through XboxPurchaser,
    tracks their marketplace downloads, loads store art and drives the song
    preview. Owned by MusicLibrary (its +0x19c member) and polled every frame by
    MusicLibrary::Poll. NO surviving source (absent from DC3 too). Everything
    here is reconstructed from the retail XEX (title 45410914, TU5).

    RTTI: `.?AVMusicLibraryStore@@`, vtable @0x820adf94, Complete Object Locator
    @0x821e1a88 (attributes 0x0 = single inheritance, no virtual bases).
    (ADDRESSES CORRECTED, lane W16-HEADERTRUTH: this used to cite vtable
    @0x820abc8c / COL @0x821da4e0 / CHD @0x821da4f4. None of the three is what
    it claimed -- 0x820abc8c has no COL at [-1] and is EH data, the word at
    +4 being the MSVC FuncInfo magic 0x19930522. The decode below is confirmed
    against the real COL: it decodes to exactly 3 classes in that order.)
    The CHD decodes to a plain single-inheritance chain (3 classes, all
    mdisp=0/pdisp=-1):

        MusicLibraryStore  ->  Hmx::Object  ->  ObjRefOwner (RTTI: "ObjRef")

    Among the standard slots it overrides the destructor and Handle (slot 6,
    0x825BD260). Retail's AssetStore vtable carries the SAME Handle address:
    the two classes' Handle bodies are identical and ICF-folded.

    METHOD NAMES ARE OURS. Retail keeps no symbols, no surviving source has
    this class, and MusicLibrary calls every method directly, so only the owning
    class is evidenced. Each name below describes what the retail body does.

    Size 0x64 (MusicLibrary::OnLoad allocates `li r3,0x64`). Members are read
    off the retail bodies cited on each line. */
class MusicLibraryStore : public Hmx::Object {
public:
    /** One marketplace download being watched by Poll (the struct name predates
        this reading and is kept because proven ICF alias records spell it).
        8 bytes: Poll steps
        `addi r29,r29,0x8` and reads the song id at +0 and the user at +4. */
    struct OverlappedIO {
        int mSongID; // 0x0
        LocalUser *mUser; // 0x4
    };
    /** The songs handed to the purchaser, kept until the purchase finishes.
        PurchaseSongs allocates 0x10 bytes and zeroes all four words; Poll
        walks the vector at +0 and copies the user from +0xc. */
    struct PendingPurchase {
        PendingPurchase() : mUser(0) {}
        std::vector<int> mSongIDs; // 0x0
        LocalUser *mUser; // 0xc
    };

    MusicLibraryStore();
    virtual ~MusicLibraryStore();
    virtual DataNode Handle(DataArray *, bool);

    /** retail 0x825BDCB8. */
    void Poll();
    /** retail 0x825BCA38 — requests the upsell offer index. MusicLibrary calls it
        from OnEnter (when the store is ready) and from Poll (once, on the first
        frame the store reaches state 2). */
    void LoadOffers();
    /** retail 0x825BC908. */
    void ClearPreview();
    /** retail 0x825BC900 — a frameless 2-instruction tail-jump thunk
        `lwz r3,0x4c(r3); b 0x827B1B78`, i.e. `{ mPreviewMgr->ClearCurrentPreview(); }`.
        Its single retail caller is MusicLibrary::ClearSongPreview (0x8253AD58).
        It has NO .pdata entry of its own — a live instance of
        ".pdata-absence is not a not-a-function test". */
    void ClearCurrentPreview();
    /** retail 0x825BCDF0 — plays the offer's preview_audio, or clears the preview
        when the offer has none. */
    void SetStorePreview(int);
    /** retail 0x825BC9D8 — linear search of mOffers by single-song id. */
    StoreOffer *FindOfferBySongID(int) const;
    /** retail 0x825BCBD0. Defined in the .cpp: MusicLibrary calls it out of line,
        and only PurchaseSongs, in this TU, inlines it. */
    bool IsDownloading(int songID) const;
    /** retail 0x825BCC10 — fetches the offer's preview_art through the net cache;
        Poll loads it into mStoreArt and sends art_loaded to `receiver`. */
    void LoadStoreArt(int songID, Hmx::Object *receiver);
    /** retail 0x825BD8C8 — buys the first song in `songIDs` not already
        downloading, through an XboxPurchaser. */
    void PurchaseSongs(LocalUser *user, const std::vector<int> &songIDs);

    int mState; // 0x28  (0=idle, 1=net cache failed, 2=ready, 3=clearing, 4=done)
    std::vector<StoreOffer *> mOffers; // 0x2c (OWNED; dtor DeleteAll's them)
    DataNetLoader *mOfferLoader; // 0x38 (LoadOffers' index download)
    DataArray *mOfferData; // 0x3c (the loaded index; Poll parses mOffers from it)
    NetCacheLoader *mArtLoader; // 0x40 (LoadStoreArt)
    Hmx::Object *mArtReceiver; // 0x44 (gets art_loaded; Poll vcalls Handle on it)
    RndTex *mStoreArt; // 0x48 (MusicLibrary's get_store_art returns this)
    StorePreviewMgr *mPreviewMgr; // 0x4c (heap, 0x60 bytes)
    PendingPurchase *mPendingPurchase; // 0x50
    std::vector<OverlappedIO> mDownloads; // 0x54
    StorePurchaser *mPurchaser; // 0x60 (an XboxPurchaser, see PurchaseSongs)

    static const char *sUpsellIndexFile;

private:
    /** retail 0x825BD1F8. */
    DataNode OnMsg(const ContentInstalledMsg &);
    /** retail 0x825BCFF0 — pushes basic_confirm_dialog_screen with
        ml_store_signin (signedIn 0) or ml_store_purchase_error. Retail tests
        the argument with `cmpwi`, so it is an int, not a bool. */
    void ShowPurchaseError(int signedIn);
    /** retail 0x825BD770 — rebuilds `offers` from the index's (offers ...) array,
        dropping test offers and offers without a valid title. */
    void ParseOffers(DataArray *data, std::vector<StoreOffer *> &offers);
};
