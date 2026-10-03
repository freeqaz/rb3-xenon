// M1 off-path link stubs (native only). AUTO-GENERATED from the rb3-song
// link's undefined C++ references. NONE are reached by the driver's
// load+query path (SongMgr::Init -> AddSongData -> BandSongMetadata parse
// -> GetShortNameFromSongID / Data / GetSongIDFromShortName / Title / Artist).
// They are pulled in only as vtable slots or as bodies of unexercised
// accessors (upgrade/license/session/profile/rockcentral/saveload paths).
// Functions return 0; the TheXxx manager globals are null pointers, which
// the exercised code null-checks before use.

.text
// TrackTypeToSym(TrackType)
// AllowedToAccessContent(int)
.weak _Z22AllowedToAccessContenti
.type _Z22AllowedToAccessContenti,@function
_Z22AllowedToAccessContenti:
    xorq %rax, %rax
    ret

// LicenseMgr::ClearCachedContent()
.weak _ZN10LicenseMgr18ClearCachedContentEv
.type _ZN10LicenseMgr18ClearCachedContentEv,@function
_ZN10LicenseMgr18ClearCachedContentEv:
    xorq %rax, %rax
    ret

// LicenseMgr::ReadCachedMetadataFromStream(BinStream&, int)
.weak _ZN10LicenseMgr28ReadCachedMetadataFromStreamER9BinStreami
.type _ZN10LicenseMgr28ReadCachedMetadataFromStreamER9BinStreami,@function
_ZN10LicenseMgr28ReadCachedMetadataFromStreamER9BinStreami:
    xorq %rax, %rax
    ret

// LicenseMgr::LicenseMgr()
.weak _ZN10LicenseMgrC1Ev
.type _ZN10LicenseMgrC1Ev,@function
_ZN10LicenseMgrC1Ev:
    xorq %rax, %rax
    ret

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

// LicenseMgr::HasLicense(Symbol) const
.weak _ZNK10LicenseMgr10HasLicenseE6Symbol
.type _ZNK10LicenseMgr10HasLicenseE6Symbol,@function
_ZNK10LicenseMgr10HasLicenseE6Symbol:
    xorq %rax, %rax
    ret

// LicenseMgr::LicenseCacheNeedsWrite() const
.weak _ZNK10LicenseMgr22LicenseCacheNeedsWriteEv
.type _ZNK10LicenseMgr22LicenseCacheNeedsWriteEv,@function
_ZNK10LicenseMgr22LicenseCacheNeedsWriteEv:
    xorq %rax, %rax
    ret

// LicenseMgr::ClearLicenseCacheNeedsWrite()
.weak _ZN10LicenseMgr27ClearLicenseCacheNeedsWriteEv
.type _ZN10LicenseMgr27ClearLicenseCacheNeedsWriteEv,@function
_ZN10LicenseMgr27ClearLicenseCacheNeedsWriteEv:
    xorq %rax, %rax
    ret

// LicenseMgr::WriteCachedMetadataToStream(BinStream&) const
.weak _ZNK10LicenseMgr27WriteCachedMetadataToStreamER9BinStream
.type _ZNK10LicenseMgr27WriteCachedMetadataToStreamER9BinStream,@function
_ZNK10LicenseMgr27WriteCachedMetadataToStreamER9BinStream:
    xorq %rax, %rax
    ret

// BandUserMgr::GetParticipatingBandUsers(std::vector<BandUser*, std::allocator<BandUser*> >&) const
.weak _ZNK11BandUserMgr25GetParticipatingBandUsersERSt6vectorIP8BandUserSaIS2_EE
.type _ZNK11BandUserMgr25GetParticipatingBandUsersERSt6vectorIP8BandUserSaIS2_EE,@function
_ZNK11BandUserMgr25GetParticipatingBandUsersERSt6vectorIP8BandUserSaIS2_EE:
    xorq %rax, %rax
    ret

// BandMachineMgr::IsSongShared(int) const
.weak _ZNK14BandMachineMgr12IsSongSharedEi
.type _ZNK14BandMachineMgr12IsSongSharedEi,@function
_ZNK14BandMachineMgr12IsSongSharedEi:
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

// BandUser::GetControllerSym() const
.weak _ZNK8BandUser16GetControllerSymEv
.type _ZNK8BandUser16GetControllerSymEv,@function
_ZNK8BandUser16GetControllerSymEv:
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

