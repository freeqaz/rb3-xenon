// The audition song's files on the utility drive: their paths, and the
// directories AuditionMgr makes before a download. Written from the retail XEX
// (TU5). Retail places these three functions at 0x825EC3A0..0x825EC4F8, in an
// unpinned stretch of .text after AccomplishmentGroup.cpp whose unit is not
// identified; no surviving source has them, so this TU and every name in it
// are ours. AuditionMgr.cpp declares what it calls.
#include "os/File.h"
#include "os/System.h"
#include "utl/MakeString.h"
#include "utl/Symbol.h"

/** retail .data 0x82C73C70, a writable pointer to .rdata 0x820B9640
    "cache:/songs": the root the audition song's files live under. Name ours. */
const char *gAuditionSongRoot = "cache:/songs";

/** The files an audition download fetches, as indices into
    AuditionDownloader::mEntries; 7 ends the list. Retail .rdata 0x8219C73C
    holds {0, 1, 2, 3, 7}, among UGCNet's .rdata, which has no source in our
    tree; it is defined here, outside AuditionMgr.cpp, for the reason
    AuditionMgr.cpp gives at its declaration. Name ours. */
extern const int gUGCAuditionFiles[] = { 0, 1, 2, 3, 7 };

// retail 0x825EC3A0
const char *AuditionSongFile(int file, const char *song) {
    switch (file) {
    case 0:
        return MakeString("%s/songs.dta", gAuditionSongRoot);
    case 1:
        return MakeString("%s/%s/%s.mid", gAuditionSongRoot, song, song);
    case 2:
        return MakeString("%s/%s/%s.mogg", gAuditionSongRoot, song, song);
    case 3:
        return MakeString("%s/%s/gen/%s.milo_xbox", gAuditionSongRoot, song, song);
    case 4:
        return MakeString("%s/%s/gen/%s_keep.png_xbox", gAuditionSongRoot, song, song);
    case 5:
        return MakeString("%s/%s/gen/%s_weights.bin", gAuditionSongRoot, song, song);
    default:
        return gNullStr;
    }
}

// retail 0x825EC460
const char *AuditionSongPath(const char *song) {
    return MakeString("%s/%s/%s", gAuditionSongRoot, song, song);
}

// retail 0x825EC480: the root's parent, the root, the song's directory and its
// gen/ directory, in that order.
void PrepareAuditionSongFiles(Symbol song) {
    FileMkDir(FileGetPath(gAuditionSongRoot));
    FileMkDir(gAuditionSongRoot);
    FileMkDir(MakeString("%s\\%s", gAuditionSongRoot, song));
    FileMkDir(MakeString("%s\\%s\\gen", gAuditionSongRoot, song));
}
