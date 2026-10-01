#ifndef BEATMATCH_PLAYBACK_H
#define BEATMATCH_PLAYBACK_H
#include "utl/Str.h"

// forward decs
class BeatMatcher;
class DataArray;

class Playback {
public:
    // Inline: retail has no out-of-line ctor. TheBeatMatchPlayback's vptr and
    // scalar members are constant-initialized in .data (0x82C78780) and its
    // dynamic initializer (0x82C40D38) only zeroes the eight sinks.
    Playback() : mPlayerIndex(0), mCommands(0), mCommandIndex(0), mTime(0.0f) {
        for (int i = 0; i < 8; i++)
            mPlayerSinks[i] = 0;
    }
    virtual ~Playback();
    void Poll(float);
    void DoCommand(DataArray *);
    bool LoadFile(const class String &);
    void AddSink(BeatMatcher *);
    void Jump(float);
    int GetPlaybackNum(BeatMatcher *);

    int mPlayerIndex;
    BeatMatcher *mPlayerSinks[8];
    DataArray *mCommands;
    int mCommandIndex;
    float mTime;
};

extern Playback TheBeatMatchPlayback;

#endif
