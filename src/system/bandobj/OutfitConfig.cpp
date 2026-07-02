// Ported from rb3-Wii src/system/bandobj/OutfitConfig.cpp (MWCC -> MSVC X360).
#include "bandobj/OutfitConfig.h"
#include "macros.h"
#include "rndobj/Cam.h"

unsigned short OutfitConfig::gRev;
unsigned short OutfitConfig::gAltRev;

RndMat *OutfitConfig::sMat;
RndCam *OutfitConfig::sCam;
BandCharDesc *OutfitConfig::sBandCharDesc;

void OutfitConfig::Terminate() {
    RELEASE(sMat);
    RELEASE(sCam);
    RELEASE(sBandCharDesc);
}

void OutfitConfig::CompressTextures() {
    if (unk3c != 2)
        unk3c = 1;
}
