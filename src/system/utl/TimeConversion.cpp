#include "utl/TimeConversion.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "utl/TempoMap.h"
#include "utl/BeatMap.h"

// This TU is scatter-included into StringTable.cpp's unit; the retail cluster is
// 0x827C90B8..0x827C93D0 and its function order is exactly TimeConversion.h's
// declaration order, so definitions below are kept in that order.

// Retail X360 0x827C90B8 is 24 B / 6 instructions -- lis/lwz TheTempoMap, lwz
// vtable, lwz +0x8 (TimeToTick), mtctr, bctr -- i.e. an unguarded TAIL CALL with
// fp1 passed straight through.  There is no room for a null test.  rb3-Wii agrees
// (`inline float MsToTick(float f) { return TheTempoMap->TimeToTick(f); }`); the
// `!TheTempoMap ? 0 :` guard came from DC3, which is the NEWER engine.
float MsToTick(float ms) { return TheTempoMap->TimeToTick(ms); }

// Retail X360 0x827C90D0 is 64 B: TheTempoMap vcall +0x8 (TimeToTick) then
// lis/lwz TheBeatMap and `bl BeatMap::Beat(float)`.  NO null test on either
// global.  The `if (TheBeatMap && TheTempoMap)` guard this had until lane W16-B
// (2026-09-14) came from DC3 (newer engine); the rb3-Wii oracle has no guard.
float MsToBeat(float ms) { return TheBeatMap->Beat(TheTempoMap->TimeToTick(ms)); }

// Retail X360 0x827C9110 is 24 B / 6 instructions -- lis/lwz TheTempoMap, lwz
// vtable, lwz +0x4 (TickToTime), mtctr, bctr -- the unguarded tail-call twin of
// MsToTick above (which takes vtable +0x8, TimeToTick).
//
// This function was DECLARED in TimeConversion.h and never DEFINED anywhere in the
// tree until lane W15-E: every TickToMs() call site in src/ referenced an undefined
// symbol, which only survives because the match build never links.  TickToSeconds
// and BeatToTick below were in the same state until lane W16-B.
float TickToMs(float tick) { return TheTempoMap->TickToTime(tick); }

// Retail X360 0x827C9128 is 84 B: lis/lwz TheBeatMap, `bl BeatMap::BeatToTick`,
// then TheTempoMap vcall +0x4 (TickToTime).  Unguarded, same DC3-guard story as
// MsToBeat.
float BeatToMs(float beat) { return TheTempoMap->TickToTime(TheBeatMap->BeatToTick(beat)); }

// Retail X360 0x827C9180 is 12 B: lis/lwz TheBeatMap; `b BeatMap::BeatToTick`
// (a tail call, no prologue).  Until lane W16-B this 12 B range was pinned into
// MetaPerformer.cpp's splits.txt entry by mistake; it now sits in StringTable's
// merged .text block where the rest of this cluster lives.
float BeatToTick(float beat) { return TheBeatMap->BeatToTick(beat); }

float TickToBeat(int tick) { return TheBeatMap->Beat(tick); }

// Retail X360 0x827C91A0 is 36 B / 9 instructions: lis/lwz TheTempoMap, lfs
// 1000.0f (lbl_820010B4), fmuls f1, lwz vtable, lwz +0x8 (TimeToTick), mtctr,
// bctr -- MsToTick inlined into a seconds->tick tail call, the exact twin of
// SecondsToBeat below.  Its single retail caller is GamePanel::UpdateNowBar
// (0x82695178; lane W16-AT ported that body and needs this call shape -- an
// inline `TheTempoMap->TimeToTick(sec * 1000)` at the call site would emit the
// vcall in GamePanel instead of a `bl`).  The NAME is convention-derived from
// SecondsToBeat/TickToSeconds; no header (ours, rb3-Wii or DC3) declares it, so
// the map row 0x827c91a0 is deliberately left ANONYMOUS rather than given this
// invented name (under name_check a placeholder callee is already forgiven at
// the GamePanel call site; naming it would be a bet, not a proof).  The map
// used to call it ??__ETheLocale, which was wrong (Locale.cpp's dynamic
// initialiser is not in this cluster).
float SecondsToTick(float sec) { return MsToTick(sec * 1000); }

// Retail X360 0x827C91C8 is 76 B: fmuls f1 by 1000.0f, TheTempoMap vcall +0x8,
// then `bl BeatMap::Beat(float)` -- MsToBeat inlined, unguarded.
float SecondsToBeat(float sec) { return MsToBeat(sec * 1000); }

// Retail X360 0x827C9218 is 64 B: TheTempoMap vcall +0x4 (TickToMs inlined) then
// fmuls by 0.001f (lbl_820010EC).  The map had this address as ??__FTheLocale,
// which was wrong -- that is why every TickToSeconds caller carried a charged
// `bl` site (Gem::AddInstance, Gem::GetStart, *TrainerPanel::AddBeatMask, ...).
float TickToSeconds(float tick) { return TickToMs(tick) / 1000; }

float BeatToSeconds(float beat) { return BeatToMs(beat) / 1000; }

// Retail X360 0x827C9288 calls SecondsToBeat (0x827C91C8), not MsToBeat -- the
// map had 0x827C9288 named OnBeatToMs and left the real OnBeatToMs (0x827C9328,
// which calls BeatToMs) anonymous; lane W16-B swapped them.
DataNode OnSecondsToBeat(DataArray *arr) { return SecondsToBeat(arr->Float(1)); }
DataNode OnBeatToSeconds(DataArray *arr) { return BeatToMs(arr->Float(1)) / 1000; }
DataNode OnBeatToMs(DataArray *arr) { return BeatToMs(arr->Float(1)); }
DataNode OnMsToTick(DataArray *arr) { return MsToTick(arr->Float(1)); }

void TimeConversionInit() {
    DataRegisterFunc("seconds_to_beat", OnSecondsToBeat);
    DataRegisterFunc("beat_to_seconds", OnBeatToSeconds);
    DataRegisterFunc("beat_to_ms", OnBeatToMs);
    DataRegisterFunc("ms_to_tick", OnMsToTick);
}
