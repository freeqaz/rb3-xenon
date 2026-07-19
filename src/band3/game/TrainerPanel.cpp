#include "game/TrainerPanel.h"
#include "TrainerPanel.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Game.h"
#include "game/GameMode.h"
#include "game/SongDB.h"
#include "beatmatch/BeatMaster.h"
#include "midi/MidiParser.h"
#include "midi/MidiParserMgr.h"
#include "game/TrainerProgressMeter.h"
#include "meta_band/BandProfile.h"
#include "meta_band/ProfileMgr.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "utl/DataPointMgr.h"
#include "utl/Messages.h"
#include "utl/Messages3.h"
#include "utl/Messages4.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "midi/DataEvent.h"
#include "obj/Data.h"
#include <map>

TrainerPanel *TheTrainerPanel;

TrainerPanel::TrainerPanel()
    : mProgressMeter(0), mCurrSection(-1), mShowProgressMeter(1) {
    mProgressMeter = new TrainerProgressMeter();
}

TrainerPanel::~TrainerPanel() { delete mProgressMeter; }

void TrainerPanel::Enter() {
    UIPanel::Enter();
    InitSections();
    mCurrSection = -1;
    TheTrainerPanel = this;
    RndDir *progressMeter = DataDir()->Find<RndDir>("progress_meter", true);
    mProgressMeter->Init(progressMeter, mSections.size());
    UpdateProgressMeter();
    unk4c.clear();
    unk4c.resize(mSections.size());
}

void TrainerPanel::Exit() {
    UIPanel::Exit();
    TheTrainerPanel = nullptr;
}

void TrainerPanel::Draw() {
    UIPanel::Draw();
    if (mShowProgressMeter) {
        TheUI->GetCam()->Select();
        mProgressMeter->Draw();
    } else {
        mProgressMeter->Hide();
    }
}

void TrainerPanel::StartSection(int sect) {
    if (mCurrSection >= 0) {
        TrainerChallenge *tc = mSections[mCurrSection].mChallenge;
        if (tc)
            tc->Exit();
    }
    mCurrSection = sect;
    Message setSectNameMsg("set_section_name", mSections[sect].mName, sect);
    Handle(setSectNameMsg, true);

    TrainerChallenge *tc = mSections[mCurrSection].mChallenge;
    if (mCurrSection >= 0 && tc) {
        Message updateRestsMsg("update_restrictions", tc->GetRestrictionToken());
        Handle(updateRestsMsg, true);
        tc->Enter();
    }

    Handle(reset_score_msg, true);
    StartSectionImpl();
    SendDataPoint("trainers/section", start_section, sect);
}

DECOMP_FORCEACTIVE(
    TrainerPanel, "begin_token", "end_token", "challenge_token", "start_early", "start_norm"
)

void TrainerPanel::RestartSection() { StartSection(mCurrSection); }

int TrainerPanel::GetCurrentStartTick() const {
    if (mCurrSection < 0)
        return 0;
    else
        return mSections[mCurrSection].mStartTick;
}

int TrainerPanel::GetCurrentEndTick() const {
    if (mCurrSection < 0)
        return 0;
    else
        return GetCurrentStartTick() + GetLoopTicks(mCurrSection);
}

void TrainerPanel::ClearSections() { mSections.clear(); }

void TrainerPanel::AddSection(const TrainerSection &sect) { mSections.push_back(sect); }

TrainerSection &TrainerPanel::GetSection(int idx) { return mSections[idx]; }
const TrainerSection &TrainerPanel::GetSection(int idx) const { return mSections[idx]; }
int TrainerPanel::GetCurrSection() const { return mCurrSection; }
int TrainerPanel::GetNumSections() const { return mSections.size(); }

void TrainerPanel::SetCurrentProgressSection(int cur) { mProgressMeter->SetCurrent(cur); }

int TrainerPanel::GetTick() const {
    return (int)TheTaskMgr.GetSongPos().GetTotalTick();
}

int TrainerPanel::GetSectionTicks(int idx) const {
    const TrainerSection &sect = mSections[idx];
    return sect.mEndTick - sect.mStartTick;
}

int TrainerPanel::GetSectionLoopStart(int idx) const {
    int start = mSections[idx].mStartTick;
    if (ShouldStartEarly()) {
        int bpm = TheSongDB->GetBeatsPerMeasure(start);
        start = start - bpm * 0x1e0;
        start = start & ~(start >> 0x1F);
    }
    return start;
}

bool TrainerPanel::IsSongSectionComplete(BandProfile *, int, Difficulty, int) {
    MILO_ASSERT(false, 0xF4);
    return false;
}

int TrainerPanel::GetLoopTicks(int idx) const {
    return GetSectionLoopEnd(idx) - GetSectionLoopStart(idx);
}

int TrainerPanel::FixupTick(int tick) const {
    int mod = tick % 0x1e0;
    if (mod < 0xf) {
        tick -= mod;
    } else if (mod > 0x1d1) {
        tick += 0x1e0 - mod;
    }
    return tick;
}

void TrainerPanel::ResetChallenge() {
    TrainerChallenge *tc = mSections[mCurrSection].mChallenge;
    if ((int)tc) {
        tc->Exit();
        mSections[mCurrSection].mChallenge->Enter();
    }
}

Symbol TrainerPanel::GetChallengeRestriction(int idx) {
    TrainerChallenge *tc = mSections[idx].mChallenge;
    if (tc)
        return tc->GetRestrictionToken();
    else
        return gNullStr;
}

void TrainerPanel::SetProgressMeterShowing(bool show) { mShowProgressMeter = show; }

void TrainerPanel::InitSections() {
    ClearSections();
    MidiParser *parser = TheGame->GetBeatMaster()->GetMidiParserMgr()->GetParser(
        TheGameMode->Property("midi_parser", true)->Sym()
    );
    // MidiParser::mEvents is private; access via offset to avoid header edit.
    InternalInitSections(*(DataEventList **)((char *)parser + 0x1c));
    if (mSections.size() == 0) {
        InternalInitSections(TheGame->GetBeatMaster()->GetMidiParserMgr()->GetEventsList());
    }
}

DECOMP_FORCEACTIVE(
    TrainerPanel,
    "Duplicate trainer section detected - %s \n",
    "song_lessons",
    "Unknown trainer section end - %s \n",
    "Invalid trainer section - %s \n",
    "Unknown trainer section name for challenge - %s \n",
    "Unknown trainer section name for start early \n",
    "Unknown trainer section name for start norm \n"
)

void TrainerPanel::InternalInitSections(const DataEventList *events) {
    ClearSections();
    std::map<Symbol, TrainerSection> sectionMap;
    for (int i = 0; i < events->Size(); i++) {
        const DataEvent &ev = events->Event(i);
        Symbol tok = ((const DataArray *)ev.mMsg)->Node(1).Sym(((const DataArray *)ev.mMsg));
        int tick = (int)BeatToTick(ev.start);
        if (tok == TheGameMode->Property("begin_token", true)->Sym()) {
            Symbol name = ((const DataArray *)ev.mMsg)->Node(2).Sym(((const DataArray *)ev.mMsg));
            std::map<Symbol, TrainerSection>::iterator it = sectionMap.find(name);
            if (it != sectionMap.end()) {
                TheDebug.Notify(MakeString("Duplicate trainer section detected - %s \n", name.Str()));
            } else {
                TrainerSection sect;
                sect.SetName(name);
                sect.SetStartTick(FixupTick(tick));
                sect.SetStartEarly(
                    !!TheGameMode->Property("song_lessons", true)->Int()
                );
                sectionMap[name] = sect;
            }
        } else if (tok == TheGameMode->Property("end_token", true)->Sym()) {
            Symbol name = ((const DataArray *)ev.mMsg)->Node(2).Sym(((const DataArray *)ev.mMsg));
            std::map<Symbol, TrainerSection>::iterator it = sectionMap.find(name);
            if (it == sectionMap.end()) {
                TheDebug.Notify(MakeString("Unknown trainer section end - %s \n", name.Str()));
            } else {
                TrainerSection &sect = it->second;
                sect.SetEndTick(FixupTick(tick));
                if (sect.SanityCheck()) {
                    AddSection(sect);
                } else {
                    TheDebug.Notify(MakeString("Invalid trainer section - %s \n", name.Str()));
                }
                sectionMap.erase(it);
            }
        } else if (tok == TheGameMode->Property("challenge_token", true)->Sym()) {
            Symbol name = ((const DataArray *)ev.mMsg)->Node(2).Sym(((const DataArray *)ev.mMsg));
            Symbol challenge = ((const DataArray *)ev.mMsg)->Node(3).Sym(((const DataArray *)ev.mMsg));
            std::map<Symbol, TrainerSection>::iterator it = sectionMap.find(name);
            if (it == sectionMap.end()) {
                TheDebug.Notify(MakeString("Unknown trainer section name for challenge - %s \n", name.Str()));
            } else {
                it->second.SetChallengeName(challenge);
            }
        } else if (tok == TheGameMode->Property("start_early", true)->Sym()) {
            Symbol name = ((const DataArray *)ev.mMsg)->Node(2).Sym(((const DataArray *)ev.mMsg));
            std::map<Symbol, TrainerSection>::iterator it = sectionMap.find(name);
            if (it == sectionMap.end()) {
                TheDebug.Notify(MakeString("Unknown trainer section name for start early \n"));
            } else {
                it->second.SetStartEarly(true);
            }
        } else if (tok == TheGameMode->Property("start_norm", true)->Sym()) {
            Symbol name = ((const DataArray *)ev.mMsg)->Node(2).Sym(((const DataArray *)ev.mMsg));
            std::map<Symbol, TrainerSection>::iterator it = sectionMap.find(name);
            if (it == sectionMap.end()) {
                TheDebug.Notify(MakeString("Unknown trainer section name for start norm \n"));
            } else {
                it->second.SetStartEarly(false);
            }
        }
    }
}

void TrainerPanel::OnSuccess(int idx) {
    if (idx == 0) {
        mProgressMeter->SetCompleted(mCurrSection, true);
    }
    SetLessonComplete(idx);
}

Symbol TrainerPanel::GetNameForSection(int idx) const {
    Symbol name = mSections[idx].mName;
    Symbol sect = TheGameMode->Property("trainer_section_postfix", true)->Sym();
    if (sect != gNullStr) {
        name = MakeString("%s_%s", name.Str(), sect.Str());
    }
    return name;
}

void TrainerPanel::SetLessonComplete(int idx) {
    Symbol name = GetNameForSection(mCurrSection);
    static Message msg("get_speed_modifier", 0);
    msg[0] = idx;
    DataNode handled = Handle(msg, true);

    GetBandProfile()->SetLessonComplete(name, handled.Float());
    if (idx == 0) {
        unk4c[mCurrSection] = 1;
    }

    float f = 1.0f;
    for (int i = 0; i < (int)mSections.size(); i++) {
        float speed = GetBandProfile()->GetLessonCompleteSpeed(GetNameForSection(i));
        if (speed < f) {
            f = speed;
        }
    }

    if (f != 0) {
        Message headerMsg("get_header");
        DataNode headerHandled = Handle(headerMsg, true);
        GetBandProfile()->SetLessonComplete(headerHandled.Sym(), f);
    }
}

int TrainerPanel::ModSectionNum(int num) const {
    MILO_ASSERT(!mSections.empty(), 0x1C8);
    if (num < 0) {
        for (; num < 0; num += mSections.size())
            ;
        return num;
    } else
        return num % mSections.size();
}

void TrainerPanel::SetCurrSection(int curr) { mCurrSection = curr; }

BandProfile *TrainerPanel::GetBandProfile() const {
    std::vector<BandUser *> list;
    TheBandUserMgr->GetParticipatingBandUsers(list);
    MILO_ASSERT(list.size() == 1, 0x1DC);
    LocalUser *user = list.front()->GetLocalBandUser();
    return TheProfileMgr.GetProfileForUser(user->GetLocalUser());
}

bool TrainerPanel::AllSectionsFinished() const {
    for (int i = 0; i < unk4c.size(); i++) {
        if (!unk4c[i])
            return false;
    }
    return true;
}

bool TrainerPanel::ShouldStartEarly() const {
    if (mCurrSection < 0)
        return false;
    else
        return mSections[mCurrSection].mStartEarly;
}

int TrainerPanel::GetSectionLoopEnd(int idx) const {
    const TrainerSection &sect = mSections[idx];
    int end = sect.mEndTick;
    int measure = TheSongDB->GetBeatsPerMeasure(end - 0x1e0) * 0x1e0;
    int remainder = (end - sect.mStartTick) % measure;
    int result = end + (measure - remainder);
    if (remainder == 0)
        result = end;
    return result;
}

void TrainerPanel::UpdateProgressMeter() { MILO_ASSERT(false, 0x207); }

void TrainerPanel::NewDifficulty(int, int) {
    unk4c.clear();
    unk4c.resize(mSections.size());
}

BEGIN_HANDLERS(TrainerPanel)
    HANDLE_ACTION(start_section, StartSection(_msg->Int(2)))
    HANDLE_ACTION(restart_section, RestartSection())
    HANDLE_EXPR(get_curr_section, mCurrSection)
    HANDLE_ACTION(set_curr_section, SetCurrSection(_msg->Int(2)))
    HANDLE_ACTION(inc_section, StartSection((mCurrSection + 1) % (int)mSections.size()))
    HANDLE_EXPR(get_num_sections, (int)mSections.size())
    HANDLE_EXPR(challenge_success, !mSections[mCurrSection].mChallenge || mSections[mCurrSection].mChallenge->Success())
    HANDLE_EXPR(get_section_name, mSections[_msg->Int(2)].mName)
    HANDLE_EXPR(get_challenge_restriction, GetChallengeRestriction(_msg->Int(2)))
    HANDLE_EXPR(has_challenge, HasChallenge(_msg->Int(2)))
    HANDLE_ACTION(on_success, OnSuccess(_msg->Int(2)))
    HANDLE_ACTION(on_fail, _msg->Int(2))
    HANDLE_ACTION(set_progress_meter_showing, SetProgressMeterShowing(_msg->Int(2)))
    HANDLE_ACTION(set_current_progress_section, SetCurrentProgressSection(_msg->Int(2)))
    HANDLE_EXPR(all_sections_finished, AllSectionsFinished())
    HANDLE_EXPR(mod_section_num, ModSectionNum(_msg->Int(2)))
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_MEMBER_PTR(DataDir())
    HANDLE_CHECK(0x237)
END_HANDLERS

TrainerSection::TrainerSection()
    : mStartTick(-1), mEndTick(-1), mChallenge(0), mStartEarly(0) {}

int TrainerSection::GetStartTick() const { return mStartTick; }
void TrainerSection::SetStartTick(int tick) { mStartTick = tick; }
int TrainerSection::GetEndTick() const { return mEndTick; }
void TrainerSection::SetEndTick(int tick) { mEndTick = tick; }
Symbol TrainerSection::GetName() const { return mName; }
void TrainerSection::SetName(const Symbol &name) { mName = name; }

void TrainerSection::SetChallengeName(const Symbol &name) {
    mChallengeName = name;
    if (!name.Null()) {
        mChallenge =
            ObjectDir::Main()->Find<TrainerChallenge>(mChallengeName.Str(), false);
        if (!mChallenge) {
            MILO_WARN("Unable to find trainer challenge %s. \n", mChallengeName.Str());
        }
    }
}

void TrainerSection::SetStartEarly(bool early) { mStartEarly = early; }

bool TrainerSection::SanityCheck() { return mStartTick < mEndTick; }

void TrainerChallenge::Enter() {
    static Message enter("enter");
    Handle(enter, true);
}

void TrainerChallenge::Exit() { Handle(exit_msg, true); }

bool TrainerChallenge::Success() { return Handle(success_msg, true).Int(); }

Symbol TrainerChallenge::GetRestrictionToken() {
    return Handle(restriction_token_msg, true).Sym();
}

int GetLoopTick(int tick, int &tickOffset) {
    tickOffset = 0;
    if (!TheGame->InTrainer() || !TheTrainerPanel) {
        return tick;
    }
    int start = TheTrainerPanel->GetCurrentStartTick();
    int end = TheTrainerPanel->GetCurrentEndTick();
    if (tick < end || start == end) {
        return tick;
    }
    int loopTick = start + (tick - start) % (end - start);
    tickOffset = tick - loopTick;
    return loopTick;
}

int GetLoopTick(int tick) {
    int tickOffset;
    return GetLoopTick(tick, tickOffset);
}
// sw3 cross-dialect scatter-include (default/band3/game/TrainerPanel <- gesture/SpeechMgr.cpp) [Object owner]
#ifndef SW_SCATTER_OWNER_INCLUDE
#define SW_SCATTER_OWNER_INCLUDE
#define gRev gRev_SpeechMgr
#define gAltRev gAltRev_SpeechMgr
#include "obj/dialect_object_push.h"
#include "gesture/SpeechMgr.cpp"
#include "obj/dialect_object_pop.h"
#undef gRev
#undef gAltRev
#undef SW_SCATTER_OWNER_INCLUDE
#endif
