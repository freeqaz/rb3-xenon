#include "os/PlatformMgr.h"
#include "game/PartyModeMgr.h"
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include "os/OnlineID.h"
#include "utl/DataPointMgr.h"
#include "utl/GlitchFinder.h"
#include "xdk/XAPILIB.h"
#include "xdk/XBC.h"
#include "xdk/XMP.h"
#include "xdk/XNET.h"
#include "xdk/NUI.h"
#include "xdk/xapilibi/winerror.h"
#include "xdk/xapilibi/xbox.h"

// Forward declarations for merged functions
extern void* merged_DataArrayNode(void*, int);
extern void* merged_82610090(const void*, unsigned int*);

struct XSTORAGE_ENUMERATE_RESULTS;
enum ServiceIdState {};

namespace {
    int mSigninSameGuest;
    int gNumSmartGlassClients;
    unsigned long gSmartGlassClientIDs[XBC_MAX_CLIENTS];
    int gNumSmartGlassSendsInProgress;
    void *mFriendsEnum;
    void *mFriendsBuffer;
    Hmx::Object *mFriendsCallback;
    void *mFriendsAsync;
    std::vector<Friend *> *mFriendsList;
    void *mListener;
    XOVERLAPPED *mServiceIDOverlapped;
    XOVERLAPPED *mServiceIDOverlapped2;
    XUID mXuidCache[4];
    XSTORAGE_ENUMERATE_RESULTS *mStorageList;
    unsigned long mPathLen;
    ServiceIdState mServiceIdState;
    unsigned long mListSize;
    unsigned long mUserID;
    unsigned long mResult;
}

PlatformMgr::PlatformMgr() {
    mSigninMask = 0;
    mScreenSaver = true;
    mSigninChangeMask = 0;
    mGuideShowing = false;
    mConfirmCancelSwapped = false;
    mConnected = false;
    mRegion = kRegionNone;
    mDiskError = kNoDiskError;
    unk69 = false;

    mSigninSameGuest = 0;
    mFriendsEnum = 0;
    mFriendsBuffer = 0;
    mFriendsCallback = 0;
    mFriendsAsync = 0;
    mFriendsList = 0;
    mListener = 0;

    mJobMgr = new JobMgr(this);

    mServiceIDOverlapped = 0;
    mXuidCache[0] = 0;
    mServiceIDOverlapped2 = 0;
    mStorageList = 0;
    mPathLen = 0x200;
    mXuidCache[1] = 0;
    mXuidCache[2] = 0;
    mXuidCache[3] = 0;
    mServiceIdState = (ServiceIdState)0;
    mListSize = 0;
    mUserID = -1;
    mResult = 0;
    mOverlapped.hEvent = 0;
}

bool PlatformMgr::IsEthernetCableConnected() { return XNetGetEthernetLinkStatus() != 0; }

void PlatformMgr::UpdateSigninState() {
    XUID oldCache[4] = { mXuidCache[0], mXuidCache[1], mXuidCache[2], mXuidCache[3] };
    int i;
    mSigninMask = 0;
    mSigninSameGuest = 0;
    for (i = 0; i < 4; i++) {
        if (XUserGetSigninState(i) != 0) {
            XUSER_SIGNIN_INFO info = {};
            mSigninMask |= (1 << i);
            XUserGetSigninInfo(i, 2, &info);
            XUserGetXUID(i, &info.xuid);
            mXuidCache[i] = info.xuid;
        } else {
            mXuidCache[i] = 0;
        }
        if (oldCache[i] != mXuidCache[i]) {
            mSigninChangeMask |= (1 << i);
            if (((oldCache[i] ^ mXuidCache[i]) & 0xff3fffffffffffff) == 0) {
                mSigninSameGuest |= (1 << i);
            }
        }
    }
}

bool PlatformMgr::HasCreatedContentPrivilege() const {
    bool allUsersRestricted = true;
    for (int userIndex = 0; userIndex < 4; ++userIndex) {
        int privilegeResult = 0;
        bool createdContentOK = XUserCheckPrivilege(userIndex, XPRIVILEGE_USER_CREATED_CONTENT, &privilegeResult) == 0
            && privilegeResult == 0;
        bool friendsOnlyContentOK =
            XUserCheckPrivilege(userIndex, XPRIVILEGE_USER_CREATED_CONTENT_FRIENDS_ONLY, &privilegeResult) == 0
            && privilegeResult == 0;
        bool userIsRestricted = !(createdContentOK && friendsOnlyContentOK);
        allUsersRestricted = allUsersRestricted & userIsRestricted;
    }

    return allUsersRestricted;
}

bool PlatformMgr::HasKinectSharePrvilege() const {
    int bptr = 0;
    return XUserCheckPrivilege(0xFF, XPRIVILEGE_SHARE_CONTENT_OUTSIDE_LIVE, &bptr) == 0 && bptr != 0;
}

bool PlatformMgr::IsSmartGlassConnected() { return gNumSmartGlassClients > 0; }

void PlatformMgr::SetPadContext(int padNum, int i2, int i3) const {
    if (padNum != -1 && ThePlatformMgr.IsSignedIn(padNum)) {
        XUserSetContext(padNum, i2, i3);
    }
}

void PlatformMgr::SetPadPresence(int padNum, int i2) const {
    if (padNum != -1 && ThePlatformMgr.IsSignedIn(padNum)) {
        XUserSetContext(padNum, 0x8001, i2);
    }
}

void PlatformMgr::ShowFriendsUI(int padNum) {
    unsigned long ul;

    if (IsSignedIn(padNum)) {
        if (sXShowCallback(ul)) {
            XShowNuiFriendsUI(ul, padNum);
        } else {
            XShowFriendsUI(padNum);
        }
    }
}

void PlatformMgr::SetBackgroundDownloadPriority(bool highPriority) {
    XBackgroundDownloadSetMode(highPriority ?
        XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW :
        XBACKGROUND_DOWNLOAD_MODE_AUTO);
}

// int __cdecl ShowControllerRequiredUIThreaded(void)

bool PlatformMgr::ShowPartyUI(int padNum) {
    unsigned long ul;
    unsigned long ret = 1;

    if (IsSignedIn(padNum)) {
        if (sXShowCallback(ul)) {
            ret = XShowNuiPartyUI(ul, padNum);
        } else {
            ret = XShowPartyUI(padNum);
        }
    }

    return ret == 0;
}

bool PlatformMgr::ShowFitnessBodyProfileUI(int padNum) {
    unsigned long ul;
    unsigned long ret = 1;

    if (IsSignedIn(padNum)) {
        if (sXShowCallback(ul)) {
            ret = XShowNuiFitnessBodyProfileUI(ul, padNum);
        } else {
            ret = XShowFitnessBodyProfileUI(padNum);
        }
    }

    return ret == 0;
}

void PlatformMgr::PreInit() { XMPOverrideBackgroundMusic(); }
void PlatformMgr::EnableXMP() { XMPRestoreBackgroundMusic(); }
void PlatformMgr::DisableXMP() { XMPOverrideBackgroundMusic(); }
void PlatformMgr::CheckMailbox() {}
void PlatformMgr::RunNetStartUtility() {}

void PlatformMgr::SetScreenSaver(bool b1) {
    mScreenSaver = b1;
    XEnableScreenSaver(b1);
}

bool PlatformMgr::IsSignedIntoLive(int padNum) const {
    MILO_ASSERT(padNum >= 0, 0x671);

    if (!IsSignedIn(padNum)) {
        return false;
    } else {
        return (XUserGetSigninState(padNum) == eXUserSigninState_SignedInToLive);
    }
}

bool PlatformMgr::IsPadAGuest(int padNum) const {
    XUSER_SIGNIN_INFO signinInfo;

    DWORD ret = XUserGetSigninInfo(padNum, 0, &signinInfo);

    if (ret == ERROR_NO_SUCH_USER) {
        return IsSignedIn(padNum);
    } else {
        MILO_ASSERT(ret == ERROR_SUCCESS, 0x929);

        return signinInfo.dwInfoFlags >> 1 & 1;
    }
}

void PlatformMgr::ShowOfferUI(int padNum) {
    unsigned long ul;
    unsigned long ret;

    if (IsSignedIn(padNum)) {
        if (sXShowCallback(ul)) {
            ret = XShowNuiMarketplaceUI(
                ul, padNum, XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST_BACKGROUND, 0, -1
            );
        } else {
            ret = XShowMarketplaceUI(
                padNum, XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST_BACKGROUND, 0, -1
            );
        }

        if (ret != ERROR_SUCCESS) {
            MILO_NOTIFY("XShowMarketplaceUI failed (0x%x)", ret);
        }
    }
}

DWORD PlatformMgr::ShowDeviceSelectorUI(
    DWORD userIndex,
    DWORD contentType,
    DWORD contentFlags,
    ULARGE_INTEGER bytesRequested,
    DWORD *deviceID,
    XOVERLAPPED *overlapped
) {
    unsigned long ul;
    unsigned long ret;

    if (sXShowCallback(ul)) {
        ret = XShowNuiDeviceSelectorUI(
            ul, userIndex, contentType, contentFlags, bytesRequested, deviceID, overlapped
        );
    } else {
        ret = XShowDeviceSelectorUI(
            userIndex, contentType, contentFlags, bytesRequested, deviceID, overlapped
        );
    }

    return ret;
}

void PlatformMgr::RegionInit() {
    if (XGetGameRegion() != 0xFF) {
        SetRegion(kRegionEurope);
    } else {
        SetRegion(kRegionNA);
    }
}

namespace {
    void DtaToJsonHelper(HJSONWRITER__ *writer, const DataArray *arr);
    HJSONWRITER__ *DtaToJson(const DataArray *arr);
    void XbcSendMsg(unsigned long clientID, const DataArray *arr);
    void SmartGlassPoll();
    DataArrayPtr JsonToDta(HJSONREADER__ *reader, bool topLevel);
    void XbcRecieveMsg(unsigned long clientID, HJSONREADER__ *reader);
    void XbcCallback(long error, _XBC_EVENT_PARAMS *params, void *state);
    void SmartGlassInit();

    void DtaToJsonHelper(HJSONWRITER__ *writer, const DataArray *arr) {
        short count = arr->Size();
        if (count != 0 && count > 0) {
            for (int i = 0; i < count; i++) {
                DataNode& nodeRef = arr->Node(i);
                DataNode* node = &nodeRef;
                unsigned int type = (unsigned int)node->Type();

                if (type >= 1) {
                    switch (type) {
                        case 18: {
                            const char* str = node->Str();
                            const char* start = str;
                            while (*str != 0) {
                                str++;
                            }
                            int len = str - start - 1;
                            const char* str2 = node->Str();
                            XJSONWriteStringValue(writer, str2, len);
                            break;
                        }
                        case 16: {
                            XJSONBeginArray(writer);
                            DataArray* subArr = node->Array();
                            DtaToJsonHelper(writer, subArr);
                            XJSONEndArray(writer);
                            break;
                        }
                        case 5: {
                            Symbol sym = node->Sym();
                            const char* symStart = sym.Str();
                            const char* symStr = symStart;
                            while (*symStr != 0) {
                                symStr++;
                            }
                            int len = symStr - symStart - 1;
                            Symbol sym2 = node->Sym();
                            XJSONWriteStringValue(writer, sym2.Str(), len);
                            break;
                        }
                        case 1: {
                            double val = node->Float();
                            XJSONWriteNumberValue(writer, val);
                            break;
                        }
                        default: {
                            unsigned int t = type;
                            const char* msg = "DtaToJson can't handle type %d r";
                            const char* formatted = (const char*)merged_82610090(&msg, &t);
                            TheDebug.Notify(formatted);
                            XJSONWriteNullValue(writer);
                            break;
                        }
                    }
                } else {
                    int intVal = node->Int();
                    double dblVal = (double)(long long)intVal;
                    XJSONWriteNumberValue(writer, dblVal);
                }
            }
        }
    }

    HJSONWRITER__ *DtaToJson(const DataArray *arr) {
        HJSONWRITER__ *writer = XJSONCreateWriter();
        XJSONBeginArray(writer);
        DtaToJsonHelper(writer, arr);
        XJSONEndArray(writer);
        return writer;
    }

    void XbcSendMsg(unsigned long clientID, const DataArray *arr) {
        HJSONWRITER__ *writer = DtaToJson(arr);
        if (clientID == 0) {
            for (int i = 0; i < XBC_MAX_CLIENTS; i++) {
                if (gSmartGlassClientIDs[i] != 0) {
                    XbcSendJSON(XBC_DELIVERY_DEFAULT, gSmartGlassClientIDs[i], writer, 0);
                    gNumSmartGlassSendsInProgress++;
                }
            }
        } else {
            XbcSendJSON(XBC_DELIVERY_DEFAULT, clientID, writer, 0);
            gNumSmartGlassSendsInProgress++;
        }
        XJSONCloseWriter(writer);
    }

    void SmartGlassPoll() {
        long result = XbcDoWork();
        if (result != 0) {
            MILO_NOTIFY("SmartGlass: error: %d\n", result);
        }
    }

    DataArrayPtr JsonToDta(HJSONREADER__ *reader, bool topLevel) {
        DataArrayPtr container;
        DataArray *fieldName = 0;
        _JSONTokenType tokenType;
        unsigned long param1, param2;

        while (XJSONReadToken(reader, &tokenType, &param1, &param2) == 0) {
            DataNode node(0);
            char charBuf[256];
            charBuf[0] = '\0';

            if ((int)tokenType >= 5 && ((int)tokenType <= 6 || tokenType == 10)) {
                wchar_t wcharBuf[128];
                XJSONGetTokenValue(reader, wcharBuf, 0x80);
                wcstombs(charBuf, wcharBuf, 0x100);
            }

            switch (tokenType) {
            case kJSONTokenBeginArray:
                node = DataNode(JsonToDta(reader, false));
                break;
            case kJSONTokenEndArray:
            case kJSONTokenEndMap:
                return container;
            case kJSONTokenBeginMap:
                node = DataNode(JsonToDta(reader, false));
                break;
            case kJSONTokenString:
                node = DataNode(charBuf);
                break;
            case kJSONTokenNumber:
                if (strchr(charBuf, '.')) {
                    node = DataNode((float)atof(charBuf));
                } else {
                    node = DataNode(atoi(charBuf));
                }
                break;
            case kJSONTokenTrue:
                node = DataNode(1);
                break;
            case kJSONTokenFalse:
                node = DataNode(0);
                break;
            case kJSONTokenNull:
                node = DataNode(0);
                break;
            case kJSONTokenFieldName:
                fieldName = new DataArray(2);
                fieldName->Node(0) = DataNode(Symbol(charBuf));
                continue;
            case kJSONTokenEnd:
            case kJSONTokenComment:
            case kJSONTokenError:
            default:
                continue;
            }

            if (fieldName) {
                fieldName->Node(1) = node;
                node = DataNode(fieldName, kDataArray);
                fieldName = 0;
            }

            if (topLevel && node.Type() == kDataArray) {
                container = node.Array();
                topLevel = false;
            } else {
                ((DataArray *)container)->Insert(((DataArray *)container)->Size(), node);
            }
        }

        return container;
    }

    void XbcRecieveMsg(unsigned long clientID, HJSONREADER__ *reader) {
        DataArrayPtr dta = JsonToDta(reader, true);
        SmartGlassMsg msg(clientID, (DataArray *)dta);
        ThePlatformMgr.Handle(msg.Data(), true);
    }

    void XbcCallback(long error, _XBC_EVENT_PARAMS *params, void *state) {
        if (error != 0) {
            MILO_NOTIFY("SmartGlass: Error in cb: 0x%08x", error);
            return;
        }
        unsigned int userIdx = params->userIndex;
        if (userIdx >= XBC_MAX_CLIENTS) {
            MILO_NOTIFY("SmartGlass: Error in cb: user index %d (event: %d)", userIdx, params->eventType);
            return;
        }
        switch (params->eventType) {
        case XBC_EVENT_CLIENT_CONNECTED:
            gNumSmartGlassClients++;
            gSmartGlassClientIDs[userIdx] = params->clientID;
            MILO_ASSERT(gNumSmartGlassClients <= XBC_MAX_CLIENTS, 0x20C);
            break;
        case XBC_EVENT_CLIENT_DISCONNECTED:
            gSmartGlassClientIDs[userIdx] = 0;
            gNumSmartGlassClients--;
            MILO_ASSERT(gNumSmartGlassClients >= 0, 0x214);
            break;
        case XBC_EVENT_SEND_COMPLETE:
            gNumSmartGlassSendsInProgress--;
            break;
        case XBC_EVENT_DATA_RECEIVED:
            XbcRecieveMsg(params->clientID, params->jsonReader);
            break;
        default:
            break;
        }
    }

    void SmartGlassInit() {
        *(unsigned long*)gSmartGlassClientIDs = 0;
        *(unsigned long*)(gSmartGlassClientIDs + 4) = 0;
        *(unsigned long*)(gSmartGlassClientIDs + 8) = 0;
        *(unsigned long*)(gSmartGlassClientIDs + 12) = 0;
        long result = XbcInitialize(XbcCallback, nullptr);
        if (result < 0) {
            MILO_FAIL("Failed to initialize Xbox SmartGlass library.\n");
        }
    }
}

DataNode PlatformMgr::OnSignInUsers(DataArray *msg) {
    unsigned long flags = 0;
    if (msg->Size() > 3) {
        if (msg->Int(3) != 0) {
            flags = 2;
        }
    }
    SignInUsers(msg->Int(2), flags);
    return DataNode(0);
}

void PlatformMgr::SmartGlassSend(unsigned long clientID, const DataArray *arr) {
    XbcSendMsg(clientID, arr);
}

#include "utl/JobMgr.h"
#include "meta/StorePanel.h"
#include "lazer/meta_ham/OptionsPanel.h"

void MultipleItemsEnumJob::Cancel(Hmx::Object *) {
    MILO_FAIL("MultipleItemsEnumJob::Cancel called");
}

PostPurchaseEnumJob::PostPurchaseEnumJob(Hmx::Object *obj, int userIndex, u64 itemID, Symbol offerSym, unsigned int purchaserID)
    : SingleItemEnumJob(obj, userIndex, itemID), mOfferSymbol(offerSym), mPurchaserID(purchaserID) {
}

PostPurchaseEnumJob::~PostPurchaseEnumJob() {}

void PostPurchaseEnumJob::OnCompletion(Hmx::Object *obj) {
    if ((mStatus == 2) && (mSuccess != 0)) {
        static Symbol sSourceSymbol("source");
        static Symbol sOfferSymbol("offer");
        static Symbol sPurchaserSymbol("purchaser");

        String dataStr(MakeString("%016llX", mItemID));
        SendDataPoint("store/purchase", sSourceSymbol, mOfferSymbol, sOfferSymbol, dataStr.c_str(), sPurchaserSymbol, mPurchaserID);
    }
    SingleItemEnumJob::OnCompletion(obj);
}

unsigned long long SingleItemEnumCompleteMsg::OfferID() const {
    return _strtoui64(mData->Str(4), 0, 16);
}

MultipleItemsEnumJob::MultipleItemsEnumJob(Hmx::Object *obj, int userIndex, std::vector<u64> &itemIDs)
    : Job(), mItemIDs(itemIDs), mPurchased() {
    mObject = obj;
    mUserIndex = userIndex;
    mStatus = 0;
    mSuccess = false;
    mEnumBuffer = 0;
    mEnumHandle = 0;
    memset(&mOverlapped, 0, sizeof(mOverlapped));
}

MultipleItemsEnumJob::~MultipleItemsEnumJob() {
    if (mStatus == 1 && mOverlapped.InternalLow == 0x3e5) {
        DWORD result = XCancelOverlapped(&mOverlapped);
        if (result != 0) {
            TheDebug.Fail(MakeString("Error cancelling enum %d", result), 0);
        }
    }
    if (mEnumHandle != 0) {
        CloseHandle(mEnumHandle);
        mEnumHandle = 0;
    }
    ::operator delete(mEnumBuffer);
    mEnumBuffer = 0;
}

void MultipleItemsEnumJob::Poll() {
    if (mStatus == 1 && mOverlapped.InternalLow != 0x3e5) {
        DWORD resultVal;
        void *result = (void *)XGetOverlappedResult(&mOverlapped, &resultVal, 0);
        if (result == 0) {
            mStatus = 2;
            u64 *enumEntry = (u64 *)mEnumBuffer;
            u64 *itemIt = &mItemIDs[0];
            unsigned int i = 0;
            auto purchasedIt = mPurchased.begin();
            unsigned int bitOffset = purchasedIt._M_offset;
            unsigned int *bitChunk = purchasedIt._M_p;
            if (mItemIDs.size() > 0) {
                do {
                    if (*enumEntry == *itemIt) {
                        unsigned int mask = 1 << bitOffset;
                        int purchased = *(int *)(enumEntry + 9);
                        if (purchased != 0) {
                            *bitChunk |= mask;
                        } else {
                            *bitChunk &= ~mask;
                        }
                        bool success = mSuccess || ((*bitChunk & mask) != 0);
                        enumEntry += 0xd;
                        mSuccess = success;
                    } else {
                        TheDebug.Notify(MakeString("Could not enumerate offerId %016llX", *itemIt));
                        *bitChunk &= ~(1 << bitOffset);
                    }
                    if (bitOffset++ == 31) {
                        bitOffset = 0;
                        bitChunk++;
                    }
                    i++;
                    itemIt++;
                } while (i < (unsigned int)mItemIDs.size());
            }
        } else {
            mStatus = 3;
            TheDebug.Notify(MakeString("Error enumerating after purchase: %d", result));
        }
        if (mEnumHandle != 0) {
            CloseHandle(mEnumHandle);
            mEnumHandle = 0;
        }
        ::operator delete(mEnumBuffer);
        mEnumBuffer = 0;
    }
}

bool MultipleItemsEnumJob::IsFinished() {
    if (mStatus == 1) {
        Poll();
    }
    return mStatus != 1;
}

void MultipleItemsEnumJob::Start() {
    mStatus = 1;
    int count = (int)mItemIDs.size();
    mPurchased.resize(count, false);
    fill(mPurchased.begin(), mPurchased.end(), false);

    DWORD bufSize = 0;
    DWORD result = XMarketplaceCreateOfferEnumeratorByOffering(
        mUserIndex, (int)mItemIDs.size(), &mItemIDs[0], (WORD)mItemIDs.size(), &bufSize, &mEnumHandle
    );
    if (result != 0) {
        if (mEnumHandle != 0) {
            CloseHandle(mEnumHandle);
            mEnumHandle = 0;
        }
        TheDebug.Notify(MakeString("Error creating enumerator after purchase: %d", result));
        mStatus = 3;
        return;
    }
    mEnumBuffer = new char[bufSize];
    memset(mEnumBuffer, 0, bufSize);
    memset(&mOverlapped, 0, sizeof(mOverlapped));
    result = XEnumerate(mEnumHandle, mEnumBuffer, bufSize, 0, &mOverlapped);
    if (result == 0x3e5) {
        return;
    }
    if (mEnumHandle != 0) {
        CloseHandle(mEnumHandle);
        mEnumHandle = 0;
    }
    ::operator delete(mEnumBuffer);
    mEnumBuffer = 0;
    TheDebug.Notify(MakeString("Error enumerating after purchase: %d", result));
    mStatus = 3;
}

void MultipleItemsEnumJob::OnCompletion(Hmx::Object *obj) {
    if (mObject != 0) {
        static MultipleItemsEnumCompleteMsg msg(false, false, (int)mItemIDs.size(), String(gNullStr));

        msg.SetSuccess(mStatus == 2);
        msg.SetPurchaseMade(mSuccess);
        int count = (int)mItemIDs.size();
        msg.SetNumOfferIDs(count);
        for (int i = 0; i < count; i++) {
            String offerStr(MakeString("%016llX", mItemIDs[i]));
            msg.SetOfferID(i, offerStr);
            msg.SetPurchased(i, mPurchased[i]);
        }
        mObject->Handle(msg.Data(), true);
    }
}

MultipleItemsPostPurchaseEnumJob::MultipleItemsPostPurchaseEnumJob(
    Hmx::Object *obj, int userIndex, std::vector<u64> &itemIDs, Symbol offerSym, unsigned int purchaserID)
    : MultipleItemsEnumJob(obj, userIndex, itemIDs), mOfferSymbol(offerSym), mPurchaserID(purchaserID) {
}

MultipleItemsPostPurchaseEnumJob::~MultipleItemsPostPurchaseEnumJob() {}

void MultipleItemsPostPurchaseEnumJob::OnCompletion(Hmx::Object *obj) {
    if (mStatus == 2 && mSuccess != 0) {
        static Symbol sSourceSymbol("source");
        static Symbol sOfferSymbol("offer");
        static Symbol sPurchaserSymbol("purchaser");

        for (unsigned int i = 0; i < mItemIDs.size(); i++) {
            String itemStr(MakeString("%016llX", mItemIDs[i]));
            SendDataPoint("store/purchase", sSourceSymbol, mOfferSymbol, sOfferSymbol, itemStr.c_str(), sPurchaserSymbol, mPurchaserID);
        }
    }
    MultipleItemsEnumJob::OnCompletion(obj);
}

void MultipleItemsEnumCompleteMsg::SetNumOfferIDs(int count) {
    mData->Node(4) = count;
    mData->Node(5).Array(mData)->Resize(count);
    mData->Node(6).Array(mData)->Resize(count);
}

void MultipleItemsEnumCompleteMsg::SetOfferID(int index, const String &s) {
    DataNode dn(s);
    mData->Node(5).Array(mData)->Node(index) = dn;
}

unsigned long long MultipleItemsEnumCompleteMsg::OfferID(int index) const {
    DataArray *arr = mData->Node(5).Array(mData);
    const char *str = arr->Node(index).Str(arr);
    return _strtoui64(str, nullptr, 0x10);
}

void MultipleItemsEnumCompleteMsg::SetPurchased(int index, bool b) {
    mData->Node(6).Array(mData)->Node(index) = b;
}
