// M1 off-path link stubs (native only, rb3-song). Originally AUTO-GENERATED
// from the rb3-song link's undefined C++ references.
//
// W16-PL: re-derived by deleting this file from the link and reading the
// --gc-sections linker's undefined list. 24 stubs -> 14:
//   * 7 LicenseMgr members are gone -- rb3-song links the real
//     src/band3/meta_band/LicenseMgr.cpp. (The stub ctor never constructed the
//     object's std::set / hash_map, so BandSongMgr::Init's `new LicenseMgr()`
//     handed back uninitialised containers.)
//   * BandUserMgr::GetParticipatingBandUsers, BandMachineMgr::IsSongShared and
//     BandUser::GetControllerSym are no longer referenced at all under gc.
// What remains is referenced only from BandSongMgr::ContentDone /
// AllowContentToBeAdded / GetRankedSongs / SyncSharedSongs and
// BandSongMetadata::HasPart, i.e. virtual slots and unexercised accessors.
// Measured with gdb breakpoints on all five over the native_health ark run:
// none is ever entered. Their real TUs (SessionMgr, ProfileMgr, RockCentral,
// SaveLoadManager, UIEventMgr, GameMode) pull the session / profile / net / UI
// graph.
//
// Functions return 0. TheGameMode / TheSaveLoadMgr / TheSessionMgr /
// TheUIEventMgr are null pointers, which the code null-checks before use.
// NB TheProfileMgr and TheRockCentral are OBJECTS in real code
// (meta_band/ProfileMgr.h, net_band/RockCentral.h), not pointers; the 8 zero
// bytes here are NOT a valid object. ContentDone is the only reader
// (TheRockCentral.IsOnline()), and it is never entered (above).

.text

// ProfileMgr::GetSignedInProfiles()
.weak _ZN10ProfileMgr19GetSignedInProfilesEv
.type _ZN10ProfileMgr19GetSignedInProfilesEv,@function
_ZN10ProfileMgr19GetSignedInProfilesEv:
    xorq %rax, %rax
    ret

// UIEventMgr::TriggerEvent(Symbol, DataArray*)
.weak _ZN10UIEventMgr12TriggerEventE6SymbolP9DataArray
.type _ZN10UIEventMgr12TriggerEventE6SymbolP9DataArray,@function
_ZN10UIEventMgr12TriggerEventE6SymbolP9DataArray:
    xorq %rax, %rax
    ret

// RockCentral::SyncAvailableSongs(std::vector<BandProfile*, std::allocator<BandProfile*> > const&, std::vector<int, std::allocator<int> > const&, std::vector<int, std::allocator<int> > const&, Hmx::Object*)
.weak _ZN11RockCentral18SyncAvailableSongsERKSt6vectorIP11BandProfileSaIS2_EERKS0_IiSaIiEESA_PN3Hmx6ObjectE
.type _ZN11RockCentral18SyncAvailableSongsERKSt6vectorIP11BandProfileSaIS2_EERKS0_IiSaIiEESA_PN3Hmx6ObjectE,@function
_ZN11RockCentral18SyncAvailableSongsERKSt6vectorIP11BandProfileSaIS2_EERKS0_IiSaIiEESA_PN3Hmx6ObjectE:
    xorq %rax, %rax
    ret

// SaveLoadManager::AutoSave()
.weak _ZN15SaveLoadManager8AutoSaveEv
.type _ZN15SaveLoadManager8AutoSaveEv,@function
_ZN15SaveLoadManager8AutoSaveEv:
    xorq %rax, %rax
    ret

// LocalBandMachine::SetAvailableSongs(std::set<int, std::less<int>, std::allocator<int> > const&)
.weak _ZN16LocalBandMachine17SetAvailableSongsERKSt3setIiSt4lessIiESaIiEE
.type _ZN16LocalBandMachine17SetAvailableSongsERKSt3setIiSt4lessIiESaIiEE,@function
_ZN16LocalBandMachine17SetAvailableSongsERKSt3setIiSt4lessIiESaIiEE:
    xorq %rax, %rax
    ret

// LocalBandMachine::SetProGuitarOrBassSongs(std::set<int, std::less<int>, std::allocator<int> > const&)
.weak _ZN16LocalBandMachine23SetProGuitarOrBassSongsERKSt3setIiSt4lessIiESaIiEE
.type _ZN16LocalBandMachine23SetProGuitarOrBassSongsERKSt3setIiSt4lessIiESaIiEE,@function
_ZN16LocalBandMachine23SetProGuitarOrBassSongsERKSt3setIiSt4lessIiESaIiEE:
    xorq %rax, %rax
    ret

// BandMachineMgr::GetLocalMachine() const
.weak _ZNK14BandMachineMgr15GetLocalMachineEv
.type _ZNK14BandMachineMgr15GetLocalMachineEv,@function
_ZNK14BandMachineMgr15GetLocalMachineEv:
    xorq %rax, %rax
    ret

// BandMachineMgr::IsSongAllowedToHavePart(int, Symbol) const
.weak _ZNK14BandMachineMgr23IsSongAllowedToHavePartEi6Symbol
.type _ZNK14BandMachineMgr23IsSongAllowedToHavePartEi6Symbol,@function
_ZNK14BandMachineMgr23IsSongAllowedToHavePartEi6Symbol:
    xorq %rax, %rax
    ret

.bss
.p2align 3
// TheGameMode (null manager pointer)
.weak TheGameMode
.type TheGameMode,@object
.size TheGameMode,8
TheGameMode:
    .zero 8

// TheProfileMgr (null manager pointer)
.weak TheProfileMgr
.type TheProfileMgr,@object
.size TheProfileMgr,8
TheProfileMgr:
    .zero 8

// TheRockCentral (null manager pointer)
.weak TheRockCentral
.type TheRockCentral,@object
.size TheRockCentral,8
TheRockCentral:
    .zero 8

// TheSaveLoadMgr (null manager pointer)
.weak TheSaveLoadMgr
.type TheSaveLoadMgr,@object
.size TheSaveLoadMgr,8
TheSaveLoadMgr:
    .zero 8

// TheSessionMgr (null manager pointer)
.weak TheSessionMgr
.type TheSessionMgr,@object
.size TheSessionMgr,8
TheSessionMgr:
    .zero 8

// TheUIEventMgr (null manager pointer)
.weak TheUIEventMgr
.type TheUIEventMgr,@object
.size TheUIEventMgr,8
TheUIEventMgr:
    .zero 8

