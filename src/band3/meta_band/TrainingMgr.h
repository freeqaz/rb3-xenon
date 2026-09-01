#pragma once
#include "beatmatch/TrackType.h"
#include "game/BandUser.h"
#include "game/Defines.h"
#include "obj/Object.h"

class TrainingMgr : public Hmx::Object {
public:
    TrainingMgr();
    virtual ~TrainingMgr();
    virtual DataNode Handle(DataArray *, bool);

    void SetUser(LocalBandUser *);
    void ParticipateUsers();
    void UnparticipateUsers();
    void SetDifficulty(Difficulty);
    void SetPreferredScoreType(ScoreType);
    void SetTrackType(TrackType);
    void SetMinimumDifficulty(Difficulty);
    void SetReturnInfo(Symbol, Symbol);
    void SetCurrentLesson(int);
    void ClearCurrentLesson();
    Symbol GetModeFromLessonName(Symbol);
    Symbol GetModeFromTrackType(TrackType);
    TrackType GetTrackTypeFromLessonName(Symbol);
    Symbol GetSongFromLessonName(Symbol);
    Difficulty GetDifficultyFromLessonName(Symbol);
    LocalBandUser *GetUser() const { return mUser; }
    Difficulty GetMinimumDifficulty() const { return mMinimumDifficulty; }

    static void Init();
    static TrainingMgr *GetTrainingMgr();

    LocalBandUser *mUser; // 0x28
    Difficulty mMinimumDifficulty; // 0x2c
    Symbol mReturnScreen; // 0x30
    Symbol mQuitToken; // 0x34
    int mCurrentLesson; // 0x38
};
