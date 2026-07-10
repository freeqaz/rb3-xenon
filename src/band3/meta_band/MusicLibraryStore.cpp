#include "meta_band/MusicLibraryStore.h"
#include "meta/StoreOffer.h"

StoreOffer *MusicLibraryStore::FindOfferBySongID(int id) const {
    for (std::vector<StoreOffer *>::const_iterator it = mOffers.begin();
         it != mOffers.end();
         ++it) {
        StoreOffer *offer = *it;
        if (offer->GetSingleSongID() == id)
            return offer;
    }
    return NULL;
}
