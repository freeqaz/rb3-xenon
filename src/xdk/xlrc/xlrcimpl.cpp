#include "types.h"
#include "xdk/xapilibi/winerror.h"

// Forward declarations
enum XLRC_CLIENT_TRANSPORT_ID {};
class CXLrcClient;
class CXLrcTransport;

namespace CXLrcImpl {
    CXLrcClient* CreateClient(unsigned int* size);
}

// CXLrcClient::CreateTransport
extern "C" CXLrcTransport* __thiscall CXLrcClient_CreateTransport(
    CXLrcClient* client,
    XLRC_CLIENT_TRANSPORT_ID transportId,
    unsigned int size
);

extern "C" long __cdecl CXLrcImpl_CreateClientWithTransport(
    XLRC_CLIENT_TRANSPORT_ID transportId,
    unsigned int* outSize,
    CXLrcClient** outClient,
    CXLrcTransport** outTransport
) {
    unsigned int size = 4;
    long result = 0;

    CXLrcClient* client = CXLrcImpl::CreateClient(&size);
    if (client == 0) {
        if (size < 4) {
            result = E_OUTOFMEMORY;
        } else {
            result = E_INVALID_OPERATION;
        }
    } else {
        CXLrcTransport* transport = CXLrcClient_CreateTransport(client, transportId, size);
        if (transport == 0) {
            result = E_FAIL;
        } else {
            *outSize = size;
            *outClient = client;
            *outTransport = transport;
        }
    }

    return result;
}
