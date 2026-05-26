#include "utl/GlitchFinder.h"
#include "obj/DataFunc.h"
#include "os/Debug.h"
#include "utl/MakeString.h"

DataNode GlitchFindScriptImpl(DataArray *arr, int iii);

GlitchFinder TheGlitchFinder;
std::vector<float> GlitchPoker::smNestedStartTimes;
#ifdef HX_NATIVE
float GlitchPoker::smThreshold;
bool GlitchPoker::smDumpLeaves;
float GlitchPoker::smLastDumpTime;
float GlitchPoker::smTotalLeafTime;
#endif

GlitchPoker::GlitchPoker() {}

GlitchPoker::~GlitchPoker() {}

void GlitchPoker::ClearData() {
    mTime = -1.0;
    mName[0] = '\0';
    mTimeEnd = -1.0;
    mChildren.clear();
    mBudget = -1.0;
    mParent = 0;
    mAvg = 0;
}

bool GlitchPoker::OverBudget() {
    if (mBudget > 0.0 && mTimeEnd - mTime > mBudget) {
        return true;
    }
    for (int i = 0; i < mChildren.size(); i++) {
        if (mChildren[i]->OverBudget()) {
            return true;
        }
    }
    return false;
}

void GlitchPoker::PrintResult(TextStream &stream) {
    float temp = mTimeEnd - mTime;
    if (mChildren.size() > 0 || temp >= 0.0049999999f) {
        stream << "{ " << mName << " (" << temp << ") ";
    } else {
        stream << "[ " << mName << " ] ";
    }
    if (mAvg) {
        stream << "<" << mAvg->mAvg << " avg, " << mAvg->mGlitchAvg << " glitch avg, "
               << mAvg->mMax << " max> ";
    }
}

void GlitchPoker::PollAveragesRecurse(bool b) {
    if (mAvg) {
        mAvg->PushInstance(mTimeEnd - mTime, b);
    }
    for (int i = 0; i < mChildren.size(); i++) {
        mChildren[i]->PollAveragesRecurse(b);
    }
}

void GlitchPoker::PrintNestedStartTimes(TextStream &stream, float f1) {
    if (!smDumpLeaves) {
        stream << f1 << " ";
        if (smNestedStartTimes.size() != 0) {
            for (int i = 0; i < smNestedStartTimes.size(); i++) {
                stream << " " << f1 - smNestedStartTimes[i] << " ";
            }
        }
    }
}

void GlitchPoker::Dump(TextStream &stream, int i1) {
    float f1 = 0.0049999999f;
    if (mTime > smLastDumpTime + 0.005f) {
        PrintNestedStartTimes(stream, smLastDumpTime);
        float timeDelta = mTime - smLastDumpTime;
        if (!smDumpLeaves) {
            stream << "TIME GAP (" << timeDelta << ")\n";
        } else if (timeDelta > smThreshold) {
            stream << "   TIME GAP (" << timeDelta << ") before " << mName;
            for (GlitchPoker *p = mParent; p; p = p->mParent) {
                stream << " : " << p->mName;
            }
            stream << "\n";
        }
        smTotalLeafTime = (mTime - smLastDumpTime) + smTotalLeafTime;
    }
    PrintNestedStartTimes(stream, mTime);
    if (!smDumpLeaves && mChildren.size() == 0 && mTimeEnd - mTime < f1) {
        stream << "[ " << mName << " ]";
        if (mAvg) {
            stream << " (" << mAvg->mAvg << " avg)";
        }
        stream << "\n";
        smLastDumpTime = mTimeEnd;
        return;
    }
    if (mParent) {
        f1 = smThreshold;
        if (mTimeEnd - mTime > f1) {
            if (mChildren.size() != 0) {
                float temp_f30 = smLastDumpTime;
                smLastDumpTime = mTime;
                for (int i = 0; i < mChildren.size(); i++) {
                    mChildren[i]->Dump(stream, i1 + 1);
                }
                if (mTimeEnd - smLastDumpTime > f1) {
                    stream << "   TIME GAP (" << mTimeEnd - smLastDumpTime
                           << ") at end of " << mName;
                    for (GlitchPoker *p = mParent; p; p = p->mParent) {
                        stream << " : " << p->mName;
                    }
                    stream << "\n";
                    smTotalLeafTime = (mTimeEnd - smLastDumpTime) + smTotalLeafTime;
                }
                smLastDumpTime = temp_f30;
            } else {
                stream << "   ";
                PrintResult(stream);
                stream << "}";
                for (GlitchPoker *p = mParent; p; p = p->mParent) {
                    stream << " : " << p->mName;
                }
                stream << "\n";
                smTotalLeafTime = (mTimeEnd - mTime) + smTotalLeafTime;
            }
            smNestedStartTimes.pop_back();
            PrintNestedStartTimes(stream, mTimeEnd);
        }
        return;
    }
    PrintResult(stream);
    if (mChildren.size() != 0) {
        stream << "\n";
    }
    float savedLastDump = smLastDumpTime;
    smLastDumpTime = mTime;
    smNestedStartTimes.push_back(mTime);
    for (int i = 0; i < mChildren.size(); i++) {
        mChildren[i]->Dump(stream, i1 + 1);
    }
    if (mTimeEnd > smLastDumpTime + f1) {
        PrintNestedStartTimes(stream, smLastDumpTime);
        if (!smDumpLeaves) {
            stream << "TIME GAP (" << mTimeEnd - smLastDumpTime << ")\n";
        } else if (mTime - smLastDumpTime > smThreshold) {
            stream << "   TIME GAP (" << mTimeEnd - smLastDumpTime << ") at end of "
                   << mName;
            for (GlitchPoker *p = mParent; p; p = p->mParent) {
                stream << " : " << p->mName;
            }
            stream << "\n";
        }
    }
    smLastDumpTime = savedLastDump;
    smNestedStartTimes.pop_back();
    PrintNestedStartTimes(stream, mTimeEnd);
    if (smDumpLeaves) {
        float totalTime = smTotalLeafTime;
        float pct = totalTime / (mTimeEnd - mTime);
        stream << "{ total leaf time: " << totalTime << " (" << pct * 100.0f << "pct)\n";
    } else if (mChildren.size() != 0 || !(mTimeEnd - mTime < f1)) {
        stream << "}" << "\n";
        smLastDumpTime = mTimeEnd;
    }
}

GlitchAverager::GlitchAverager()
    : mAvg(0.0), mMax(0.0), mCount(0), mGlitchAvg(0.0), mGlitchCount(0) {}

void GlitchAverager::PushInstance(float f1, bool b) {
    mAvg = (f1 - mAvg) / ++mCount + mAvg;
    if (b) {
        mGlitchAvg = (f1 - mGlitchAvg) / ++mGlitchCount + mGlitchAvg;
    }
    if (f1 > mMax) {
        mMax = f1;
    }
}

GlitchFinder::GlitchFinder()
    : mFrameCount(0), mGlitchCount(0), mStop(true), mLastTime(0.0), mPokerIndex(-1),
      mStartPoker(0), mCurPoker(0), mActive(true), mDumpLeavesOnly(false),
      mLeafThreshold(0.0), mOverheadCycles(0) {
    mTime.Start();
}

GlitchFinder::~GlitchFinder() {
}

void GlitchFinder::Init() {
    DataRegisterFunc("glitch_find", OnGlitchFind);
    DataRegisterFunc("glitch_find_budget", OnGlitchFindBudget);
    DataRegisterFunc("glitch_find_leaves", OnGlitchFindLeaves);
    DataRegisterFunc("glitch_find_poke", OnGlitchFindPoke);
}

DataNode GlitchFinder::OnGlitchFindPoke(DataArray *da) {
    unsigned int mftb = __mftb();
    TheGlitchFinder.Poke(da->Str(1), mftb);
    return 0;
}

DataNode GlitchFinder::OnGlitchFind(DataArray *da) { return GlitchFindScriptImpl(da, 3); }

DataNode GlitchFinder::OnGlitchFindBudget(DataArray *da) {
    return GlitchFindScriptImpl(da, 4);
}

DataNode GlitchFinder::OnGlitchFindLeaves(DataArray *da) {
    return GlitchFindScriptImpl(da, 5);
}

void GlitchFinder::Poke(const char *c, unsigned int ui) {
    PokeStart(c, 0, -1, 0, 0);
    PokeEnd(ui);
}

GlitchPoker *GlitchFinder::NewPoker() {
    if (2048 <= mPokerIndex) {
        MILO_FAIL("too many glitch pokers : %d\n", mPokerIndex);
    }
    GlitchPoker *thePoker = &mPokerPool[mPokerIndex++];
    thePoker->ClearData();
    return thePoker;
}

void GlitchFinder::PokeStart(
    const char *c, unsigned int ui, float f1, float f2, GlitchAverager *avg
) {
    if (!mStartPoker && f1 < 0.0)
        return;
    else {
        if (mStop) {
            mStop = 0;
            mTime.Restart();
            mCurPoker = 0;
            mStartPoker = 0;
            mPokerIndex = 0;
        }
        GlitchPoker *poker = NewPoker();
        strncpy(poker->mName, c, 0x40);
        poker->mTime = mTime.SplitMs();
        poker->mParent = mCurPoker;
        poker->mBudget = f1;
        poker->mAvg = avg;
        if (!mStartPoker) {
            mCurPoker = poker;
            mStartPoker = poker;
            mLeafThreshold = f2;
            mOverheadCycles = 0;
        } else {
            mCurPoker->mChildren.push_back(poker);
            mCurPoker = poker;
            if (ui != 0) {
                unsigned int cycles = __mftb();
                mOverheadCycles += cycles - ui;
            }
        }
    }
}

void GlitchFinder::PokeEnd(unsigned int ui) {
    if (mCurPoker) {
        mCurPoker->mTimeEnd = mTime.SplitMs();
        mCurPoker = mCurPoker->mParent;
        if (!mCurPoker) {
            CheckDump();
        }
    }
    if (ui) {
        unsigned int mftb = __mftb();
        mOverheadCycles = mOverheadCycles - ui + mftb;
    }
}

AutoGlitchPoker::~AutoGlitchPoker() {
    if (mActive) {
        unsigned int time = __mftb();
        TheGlitchFinder.PokeEnd(time);
    }
}

DataNode GlitchFindScriptImpl(DataArray *arr, int iii) {
    unsigned int cycles = __mftb();
    DataNode result;
    if (arr->Node(2).NotNull()) {
        switch (iii) {
        case 3:
            TheGlitchFinder.PokeStart(arr->Str(1), cycles, -1.0f, 0.0f, 0);
            break;
        case 4:
            TheGlitchFinder.PokeStart(arr->Str(1), cycles, arr->Float(3), 0.0f, 0);
            break;
        case 5:
            TheGlitchFinder.PokeStart(arr->Str(1), cycles, arr->Float(3), arr->Float(4), 0);
            break;
        default:
            MILO_FAIL("improper use of internal glitch finder code");
            break;
        }
        for (int i = iii; i < arr->Size(); i++) {
            result = arr->Command(i)->Execute();
        }
        unsigned int now_cycles = __mftb();
        TheGlitchFinder.PokeEnd(now_cycles);
        return DataNode(result);
    } else {
        unsigned int now_cycles = __mftb();
        TheGlitchFinder.mOverheadCycles = TheGlitchFinder.mOverheadCycles - cycles + now_cycles;
        for (int i = iii; i < arr->Size(); i++) {
            result = arr->Command(i)->Execute();
        }
        return DataNode(result);
    }
}

void GlitchFinder::CheckDump() {
    if (mStop)
        return;
    if (!mStartPoker)
        return;

    mStop = true;
    mStartPoker->mTimeEnd = mTime.SplitMs();

    static unsigned int sStart;
    if (sStart == 0) {
        sStart = __mftb();
    }

    bool overBudget = mActive && mStartPoker->OverBudget();

    mStartPoker->PollAveragesRecurse(overBudget);

    if (overBudget) {
        GlitchPoker::smDumpLeaves = mLeafThreshold > 0.0f;
        GlitchPoker::smThreshold = mLeafThreshold;
        GlitchPoker::smTotalLeafTime = 0;

        String str(0x2000, '\0');
        str << "-------- GLITCH #" << mGlitchCount << " -------- Frame " << mFrameCount
            << " -----\n";

        GlitchPoker::smLastDumpTime = mStartPoker->mTime;
        mStartPoker->Dump(str, 0);

        str << "Overhead: " << Timer::CyclesToMs(mOverheadCycles) << "\n";
        str << "-------- GLITCH END --------\n";

        int strLen = strlen(str.c_str());
        if (strLen > 0x400) {
            char buf[1024];
            int i = 0;
            for (; i + 0x400 < strLen; i += 0x400) {
                strncpy(buf, str.c_str() + i, 0x400);
                buf[0x400] = '\0';
                FormatString fmt(buf);
                TheDebug << fmt.Str();
            }
            strncpy(buf, str.c_str() + i, strLen - i);
            buf[strLen - i] = '\0';
            {
                FormatString fmt(buf);
                TheDebug << fmt.Str();
            }
        } else {
            FormatString fmt(str.c_str());
            TheDebug << fmt.Str();
        }

        mGlitchCount++;
    }

    if (mActive) {
        mFrameCount++;
    }

    mStartPoker = 0;
    mPokerIndex = 0;
    mCurPoker = 0;
}
