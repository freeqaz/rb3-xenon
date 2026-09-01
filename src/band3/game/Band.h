#pragma once
#include "game/CommonPhraseCapturer.h"
#include "game/Performer.h"
#include "game/Player.h"
#include "obj/Object.h"
#include "utl/SongPos.h"

class BandPerformer;
class BandUser;
class BeatMaster;

class Band : public Hmx::Object {
public:
    Band(bool, int, BandUser *, BeatMaster *);
#ifdef HX_NATIVE
    // Native scoring ctor (M6): a Band that only carries the EnergyMultiplier
    // state Player::GetMultiplier reads (mMultiplier / mMultiplierActive /
    // mBonusLevel). It skips the retail ctor's recursive player creation,
    // BandPerformer/CommonPhraseCapturer allocation, and scoring/bonuses config
    // parse — those drag the whole band domain (a shimmed leaf). X360 never
    // sees this ctor (gated out).
    Band(bool /*native_score_tag*/, int mult, bool /*disambig*/)
        : mBandPerformer(0), unk30(0), mAccumulatedScore(0), mTotalStars(0),
          unk3c(0), unk40(0), unk44(0), mBonusLevel(0), mMultiplier(mult),
          mMaxMultiplier(mult), mMsWithMultiplier(0), mMsWhenMultiplierStarted(0),
          mMultiplierActive(1), unk60(0), mMaxBonusLevel(0),
          mCommonPhraseCapturer(0) {}
    void NativeSetMultiplier(int m) { mMultiplier = m; }
    // M7: run the REAL retail bonuses parse (identical to the lines the retail
    // Band ctor executes) so mMaxBonusLevel / unk68 (multiplier-by-bonus-level)
    // / unk70 (crowd_boost) come from the genuine SystemConfig("scoring",
    // "bonuses") data instead of a hand-set table. UpdateBonusLevel then drives
    // mMultiplier = unk68[bonusLevel] through the real machinery. Declared here,
    // defined out-of-line in Band.cpp (needs SystemConfig); X360 never sees it.
    void NativeLoadBonuses();
#endif
    virtual ~Band();
    virtual DataNode Handle(DataArray *, bool);

    int EnergyMultiplier() const;
    int EnergyCrowdBoost() const;
    void ForceStars(int);
    void UpdateBonusLevel(float);
    int DeployBandEnergy(BandUser *);
    int GetMultiplier(bool, int &, int &, int &) const;
    Performer *MainPerformer() const;
    Performer *GetBand() const;
    Player *AddPlayer(BeatMaster *, BandUser *);
    Player *GetActivePlayer(int) const;
    int NumActivePlayers() const;
    int NumNonQuarantinedPlayers() const;
    std::vector<Player *> &GetActivePlayers();
    void SetAccumulatedScore(int);
    void SetMultiplierActive(bool);
    void SetCrowdMeterActive(bool);
    int GetLongestStreak() const;
    void SaveAll();
    void BlowCoda(Player *);
    void LocalBlowCoda(Player *);
    void FinishedCoda(Player *);
    void LocalFinishedCoda(Player *);
    void AddUserDynamically(BandUser *);
    Player *AddPlayerDynamically(BeatMaster *, BandUser *);
    Player *NewPlayer(BeatMaster *, BandUser *);
    bool EveryoneDoneWithSong() const;
    bool EveryoneFinishedCoda();
    void DealWithCodaGem(Player *, int, bool, bool);
    bool AnyoneSaveable() const;
    void SetGameOver();
    void Restart(bool);
    void RemoveUser(BandUser *);
    void Poll(float, SongPos &);
    void CheckCoda(SongPos &);
    bool IsEndOfCoda(int);
    void WinCoda();
    bool IsMultiplierActive() const { return mMultiplierActive; }
    int AccumulatedScore() const { return mAccumulatedScore; }
    float GetTotalStars() const { return mTotalStars; }

    BandPerformer *mBandPerformer; // 0x28
    std::vector<Player *> mActivePlayers; // 0x2c
    std::vector<float> mCrowdRatings; // 0x38
    float unk30; // 0x44
    int mAccumulatedScore; // 0x48
    float mTotalStars; // 0x4c
    int unk3c; // 0x50
    bool unk40; // 0x54
    int unk44; // 0x58
    int mBonusLevel; // 0x5c
    int mMultiplier; // 0x60
    int mMaxMultiplier; // 0x64
    float mMsWithMultiplier; // 0x68
    float mMsWhenMultiplierStarted; // 0x6c
    bool mMultiplierActive; // 0x70
    char unk60; // 0x71 (byte; packs into mMultiplierActive word so mMaxBonusLevel lands at 0x74)
    int mMaxBonusLevel; // 0x74
    std::vector<int> unk68; // 0x78
    std::vector<int> unk70; // 0x84
    CommonPhraseCapturer *mCommonPhraseCapturer; // 0x90
};