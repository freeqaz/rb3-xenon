#include "Gem.h"
#include "bandtrack/GemManager.h"
#include "bandtrack/TrackConfig.h"
#include "beatmatch/RGUtl.h"
#include "decomp.h"
#include "game/SongDB.h"
#include "math/Rand.h"
#include "meta_band/ModifierMgr.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "rndobj/Mesh.h"
#include "track/TrackWidget.h"
#include "utl/MBT.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include "utl/TimeConversion.h"

// Retail RB3 compiled this TU's MILO_WARN with arguments EVALUATED for side
// effects (the global sizeof()-form strips arg evaluation). The optimizer then
// DCE'd pure args (Symbol loads, inlined trivial accessors) but kept args with
// un-analyzable side effects — e.g. CreateWidgetInstances' MILO_WARN evaluates
// TickFormat(GetTick(), *TheSongDB->GetData()->GetMeasureMap()) where the
// virtual GetMeasureMap() call survives. Mirror MILO_FAIL's (void)(args) comma
// form per-TU so those args are emitted while the message string vanishes.
#ifndef HX_NATIVE
#undef MILO_WARN
#define MILO_WARN(...) ((void)(__VA_ARGS__))
#endif

Gem::Gem(
    const GameGem &gg, unsigned int ui, float f1, float f2, bool hopo, int beardTick, int i2, bool b2
)
    : mGameGem(gg), mStart(f1), mEnd(f2), mTailStart(0), mSlots(ui), mBeardTick(beardTick),
      mArrhythmicDurationSeconds(0), unk_0x40(0), unk_0x44(0), unk_0x48(0),
      mChordLabel(""), unk_0x58(-1), mFirstFret(-1), mFirstFretString(-1), mFretPos(0),
      mKeyFingerNumber(-1), mHit(0), mMissed(0), mReleased(0), mHopo(hopo), mInvisible(0),
      mBeard(beardTick != -1), mInArrhythmic(0), mIsCymbalLane(0), mIsRepeatChord(0), mInArpeggio(0),
      mSuppressChordLabel(0), mSuppressFretLabel(0), mSlideUp(0) {
    InitChordInfo(i2, b2);
}

Gem::~Gem() { DeleteAll(mTails); }

Gem &Gem::operator=(const Gem &g) {
    (GameGem &)mGameGem = g.mGameGem;
    mSlots = g.mSlots;
    mStart = g.mStart;
    mTailStart = g.mTailStart;
    mEnd = g.mEnd;
    mHopo = g.mHopo;
    mMissed = g.mMissed;
    mHit = g.mHit;
    mReleased = g.mReleased;
    mInvisible = g.mInvisible;
    mTails = g.mTails;
    mWidgets = g.mWidgets;
    mArrhythmicDurationSeconds = g.mArrhythmicDurationSeconds;
    unk_0x58 = g.unk_0x58;
    unk_0x40 = g.unk_0x40;
    mIsCymbalLane = g.mIsCymbalLane;
    unk_0x44 = g.unk_0x44;
    mFirstFret = g.mFirstFret;
    mFirstFretString = g.mFirstFretString;
    mIsRepeatChord = g.mIsRepeatChord;
    mInArpeggio = g.mInArpeggio;
    mKeyFingerNumber = g.mKeyFingerNumber;
    return (Gem &)g;
}

bool Gem::OnScreen(float ms) {
    float bottomSeconds = mGemManager->mTrackDir->BottomSeconds();
    float computed = (ms / 1000.0f) + bottomSeconds;
    return mEnd > computed;
}

void Gem::Poll(float f1, float f2, float f3, float f4, float f5) {
    if (CompareBounds()) {
        float fvar1 = mStart;
        if (f4 > fvar1) {
            float fvar2;
            if (f4 < mEnd)
                fvar2 = f4 - fvar1;
            else
                fvar2 = mEnd - fvar1;
            float fvar3 = mTailStart;
            ApplyDuration(
                mGemManager->mTrackDir->SecondsToY(fvar3),
                mGemManager->mTrackDir->SecondsToY(f5 - fvar1),
                mGemManager->mTrackDir->SecondsToY(fvar2)
            );
            for (int i = 0.0f; i < mTails.size(); i++) {
                mTails[i]->Poll(f1, f2, f3);
            }
        }
    }
}

void Gem::AddRep(
    GemRepTemplate &repTemp, RndGroup *grp, Symbol s, const TrackConfig &cfg, bool b5
) {
    static float startOffset = 1.5f;
    SetType(s);
    if (s != "invisible" && CompareBounds() && b5) {
        float f9 = mGemManager->mTrackDir->SecondsToY(mGameGem.mMs / 1000.0f);
        Tail *i1 = 0;
        if (mTailStart < 0)
            mTailStart = 0;
        bool rg = UseRGChordStyle();
        float f10 = mGemManager->mTrackDir->SecondsToY(mEnd - mStart);
        int i3 = mGemManager->GetMaxSlots();
        for (int i = 0; i < i3; i++) {
            if ((mSlots & 1 << i) && (!rg || mGameGem.GetRGNoteType(i) != 1)) {
                Tail *tail = new Tail(repTemp);
                mTails.push_back(tail);
                mTails.back()->SetDuration(0, 0, f10);
                Tail::SlideInfo info;
                if (mGameGem.LeftHandSlide()) {
                    info.unk0 = true;
                    if (mSlideUp) {
                        info.unk4 = -startOffset;
                        info.unk8 = startOffset;
                    } else {
                        info.unk4 = startOffset;
                        info.unk8 = -startOffset;
                    }
                    info.unkc = f10;
                }
                Transform tf90 = mGemManager->mTrackDir->mSlots[i];
                tf90.v.y = f9;
                tail->Init(i, tf90, rg, s, grp, info, i1);
                if (!i1 && !cfg.AllowsPartialHits())
                    i1 = tail;
            }
        }
    }
}

FORCE_LOCAL_INLINE bool Gem::UseRGChordStyle() const {
    return mGameGem.IsRealGuitarChord() || mInArpeggio || mGameGem.IsMuted();
}
END_FORCE_LOCAL_INLINE

void Gem::RemoveRep() {
    for (int i = 0; i < mTails.size(); i++) {
        delete mTails[i];
    }
    mTails.clear();
}

void Gem::AddInstance(Symbol s1, int i2) {
    if (mGemManager && mGemManager->mTrackDir && mGemManager->SlotEnabled(i2)) {
        const TrackConfig &cfg = mGemManager->mTrackConfig;
        Symbol s1b8;
        Symbol s1bc = s1.Str();
        if (mHopo && !cfg.GetDisableHopos()) {
            s1bc = MakeString("%s_hopo", s1bc.Str());
        } else if (Check66B0()) {
            s1bc = MakeString("%s_cymbal", s1bc.Str());
        }

        if (mInArrhythmic) {
            s1bc = MakeString("%s_arrhythmic", s1bc.Str());
        }

        bool found = mGemManager->GetWidgetName(s1b8, i2, s1bc);
        if (!found)
            found = mGemManager->GetWidgetName(s1b8, i2, s1);
        if (found) {
            if (mGameGem.IsRealGuitar()) {
                int i6 = mFretPos;
                if (cfg.IsLefty())
                    i6 = 4 - i6;
                s1b8 = MakeString("%s%02d.wid", s1b8.Str(), i6);
            }
            TrackWidget *w = mGemManager->GetWidgetByName(s1b8);
            AddWidgetInstanceImpl(w, i2);
        } else
            MILO_WARN(
                "could not find widget for slot %d %s gem in %s",
                i2,
                s1.Str(),
                mGemManager->mTrackDir->Name()
            );

        if (mBeard) {
            static Symbol beard("beard");
            Symbol s1c0;
            if (mGemManager->GetWidgetName(s1c0, i2, beard)) {
                TrackWidget *w2 = mGemManager->GetWidgetByName(s1c0);
                Transform tf68;
                mGemManager->mTrackDir->MakeWidgetXfm(i2, TickToSeconds(mBeardTick), tf68);
                w2->AddInstance(tf68, TickToSeconds(GetBeardThreshold()));
            }
        }

        if (mArrhythmicDurationSeconds != 0) {
            static Symbol mash("mash");
            Symbol s1c4;
            if (mGemManager->GetWidgetName(s1c4, i2, mash)) {
                TrackWidget *w2 = mGemManager->GetWidgetByName(s1c4);
                Transform tf98;
                mGemManager->mTrackDir->MakeWidgetXfm(i2, mGameGem.mMs / 1000.0f, tf98);
                w2->AddInstance(tf98, mArrhythmicDurationSeconds);
            }
        }

        if (mKeyFingerNumber != -1) {
            static Symbol fret_num("fret_num");
            Symbol s1c8;
            if (mGemManager->GetWidgetName(s1c8, i2, fret_num)) {
                TrackWidget *wcc = mGemManager->GetWidgetByName(s1c8);
                Transform tfc8;
                mGemManager->mTrackDir->MakeWidgetXfm(i2, mGameGem.mMs / 1000.0f, tfc8);
                Vector3 v164(0, 0, 0);
                if (cfg.IsLefty()) {
                    v164.x = -v164.x;
                }
                Multiply(v164, tfc8, tfc8.v);
                static Symbol is_white("is_white");
                bool i1 = mGemManager->GetSlotIntData(i2, is_white);
                String str170(1, mKeyFingerNumber + 'A');
                wcc->AddTextInstance(tfc8, str170, !i1);
                mWidgets.insert(wcc);
            }
        }

        if (mGameGem.IsRealGuitar() && mGameGem.GetFret(i2) != -1) {
            static Symbol fret_num("fret_num");
            Symbol s1d0;
            if (mGemManager->GetWidgetName(s1d0, i2, fret_num)) {
                TrackWidget *w1d4 = mGemManager->GetWidgetByName(s1d0);
                Transform tff8;
                mGemManager->mTrackDir->MakeWidgetXfm(i2, mGameGem.mMs / 1000.0f, tff8);
                float offset = mGemManager->mTrackDir->GetFretPosOffset(mFretPos);
                if (cfg.IsLefty())
                    offset = -offset;
                static DataNode &rg_widget_scale = DataVariable("rg_widget_scale");
                float scaleFloat = rg_widget_scale.Float();
                if (scaleFloat > 0)
                    offset *= scaleFloat;
                tff8.v.x += offset;
                if (scaleFloat > 0) {
                    w1d4->SetScale(scaleFloat);
                    Scale(
                        tff8.v,
                        Vector3(1.0f / scaleFloat, 1.0f, 1.0f / scaleFloat),
                        tff8.v
                    );
                }
                String str17c(RGFretNumberToString(mGameGem.GetFret(i2)));
                w1d4->AddTextInstance(tff8, str17c, false);
                mWidgets.insert(w1d4);
            }
        }
    }
}

void Gem::AddChordInstance(Symbol s1) {
    if (mGemManager && mGemManager->mTrackDir) {
        float f1 = mGameGem.mMs / 1000.0f;
        RndMesh *mesh = mGemManager->mTrackDir->GetChordMesh(
            unk_0x44, mGemManager->mTrackConfig.IsLefty()
        );
        TrackWidget *w5c = nullptr;
        TrackWidget *w60 = nullptr;
        TrackWidget *w10 = nullptr;
        Symbol s64;
        static Symbol chord("chord");
        if (mGemManager->GetChordWidgetName(s1, chord, s64)) {
            w5c = mGemManager->GetWidgetByName(s64);
            mWidgets.insert(w5c);
        } else
            MILO_WARN(
                "could not find widget for %s for %s chord gem in %s",
                chord,
                s1,
                mGemManager->mTrackDir->Name()
            );

        if (!mSuppressFretLabel && mFirstFretString != -1) {
            Symbol s68;
            static Symbol chord_fret("chord_fret");
            if (mGemManager->GetChordWidgetName(s1, chord_fret, s68)) {
                w60 = mGemManager->GetWidgetByName(s68);
                mWidgets.insert(w60);
            } else
                MILO_WARN(
                    "could not find widget for %s for %s chord gem in %s",
                    chord_fret,
                    s1,
                    mGemManager->mTrackDir->Name()
                );
        }

        if (!mSuppressChordLabel && !mIsRepeatChord && !mChordLabel.empty()) {
            Symbol s6c;
            static Symbol chord_label("chord_label");
            if (mGemManager->GetChordWidgetName(s1, chord_label, s6c)) {
                w10 = mGemManager->GetWidgetByName(s6c);
            } else
                MILO_WARN(
                    "could not find widget for %s for %s chord gem in %s",
                    chord_label,
                    s1,
                    mGemManager->mTrackDir->Name()
                );
        }

        static std::vector<int> fretNums;
        if (fretNums.empty())
            fretNums.resize(6);

        bool mod = TheModifierMgr->IsModifierActive("mod_chord_numbers");
        bool b2 = mGameGem.ShowChordNums() || mod;

        for (int i = 0; i < 6; i++) {
            if (i == mFirstFretString || b2) {
                fretNums[i] = mGameGem.GetFret(i);
            } else {
                fretNums[i] = -1;
            }
        }
        mGemManager->mTrackDir->AddChordRepImpl(
            mesh, w5c, w60, w10, f1, fretNums, mChordLabel
        );
    }
}

void Gem::AddStrumInstance(Symbol s1, Symbol s2) {
    GemManager * &_ref0 = mGemManager;
    if (_ref0 && _ref0->mTrackDir) {
        int lowString = mGameGem.GetLowestString();
        int highString = mGameGem.GetHighestString();
        MILO_ASSERT(lowString != -1, 572);
        MILO_ASSERT(highString != -1, 573);
        Symbol s88;
        if (!_ref0->GetChordWidgetName(s1, s2, s88)) {
            MILO_WARN(
                "could not find widget for %s for %s chord gem in %s",
                s2,
                s1,
                _ref0->mTrackDir->Name()
            );
        } else {
            int range = (highString - lowString) + 1;
            bool lefty = _ref0->mTrackConfig.IsLefty();
            if (s2 == area_strum) {
                MILO_ASSERT_RANGE_EQ(range, 4, 6, 0x250);
                char u6 = ' ';
                switch (mGameGem.GetRGStrumType()) {
                case 3:
                    u6 = 'L';
                    if (lefty)
                        u6 = 'R';
                    break;
                case 1:
                    u6 = 'M';
                    break;
                case 2:
                    u6 = 'R';
                    if (lefty)
                        u6 = 'L';
                    break;
                default:
                    MILO_FAIL("Invalid RG strum type");
                    break;
                }
                s88 = MakeString("%s_%d%c.wid", s88, range, u6);
            } else if (s2 == muted_strum) {
                MILO_ASSERT_RANGE_EQ(range, 1, 6, 0x261);
                s88 = MakeString("%s_%d.wid", s88, range);
            } else
                MILO_FAIL("Invalid strum type for real guitar strum gem");
            TrackWidget *w8c = _ref0->GetWidgetByName(s88);
            if (lefty)
                lowString = highString;
            Transform tf48;
            _ref0->mTrackDir->MakeWidgetXfm(lowString, mGameGem.mMs / 1000.0f, tf48);
            w8c->AddInstance(tf48, 0);
            mWidgets.insert(w8c);
        }
    }
}

DECOMP_FORCEACTIVE(Gem, "mesh")

void Gem::AddWidgetInstanceImpl(TrackWidget *w, int i) {
    Transform tf38;
    mGemManager->mTrackDir->MakeWidgetXfm(i, mGameGem.mMs / 1000.0f, tf38);
    if (mGemManager->mTrackConfig.IsRealGuitarTrack()) {
        static DataNode &rg_widget_scale = DataVariable("rg_widget_scale");
        float rgFloat = rg_widget_scale.Float();
        if (rgFloat > 0) {
            Scale(tf38.m, Vector3(rgFloat, rgFloat, rgFloat), tf38.m);
        }
    }
    w->AddInstance(tf38, 0);
    mWidgets.insert(w);
}

void Gem::AddHopoTails(Symbol s1) {
    if (!mGemManager || !mGemManager->mTrackDir
        || !mGemManager->mTrackConfig.IsRealGuitarTrack() || !mHopo || mInArrhythmic)
        return;
    else {
        Symbol hopoSym = s1 == miss ? hopo_tail_miss : hopo_tail;
        Symbol s20;
        mGemManager->GetWidgetName(s20, 0, hopoSym);
        TrackWidget *w = mGemManager->GetWidgetByName(s20);
        int slots = mGemManager->GetMaxSlots();
        for (int i = 0; i < slots; i++) {
            if (mSlots & 1 << i) {
                AddWidgetInstanceImpl(w, i);
            }
        }
    }
}

void Gem::RemoveAllInstances() {
    if (mGemManager && mGemManager->mTrackDir) {
        float f1 = mGameGem.mMs / 1000.0f;
        if (unk_0x44 != 0) {
            FOREACH (it, mWidgets) {
                (*it)->RemoveAt(f1);
            }
        } else {
            int slots = mGemManager->GetMaxSlots();
            for (int i = 0; i < slots; i++) {
                if (mSlots & 1 << i) {
                    FOREACH (it, mWidgets) {
                        (*it)->RemoveAt(f1, i);
                    }
                    if (mBeardTick != -1) {
                        static Symbol beard("beard");
                        Symbol s40;
                        if (mGemManager->GetWidgetName(s40, i, beard)) {
                            TrackWidget *w = mGemManager->GetWidgetByName(s40);
                            w->RemoveAt(TickToSeconds(mBeardTick));
                        }
                        if (!mBeard)
                            mBeardTick = -1;
                    }
                }
            }
        }
        mWidgets.clear();
    }
}

void Gem::SetType(Symbol s) {
    mInvisible = s == "invisible";
    RemoveAllInstances();
    if (!mInvisible) {
        bool rg = UseRGChordStyle();
        for (int i = 0; i < mTails.size(); i++) {
            mTails[i]->SetType(s, rg);
        }
        UpdateTailPositions();
        CreateWidgetInstances(s);
    }
}

void Gem::UpdateTailPositions() {
    if (CompareBounds()) {
        for (int i = 0; i < mTails.size(); i++) {
            mTails[i]->MoveSlot(mGemManager->mTrackDir->SlotAt(mTails[i]->mSlot));
        }
    }
}

void Gem::CreateWidgetInstances(Symbol s) {
    if (mGameGem.IsRealGuitar() && mGameGem.IsMuted() && mSlots != 0) {
        static Symbol muted_strum("muted_strum");
        AddStrumInstance(s, muted_strum);
    } else {
        if (mGameGem.IsRealGuitarChord() && mSlots != 0) {
            if (unk_0x44 != 0) {
                AddChordInstance(s);
            } else {
                if (false) {
                    static Symbol _stripped("");
                }
                MILO_WARN(
                    "RG chord gem at %s has only ghost notes; gem will be invisible",
                    TickFormat(mGameGem.GetTick(), *TheSongDB->GetData()->GetMeasureMap())
                );
            }
        } else {
            int slots = mGemManager->GetMaxSlots();
            for (int i = 0; i < slots; i++) {
                if (mSlots & 1 << i) {
                    static DataNode &key_gem_fingering =
                        DataVariable("key_gem_fingering");
                    if (key_gem_fingering.Int()) {
                        mKeyFingerNumber = RandomFloat() * 5.0f + 1.0f;
                    }
                    AddInstance(s, i);
                }
            }
            AddHopoTails(s);
        }
    }
}

void Gem::Miss() {}

void Gem::Hit() {
    for (int i = 0; i < mTails.size(); i++)
        mTails[i]->Hit();
    mBeard = false;
    RemoveAllInstances();
    mHit = true;
}

void Gem::PartialHit(unsigned int ui) {
    if (!mInvisible) {
        mBeard = false;
        float f1 = mGameGem.mMs / 1000.0f;
        int slots = mGemManager->GetMaxSlots();
        FOREACH (it, mWidgets) {
            TrackWidget *cur = *it;
            for (int i = 0; i < slots; i++) {
                int mask = 1 << i;
                if (mSlots & mask && ui & mask) {
                    cur->RemoveAt(f1, i);
                }
            }
        }
        int tailIdx = 0;
        for (int i = 0; i < slots; i++) {
            int mask = 1 << i;
            if (mSlots & mask) {
                if (ui & mask && tailIdx < mTails.size()) {
                    mTails[tailIdx]->Hit();
                }
                tailIdx++;
            }
        }
    }
}

void Gem::Release() {
    for (int i = 0; i < mTails.size(); i++)
        mTails[i]->Release();
    mReleased = true;
}

void Gem::ApplyDuration(float f1, float f2, float f3) {
    for (int i = 0; i < mTails.size(); i++) {
        mTails[i]->SetDuration(f1, f2, f3);
    }
}

void Gem::ReleaseSlot(int slot) {
    if (mTails.empty()) {
        MILO_WARN("ReleaseSlot() called on a gem without tails!");
    } else {
        MILO_ASSERT(slot < mGemManager->GetMaxSlots(), 0x3D2);
        if (mSlots & 1 << slot) {
            int tailIndex = 0;
            for (int i = 0; i < slot; i++) {
                if (mSlots & 1 << i)
                    tailIndex++;
            }
            MILO_ASSERT(tailIndex < mTails.size(), 0x3E0);
            mTails[tailIndex]->Release();
        }
    }
}

void Gem::KillDuration() {
    for (int i = 0; i < mTails.size(); i++)
        mTails[i]->Done();
}

void Gem::Reset() {
    mHit = false;
    mMissed = false;
    mReleased = false;
    mTailStart = 0.0;
    mBeard = false;
    mBeardTick = -1;
}

float Gem::GetStart() const { return mBeard ? TickToSeconds(mBeardTick) : mStart; }

void Gem::InitChordInfo(int i1, bool b2) {
    if (mGameGem.IsRealGuitarChord()) {
        unk_0x44 = RGGetChordShapeID(mGameGem, false);
        unk_0x48 = RGGetChordShapeID(mGameGem, true);
        if (mGameGem.GetShowChordNames()) {
            const char *name = mGameGem.GetChordNameOverride();
            if (name[0] != '\0') {
                char buf[32];
                RGParseOverrideChord(buf, 32, name);
                mChordLabel = buf;
            } else {
                char buf[32];
                RGGetChordName(buf, 32, mGameGem, -1, i1, b2);
                mChordLabel = buf;
            }
        }
        RGGetFretLabelInfo(mGameGem, mFirstFretString, mFirstFret, true);
    }
}

void Gem::SetFretPos(int i) { mFretPos = i; }

void Gem::GetChordFretLabelInfo(String &s, int &i) const {
    s = RGFretNumberToString(mFirstFret);
    i = mFirstFretString;
}

// sw2 scatter-include (default/band3/bandtrack/Gem <- bandobj/OutfitConfig.cpp)
#define gRev gRev_OutfitConfig
#define gAltRev gAltRev_OutfitConfig
#include "bandobj/OutfitConfig.cpp"
#undef gRev
#undef gAltRev

// laneAE scatter force-emit (default/band3/bandtrack/Gem).
// Retail's /Gy COMDAT for
//   ??$__median@PAVAllocInfo@@P6A_NABQAV1@0@Z@stlpmtx_std@@...
// (std::__median with the MemTracker StackLess comparator) landed inside the
// .text span pinned to this unit; our build emits it only from
// system/utl/MemTracker.obj.  objdiff pairs by name within a unit, so the target
// was structurally unpairable.  __median is `inline`, so only an explicit
// instantiation forces the out-of-line COMDAT into this obj.
#ifndef HX_NATIVE
#include "utl/AllocInfo.h"
#include <algorithm>
template AllocInfo *const &stlpmtx_std::__median<
    AllocInfo *, bool (*)(AllocInfo *const &, AllocInfo *const &)>(
    AllocInfo *const &,
    AllocInfo *const &,
    AllocInfo *const &,
    bool (*)(AllocInfo *const &, AllocInfo *const &)
);
#endif
// Lane-AE b2 scatter-include: retail placed GemRepTemplate::GetSlotMat's COMDAT
// (0x40 B -- verified: MILO_ASSERT_RANGE + mSlotMats[+0x38][mat+slot]) inside the
// .text span pinned to default/band3/bandtrack/Gem.
#include "band3/bandtrack/GemRepTemplate.cpp"
