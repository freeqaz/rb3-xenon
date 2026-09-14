#include "utl/FakeSongMgr.h"
#include "os/File.h"
#include "utl/SongInfoCopy.h"

FakeSongMgr *TheFakeSongMgr;
DataArray *gSongs;

DataArray *FakeSongMgr::GetSongConfig(Symbol sym) {
    return gSongs->FindArray(sym)->FindArray("song");
}

const char *FakeSongMgr::GetPath(const SongInfo *sinfo, const char *cc) {
    const char *sname = sinfo->GetBaseFileName();
    if (*cc == '\0' || *cc == '.')
        return MakeString("%s%s", sname, cc);
    else {
        return MakeString("%s/%s", FileGetPath(sname), cc);
    }
}

const char *FakeSongMgr::MidiFile(const SongInfo *sinfo) {
    return GetPath(sinfo, ".mid");
}

// M4 added this for native rb3-hit, where SongData::SongFullPath() falls back
// to MidiFullPath when mSongPath is empty, and gated it `#ifdef HX_NATIVE` "so
// the X360 decomp/match build is unaffected".
//
// ⚠ THAT GATE WAS WRONG, and un-gating is the fix rather than a native-only
// nicety: retail HAS this function -- ?MidiFullPath@FakeSongMgr@@SAPBDPBVSongInfo@@@Z,
// 64 B, the only unmatched row in the whole FakeSongMgr unit -- and
// SongData::SongFullPath() at beatmatch/SongData.cpp:1244 calls it
// UNCONDITIONALLY, under no gate at all.  Because the match build compiles but
// never LINKS, that call was simply a dangling UNDEF external that nothing
// could report; the only symptom was the retail row sitting at fuzzy 0 with no
// definition anywhere to pair against.  Same class as DataArray::Release
// (W15-B, 4c35e0f3).  Native behaviour is unchanged: HX_NATIVE builds compiled
// this before and still do.
const char *FakeSongMgr::MidiFullPath(const SongInfo *sinfo) {
    return FileMakePath(FileRoot(), GetPath(sinfo, ".mid"));
}
