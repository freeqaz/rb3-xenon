// rb3-xenon native — minimal DTA loader.
// Boots the bare minimum of the Milo engine and parses a RB3 songs.dta,
// printing each song's id / name / artist. Proves the native engine can
// load RB3 song data without a renderer or audio device.

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "utl/Symbol.h"
#include <cstdio>
#include <cstdlib>

extern void InitMakeString();

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <songs.dta> [maxSongs]\n", argv[0]);
        return 1;
    }
    int maxSongs = (argc >= 3) ? atoi(argv[2]) : 10;

    InitMakeString();
    Symbol::Init(); // creates the global StringTable used to intern symbols

    DataArray *root = DataReadFile(argv[1], true);
    if (!root) {
        fprintf(stderr, "FAILED to parse %s\n", argv[1]);
        return 1;
    }

    printf("Parsed %s: %d top-level nodes\n", argv[1], root->Size());

    int shown = 0;
    for (int i = 0; i < root->Size() && shown < maxSongs; i++) {
        if (root->Node(i).Type() != kDataArray)
            continue;
        DataArray *song = root->Array(i);
        if (!song || song->Size() < 1 || song->Node(0).Type() != kDataSymbol)
            continue;

        const char *id = song->Sym(0).Str();
        DataArray *nameArr = song->FindArray(Symbol("name"), false);
        DataArray *artistArr = song->FindArray(Symbol("artist"), false);
        const char *name = nameArr ? nameArr->Str(1) : "(none)";
        const char *artist = artistArr ? artistArr->Str(1) : "(none)";
        printf("  [%2d] %-24s \"%s\" by %s\n", i, id, name, artist);
        shown++;
    }
    printf("Done. Showed %d song(s).\n", shown);
    return 0;
}
