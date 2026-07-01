#include "beatmatch/SongParser.h"
#include "beatmatch/GemInfo.h"
#include "beatmatch/TrackType.h"
#include "os/Debug.h"
#include "utl/Str.h"

// Ported from the rb3-Wii MWCC decomp (src/system/beatmatch/SongParser.cpp).
// Only the worklist-identified functions live here — the remainder of the TU
// is still upstream. These three are small, self-contained members with no
// cross-references into the not-yet-ported body.

NoStrumState SongParser::GetNoStrumState(int i, DifficultyInfo &info) {
    if (!mTrackAllowsHopos)
        return kStrumForceOff;
    if (info.mForceHopoOnStart <= i && i < info.mForceHopoOnEnd)
        return kStrumForceOn;
    if (info.mForceHopoOffStart <= i && i < info.mForceHopoOffEnd)
        return kStrumForceOff;
    return kStrumDefault;
}

bool SongParser::CheckDrumFillMarker(int pitch, bool b) {
    int slots = mNumSlots;
    bool ret;
    if (mTrackType == kTrackRealKeys) {
        slots = 5;
    }
    if (pitch < 120 || pitch >= slots + 120) {
        ret = false;
    } else {
        if (b) {
            if (mTrackType == kTrackRealKeys) {
                if (pitch != 120) {
                    MILO_WARN(
                        "%s (%s): Keyboards only use pitch 120 (C8) for BREs, but pitch %d is authored.",
                        mFilename,
                        mTrackName,
                        pitch
                    );
                }
                mCurrentFillLanes = 0x1ffffff;
            } else if (mTrackType == kTrackRealGuitar
                       || mTrackType == kTrackRealGuitar22Fret) {
                mCurrentFillLanes = 0x3F;
            } else {
                mCurrentFillLanes |= 1 << (pitch - 120);
            }
        }
        return true;
    }
    return ret;
}

bool SongParser::IsPartTrackName(const char *cc, const char **ccptr) const {
    if (strneq(cc, "PART", 4)) {
        if (ccptr)
            *ccptr = cc + 5;
        return true;
    } else if (strneq(cc, "HARM", 4)) {
        if (ccptr)
            *ccptr = cc;
        return true;
    } else
        return false;
}
