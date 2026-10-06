#pragma once
#include "utl/BinStream.h"
#include "utl/MemStream.h"
#include <vector>

class FileStream;
class OggValidator;

/** Supplies raw mogg bytes to an OggValidator. Retail TU5 RTTI lists it as
 *  OggMap's only base. It declares no destructor: OggMap's virtual dtor takes
 *  vtable slot 1, and a base dtor would add an unwind state to OggMap's ctor
 *  and dtor that retail does not have. */
class OggValidatorFileSource {
public:
    /** Pull up to `bytes` more bytes of the source into the validator's
     *  stream; returns how many were supplied (0 at end of file). */
    virtual int ReadData(int bytes) = 0;
};

/** A collection of ogg samples, meant for quick seeking into an ogg.
 *  TU5 also made it the file source for mogg validation: Validate() opens a
 *  mogg, checks its header and seek table, sets up decryption, and streams the
 *  decrypted ogg through an OggValidator. */
class OggMap : public OggValidatorFileSource {
public:
    OggMap();
    virtual ~OggMap();
    virtual int ReadData(int bytes);

    void Read(BinStream &);
    void GetSeekPos(int, int &, int &);
    int GetSongLengthSamples();

    /** Sets the key used for version-11 moggs from a hex string. */
    static void SetKey(const char *hexKey);
    /** Advances validation of `file`. Returns true once validation has
     *  finished (or could not start); `valid` then holds the verdict. */
    bool Validate(const char *file, bool &valid);

private:
    bool OpenMogg(const char *file);
    bool ReadMap(BinStream &);

    /** Granularity, aka how precise the ogg samples are.
        i.e. if mGran = 1000, the samples are accurate up to the nearest ms. */
    int mGran; // 0x4
    /** Decrypted ogg bytes handed to the validator. */
    MemStream mStream; // 0x8
    FileStream *mFile; // 0x28
    bool mEncrypted; // 0x2c
    OggValidator *mValidator; // 0x30
    /** The LUT of ogg samples.
        pair's first int = the seek position
        pair's second int = the active sample. */
    std::vector<std::pair<int, int> > mLookup; // 0x34
};
