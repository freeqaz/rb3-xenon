#include "beatmatch/VocalNote.h"
#include "os/Debug.h"

void VocalNoteList::UpdatePitchRangeTickDelimited(
    int startTick, int endTick, float &min, float &max
) {
    VocalNote *it = mNotes.begin();
    VocalNote *end = mNotes.end();
    for (; it != end; ++it) {
        if (it->IsUnpitched())
            continue;
        if (it->GetTick() < startTick)
            continue;
        if (endTick > -1 && it->GetTick() > endTick)
            break;
        int startPitch = it->StartPitch();
        if ((float)startPitch < min)
            min = (float)startPitch;
        if ((float)startPitch > max)
            max = (float)startPitch;
        int endPitch = it->EndPitch();
        if ((float)endPitch < min)
            min = (float)endPitch;
        if ((float)endPitch > max)
            max = (float)endPitch;
    }
}

int VocalNoteList::HasNoteInRange(int startTick, int endTick) const {
    for (const VocalNote *it = mNotes.begin(); it != mNotes.end(); ++it) {
        if (!it->IsUnpitched() && it->GetTick() <= endTick
            && it->EndTick() >= startTick) {
            return it->GetTick();
        }
    }
    return -1;
}
