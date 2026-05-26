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
INT XNetXnAddrToMachineId(const XNADDR *pxnaddr, ULONGLONG *pqwMachineId);

#ifdef __cplusplus
}
#endif
