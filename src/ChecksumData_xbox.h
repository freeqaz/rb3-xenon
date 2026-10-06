#pragma once

// Both wrappers live at 0x82272E40 / 0x82272E50 and each hands one table to
// SetFileChecksumData(FileChecksum *, int), which appends it.
// 0x82272E40: the 1,065-entry table of game files (char/gen/*.dtb, ...).
void SetFileChecksumData();
// 0x82272E50: the 62-entry table of on-disc song .mid files. Its retail name
// is not attested anywhere; this one is descriptive.
void SetSongMidiChecksumData();
