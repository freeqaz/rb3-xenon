#pragma once
#include "obj/Object.h"
#include "os/ContentMgr.h"
#include "synth/Faders.h"
#include "synth/Stream.h"
#include "utl/Symbol.h"

class ChordPreview : public ContentMgr::Callback, public Hmx::Object {
public:
    class StreamData {
    public:
        void Reset(bool);

        Stream *stream;
        int state;
        Fader *fader;
        bool needReset;
        float startMs;
        float endMs;
    };
    void Start(Symbol);

    StreamData mStreamData[3]; // 0x2c
    Fader *mGuitarFader; // 0x74
    Fader *mSilenceFader; // 0x78
    int mNumChannels; // 0x7c
    float mFadeMs; // 0x80
    Symbol mSong; // 0x84
    Symbol mSongContent; // 0x88
    float mStartMs; // 0x8c
    float mEndMs; // 0x90
    bool mRegisteredWithCM; // 0x94
};