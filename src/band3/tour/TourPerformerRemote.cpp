#include "tour/TourPerformerRemote.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "tour/Tour.h"
#include "tour/TourPerformer.h"
#include "tour/TourProgress.h"
#include "utl/Symbol.h"

TourPerformerRemote::TourPerformerRemote(BandUserMgr &bum) : TourPerformerImpl(bum) {}

TourPerformerRemote::~TourPerformerRemote() {}

void TourPerformerRemote::SyncLoad(BinStream &bs, uint ui) {
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 38);
    pProgress->SyncLoad(bs);
    bs >> mQuestFilter;
    int i = 0;
    uint bs_name = 0;
    bs >> bs_name;
    mFilterType = (TourSetlistType)bs_name;
    std::vector<GigData> &gd_vec = mGigData;
    gd_vec.erase(gd_vec.begin(), gd_vec.end());
    int siz;
    bs >> siz;
    for (; i < siz; i++) {
        GigData gd(420);
        bs >> gd.unk0;
        bs >> gd.unk4;
        bs >> gd.unk8;
        bs >> gd.unkc;
        gd_vec.push_back(gd);
    }
}

void TourPerformerRemote::OnSynchronized(uint ui) {
    static Symbol update_tour_display("update_tour_display");
    DataArrayPtr pDA(update_tour_display);
    pDA->Execute();
}
