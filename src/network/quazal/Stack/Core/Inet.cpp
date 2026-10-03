// Quazal NetZ - Inet
//
// Retail TU: .text 0x82B392D0..0x82B39320, compiled /Od /Oi- /Ob1 /GR- (see
// objects.json). It has no __FILE__ string and no .rdata; it is placed between
// IOCompletionNotifier and QueuingSocket. UDPTransport::Initialize calls
// Initialize (0x82B392D0) and ~UDPTransport calls Terminate (0x82B392F0).
//
// Both forward to the platform network start-up/clean-up (0x82A8ECE0, which
// runs XNetStartup and WSAStartup, and 0x82A8EE10, which runs WSACleanup and
// XNetCleanup). Terminate has a second, unreachable return.

namespace Quazal {

    class PlatformNetwork {
    public:
        static bool Startup();
        static void Cleanup();
    };

    class Inet {
    public:
        static bool Initialize();
        static bool Terminate();
    };

    bool Inet::Initialize() { return PlatformNetwork::Startup(); }

    bool Inet::Terminate() {
        PlatformNetwork::Cleanup();
        return true;
        return true;
    }

}
