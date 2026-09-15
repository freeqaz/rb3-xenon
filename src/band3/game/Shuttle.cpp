#include "game/Shuttle.h"

// Retail 0x826BBC08, 32 B. Shuttle::Shuttle was DECLARED in Shuttle.h with no
// definition anywhere in the tree, so `new Shuttle()` in Game::Game
// (Game.cpp:185) has been an unresolved external.
//
// This file exists because retail's ctor is OUT-OF-LINE. Game::Game reaches it
// with `bl fn_826BBC08` (retail 0x823623EC) -- and there is no LTCG in this
// build, so a 32-byte ctor visible as `inline` in Shuttle.h could not have
// survived /O1 /Ob2 uninlined at that call site. It is therefore defined in a
// TU of its own, which is what this file restores.
//
// The retail call site also corroborates the whole class independently of the
// ctor body: `li r3,0x10` before the operator-new call is sizeof(Shuttle) ==
// 16 (4+4+1+pad+4), and the returned pointer is stored to `stw r3,0xe0(r30)`,
// matching `Shuttle *mShuttle; // 0xe0` in Game.h.
//
// The body itself is a leaf with no stack frame:
//   li r11,0; stb r11,8(r3); stw r11,0xc(r3)   -- mActive=false, mPadNum=0
//   lfs f0, lbl_82000D78; stfs f0,0(r3); stfs f0,4(r3)
// where lbl_82000D78 is `.float 0` (read from
// build/45410914/asm/auto_00_82000400_rdata.s), so both floats are 0.0f. The
// integer stores landing before the float ones is scheduling around the lfs,
// not a different initialisation order.
Shuttle::Shuttle() : mMs(0.0f), mEndMs(0.0f), mActive(false), mPadNum(0) {}

// Retail's `~Shuttle` call site in Game::~Game (RELEASE(mShuttle)) is
//   mr r3,r28 ; bl 0x826c3888 ; mr r3,r28 ; bl <operator delete>
// and 0x826c3888 disassembles to a single `blr` (4 B, no relocations). A
// one-argument destructor-then-delete pair cannot be the two-argument
// StlNodeAlloc copy-ctor the map names at that address; that name is the
// arbitrary survivor spelling of an ICF fold over every empty function, and
// scripts/symbol_aliases.json already carries the group (0x826c3888, tier
// FT-EMPTY, 10 folded spellings).
//
// The destructor was DECLARED in Shuttle.h with no definition anywhere in the
// tree, so ??1Shuttle@@QAA@XZ was an unresolved external and we compiled no
// COMDAT for it -- which is also why the fold-membership evidence could not be
// produced for this spelling. The class is four PODs (float, float, bool, int)
// with no owned resources, so an empty body is the only one consistent with
// both the retail bytes and the class contents.
Shuttle::~Shuttle() {}
