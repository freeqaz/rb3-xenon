#include "meta_band/FriendsProvider.h"
#include "os/Friend.h"
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
// The three UIListProvider overrides this class declares (Text / DataSymbol /
// NumData) live OUTSIDE this run in retail (NumData is fn_82657CB8, in an
// unpinned auto_* region) and are deliberately left as declarations: the match
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
