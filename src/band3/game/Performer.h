#pragma once
#include "obj/Object.h"
#include "utl/SongPos.h"
#include "game/CrowdRating.h"
#include "game/Stats.h"

class BandUser;
class Band;

class Performer : public virtual Hmx::Object {
public:
    Performer(BandUser *, Band *);
#ifdef HX_NATIVE
    // Native scoring-core ctor (M6): builds the REAL scoring state (mStats,
    // mBand, mScore) WITHOUT the retail ctor's CrowdRating/Game/track_graphics
    // singleton drags. mCrowd stays null (crowd meter is a shimmed leaf), and
    // the net/UI streak+score broadcast flags (unk1fd/unk1fe) start disabled so
    // the headless scorer never dispatches send_* messages. X360 never sees
    // this ctor (gated out) — the retail preprocessed output is unchanged.
    Performer(Band *band, bool /*native_score_tag*/)
        : mPollMs(0), mCrowd(0), mStats(Stats()), mBand(band), unk1e0(0),
          unk1e1(0), unk1e2(0), mScore(0), mQuarantined(0), unk1fd(0), unk1fe(0),
          unk1ff(1), mProgressMs(0), mGameOver(0), mMultiplierActive(1),
          mNumRestarts(0) {}
#endif
    virtual DataNode Handle(DataArray *, bool);
    virtual ~Performer();
    virtual int GetScore() const;
    virtual int GetAccumulatedScore() const;
    virtual int CodaScore() const;
    virtual int GetMultiplier(bool, int &, int &, int &) const;
    virtual float GetCrowdRating() const;
    virtual float GetCrowdWarningLevel() const;
    virtual float GetRawCrowdRating() const;
    virtual bool IsNet() const;
    virtual Symbol GetStarRating() const;
    virtual int GetNumStars() const = 0;
    virtual float GetNumStarsFloat() const = 0;
    virtual float GetTotalStars() const;
    virtual bool PastFinalNote() const = 0;
    virtual ExcitementLevel GetExcitement() const;
    virtual void Poll(float, const SongPos &);
    virtual void AddPoints(float, bool, bool);
    virtual void Hit() {}
    virtual void BuildHitStreak(int, float);
    virtual void EndHitStreak();
    virtual void Miss() {}
    virtual void BuildMissStreak(int);
    virtual void EndMissStreak();
    virtual void Restart(bool);
    virtual void SetMultiplierActive(bool);
    virtual float GetPartialStreakFraction() const;
    virtual bool IsInCrowdWarning() const;
    virtual void ForceScore(int);
    virtual float GetNotesHitFraction(bool *) const = 0;
    virtual void SetQuarantined(bool q) { mQuarantined = q; }
    virtual Symbol GetStreakType() const { return "default"; }
    virtual float GetCrowdBoost() const;
    virtual void RemoteUpdateCrowd(float);
    virtual int GetScoreForStars(int) const { return 0; }
    virtual void FinalizeStats() {}
    virtual bool CanStreak() const { return false; }

    int GetIndividualScore() const;
    int GetPercentComplete() const;
    int GetSongNumVocalParts() const;
    int GetNotesPerStreak() const;
    void WinGame(int);
    bool LoseGame();
    float GetRawValue() const;
    float GetDisplayValue() const;
    void UpdateScore(int);
    void SendRemoteStats(BandUser *);
    void SetRemoteStreak(int);
    void RemoteFinishedSong(int);
    void SetLost();
    bool GetMultiplierActive() const;
    float PollMs() const;
    void SetCrowdMeterActive(bool);
    bool GetCrowdMeterActive();
    void SetStats(int, const Stats &);
    void SendStreak();
    void TrulyWinGame();
    void CheckGameWon();
    void ForceStars(int);
    int GetNumRestarts() const;
    void SetNoScorePercent(float);
    bool IsLocal() const { return !IsNet(); }
    Band *GetBand() const { return mBand; }
    bool GetQuarantined() const { return mQuarantined; }
    const Stats &GetStats() const { return mStats; }
    CrowdRating *Crowd() const { return mCrowd; }
    void SetGameOver() { mGameOver = true; }
    bool IsGameOver() const { return mGameOver; }

    float mPollMs; // 0x8
    CrowdRating *mCrowd; // 0xc
    Stats mStats; // 0x10
    Band *mBand; // 0x1dc
    bool unk1e0;
    bool unk1e1;
    bool unk1e2;
    float mScore; // 0x1e4
    SongPos mSongPos; // 0x1e8
    bool mQuarantined; // 0x1fc
    bool unk1fd;
    bool unk1fe;
    bool unk1ff;
    float mProgressMs; // 0x200
    bool mGameOver; // 0x204
    bool mMultiplierActive; // 0x205
    int mNumRestarts; // 0x208
};