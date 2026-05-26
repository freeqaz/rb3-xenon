#include "char/CharDriverMidi.h"
#include "char/CharDriver.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "utl/Symbol.h"
#include "utl/TimeConversion.h"
#include "math/Utl.h"

CharDriverMidi::CharDriverMidi() : mClipFlags(0), mBlendOverridePct(1.0f) {}

CharDriverMidi::~CharDriverMidi() {}

BEGIN_PROPSYNCS(CharDriverMidi)
    SYNC_PROP(parser, mParser)
    SYNC_PROP(flag_parser, mFlagParser)
    SYNC_PROP(blend_override_pct, mBlendOverridePct)
    SYNC_SUPERCLASS(CharDriver)
END_PROPSYNCS

BEGIN_SAVES(CharDriverMidi)
    SAVE_REVS(7, 0)
    SAVE_SUPERCLASS(CharDriver)
    bs << mParser;
    bs << mFlagParser;
    bs << mBlendOverridePct;
END_SAVES

BEGIN_COPYS(CharDriverMidi)
    COPY_SUPERCLASS(CharDriver)
    CREATE_COPY_AS(CharDriverMidi, c)
    BEGIN_COPYING_MEMBERS_FROM(c)
        COPY_MEMBER(mActive)
        COPY_MEMBER(mParser)
        COPY_MEMBER(mFlagParser)
        COPY_MEMBER(mBlendOverridePct)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(7, 0)

BEGIN_LOADS(CharDriverMidi)
    LOAD_REVS(bs)
    ASSERT_REVS(7, 0)
    LOAD_SUPERCLASS(CharDriver)
    if (d.rev < 7) {
        mDefaultClip.Load(bs, false, mClips);
    }
    if (d.rev == 2) {
        String str;
        d >> str;
    } else if (d.rev > 3)
        d >> mParser;
    if (d.rev > 4)
        d >> mFlagParser;
    if (d.rev > 5)
        d >> mBlendOverridePct;
END_LOADS

void CharDriverMidi::Poll() { CharDriver::Poll(); }

void CharDriverMidi::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    CharDriver::PollDeps(changedBy, change);
}

void CharDriverMidi::Enter() {
    mActive = true;
    CharDriver::Enter();
    Hmx::Object *msgParser =
        Dir()->FindObject(mParser.Str(), true, true);
    if (msgParser)
        msgParser->AddSink(this);
    Hmx::Object *msgFlagParser =
        Dir()->FindObject(mFlagParser.Str(), true, true);
    if (msgFlagParser)
        msgFlagParser->AddSink(this);
}

void CharDriverMidi::Exit() {
    CharDriver::Exit();
    Hmx::Object *msgParser = ObjectDir::Main()->Find<Hmx::Object>(mParser.Str(), false);
    if (msgParser)
        msgParser->RemoveSink(this);
    Hmx::Object *msgFlagParser =
        ObjectDir::Main()->Find<Hmx::Object>(mFlagParser.Str(), false);
    if (msgFlagParser)
        msgFlagParser->RemoveSink(this);
}

DataNode CharDriverMidi::OnMidiParser(DataArray *da) {
    CharClip *clip;
    if (!mActive && mDefaultClip)
        clip = dynamic_cast<CharClip *>(mDefaultClip.Ptr());
    else
        clip = FindClip(da->Node(2), false);
    if (!clip)
        return 0;
    float somefloat = da->Float(3);
    if (clip->PlayFlags() & 0x200) {
        float beat = TheTaskMgr.Beat();
        float bts = BeatToSeconds(beat + somefloat);
        float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime);
        float diff = bts - secs;
        somefloat = diff * clip->AverageBeatsPerSecond();
    }
    MaxEq(somefloat, 0.0f);
    Play(clip, 0, somefloat * mBlendOverridePct, -somefloat, 0.0f);
    return 0;
}

DataNode CharDriverMidi::OnMidiParserFlags(DataArray *da) {
    mClipFlags = da->Int(2);
    return 0;
}

DataNode CharDriverMidi::OnMidiParserGroup(DataArray *da) {
    const char *name = da->Str(2);
    CharClipGroup *grp = mClips->Find<CharClipGroup>(name, false);
    if (!grp) {
        // BUG: grp is NULL here, grp->Name() will crash. Preserved from original.
        MILO_WARN("%s could not find group %s in %s", PathName(this), name, grp->Name());
        return 0;
    } else {
        CharClip *clip;
        if (mActive || !mDefaultClip) {
            clip = grp->GetClip(mClipFlags);
        } else {
            clip = dynamic_cast<CharClip *>(mDefaultClip.Ptr());
        }
        if (!clip) {
            MILO_WARN(
                "%s could not find clip with flags %d in %s",
                PathName(this),
                mClipFlags,
                PathName(grp)
            );
            return 0;
        } else {
            // NOTE: Condition is redundant (clip || clip != X) - first part always succeeds if true
            // Preserved from original code
            if (clip || clip != FirstClip()) {
                float somefloat = da->Float(3);
                if (clip->PlayFlags() & 0x200) {
                    somefloat *= clip->AverageBeatsPerSecond();
                }
                MaxEq(somefloat, 0.0f);
                Play(clip, 0, -somefloat, 1e+30f, 0.0f)->mBlendWidth = somefloat * mBlendOverridePct;
            }
        }
    }
    return 0;
}

BEGIN_HANDLERS(CharDriverMidi)
    HANDLE(midi_parser, OnMidiParser)
    HANDLE(midi_parser_group, OnMidiParserGroup)
    HANDLE(midi_parser_flags, OnMidiParserFlags)
    HANDLE_SUPERCLASS(CharDriver)
END_HANDLERS
