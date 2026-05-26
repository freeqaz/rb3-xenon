#include "Utl.h"
#include "os/File.h"
#include "os/HolmesClient.h"
#include "os/System.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"

// Time values in measures (as fractions of a whole note)
// Corresponds to: sixteenth, eighth, dotted_eighth, quarter, dotted_quarter, half, whole
float measuresMs[7] = {0.0625, 0.125, 0.1875, 0.25, 0.375, 0.5, 1.0};

WavFileCacheHelper gWavFileCacheHelper;

const char *WavFileCacheHelper::CacheFile(const char *file) {
    CacheResourceResult result;
    return CacheWav(file, result);
}

static char sCacheWavBuf[0x100];

const char *CacheWav(const char *file, CacheResourceResult &result) {
    result = (CacheResourceResult)0;
    Platform platform = TheLoadMgr.GetPlatform();
    if (!file || *file == '\0' || platform == kPlatformNone) {
        return nullptr;
    }
    if (platform == kPlatformPC) {
        return file;
    }
    const char *localized = FileLocalize(file, nullptr);
    const char *ext = FileGetExt(localized);
    const char *base = FileGetBase(localized);
    const char *path = FileGetPath(localized);
    char *genPath = (char *)MakeString("%s/gen/%s.%s_%s", path, base, ext, PlatformSymbol(platform));
    // Copy into static buffer
    char *dst = sCacheWavBuf;
    int offset = sCacheWavBuf - genPath;
    char c;
    do {
        c = *genPath;
        genPath[offset] = c;
        genPath++;
    } while (c != '\0');
    if (UsingCD()) {
        return dst;
    }
    String qualifiedName;
    FileQualifiedFilename(qualifiedName, localized);
    CacheResourceResult cacheResult = HolmesClientCacheResource(qualifiedName.c_str(), sCacheWavBuf);
    result = cacheResult;
    if ((int)cacheResult > 0)
        dst = nullptr;
    return dst;
}

void SynthUtlInit() {
    FileCache::RegisterWavCacheHelper(&gWavFileCacheHelper);
}

void SynthUtlTerm() {}

float CalcSpeedFromTranspose(float f1) {
    return std::pow(2.0, f1 * 0.083333333f);
}

float CalcTransposeFromSpeed(float f1) {
    float log = std::log(f1);
    return log * 17.31234f;
}

// Calculate playback rate for tempo-synced effects
// bpm: beats per minute
// sym: musical measure (sixteenth, eighth, quarter, half, whole, etc.)
// Returns: rate multiplier for the given tempo and measure
float CalcRateForTempoSync(Symbol sym, float bpm) {
    static Symbol measures[7] = {"sixteenth", "eighth", "dotted_eighth",
        "quarter", "dotted_quarter", "half", "whole"};

    float measureValue;
    float bpmInMs;
    unsigned int i;

    // Convert BPM to milliseconds (1/60 = 0.016666667)
    bpmInMs = bpm * 0.016666667f;
    measureValue = 1.0;
    for (i = 0; i < 7; i++) {
        if (measures[i] == sym) {
            measureValue = measuresMs[i];
            break;
        }
    }
    return bpmInMs / measureValue;
}
