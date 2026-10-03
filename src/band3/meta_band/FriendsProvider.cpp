#include "meta_band/FriendsProvider.h"
#include "net/NetSession.h"
#include "os/Friend.h"
#include "rndobj/Dir.h"
#include "rndobj/Mat.h"
#include "ui/UIListMesh.h"
#include "utl/Locale.h"
#include "utl/Std.h"

// Retail RB3 X360 FriendsProvider bodies, read from the retail XEX by lane
// W16-T. Addresses are the pre-renamer dtk spellings in
// build/45410914/asm/{UIList,BandLabel}.s:
//
//   0x826661D8  132 B  ctor                       (already named in the map)
//   0x82666288    8 B  `??_E…W3` adjustor thunk    (compiler-generated)
//   0x826662E0    8 B  Reload  -> tail call to DeleteAll<vector<Friend*>>
//   0x826662F0  128 B  dtor
//   0x826663F0   76 B  scalar deleting dtor        (compiler-generated)
//
// Retail's primary vtable (0x820D73BC) overrides exactly: the dtor, Text
// (slot 1, 0x82665FD8), Mat (2, 0x82666078), NumData (10, 0x8269A8E8 -- a
// vector-size body ICF-shared with Band::NumActivePlayers) and InitData (13,
// 0x82665F70); DataSymbol is UIListProvider's (lane W16-OP). Mat, NumData
// and InitData are defined below. Text (0x82665FD8) is deliberately left as a
// declaration: its record type is AppLabel::FriendRecord, a layout-identical
// stand-in for Friend, and unifying the two renames a mapped, matching symbol.
// The match build only COMPILES, so the vtable's reference to it is an
// ordinary undefined external, and this TU is not part of the native link
// (native CMakeLists lists src/band3/meta_band explicitly).
//
// The TU's leading run 0x82665DE4-0x82666160 (InviteFriend, a Friend sort
// comparator, InitData, Text, Mat, an STL heap helper) and the STL helper at
// 0x82666290 used to be pinned to UIList.cpp, which cannot define any of them;
// lane W16-OR re-homed both blocks here.

FriendsProvider::FriendsProvider() {}

// retail 0x826662F0: DeleteAll(mFriends) then the implicit ~vector() (the
// `subf/srawi/slwi; bl MemOrPoolFreeSTL` tail) and ~Hmx::Object() on this+4.
FriendsProvider::~FriendsProvider() { DeleteAll(mFriends); }

// retail 0x826662E0 is exactly two instructions: `addi r3, r3, 0x2c` then
// `b DeleteAll<vector<Friend*,StlNodeAlloc<Friend*>>>` -- a tail call, so the
// body is the single DeleteAll and nothing else.
void FriendsProvider::Reload() { DeleteAll(mFriends); }

// retail 0x82665DF0 (lane W16-OR): two function-local static Symbols (guard
// bits 0 and 1, in this order), then NetSession's slot-3 virtual
// InviteFriend(Friend*, subject, body). Its two 32-byte guard-reset funclets
// are retail 0x82665EC0 / 0x82665EE0.
void FriendsProvider::InviteFriend(int i) {
    static Symbol invite_subject("invite_subject");
    static Symbol invite_body("invite_body");
    TheNetSession->InviteFriend(
        mFriends[i], Localize(invite_subject, nullptr), Localize(invite_body, nullptr)
    );
}

// retail 0x82665F70
void FriendsProvider::InitData(RndDir *dir) {
    mOnlineMat = dir->Find<RndMat>("status_online.mat", false);
    mOfflineMat = dir->Find<RndMat>("status_offline.mat", false);
}

// retail 0x82666078: only the "online_status" slot gets a material, chosen by
// the friend's online flag (Friend+0xc).
RndMat *FriendsProvider::Mat(int, int data, UIListMesh *slot) const {
    if (slot->Matches("online_status")) {
        return mFriends[data]->mOnline ? mOnlineMat : mOfflineMat;
    }
    return 0;
}

// retail 0x8269A8E8 (body shared by ICF with Band::NumActivePlayers):
// (end - begin) >> 2 over the vector at 0x2c.
int FriendsProvider::NumData() const { return mFriends.size(); }
