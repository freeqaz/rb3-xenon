#include "meta_band/BandStoreOffer.h"
#include "meta_band/BandSongMgr.h"
#include "meta/StoreOffer.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"
#include <vector>

BandStoreOffer::BandStoreOffer(DataArray *da, SongMgr *mgr) : StoreOffer(da, mgr) {
    mUpgradeAvailable = false;
    // Retail declares both property Symbols as FUNCTION-LOCAL statics sharing
    // one guard word at 0x82E01FE0 (bit 0 = demo_id @0x82E01FDC, bit 1 =
    // upgrade_id @0x82E01FD8).
    static Symbol demo_id("demo_id");
    static Symbol upgrade_id("upgrade_id");
    const char *str;
    if (mStoreOfferData->FindData(demo_id, str, false)) {
        mDemo.songID = StorePurchaseable::OfferStringToID(str);
    }
    if (mStoreOfferData->FindData(upgrade_id, str, false)) {
        // NOTE(NCCC f377): the named local is CODEGEN-LOAD-BEARING -- do not
        // fold it back into `mUpgrade.songID = OfferStringToID(str);`.  With
        // the direct form the store is the call temp's sole consumer, so MSVC
        // glues it to the call and cannot hoist the `mSongMgr` load above it
        // (94.8%).  Naming the value makes the store its own statement, the
        // scheduler hoists `lwz r3,0xd0(r30)` to the top of the block, and RA
        // inserts the `mr r11,r3` that retail has -- 100%.  The mDemo branch
        // above keeps the direct form because retail does too.
        unsigned long long uid = StorePurchaseable::OfferStringToID(str);
        mUpgrade.songID = uid;
        BandSongMgr *bandSongMgr = dynamic_cast<BandSongMgr *>(mSongMgr);
        mUpgradeAvailable = mSongsInOffer.size() != 0;
        for (std::vector<int>::const_iterator it = mSongsInOffer.begin();
             it != mSongsInOffer.end();
             ++it) {
            if (!bandSongMgr || !bandSongMgr->GetUpgradeData(*mSongsInOffer.begin())) {
                mUpgradeAvailable = false;
                break;
            }
        }
    }
}

bool BandStoreOffer::IsCompletelyUnavailable() const {
    return StoreOffer::IsCompletelyUnavailable() && !mDemo.IsAvailable()
        && !mUpgrade.IsAvailable();
}

BEGIN_HANDLERS(BandStoreOffer)
    HANDLE_EXPR(has_available_demo, mDemo.IsAvailable())
    HANDLE_EXPR(demo_purchased, mDemo.IsPurchased())
    HANDLE_EXPR(demo, &mDemo)
    HANDLE_EXPR(has_available_upgrade, mUpgrade.IsAvailable())
    HANDLE_EXPR(upgrade_purchased, mUpgrade.IsPurchased())
    HANDLE_EXPR(upgrade_in_library, mUpgradeAvailable)
    HANDLE_EXPR(upgrade, &mUpgrade)
    HANDLE_SUPERCLASS(StoreOffer)
    HANDLE_CHECK(0x79)
END_HANDLERS
