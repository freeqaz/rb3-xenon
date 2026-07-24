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

#ifdef HX_NATIVE
// M4: the real SongData::SongFullPath() (native rb3-hit) falls back to
// MidiFullPath when mSongPath is empty. Our tree's FakeSongMgr is a slimmed
// native reimplementation that lacked this; add it (adapted to our File.h's
// 2-arg FileMakePath). Gated so the X360 decomp/match build is unaffected.
const char *FakeSongMgr::MidiFullPath(const SongInfo *sinfo) {
    return FileMakePath(FileRoot(), GetPath(sinfo, ".mid"));
}
#endif
