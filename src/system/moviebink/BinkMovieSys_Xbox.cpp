#include "moviebink/BinkMovieSys.h"
#include "ppcintrinsics.h"
#include "synth_xbox/Synth.h"

extern "C" void BinkSetSoundSystem(void *, unsigned long);
extern "C" void BinkOpenXAudio2();

// Retail is 6 instructions with NO guard on TheXboxSynth -- it loads
// TheXboxSynth, then (TheXboxSynth+0xc8), and TAIL-CALLs BinkSetSoundSystem.
// DC3's copy wraps this in `if (TheXboxSynth)`; that guard is newer than RB3
// and cost 12 bytes here.  (lane DW-3)
void BinkMovieSys::PlatformInit() {
    BinkSetSoundSystem(BinkOpenXAudio2, TheXboxSynth->unkc8);
}

void BinkMovieSys::PlatformStoreCache(void *ptr, unsigned int size) {
    char *addr = (char *)ptr;
    if (size == 0) {
        return;
    }
    unsigned int count = ((size - 1) >> 7) + 1;
    do {
        __dcbst(0, addr);
        addr += 128;
        count--;
    } while (count != 0);
}
