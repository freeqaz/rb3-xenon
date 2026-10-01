#include "net_band/XboxEntityUploader.h"
#include "meta_band/BandProfile.h"
#include "meta_band/SavedSetlist.h"
#include "net_band/RockCentralMsgs.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "tour/TourBand.h"
#include "tour/TourCharLocal.h"
#include "tour/TourSavable.h"
#include "utl/UTF8.h"
#include <cstring>
#include <cwchar>

// Retail .data 0x82C716EC: a pointer to "en-us" (.rdata 0x82085048), passed as
// XStringVerify's locale.
const char *gXboxStringVerifyLocale = "en-us";

// Retail .data 0x82C716F0 holds &the uploader (0x82CC97A4). The object's
// dynamic initializer (0x82C3F610) runs EntityUploader's ctor and then stores
// the XboxEntityUploader vtable and nothing else, so the class has no
// constructor of its own.
XboxEntityUploader gXboxEntityUploader;
EntityUploader &TheEntityUploader = gXboxEntityUploader;

// 0x82508708: clears the verify state, then tail-calls EntityUploader::Init.
void XboxEntityUploader::Init() {
    mStringData = 0;
    mVerifyResults = 0;
    mOverlapped = 0;
    mNumStrings = 0;
    mBatchSize = 0;
    mBatchStart = 0;
    EntityUploader::Init();
}

// 0x82508728. `this` is unused. A verified char/band name (op types 0-2, and
// a band op's band-name string) clears the savable's dirty flag; a rejected
// one sets it. Setlist ops are left alone.
void XboxEntityUploader::NotifySavable(
    TourSavable *savable, int opType, int stringKind, bool ok
) {
    bool dirty = !ok;
    if (opType == 2 || opType == 0 || opType == 1) {
        savable->SetDirty(dirty, 4);
    } else if (opType == 3 && stringKind == kStringBandName) {
        savable->SetDirty(dirty, 4);
    }
}

// 0x82508790. Frees the current batch; with `all`, also the response buffer
// and the batch cursor.
void XboxEntityUploader::FreeVerifyData(bool all) {
    if (mOverlapped) {
        if (XCancelOverlapped(mOverlapped) == 0) {
            delete mOverlapped;
            mOverlapped = 0;
        }
    }
    if (mStringData) {
        for (unsigned int i = 0; i < mBatchSize; i++) {
            delete[] mStringData[i].pszString;
        }
        delete[] mStringData;
        mStringData = 0;
    }
    mBatchSize = 0;
    if (all) {
        if (mVerifyResults) {
            delete[] (char *)mVerifyResults;
            mVerifyResults = 0;
        }
        mNumStrings = 0;
        mBatchStart = 0;
    }
}

// 0x82508850. Converts the next (up to 10) strings to UTF-16 and starts an
// overlapped XStringVerify on them.
void XboxEntityUploader::StartStringVerify() {
    mNumStrings = mStringChecks->Size();
    unsigned int batch = mNumStrings - mBatchStart;
    if (batch > 10) {
        batch = 10;
    }
    mBatchSize = batch;
    mStringData = new STRING_DATA[batch];
    for (unsigned int i = mBatchStart; i < mBatchStart + mBatchSize; i++) {
        WCHAR *str = new WCHAR[strlen(mStringChecks->Array(i)->Str(2)) + 1];
        UTF8toUTF16((unsigned short *)str, mStringChecks->Array(i)->Str(2));
        mStringData[i - mBatchStart].pszString = str;
        mStringData[i - mBatchStart].wStringSize =
            wcslen(mStringData[i - mBatchStart].pszString) + 1;
    }
    mVerifyResultsSize = mBatchSize * 4 + 6;
    mVerifyResults = (STRING_VERIFY_RESPONSE *)new char[mVerifyResultsSize];
    memset(mVerifyResults, 0, mVerifyResultsSize);
    mOverlapped = new XOVERLAPPED;
    memset(mOverlapped, 0, sizeof(XOVERLAPPED));
    DWORD res = XStringVerify(
        0,
        gXboxStringVerifyLocale,
        mBatchSize,
        mStringData,
        mVerifyResultsSize,
        mVerifyResults,
        mOverlapped
    );
    if (res == ERROR_SUCCESS || res == ERROR_IO_PENDING) {
        mState = kPlatformChecking;
        RecordSubmissionTime();
    } else {
        mState = 4;
        FreeVerifyData(true);
    }
}

// 0x82508A58. Copies each string's verdict onto its op; a rejected string
// fails the op the way a RockCentral rejection would.
void XboxEntityUploader::ApplyStringVerifyResults() {
    for (int i = 0; i < mStringChecks->Size(); i++) {
        int opID = mStringChecks->Array(i)->Int(0);
        int stringKind = mStringChecks->Array(i)->Int(1);
        int result = mStringChecks->Array(i)->Int(3);
        for (int j = 0; j < mNumUploadOps; j++) {
            if (opID == mUploadOps[j]->mOpID) {
                bool ok = true;
                int opType = mUploadOps[j]->mOpType;
                if (result != 0) {
                    ok = false;
                    if (mUploadOps[j]->mRetCode == 0) {
                        mUploadOps[j]->mRetCode = result;
                        if (result != 1 && opType != 2) {
                            if (opType == 3) {
                                TourBand *band = (TourBand *)mUploadOps[j]->mSavableObject;
                                band->ProcessRetCode(mUploadOps[j]->mRetCode);
                                band->UploadComplete();
                            } else if (opType == 4) {
                                LocalSavedSetlist *setlist =
                                    (LocalSavedSetlist *)mUploadOps[j]->mSavableObject;
                                setlist->ProcessRetCode(mUploadOps[j]->mRetCode);
                                setlist->UploadComplete();
                            }
                        }
                    }
                }
                NotifySavable(mUploadOps[j]->mSavableObject, opType, stringKind, ok);
                break;
            }
        }
    }
}

// 0x82508C10
void XboxEntityUploader::Terminate() { FreeVerifyData(true); }

// 0x82508C20. Drives the verify batches; once every string has a verdict the
// ops go to RockCentral (state 2 -> 3) and EntityUploader::Poll takes over.
void XboxEntityUploader::Poll() {
    if (mState == kPlatformChecking) {
        DWORD res = XGetOverlappedResult(mOverlapped, 0, 0);
        if (res == ERROR_IO_INCOMPLETE) {
            if (HasServerTimedOut()) {
                mState = 4;
                FreeVerifyData(true);
            }
        } else if (res == ERROR_SUCCESS) {
            int j = 0;
            for (unsigned int i = mBatchStart; i < mBatchStart + mBatchSize; i++, j++) {
                int code = 0;
                if (mVerifyResults->pStringResult[j] != 0) {
                    int kind = mStringChecks->Array(i)->Int(1);
                    switch (kind) {
                    case kStringCharName:
                        code = 4;
                        break;
                    case kStringBandName:
                        code = 2;
                        break;
                    case kStringSetlistTitle:
                        code = 0xf;
                        break;
                    case kStringSetlistDesc:
                        code = 0x10;
                        break;
                    }
                }
                mStringChecks->Array(i)->Node(3) = code;
            }
            mBatchStart += mBatchSize;
            if (mBatchStart < mNumStrings) {
                FreeVerifyData(false);
                RecordSubmissionTime();
                StartStringVerify();
            } else {
                ApplyStringVerifyResults();
                mState = 2;
                FreeVerifyData(true);
                mStringChecks->Release();
            }
        } else {
            mState = 4;
            FreeVerifyData(true);
            mStringChecks->Release();
        }
    }
    if (mState == 2) {
        mState = 3;
        if (BeginRockCentralOps(6) == 0) {
            mState = 5;
        }
        RecordSubmissionTime();
    }
    EntityUploader::Poll();
}

// 0x82508E58
bool XboxEntityUploader::VerifyBandName(
    const char *bandName, TourSavable *tour, Hmx::Object *pObjCallback
) {
    if (mState != 0) {
        if (pObjCallback) {
            RockCentralOpCompleteMsg msg(false, 1, DataNode(mEmptyArray, kDataArray));
            pObjCallback->Handle(msg);
        }
        return false;
    } else {
        mCallbackObj = pObjCallback;
        int opID = BuildStringCheckOp(mUploadOps, bandName, 1, tour);
        mStringChecks = new DataArray(1);
        DataArray *check = new DataArray(4);
        check->Node(0) = opID;
        check->Node(1) = kStringBandName;
        check->Node(2) = bandName;
        check->Node(3) = 0;
        {
            DataNode n(check, kDataArray);
            mStringChecks->Node(0) = n;
        }
        check->Release();
        mCallType = 1;
        StartStringVerify();
        return true;
    }
}

// 0x825091C8. Same as VerifyBandName but op type 0 / char-name kind; retail
// sets call type 1 here too.
bool XboxEntityUploader::VerifyCharName(
    const char *charName, TourSavable *tour, Hmx::Object *pObjCallback
) {
    if (mState != 0) {
        if (pObjCallback) {
            RockCentralOpCompleteMsg msg(false, 1, DataNode(mEmptyArray, kDataArray));
            pObjCallback->Handle(msg);
        }
        return false;
    } else {
        mCallbackObj = pObjCallback;
        int opID = BuildStringCheckOp(mUploadOps, charName, 0, tour);
        mStringChecks = new DataArray(1);
        DataArray *check = new DataArray(4);
        check->Node(0) = opID;
        check->Node(1) = kStringCharName;
        check->Node(2) = charName;
        check->Node(3) = 0;
        {
            DataNode n(check, kDataArray);
            mStringChecks->Node(0) = n;
        }
        check->Release();
        mCallType = 1;
        StartStringVerify();
        return true;
    }
}

// 0x82509530
bool XboxEntityUploader::UpdateChar(TourCharLocal *c, Hmx::Object *pObjCallback) {
    if (mState != 0) {
        if (pObjCallback) {
            RockCentralOpCompleteMsg msg(false, 1, DataNode(mEmptyArray, kDataArray));
            pObjCallback->Handle(msg);
        }
        return false;
    } else {
        mCallbackObj = pObjCallback;
        int opID = BuildUpdateCharOp(mUploadOps, c);
        if (opID == 0) {
            RockCentralOpCompleteMsg msg(true, 0, DataNode(mEmptyArray, kDataArray));
            mCallbackObj->Handle(msg);
        } else {
            mStringChecks = new DataArray(1);
            DataArray *check = new DataArray(4);
            check->Node(0) = opID;
            check->Node(1) = kStringCharName;
            check->Node(2) = c->GetCharacterName();
            check->Node(3) = 0;
            {
                DataNode n(check, kDataArray);
                mStringChecks->Node(0) = n;
            }
            check->Release();
            mCallType = 3;
            StartStringVerify();
        }
        return true;
    }
}

// 0x82509938. One check per char and band op; a setlist op contributes its
// title and its description.
bool XboxEntityUploader::UpdateFromProfile(BandProfile *pProfile, Hmx::Object *pObjCallback) {
    if (mState != 0) {
        if (pObjCallback) {
            RockCentralOpCompleteMsg msg(false, 1, DataNode(mEmptyArray, kDataArray));
            pObjCallback->Handle(msg);
        }
        return false;
    } else {
        mCallbackObj = pObjCallback;
        int numOps = BuildProfileUploadOps(mUploadOps, pProfile);
        if (numOps == 0) {
            RockCentralOpCompleteMsg msg(true, 0, DataNode(mEmptyArray, kDataArray));
            mCallbackObj->Handle(msg);
        } else {
            mStringChecks = new DataArray(numOps);
            int numChecks = 0;
            for (int i = 0; i < numOps; i++) {
                EntityData *op = mUploadOps[i];
                int opType = op->mOpType;
                if (opType == 2) {
                    TourCharLocal *c = (TourCharLocal *)op->mSavableObject;
                    DataArray *check = new DataArray(4);
                    check->Node(0) = mUploadOps[i]->mOpID;
                    check->Node(1) = kStringCharName;
                    check->Node(2) = c->GetCharacterName();
                    check->Node(3) = 0;
                    {
                        DataNode n(check, kDataArray);
                        mStringChecks->Node(numChecks++) = n;
                    }
                    check->Release();
                } else if (opType == 3) {
                    TourBand *band = (TourBand *)op->mSavableObject;
                    DataArray *check = new DataArray(4);
                    check->Node(0) = mUploadOps[i]->mOpID;
                    check->Node(1) = kStringBandName;
                    check->Node(2) = band->GetName();
                    check->Node(3) = 0;
                    {
                        DataNode n(check, kDataArray);
                        mStringChecks->Node(numChecks++) = n;
                    }
                    check->Release();
                } else if (opType == 4) {
                    LocalSavedSetlist *setlist = (LocalSavedSetlist *)op->mSavableObject;
                    DataArray *check = new DataArray(4);
                    check->Node(0) = mUploadOps[i]->mOpID;
                    check->Node(1) = kStringSetlistTitle;
                    check->Node(2) = setlist->GetTitle();
                    check->Node(3) = 0;
                    {
                        DataNode n(check, kDataArray);
                        mStringChecks->Node(numChecks++) = n;
                    }
                    check->Release();
                    mStringChecks->Resize(mStringChecks->Size() + 1);
                    check = new DataArray(4);
                    check->Node(0) = mUploadOps[i]->mOpID;
                    check->Node(1) = kStringSetlistDesc;
                    check->Node(2) = setlist->GetDescription();
                    check->Node(3) = 0;
                    {
                        DataNode n(check, kDataArray);
                        mStringChecks->Node(numChecks++) = n;
                    }
                    check->Release();
                }
            }
            mStringChecks->Resize(numChecks);
            mCallType = 6;
            StartStringVerify();
        }
        return true;
    }
}

// 0x8250A440
BEGIN_HANDLERS(XboxEntityUploader)
    HANDLE_SUPERCLASS(EntityUploader)
    HANDLE_CHECK(0)
END_HANDLERS
