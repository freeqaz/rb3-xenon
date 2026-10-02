// XSessionSearcher, the Xbox SessionSearcher (retail RTTI .?AVXSessionSearcher@@,
// vtables 0x82057EC4, and 0x82057E6C for the Hmx::Object vbase at 0xA4).
// Retail .text 0x823EDE08-0x823EEAB8, between the XboxServer code and the
// XboxSession / XSessionData code. sizeof 0xCC (SessionSearcher::New).
//
// Searching is XSessionSearchEx into a results buffer, then one QoS probe of
// every result's host; a host that answers with its session data becomes a
// NetSearchResult. An accepted invite probes the inviting host the same way.
#include "net/SessionSearcher.h"
#include "meta_band/BandNetGameData.h"
#include "net/Net.h"
#include "net/NetSearchResult.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "utl/MemStream.h"
#include "xdk/XAPILIB.h"
#include "xdk/xnet/xnetapi.h"
#include "xdk/xonline/xonline.h"

class XSessionSearcher : public SessionSearcher {
public:
    XSessionSearcher();
    virtual ~XSessionSearcher();
    virtual void Poll();
    virtual void StartSearching(User *, const SearchSettings &);
    virtual void StopSearching();
    virtual void ClearSearchResults();
    virtual bool OnMsg(const InviteAcceptedMsg &);

    XSESSION_SEARCHRESULT_HEADER *mResults; // 0x34
    XOVERLAPPED *mOverlapped; // 0x38
    XNQOS *mQos; // 0x3c, the probe of the search results
    XNQOS *mInviteQos; // 0x40, the probe of an accepted invite's host
    XINVITE_INFO mInviteInfo; // 0x44
    int mInvitePad; // 0x98
    int mSearchLimit; // 0x9c, net/searcher/search_limit
};

// Retail .bss 0x82CBFF30 / 0x82CBFF20 / 0x82CBFDD0. External, not file
// statics: StartSearching addresses each through its own relocation, where
// MSVC would reach internal statics off one shared base. Names descriptive.
DWORD gSearchResultsSize;
XUSER_CONTEXT gSearchContexts[2];
XUSER_PROPERTY gSearchProperties[14];

// 0x823EDE08
XSessionSearcher::XSessionSearcher()
    : mResults(0), mOverlapped(0), mQos(0), mInviteQos(0) {
    DataArray *cfg = SystemConfig("net", "searcher");
    cfg->FindData("search_limit", mSearchLimit, true);
}

// 0x823EDF78
XSessionSearcher::~XSessionSearcher() {
    delete mResults;
    mResults = 0;
    if (mQos)
        XNetQosRelease(mQos);
    if (mInviteQos)
        XNetQosRelease(mInviteQos);
    if (mOverlapped) {
        XCancelOverlapped(mOverlapped);
        delete mOverlapped;
    }
}

// 0x823EE690
void XSessionSearcher::Poll() {
    SessionSearcher::Poll();
    if (mSearching) {
        if (!mQos && mOverlapped->InternalLow != ERROR_IO_PENDING) {
            delete mOverlapped;
            mOverlapped = 0;
            if (mResults->dwSearchResults > 0) {
                const XNADDR *addrs[20];
                const XNKID *ids[20];
                const XNKEY *keys[20];
                for (unsigned int i = 0; i < mResults->dwSearchResults; i++) {
                    addrs[i] = &mResults->pResults[i].info.hostAddress;
                    ids[i] = &mResults->pResults[i].info.sessionID;
                    keys[i] = &mResults->pResults[i].info.keyExchangeKey;
                }
                XNetQosLookup(
                    mResults->dwSearchResults, addrs, ids, keys, 0, 0, 0, 8, 0x8000, 0, 0, &mQos
                );
            } else {
                StopSearching();
            }
        }
        if (mQos && mQos->cxnqosPending == 0) {
            for (unsigned int i = 0; i < mResults->dwSearchResults; i++) {
                if ((mQos->axnqosinfo[i].bFlags & XNET_XNQOSINFO_TARGET_CONTACTED)
                    && !(mQos->axnqosinfo[i].bFlags & XNET_XNQOSINFO_TARGET_DISABLED)
                    && mQos->axnqosinfo[i].pbData) {
                    NetSearchResult *result = NetSearchResult::New();
                    MemStream stream(false);
                    stream.Resize(mQos->axnqosinfo[i].cbData);
                    stream.Write(mQos->axnqosinfo[i].pbData, mQos->axnqosinfo[i].cbData);
                    stream.Seek(0, BinStream::kSeekBegin);
                    result->Load(stream);
                    UpdateSearchList(result);
                }
            }
            StopSearching();
        }
    }
    if (mInviteQos && mInviteQos->cxnqosPending == 0) {
        if ((mInviteQos->axnqosinfo[0].bFlags & XNET_XNQOSINFO_TARGET_CONTACTED)
            && !(mInviteQos->axnqosinfo[0].bFlags & XNET_XNQOSINFO_TARGET_DISABLED)
            && mInviteQos->axnqosinfo[0].pbData) {
            MemStream stream(false);
            stream.Resize(mInviteQos->axnqosinfo[0].cbData);
            stream.Write(mInviteQos->axnqosinfo[0].pbData, mInviteQos->axnqosinfo[0].cbData);
            stream.Seek(0, BinStream::kSeekBegin);
            mLastInviteResult->Load(stream);
            InviteAcceptedMsg msg(mInvitePad, 0, false);
            MsgSource::Handle(msg, false);
        } else {
            static InviteAcceptedMsg expiredMsg(0, 0, true);
            MsgSource::Handle(expiredMsg, false);
        }
        XNetQosRelease(mInviteQos);
        mInviteQos = 0;
    }
}

// 0x823EE1B8
void XSessionSearcher::StartSearching(User *user, const SearchSettings &settings) {
    SessionSearcher::StartSearching(user, settings);
    LocalUser *localUser = user->GetLocalUser();
    int pad = localUser->GetPadNum();
    if (ThePlatformMgr.IsUserSignedIn(localUser)) {
        delete mResults;
        mResults = 0;
        gSearchResultsSize = 0;
        int numUsers = TheNetSession->NumUsers();
        XSessionSearchEx(
            0, pad, mSearchLimit, numUsers, 0, 0, 0, 0, &gSearchResultsSize, 0, 0
        );
        mResults = (XSESSION_SEARCHRESULT_HEADER *)operator new(gSearchResultsSize);
        gSearchContexts[0].dwContextId = X_CONTEXT_GAME_TYPE;
        gSearchContexts[0].dwValue = !settings.Ranked();
        gSearchContexts[1].dwContextId = X_CONTEXT_GAME_MODE;
        gSearchContexts[1].dwValue = settings.ModeFilter();
        int numCustom = settings.NumCustomSettings();
        for (int i = 0; i < numCustom; i++) {
            gSearchProperties[i].dwPropertyId = settings.GetCustomID(i);
            gSearchProperties[i].value.type = XUSER_DATA_TYPE_INT32;
            gSearchProperties[i].value.nData = settings.GetCustomValue(i);
        }
        if (mOverlapped) {
            XCancelOverlapped(mOverlapped);
            delete mOverlapped;
        }
        mOverlapped = new XOVERLAPPED();
        memset(mOverlapped, 0, sizeof(XOVERLAPPED));
        XSessionSearchEx(
            settings.mQueryID,
            pad,
            mSearchLimit,
            numUsers,
            numCustom,
            2,
            gSearchProperties,
            gSearchContexts,
            &gSearchResultsSize,
            mResults,
            mOverlapped
        );
        if (mQos) {
            XNetQosRelease(mQos);
            mQos = 0;
        }
    }
}

// 0x823EE030
void XSessionSearcher::StopSearching() {
    SessionSearcher::StopSearching();
    if (mOverlapped) {
        if (mOverlapped->InternalLow == ERROR_IO_PENDING)
            XCancelOverlapped(mOverlapped);
        delete mOverlapped;
        mOverlapped = 0;
    }
    if (mQos) {
        XNetQosRelease(mQos);
        mQos = 0;
    }
}

// 0x823EE0A8
void XSessionSearcher::ClearSearchResults() {
    SessionSearcher::ClearSearchResults();
    delete mResults;
    mResults = 0;
}

// 0x823EE3C0: fetch the invite, and if it is for this title probe its host.
bool XSessionSearcher::OnMsg(const InviteAcceptedMsg &msg) {
    mInvitePad = msg.GetPadNum();
    XInviteGetAcceptedInfo(msg.GetPadNum(), &mInviteInfo);
    if (mInviteInfo.dwTitleID != TheNet.GetGameData()->PublicID())
        return false;
    if (mInviteQos) {
        XNetQosRelease(mInviteQos);
        mInviteQos = 0;
    }
    const XNADDR *addr = &mInviteInfo.hostInfo.hostAddress;
    const XNKID *id = &mInviteInfo.hostInfo.sessionID;
    const XNKEY *key = &mInviteInfo.hostInfo.keyExchangeKey;
    XNetQosLookup(1, &addr, &id, &key, 0, 0, 0, 1, 0x8000, 0, 0, &mInviteQos);
    return true;
}

// 0x823EE140
SessionSearcher *SessionSearcher::New() { return new XSessionSearcher(); }
