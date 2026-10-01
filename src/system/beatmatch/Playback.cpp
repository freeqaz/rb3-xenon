#include "beatmatch/Playback.h"
#include "beatmatch/BeatMatcher.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "os/Debug.h"

// Retail Playback.cpp is its own TU: .text 0x82792538-0x82792D50, between
// FillInfo.cpp and DrumMixDB.cpp. Vtable 0x8210F5EC.

Playback TheBeatMatchPlayback;

// 0x82792B38
Playback::~Playback() {
    if (mCommands) {
        mCommands->Release();
        mCommands = 0;
    }
}

// 0x82792B88
void Playback::Poll(float f) {
    if (mCommands) {
        int cmdSize = mCommands->Size();
        if (mCommandIndex < cmdSize) {
            DataArray *arr = mCommands->Array(mCommandIndex);
            float floc = 0.0f;
            if (1 < arr->Size()) {
                DataNode &node = arr->Node(1);
                if (node.Type() == kDataFloat) {
                    floc = node.Float();
                }
            }
            while (floc <= mTime
                   || mTime < f && (std::fabs(floc - mTime) < std::fabs(floc - f))) {
                DoCommand(arr);
                mCommandIndex++;
                if (mCommandIndex >= cmdSize)
                    break;
                arr = mCommands->Array(mCommandIndex);
                if (1 < arr->Size()) {
                    DataNode &node = arr->Node(1);
                    if (node.Type() == kDataFloat) {
                        floc = node.Float();
                    }
                }
            }
            mTime = f;
        }
    }
}

// 0x827925A8. The command symbols are function-local statics (one guard word,
// 0x82E06484, one bit each, constructed before the compare chain); retail has
// no player-range assert.
void Playback::DoCommand(DataArray *arr) {
    if (arr->Size() >= 4) {
        BeatMatcher *sink = mPlayerSinks[arr->Int(0)];
        if (sink) {
            static Symbol SWING("SWING");
            static Symbol UP("UP");
            static Symbol DOWN("DOWN");
            static Symbol TRACK("TRACK");
            static Symbol HOPO("HOPO");
            static Symbol FLIP("FLIP");
            static Symbol FFLIP("FFLIP");
            Symbol sym = arr->Sym(2);
            if (sym == SWING) {
                sink->Swing(
                    arr->Int(3),
                    arr->Int(4) != 0,
                    true,
                    false,
                    arr->Int(4) != 0,
                    kGemHitFlagNone
                );
            } else if (sym == UP) {
                sink->FretButtonUp(arr->Int(3));
            } else if (sym == DOWN) {
                sink->FretButtonDown(arr->Int(3), -1);
            } else if (sym == TRACK) {
                sink->SetTrack(arr->Int(3));
            } else if (sym == HOPO) {
                sink->NonStrumSwing(arr->Int(3), arr->Int(4) != 0, false);
            } else if (sym == FLIP) {
                sink->MercurySwitch(arr->Float(3));
            } else if (sym == FFLIP) {
                sink->ForceMercurySwitch(arr->Int(3) != 0);
            }
        }
    }
}

// 0x82792C90
bool Playback::LoadFile(const String &str) {
    if (mCommands)
        mCommands->Release();
    mCommands = 0;
    mCommandIndex = 0;
    mCommands = DataReadFile(str.c_str(), true);
    return mCommands != 0;
}

// 0x82792538 (dtk carves the tail from 0x82792564)
void Playback::AddSink(BeatMatcher *bm) {
    for (int i = 0; i < 8; i++) {
        if (mPlayerSinks[i] == bm) {
            mPlayerSinks[i] = 0;
            break;
        }
    }
    mPlayerSinks[mPlayerIndex++] = bm;
    if (mPlayerIndex < 8)
        return;
    mPlayerIndex = 0;
}

// 0x82792A18. The scan indexes with mCommandIndex, which is never advanced.
void Playback::Jump(float f) {
    mCommandIndex = 0;
    mTime = f;
    if (mCommands) {
        for (int i = 0; i < mCommands->Size(); i++) {
            DataArray *arr = mCommands->Array(mCommandIndex);
            if (1 < arr->Size()) {
                DataNode &node = arr->Node(1);
                if (node.Type() == kDataFloat) {
                    if (node.Float() > f) {
                        mCommandIndex = Max(0, mCommandIndex - 1);
                        return;
                    }
                }
            }
        }
    }
}

// 0x82792B00: returns -1 with no failure report.
int Playback::GetPlaybackNum(BeatMatcher *bm) {
    for (int i = 0; i < 8; i++) {
        if (mPlayerSinks[i] == bm)
            return i;
    }
    return -1;
}
