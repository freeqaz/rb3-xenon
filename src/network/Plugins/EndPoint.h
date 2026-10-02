#pragma once
#include "Platform/RootObject.h"
#include "Platform/Result.h"
#include "Platform/UserContext.h"
#include "Plugins/StationURL.h"

namespace Quazal {
    class Buffer;
    class ObjDupProtocol;
    class EndPoint;
    typedef void (*pfCompletion)(EndPoint *, qResult, const UserContext *);

    // Virtual order from the retail PRUDPEndPoint vtable (JobJoinSession calls
    // slots 4, 7 and 17; JobConnectStation slots 4, 16 and 20). JobJoinSession
    // reads the address at +8.
    class EndPoint : public RootObject {
    public:
        virtual bool IsNotConnected() = 0;
        virtual bool IsConnecting() = 0;
        virtual bool IsDisconnecting() = 0;
        virtual bool IsFaulty() = 0;
        virtual bool IsConnected() = 0;
        virtual bool PeerIsConnected() = 0;
        virtual bool PeerIsDisconnected() = 0;
        virtual void Unk7();
        virtual void SetKeepAliveTimeout(unsigned int) = 0;
        virtual void Unk9();
        virtual unsigned int GetKeepAliveTimeout() = 0;
        virtual unsigned int GetMaxSilenceTime() = 0;
        virtual void SetPeerConnected() = 0;
        virtual void SetPeerDisconnected() = 0;
        virtual int GetConnectionState() = 0;
        virtual bool SetConnectionState(int) = 0;
        virtual qResult RegisterProtocol(ObjDupProtocol *);
        virtual void SignalEvent(unsigned int);
        virtual unsigned int GetRTT() = 0;
        virtual unsigned int GetRTTAverage() = 0;
        virtual qResult
        _Connect(Buffer *, Buffer *, pfCompletion, const UserContext &, unsigned int) = 0;

        // Inline wrapper: callers evaluate the arguments into its parameter
        // homes before the virtual call.
        qResult Connect(
            Buffer *pBuffer, Buffer *pReply, pfCompletion pfCallback,
            const UserContext &oContext, unsigned int uiTimeout
        ) {
            return _Connect(pBuffer, pReply, pfCallback, oContext, uiTimeout);
        }

        void Close();
        void Open();
        void Release();
        void SetPID(unsigned int);
        void SetStationHandle(unsigned int);
        const StationURL &GetAddress() const { return m_oAddress; }

        void *m_pStream; // 0x4
        StationURL m_oAddress; // 0x8
    };
}
