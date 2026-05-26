#pragma once
#include "flow/PropertyEventProvider.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/Skeleton.h"
#include "hamobj/Difficulty.h"
#include "obj/Object.h"
#include "stl/_vector.h"
#include "utl/Str.h"
#include "utl/Symbol.h"

class HamPlayerData : public Hmx::Object {
public:
    HamPlayerData(int);
    // Hmx::Object
    virtual ~HamPlayerData() {}
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    void SetCrew(Symbol);
    void SetDifficulty(Difficulty);
    void SetPlaying(bool);
    bool IsPlaying() const;
    void SetCharacterOutfit(Symbol);
    void SetCharacter(Symbol);
    bool SetAssociatedPadNum(int, String);
    float TrackingAgeSeconds() const;
    const Skeleton *GetSkeleton() const;
    const Skeleton *GetSkeleton(const Skeleton *const (&)[6]) const;
    void SetPreferredOutfit(Symbol);
    void SetOutfit(Symbol);
    Symbol CharacterOutfit(Symbol) const;
    String GetPlayerName() const;
    SkeletonSide Side() const;
    void SetUsingFitness(bool);
    Symbol GetPreferredOutfit() const;

    Difficulty GetDifficulty() { return mDifficulty; }
    PropertyEventProvider *Provider() const { return mProvider; }
    bool InFreestyle() const { return mSkeletonTrackingID >= 0; }
    Symbol Crew() const { return mCrew; }
    Symbol Char() const { return mChar; }
    Symbol Outfit() const { return mOutfit; }
    Symbol Autoplay() const { return mAutoplay; }
    int GetSkeletonTrackingID() const { return mSkeletonTrackingID; }
    void AssignSkeleton(int id) { SetSkeletonTrackingID(id); }
    int PadNum() const { return mPadNum; }
    int PlayerIndex() const { return mPlayerIndex; }
    bool IsAutoplaying() const { return !mAutoplay.Null(); }
    void SetMiniGameCharacter(Symbol s) { mMiniGameCharacter = s; }
    Symbol MiniGameCharacter() const { return mMiniGameCharacter; }
    void SetAutoplay(Symbol s) { mAutoplay = s; }
    const String &CurrentDancer() const { return mCurrentDancer; }
    String &CurrentDancer() { return mCurrentDancer; }
    std::vector<String> &AvailableDancers() { return mAvailableDancers; }

private:
    void SetSkeletonTrackingID(int);

protected:
    String mCurrentDancer; // 0x2c - set from available dancers list front
    std::vector<String> mAvailableDancers; // 0x34 - populated from dancer data
    int mPlayerIndex; // 0x40 - initialized from constructor parameter
    Symbol mChar; // 0x44
    Symbol mMiniGameCharacter; // 0x48
    Symbol mPreferredOutfit; // 0x4c
    Symbol mOutfit; // 0x50
    Symbol mCrew; // 0x54
    Difficulty mDifficulty; // 0x58
    float mSkeletonTrackingStartTime; // 0x5c - initialized to -1, time tracking started
    int mSkeletonTrackingID; // 0x60
    Symbol mAutoplay; // 0x64
    ObjPtr<PropertyEventProvider> mProvider; // 0x68
    int mPadNum; // 0x7c
    int mSameRatingCount; // 0x80
    int mLastRatingIdx; // 0x84
};
