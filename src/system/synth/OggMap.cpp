#include "synth/OggMap.h"
#include "KeyChain.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "os/Debug.h"
#include "synth/ByteGrinder.h"
#include "synth/tomcrypt/mycrypt.h"
#include "utl/FileStream.h"
#include "utl/Str.h"
#include <stdio.h>
#include <string.h>

/** Streams a mogg's decrypted ogg data and checks it page by page. */
class OggValidator {
public:
    static OggValidator *Create(BinStream *, OggValidatorFileSource *);
    /** Returns nonzero once the whole file has been checked. */
    int Process();
    /** Frees the validator; returns 0 if the file was valid. */
    int Finish();
};

// Mogg decryption state shared by every OggMap (one validation runs at a time).
// MSVC lays these file statics out in REVERSE declaration order, so they are
// declared highest address first; retail's order (offsets from gKey) is noted.
static int gKeySize = -1;
static int gKeyIndex; // 0x368
static int gMagicB; // 0x364
static int gMagicA; // 0x360
static int gUnused35C; // 0x35c, see OggMapReserveUnusedStatics
static int gMagicHashB; // 0x358
static bool gDecrypt; // 0x355
static bool gUnused354; // 0x354, see OggMapReserveUnusedStatics
static int gMagicHashA; // 0x350
static unsigned char gKeyMask[16]; // 0x340
static symmetric_CTR gCtr; // 0x30
static unsigned char gNonce[16]; // 0x20
static unsigned char gKey[32]; // 0x0

// Retail reserves a byte at gKey+0x354 and a word at gKey+0x35c that no
// surviving function in this TU reads or writes: whatever referenced them was
// discarded by the linker as unreferenced, as GetSongLengthSamples was. MSVC
// drops a static that no emitted code touches (a `(void)` reference is not
// enough), which would pack gDecrypt to 0x354 and shift gMagicA/B/gKeyIndex.
// This never-called function keeps both slots; it is NOT reconstructed retail
// code.
void OggMapReserveUnusedStatics() {
    gUnused354 = false;
    gUnused35C = 0;
}

int OggMap::GetSongLengthSamples() { return mGran * mLookup.size(); }

void OggMap::SetKey(const char *hexKey) {
    int len = strlen(hexKey);
    gKeySize = len / 2;
    String key(hexKey);
    int keyLen = strlen(key.c_str());
    int numBytes = keyLen / 2;
    for (int i = 0; i < numBytes; i++) {
        String byteStr = key.substr(i * 2, 2);
        int byte;
        sscanf(byteStr.c_str(), "%x", &byte);
        gKey[i] = byte;
    }
}

void OggMap::GetSeekPos(int sampTarget, int &seekPos, int &actSamp) {
    // retail leaves both outputs untouched when there is no lookup table
    if (!mLookup.empty()) {
        int idx = sampTarget / mGran;
        int maxLookupIdx = mLookup.size() - 1;
        if (idx < 0)
            idx = 0;
        else if (idx > maxLookupIdx)
            idx = maxLookupIdx;
        seekPos = mLookup[idx].first;
        actSamp = mLookup[idx].second;
    }
}

// Derives the version-12+ AES key and the two page-header magic hashes.
static void SetupCypher(int version) {
    char script[256];
    unsigned char masterKey[256];
    int cipher = register_cipher(&rijndael_desc);
    DataArray *arr = DataReadString("{Na 42 'O32'}");
    unsigned int iEval = arr->Evaluate(0).Int();
    arr->Release();

    char i6 = (iEval % 13);
    int masterKeyArg = (int)masterKey ^ iEval;
    i6 = i6 + 'A';
    sprintf(script, "{%c %d %c}", i6, masterKeyArg, i6);
    DataArray *scriptArr = DataReadString(script);
    scriptArr->Evaluate(0);
    scriptArr->Release();

    KeyChain::getKey(gKeyIndex, gKey, masterKey);
    ByteGrinder grinder;
    grinder.Init();
    grinder.GrindArray(gMagicA, gMagicB, gKey, 0x10, version);
    for (int i = 0; i < 16; i++) {
        gKey[i] ^= gKeyMask[i];
    }
    ctr_start(cipher, gNonce, gKey, gKeySize, 0, &gCtr);
    memset(gKey, 0, gKeySize);

    sprintf(script, "{ha %d 1}", gMagicA);
    DataArray *magicGenA = DataReadString(script);
    gMagicHashA = magicGenA->Evaluate(0).Int();
    magicGenA->Release();

    sprintf(script, "{ha %d 2}", gMagicB);
    DataArray *magicGenB = DataReadString(script);
    gMagicHashB = magicGenB->Evaluate(0).Int();
    magicGenB->Release();
}

int OggMap::ReadData(int bytes) {
    unsigned char out[0x2580];
    unsigned char in[0x2580];
    if (mFile->Fail() || mFile->Eof()) {
        return 0;
    }
    int left = mFile->Size() - mFile->Tell();
    if (bytes >= left) {
        bytes = left;
    }
    if (bytes == 0) {
        return 0;
    }
    mFile->Read(in, bytes);
    if (mFile->Fail()) {
        return 0;
    }
    if (mStream.Size() != 0) {
        mStream.Seek(0, BinStream::kSeekEnd);
        mStream.Compact();
    }
    if (gDecrypt) {
        ctr_decrypt(in, out, bytes, &gCtr);
        if ((gMagicHashA != 0 || gMagicHashB != 0) && out[0] == 'H' && out[1] == 'M'
            && out[2] == 'X' && out[3] == 'A') {
            out[2] = 'g';
            out[0] = 'O';
            out[1] = 'g';
            out[3] = 'S';
            if (bytes >= 16) {
                *(unsigned int *)&out[12] ^= gMagicHashA;
            }
            if (bytes >= 24) {
                *(unsigned int *)&out[20] ^= gMagicHashB;
            }
        }
        mStream.Write(out, bytes);
    } else {
        mStream.Write(in, bytes);
    }
    mStream.Seek(0, BinStream::kSeekBegin);
    return bytes;
}

OggMap::~OggMap() { mLookup.clear(); }

OggMap::OggMap() : mGran(1000), mStream(false), mFile(0), mValidator(0), mLookup() {
    mLookup.push_back(std::pair<int, int>(0, 0));
}

void OggMap::Read(BinStream &bs) {
    int version;
    bs >> version;
    if (version < 0xb)
        MILO_FAIL("Incorrect oggmap version.");
    bs >> mGran >> mLookup;
}

// Reads the seek table out of a mogg header, rejecting anything malformed.
bool OggMap::ReadMap(BinStream &bs) {
    if (bs.Fail() || bs.Eof()) {
        return false;
    }
    int version;
    bs >> version;
    if (version < 0xb) {
        return false;
    }
    if (bs.Fail() || bs.Eof()) {
        return false;
    }
    bs >> mGran;
    if (mGran != 20000) {
        return false;
    }
    if (bs.Fail() || bs.Eof()) {
        return false;
    }
    unsigned int count;
    bs >> count;
    if (count == 0 || count > 10000) {
        return false;
    }
    mLookup.resize(count, std::pair<int, int>(0, 0));
    for (std::vector<std::pair<int, int> >::iterator it = mLookup.begin();
         it != mLookup.end();
         ++it) {
        if (bs.Fail() || bs.Eof()) {
            return false;
        }
        bs >> *it;
    }
    int prevPos = -1;
    int prevSamp = -1;
    for (unsigned int i = 0; i < mLookup.size(); i++) {
        if (mLookup[i].first < prevPos) {
            return false;
        }
        if (mLookup[i].second < prevSamp) {
            return false;
        }
        prevPos = mLookup[i].first;
        prevSamp = mLookup[i].second;
    }
    return true;
}

#define OGGMAP_CHECK_FILE()                                                              \
    if (mFile->Fail() || mFile->Eof()) {                                                 \
        delete mFile;                                                                    \
        mFile = nullptr;                                                                 \
        return false;                                                                    \
    }

// Opens a mogg, validates its header and seek table, and primes decryption.
bool OggMap::OpenMogg(const char *file) {
    mFile = new FileStream(file, FileStream::kRead, true);
    if (!mFile) {
        return false;
    }
    int version;
    *mFile >> version;
    if (version < 10 || version > 15) {
        delete mFile;
        mFile = nullptr;
        return false;
    }
    mEncrypted = version >= 11;
    gDecrypt = false;
    int hdrSize;
    *mFile >> hdrSize;
    if (!ReadMap(*mFile)) {
        delete mFile;
        mFile = nullptr;
        return false;
    }
    if (mEncrypted) {
        unsigned char buf[16];
        gMagicHashA = 0;
        gMagicHashB = 0;
        gDecrypt = true;
        if (version == 11) {
            mFile->Read(buf, 16);
            OGGMAP_CHECK_FILE();
            int cipher = register_cipher(&rijndael_desc);
            ctr_start(cipher, buf, gKey, gKeySize, 0, &gCtr);
        } else if (version >= 12 && version <= 15) {
            mFile->Read(gNonce, 16);
            OGGMAP_CHECK_FILE();
            long long magicA;
            *mFile >> magicA;
            gMagicA = magicA;
            OGGMAP_CHECK_FILE();
            long long magicB;
            *mFile >> magicB;
            gMagicB = magicB;
            OGGMAP_CHECK_FILE();
            mFile->Read(buf, 16);
            OGGMAP_CHECK_FILE();
            mFile->Read(buf, 16);
            OGGMAP_CHECK_FILE();
            long long keyIndex;
            *mFile >> keyIndex;
            gKeyIndex = keyIndex;
            OGGMAP_CHECK_FILE();
            gKeyIndex = gKeyIndex % 6 + 6;
            ByteGrinder grinder;
            grinder.Init();
            grinder.HvDecrypt(buf, gKeyMask, version);
            SetupCypher(version);
        }
    }
    return true;
}

bool OggMap::Validate(const char *file, bool &valid) {
    valid = false;
    if (!mValidator && OpenMogg(file)) {
        mValidator = OggValidator::Create(&mStream, this);
        if (!mValidator) {
            return true;
        }
    }
    if (mValidator->Process()) {
        valid = mValidator->Finish() == 0;
        mValidator = nullptr;
        delete mFile;
        mFile = nullptr;
        return true;
    }
    return false;
}
