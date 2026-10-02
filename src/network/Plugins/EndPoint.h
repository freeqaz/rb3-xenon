#pragma once
#include "Platform/RootObject.h"
#include "Plugins/StationURL.h"

namespace Quazal {
    // Virtual order as PRUDPEndPoint.cpp lays it out; JobJoinSession calls
    // slots 4 (IsConnected), 7 and 17 and reads the address at +8.
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
        virtual void Unk16();
        virtual void SignalEvent(unsigned int);

        void Close();
        void Open();
        void SetPID(unsigned int);
        const StationURL &GetAddress() const { return m_oAddress; }

        void *m_pStream; // 0x4
        StationURL m_oAddress; // 0x8
    };
}