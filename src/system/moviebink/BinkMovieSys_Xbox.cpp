#include "moviebink/BinkMovieSys.h"
#include "ppcintrinsics.h"
#include "synth_xbox/Synth.h"

extern "C" void BinkSetSoundSystem(void *, unsigned long);
extern "C" void BinkOpenXAudio2();

void BinkMovieSys::PlatformInit() {
    if (TheXboxSynth) {
        BinkSetSoundSystem(BinkOpenXAudio2, TheXboxSynth->unkec);
    }
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
