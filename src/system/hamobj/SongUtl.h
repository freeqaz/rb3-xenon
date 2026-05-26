#pragma once
#include "obj/Data.h"
#include "obj/PropSync.h"

struct Range {
    Range() : start(0), end(0) {}
    Range &operator=(const Range &r) {
        start = r.start;
        end = r.end;
        return *this;
    }
    int start;
    int end;
};

bool PropSync(Range &, DataNode &, DataArray *, int, PropOp);

float FrameToBeat(float frame);
float BeatToFrame(float beat);
