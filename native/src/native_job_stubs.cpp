// Job system / store enumeration stubs — Xbox marketplace APIs
// Separate file to avoid type conflicts with asm-label stubs in engine_stubs_generated.cpp

#ifdef HX_NATIVE

#include "utl/JobMgr.h"
#include "xdk/LIBCMT/vectorintrinsics.h"
#include <cstring>

_XMMATRIX::_XMMATRIX() { std::memset(this, 0, sizeof(*this)); }

SingleItemEnumJob::SingleItemEnumJob(Hmx::Object *obj, int idx, u64 id)
    : Job(), mObject(obj), mUnkc(idx), mItemID(id), mStatus(0), mSuccess(false),
      unk20(0), unk24(0), mOverlapped() {
    std::memset(&mOverlapped, 0, sizeof(mOverlapped));
}
SingleItemEnumJob::~SingleItemEnumJob() {}
void SingleItemEnumJob::Start() { mStatus = 2; }
bool SingleItemEnumJob::IsFinished() { return true; }
void SingleItemEnumJob::Cancel(Hmx::Object *) {}
void SingleItemEnumJob::OnCompletion(Hmx::Object *) {}

MultipleItemsEnumJob::MultipleItemsEnumJob(
    Hmx::Object *obj, int userIndex, std::vector<u64> &itemIDs
) : Job(), mObject(obj), mUserIndex(userIndex), mItemIDs(itemIDs), mPurchased(), mStatus(0),
    mSuccess(false), mEnumBuffer(nullptr), mEnumHandle(0), mOverlapped() {
    std::memset(&mOverlapped, 0, sizeof(mOverlapped));
}
MultipleItemsEnumJob::~MultipleItemsEnumJob() {}
void MultipleItemsEnumJob::Start() { mStatus = 2; }
bool MultipleItemsEnumJob::IsFinished() { return true; }
void MultipleItemsEnumJob::Cancel(Hmx::Object *) {}
void MultipleItemsEnumJob::OnCompletion(Hmx::Object *) {}

PostPurchaseEnumJob::PostPurchaseEnumJob(
    Hmx::Object *obj, int userIndex, u64 itemID, Symbol offerSym, unsigned int purchaserID
) : SingleItemEnumJob(obj, userIndex, itemID), mOfferSymbol(offerSym),
    mPurchaserID(purchaserID) {}
PostPurchaseEnumJob::~PostPurchaseEnumJob() {}
void PostPurchaseEnumJob::OnCompletion(Hmx::Object *obj) {
    SingleItemEnumJob::OnCompletion(obj);
}

MultipleItemsPostPurchaseEnumJob::MultipleItemsPostPurchaseEnumJob(
    Hmx::Object *obj, int userIndex, std::vector<u64> &itemIDs, Symbol offerSym,
    unsigned int purchaserID
) : MultipleItemsEnumJob(obj, userIndex, itemIDs) {
    mOfferSymbol = offerSym;
    mPurchaserID = purchaserID;
}
MultipleItemsPostPurchaseEnumJob::~MultipleItemsPostPurchaseEnumJob() {}
void MultipleItemsPostPurchaseEnumJob::OnCompletion(Hmx::Object *obj) {
    MultipleItemsEnumJob::OnCompletion(obj);
}

unsigned long long SingleItemEnumCompleteMsg::OfferID() const { return 0ULL; }

#endif // HX_NATIVE
