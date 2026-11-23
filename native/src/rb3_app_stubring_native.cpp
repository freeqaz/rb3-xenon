// rb3-app native bodies for in-scope link stubs (lane stubring, 2026-11-23).
// Each replaces an aborting stub of rb3_app_link_stubs.s; the record is
// docs/decomp/STUBRING_RB3APP_INSCOPE_STUBS_2026-11-23.md.
#include "os/PlatformMgr.h"
#include "os/OnlineID.h"
#include "os/User.h"
#include "os/Joypad.h"
#include "meta/Profile.h"
#include "xdk/LIBCMT/vectorintrinsics.h"
#include "xdk/xbdm/xbdm.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// ---- VMX128 intrinsics (ring a1-xbox-vmx) ----------------------------------
// Their one caller is ShiftedDotProduct's fast path (dsp/SndAnalysis.cpp,
// retail 0x82B816F0), which PitchDetector::Process runs for a live mic
// (GameMic, vocals). Each is the single VMX instruction the X360 compiler
// emits for it; lane semantics follow the Power ISA, with the 32-byte vperm
// source indexed in big-endian byte order (byte 0 = most significant byte of
// word 0), which is how the selector table in SndAnalysis.cpp is written.
// Not modelled: VMX128's non-IEEE mode (denormals flushed to zero).
extern "C" {

XMVECTOR __vmaddfp(XMVECTOR mul1, XMVECTOR mul2, XMVECTOR addend) {
    XMVECTOR r;
    for (int i = 0; i < 4; i++)
        r.v[i] = fmaf(mul1.v[i], mul2.v[i], addend.v[i]); // vmaddfp rounds once
    return r;
}

XMVECTOR __vspltw(XMVECTOR src, unsigned int imm) {
    XMVECTOR r;
    unsigned int w = src.u[imm & 3];
    for (int i = 0; i < 4; i++)
        r.u[i] = w;
    return r;
}

XMVECTOR __vperm(XMVECTOR a, XMVECTOR b, XMVECTOR perm) {
    auto beByte = [](const XMVECTOR &v, int k) -> unsigned int {
        return (v.u[k >> 2] >> (24 - 8 * (k & 3))) & 0xFF;
    };
    XMVECTOR r;
    for (int w = 0; w < 4; w++) {
        unsigned int out = 0;
        for (int j = 0; j < 4; j++) {
            int sel = (int)beByte(perm, w * 4 + j) & 0x1F;
            unsigned int byte = sel < 16 ? beByte(a, sel) : beByte(b, sel - 16);
            out |= byte << (24 - 8 * j);
        }
        r.u[w] = out;
    }
    return r;
}

// ---- xbdm (ring a1-absent-body) --------------------------------------------
// AllocInfo::FillStackTrace (utl/AllocInfo.cpp, HX_NATIVE) is its only
// caller: MemTracker's per-allocation record. Retail's stack words are 32-bit
// PPC return addresses; an LP64 address does not fit the int slots the caller
// passes, so the trace is empty (first word 0 ends the caller's copy loop).
HRESULT DmCaptureStackBackTrace(ULONG frames, VOID *out) {
    if (out && frames)
        memset(out, 0, frames * sizeof(int));
    return 0;
}

// XRemapUserDevices (PlatformMgr_Xbox.cpp declares it extern "C"): the system
// remaps which controller each signed-in user owns. Natively the pad order is
// the input sources' order and nothing outside the game tracks ownership, so
// the remap always succeeds and the game-side swap (JoypadSwapPads) is the
// whole effect, as on retail after a successful call.
DWORD XRemapUserDevices(DWORD *mapping) {
    fprintf(stderr, "rb3-app: XRemapUserDevices %lu %lu %lu %lu\n", (unsigned long)mapping[0],
            (unsigned long)mapping[1], (unsigned long)mapping[2], (unsigned long)mapping[3]);
    return 0;
}

} // extern "C"

// ---- PlatformMgr (ring a1-platformmgr) -------------------------------------
// The bodies are PlatformMgr_Xbox.cpp's, which rb3-app does not compile
// (platform/PlatformMgr_Native.cpp stands in for the rest of the class).

// Retail 0x8251D6C8: the 4-pad permutation that exchanges the two users'
// pads, the system remap, then on success JoypadSwapPads (0x82525DE0) and a
// ProfileSwappedMsg (static, ctor 0x8251CD08) exported to the sinks. Caller:
// OvershellSlot::SwapUserProfile (0x825DF9C8), the overshell's "swap
// profile" confirm (ConfirmSwapUserProfile).
void PlatformMgr::SwapUserPads(LocalUser *user1, LocalUser *user2) {
    int pad1 = user1->GetPadNum();
    int pad2 = user2->GetPadNum();
    DWORD mapping[4];
    for (int i = 0; i < 4; i++) {
        int pad = i;
        if (i == pad1)
            pad = pad2;
        else if (i == pad2)
            pad = pad1;
        mapping[i] = pad;
    }
    if (XRemapUserDevices(mapping) == 0) {
        JoypadSwapPads(pad1, pad2);
        static ProfileSwappedMsg msg(nullptr, nullptr);
        msg[0] = user1;
        msg[1] = user2;
        Export(msg, true);
    }
}

// Retail 0x8251C208: OnlineID::GetXUID, then the two guest bits
// (3 << 54 = 0x00C0000000000000). Callers: SessionUsersProviders (the
// overshell's user list, guest filter) and SongSelectPanel.
bool PlatformMgr::IsGuestOnlineID(const OnlineID *oid) const {
    if (oid->GetXUID() & 0x00C0000000000000ULL)
        return true;
    return false;
}
