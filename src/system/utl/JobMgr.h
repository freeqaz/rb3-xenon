#pragma once
#include "obj/Msg.h"
#include "obj/Object.h"
#include "stdlib.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include "xdk/xapilibi/xbase.h"
#include <vector>

class Job {
public:
    Job();
    virtual ~Job() {}
    virtual void Start() = 0;
    virtual bool IsFinished() = 0;
    virtual void Cancel(Hmx::Object *) = 0;
    virtual void OnCompletion(Hmx::Object *) {}

    int ID() const { return mID; }

    MEM_OVERLOAD(Job, 0x11);

private:
    int mID; // 0x4
};

class JobMgr {
public:
    void Poll();
    void CancelJob(int);
    JobMgr(Hmx::Object *);
    void QueueJob(Job *);
    ~JobMgr();

    MEM_OVERLOAD(JobMgr, 0x2A);

    Hmx::Object *mCallback; // 0x0
    std::list<Job *> mJobQueue; // 0x4
    bool mPreventStart; // 0xc

private:
    void CancelAllJobs();
};

class SingleItemEnumJob : public Job {
public:
    SingleItemEnumJob(Hmx::Object *, int, u64);
    virtual ~SingleItemEnumJob();
    virtual void Start();
    virtual bool IsFinished();
    virtual void Cancel(Hmx::Object *);
    virtual void OnCompletion(Hmx::Object *);

protected:
    Hmx::Object *mObject;           // 0x8
    int mUnkc;                      // 0xc
    u64 mItemID;                    // 0x10
    int mStatus;                    // 0x18
    bool mSuccess;                  // 0x1c
    int unk20;
    int unk24;
    XOVERLAPPED mOverlapped;        // 0x28
};

class PostPurchaseEnumJob : public SingleItemEnumJob {
public:
    PostPurchaseEnumJob(Hmx::Object *, int, u64, Symbol, unsigned int);
    virtual ~PostPurchaseEnumJob();
    virtual void OnCompletion(Hmx::Object *);

private:
    Symbol mOfferSymbol;            // 0x48
    int mPurchaserID;               // 0x4C
};

class MultipleItemsEnumJob : public Job {
public:
    MultipleItemsEnumJob(Hmx::Object *, int, std::vector<u64> &);
    virtual ~MultipleItemsEnumJob();
    virtual void Start();
    virtual bool IsFinished();
    virtual void Cancel(Hmx::Object *);
    virtual void OnCompletion(Hmx::Object *);

protected:
    void Poll();

    Hmx::Object *mObject;                  // 0x08
    int mUserIndex;                         // 0x0c
    std::vector<u64> mItemIDs;              // 0x10 (begin, end, capacity = 12 bytes)
    std::vector<bool> mPurchased;           // 0x1c (20 bytes)
    int mStatus;                            // 0x30
    bool mSuccess;                          // 0x34
    void *mEnumBuffer;                      // 0x38
    HANDLE mEnumHandle;                     // 0x3c
    XOVERLAPPED mOverlapped;               // 0x40
};

class MultipleItemsPostPurchaseEnumJob : public MultipleItemsEnumJob {
public:
    MultipleItemsPostPurchaseEnumJob(Hmx::Object *, int, std::vector<u64> &, Symbol, unsigned int);
    virtual ~MultipleItemsPostPurchaseEnumJob();
    virtual void OnCompletion(Hmx::Object *);

protected:
    Symbol mOfferSymbol;                    // 0x5c
    int mPurchaserID;                       // 0x60
};

DECLARE_MESSAGE(SingleItemEnumCompleteMsg, "single_item_enum_complete")
SingleItemEnumCompleteMsg(bool success, bool purchaseMade, const String &offerID)
    : Message(Type(), success, purchaseMade, offerID) {}
bool Success() const { return mData->Int(2); }
bool HasOfferID() const { return mData->Int(3); }
unsigned long long OfferID() const;
void SetPurchaseMade(bool b) { mData->Node(3) = b; }
void SetOfferID(const String &s) { mData->Node(4) = DataNode(s); }
END_MESSAGE

