#pragma once
#include "../win_types.h"
#include "winsockx.h"

#ifdef __cplusplus
extern "C" {
#endif

INT XNetRandom(BYTE *pb, UINT cb);
DWORD XNetGetEthernetLinkStatus();
INT XNetServerToInAddr(const IN_ADDR inaServer, DWORD dwServiceId, IN_ADDR *pina);

DWORD XNetGetConnectStatus(const IN_ADDR ina);
INT XNetUnregisterInAddr(const IN_ADDR ina);
INT XNetConnect(const IN_ADDR ina);

DWORD XNetGetTitleXnAddr(XNADDR *pxna);

/* QoS probing (XSessionSearcher, network/net/SessionSearcher_Xbox.cpp). It
   reads cxnqosPending at +0x4 and each 0x18-byte entry's bFlags / cbData /
   pbData at +0x8 / +0xe / +0x10 of the XNQOS. */
typedef struct _XNQOSINFO { /* Size=0x18 */
    /* 0x0000 */ BYTE bFlags;
    /* 0x0001 */ BYTE bReserved;
    /* 0x0002 */ WORD cProbesXmit;
    /* 0x0004 */ WORD cProbesRecv;
    /* 0x0006 */ WORD cbData;
    /* 0x0008 */ BYTE *pbData;
    /* 0x000c */ WORD wRttMinInMsecs;
    /* 0x000e */ WORD wRttMedInMsecs;
    /* 0x0010 */ DWORD dwUpBitsPerSec;
    /* 0x0014 */ DWORD dwDnBitsPerSec;
} XNQOSINFO;

typedef struct _XNQOS { /* Size=0x20 */
    /* 0x0000 */ UINT cxnqos;
    /* 0x0004 */ UINT cxnqosPending;
    /* 0x0008 */ XNQOSINFO axnqosinfo[1];
} XNQOS;

#define XNET_XNQOSINFO_COMPLETE 0x01
#define XNET_XNQOSINFO_TARGET_CONTACTED 0x02
#define XNET_XNQOSINFO_TARGET_DISABLED 0x04
#define XNET_XNQOSINFO_DATA_RECEIVED 0x08
#define XNET_XNQOSINFO_PARTIAL_COMPLETE 0x10

INT XNetQosLookup(
    UINT cxna,
    const XNADDR *apxna[],
    const XNKID *apxnkid[],
    const XNKEY *apxnkey[],
    UINT cina,
    const IN_ADDR aina[],
    const DWORD adwServiceId[],
    UINT cProbes,
    DWORD dwBitsPerSec,
    DWORD dwFlags,
    HANDLE hEvent,
    XNQOS **ppxnqos
);
INT XNetQosRelease(XNQOS *pxnqos);
INT XNetXnAddrToMachineId(const XNADDR *pxnaddr, ULONGLONG *pqwMachineId);

#ifdef __cplusplus
}
#endif
