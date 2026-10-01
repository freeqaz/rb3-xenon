#pragma once
#include "net_band/EntityUploader.h"
#include "xdk/XAPILIB.h"
#include "xdk/XONLINE.h"

// Retail TU5 class, RTTI `.?AVXboxEntityUploader@@`, vtable 0x82085054
// (29 slots). No surviving source has it; everything here is read off the
// retail bytes at 0x82508708-0x8250A510.
//
// It runs every name an EntityUploader op is about to send through Xbox LIVE's
// XStringVerify (locale "en-us", up to 10 strings per call), records the
// per-string verdict, and only then hands the ops to
// EntityUploader::BeginRockCentralOps. The vtable overrides Handle, Init,
// Terminate, VerifyBandName, VerifyCharName, UpdateChar, UpdateFromProfile and
// Poll; UpdateSetlist (slot 26) and the deleting destructor (slot 0, shared
// with EntityUploader at 0x8250D530) are inherited.
//
// Retail carries no symbol names: the non-virtual helpers are named by what
// they do.
class XboxEntityUploader : public EntityUploader {
public:
    // Each mStringChecks entry is a DataArray of 4: { op id, string kind,
    // string, verify result }. The kind picks the error code a rejected
    // string reports (see Poll).
    enum StringKind {
        kStringCharName = 1,
        kStringBandName = 2,
        kStringSetlistTitle = 7,
        kStringSetlistDesc = 8
    };

    virtual DataNode Handle(DataArray *, bool);
    virtual void Init();
    virtual void Terminate();
    virtual bool VerifyBandName(char const *, TourSavable *, Hmx::Object *);
    virtual bool VerifyCharName(char const *, TourSavable *, Hmx::Object *);
    virtual bool UpdateChar(TourCharLocal *, Hmx::Object *);
    virtual bool UpdateFromProfile(BandProfile *, Hmx::Object *);
    virtual void Poll();

    void NotifySavable(TourSavable *, int, int, bool);
    void FreeVerifyData(bool);
    void StartStringVerify();
    void ApplyStringVerifyResults();

    STRING_DATA *mStringData; // 0x4c
    unsigned int mNumStrings; // 0x50
    unsigned int mBatchSize; // 0x54
    unsigned int mBatchStart; // 0x58
    STRING_VERIFY_RESPONSE *mVerifyResults; // 0x5c
    unsigned int mVerifyResultsSize; // 0x60
    XOVERLAPPED *mOverlapped; // 0x64
    DataArray *mStringChecks; // 0x68
};
