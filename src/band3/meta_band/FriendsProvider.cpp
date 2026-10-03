#include "meta_band/FriendsProvider.h"
#include "os/Friend.h"
#include "rndobj/Dir.h"
#include "rndobj/Mat.h"
#include "ui/UIListMesh.h"
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
// and InitData are defined below. Text lives OUTSIDE this run in retail and is
// deliberately left as a declaration (its record type is AppLabel::FriendRecord,
// a layout-identical stand-in for Friend): the match
// build only COMPILES, so the vtable's references to them are ordinary
// undefined externals, and this TU is not part of the native link (native
// CMakeLists lists src/band3/meta_band explicitly; only src/system/* and
// src/platform are globbed). Writing speculative bodies for them would be
// fabrication, not decomp.

FriendsProvider::FriendsProvider() {}

// retail 0x826662F0: DeleteAll(mFriends) then the implicit ~vector() (the
// `subf/srawi/slwi; bl MemOrPoolFreeSTL` tail) and ~Hmx::Object() on this+4.
FriendsProvider::~FriendsProvider() { DeleteAll(mFriends); }

// retail 0x826662E0 is exactly two instructions: `addi r3, r3, 0x2c` then
// `b DeleteAll<vector<Friend*,StlNodeAlloc<Friend*>>>` -- a tail call, so the
// body is the single DeleteAll and nothing else.
void FriendsProvider::Reload() { DeleteAll(mFriends); }

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
