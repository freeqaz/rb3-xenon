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
