#pragma once
#include "../win_types.h"
#include "xdk/xapilibi/xbase.h"
#include "xdk/xnet/winsockx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _XTITLE_SERVER_INFO { /* Size=0xd0 */
    /* 0x0000 */ IN_ADDR inaServer;
    /* 0x0004 */ char pad[0xcc];
} XTITLE_SERVER_INFO;

/* The XN_LIVE_CONNECTIONCHANGED parameter that means "logged on". */
#define XONLINE_S_LOGON_CONNECTION_ESTABLISHED 0x001510F0L

enum XONLINE_NAT_TYPE {
    XONLINE_NAT_OPEN = 0x0001,
    XONLINE_NAT_MODERATE = 0x0002,
    XONLINE_NAT_STRICT = 0x0003,
};

typedef struct _XSESSION_VIEW_PROPERTIES { /* Size=0xc */
    /* 0x0000 */ DWORD dwViewId;
    /* 0x0004 */ DWORD dwNumProperties;
    /* 0x0008 */ _XUSER_PROPERTY *pProperties;
} XSESSION_VIEW_PROPERTIES;

typedef struct _XSTORAGE_FILE_INFO { /* Size=0x41 */
    /* 0x0000 */ DWORD dwTitleID;
    /* 0x0004 */ DWORD dwTitleVersion;
    /* 0x0008 */ QWORD qwOwnerPUID;
    /* 0x0010 */ BYTE bCountryID;
    /* 0x0011 */ QWORD qwReserved;
    /* 0x0019 */ DWORD dwContentType;
    /* 0x001d */ DWORD dwStorageSize;
    /* 0x0021 */ DWORD dwInstalledSize;
    /* 0x0025 */ FILETIME ftCreated;
    /* 0x002d */ FILETIME ftLastModified;
    /* 0x0035 */ WORD wAttributesSize;
    /* 0x0037 */ WORD cchPathName;
    /* 0x0039 */ WCHAR *pwszPathName;
    /* 0x003d */ BYTE *pbAttributes;
} XSTORAGE_FILE_INFO;

typedef struct _XSTORAGE_ENUMERATE_RESULTS { /* Size=0xc */
    /* 0x0000 */ DWORD dwTotalNumItems;
    /* 0x0004 */ DWORD dwNumItemsReturned;
    /* 0x0008 */ XSTORAGE_FILE_INFO *pItems;
} XSTORAGE_ENUMERATE_RESULTS;

DWORD XTitleServerCreateEnumerator(
    LPCSTR pszServerInfo, DWORD cItem, DWORD *pcbBuffer, HANDLE *hEnum
);

DWORD XSessionStart(HANDLE hSession, DWORD dwFlags, XOVERLAPPED *pXOverlapped);
DWORD XSessionEnd(HANDLE hSession, XOVERLAPPED *pXOverlapped);

DWORD XSessionWriteStats(
    HANDLE hSession,
    XUID xuid,
    DWORD dwNumViews,
    XSESSION_VIEW_PROPERTIES *pViews,
    XOVERLAPPED *pXOverlapped
);
DWORD XSessionCreate(
    DWORD dwFlags,
    DWORD dwUserIndex,
    DWORD dwMaxPublicSlots,
    DWORD dwMaxPrivateSlots,
    ULONGLONG *pqwSessionNonce,
    XSESSION_INFO *pSessionInfo,
    XOVERLAPPED *pXOverlapped,
    HANDLE *ph
);
DWORD XSessionDelete(HANDLE hSession, XOVERLAPPED *pXOverlapped);
DWORD XSessionJoinLocal(
    HANDLE hSession,
    DWORD dwUserCount,
    const DWORD *pdwUserIndexes,
    const BOOL *pfPrivateSlots,
    XOVERLAPPED *pXOverlapped
);
DWORD XSessionLeaveLocal(
    HANDLE hSession,
    DWORD dwUserCount,
    const DWORD *pdwUserIndexes,
    XOVERLAPPED *pXOverlapped
);
DWORD XSessionModify(
    HANDLE hSession,
    DWORD dwFlags,
    DWORD dwMaxPublicSlots,
    DWORD dwMaxPrivateSlots,
    XOVERLAPPED *pXOverlapped
);
DWORD XSessionJoinRemote(
    HANDLE hSession,
    DWORD dwXuidCount,
    const XUID *pXuids,
    const BOOL *pfPrivateSlots,
    XOVERLAPPED *pXOverlapped
);
DWORD XSessionLeaveRemote(
    HANDLE hSession, DWORD dwXuidCount, const XUID *pXuids, XOVERLAPPED *pXOverlapped
);

typedef struct _XSESSION_REGISTRATION_RESULTS XSESSION_REGISTRATION_RESULTS;
DWORD XSessionArbitrationRegister(
    HANDLE hSession,
    DWORD dwFlags,
    ULONGLONG qwSessionNonce,
    DWORD *pcbRegistrationResults,
    XSESSION_REGISTRATION_RESULTS *pRegistrationResults,
    XOVERLAPPED *pXOverlapped
);

#define XSESSION_CREATE_HOST 0x00000001
#define XSESSION_CREATE_USES_PRESENCE 0x00000002
#define XSESSION_CREATE_USES_STATS 0x00000004
#define XSESSION_CREATE_USES_MATCHMAKING 0x00000008
#define XSESSION_CREATE_USES_ARBITRATION 0x00000010
#define XSESSION_CREATE_USES_PEER_NETWORK 0x00000020
#define XSESSION_CREATE_INVITES_DISABLED 0x00000100
#define XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED 0x00000200

#define XONLINE_GAMERTAG_SIZE 16
#define XONLINE_FRIENDSTATE_FLAG_SENTREQUEST 0x40000000
#define XONLINE_FRIENDSTATE_FLAG_RECEIVEDREQUEST 0x80000000

#pragma pack(push, 4)
typedef struct _XONLINE_FRIEND { /* Size=0xc4 */
    XUID xuid;
    CHAR szGamertag[XONLINE_GAMERTAG_SIZE];
    DWORD dwFriendState; /* 0x18 */
    BYTE pad[0xa8];
} XONLINE_FRIEND;
#pragma pack(pop)

DWORD XFriendsCreateEnumerator(
    DWORD dwUserIndex,
    DWORD dwStartingIndex,
    DWORD dwFriendsToReturn,
    DWORD *pcbBuffer,
    HANDLE *ph
);
// XStringVerify (retail import @0x82A6A8F8). The two structs are 2-byte
// packed in retail: XboxEntityUploader allocates STRING_DATA arrays with a
// 6-byte stride and reads pszString / pStringResult at +2.
#pragma pack(push, 2)
typedef struct _STRING_DATA { /* Size=0x6 */
    /* 0x0000 */ WORD wStringSize;
    /* 0x0002 */ WCHAR *pszString;
} STRING_DATA;

typedef struct _STRING_VERIFY_RESPONSE { /* Size=0x6 */
    /* 0x0000 */ WORD wNumStrings;
    /* 0x0002 */ HRESULT *pStringResult;
} STRING_VERIFY_RESPONSE;
#pragma pack(pop)

DWORD XStringVerify(
    DWORD dwFlags,
    const CHAR *szLocale,
    DWORD dwNumStrings,
    const STRING_DATA *pStringData,
    DWORD cbResults,
    STRING_VERIFY_RESPONSE *pResults,
    XOVERLAPPED *pXOverlapped
);

/* Session search and invites (XSessionSearcher, network/net/SessionSearcher_Xbox.cpp). */
#define X_CONTEXT_GAME_TYPE 0x0000800A
#define X_CONTEXT_GAME_MODE 0x0000800B

typedef struct _XUSER_CONTEXT { /* Size=0x8 */
    /* 0x0000 */ DWORD dwContextId;
    /* 0x0004 */ DWORD dwValue;
} XUSER_CONTEXT;

typedef struct _XSESSION_SEARCHRESULT { /* Size=0x5c */
    /* 0x0000 */ XSESSION_INFO info;
    /* 0x003c */ DWORD dwOpenPublicSlots;
    /* 0x0040 */ DWORD dwOpenPrivateSlots;
    /* 0x0044 */ DWORD dwFilledPublicSlots;
    /* 0x0048 */ DWORD dwFilledPrivateSlots;
    /* 0x004c */ DWORD cProperties;
    /* 0x0050 */ DWORD cContexts;
    /* 0x0054 */ XUSER_PROPERTY *pProperties;
    /* 0x0058 */ XUSER_CONTEXT *pContexts;
} XSESSION_SEARCHRESULT;

typedef struct _XSESSION_SEARCHRESULT_HEADER { /* Size=0x8 */
    /* 0x0000 */ DWORD dwSearchResults;
    /* 0x0004 */ XSESSION_SEARCHRESULT *pResults;
} XSESSION_SEARCHRESULT_HEADER;

DWORD XSessionSearchEx(
    DWORD dwProcedureIndex,
    DWORD dwUserIndex,
    DWORD dwNumResults,
    DWORD dwNumUsers,
    WORD wNumProperties,
    WORD wNumContexts,
    XUSER_PROPERTY *pSearchProperties,
    XUSER_CONTEXT *pSearchContexts,
    DWORD *pcbResultsBuffer,
    XSESSION_SEARCHRESULT_HEADER *pSearchResults,
    XOVERLAPPED *pXOverlapped
);

/* 4-byte packed: XSessionSearcher holds one at +0x44 with its own members
   right after it at +0x98. */
#pragma pack(push, 4)
typedef struct _XINVITE_INFO { /* Size=0x54 */
    /* 0x0000 */ XUID xuidInviter;
    /* 0x0008 */ XUID xuidInvitee;
    /* 0x0010 */ DWORD dwTitleID;
    /* 0x0014 */ XSESSION_INFO hostInfo;
    /* 0x0050 */ BOOL fFromGameInvite;
} XINVITE_INFO;
#pragma pack(pop)

DWORD XInviteGetAcceptedInfo(DWORD dwUserIndex, XINVITE_INFO *pInfo);

DWORD XOnlineStartup();
DWORD XOnlineCleanup();

#ifdef __cplusplus
}
#endif
