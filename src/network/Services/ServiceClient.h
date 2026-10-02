#pragma once
#include "Platform/RootObject.h"
#include "Protocol/Protocol.h"

namespace Quazal {

    class ServiceClient : public RootObject {
    public:
        ServiceClient(unsigned int);
        virtual ~ServiceClient();
        virtual bool IsConnected() const;
        virtual void UseLocalLoopback(unsigned int, unsigned int);
        virtual void ConnectionStateHasChanged();
        virtual bool IsFaulty() const;
        virtual bool RegisterProtocols();

        bool RegisterExtraProtocol(Protocol *, unsigned char);
        // Retail 0x82A8D128 and 0x82A8D280: XboxServer::Poll binds each client it
        // creates to the login credentials and logs out if one fails; the logout
        // path calls the second on each client before deleting it. Retail keeps
        // no names; these are descriptive.
        bool Bind(class Credentials *);
        void Unbind();
    };

}