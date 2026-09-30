#include "game/PlayerBehavior.h"

// Retail keeps this as its own TU at 0x826EEC38-0x826EECD8: the ctor, then the
// setters in declaration order. SetStreakType/SetMaxMultiplier are ICF-folded with
// TrackConfig::SetTrackNum/SetMaxSlots, and SetCanFreestyleBeforeGems has no slot
// here (folded elsewhere).
PlayerBehavior::PlayerBehavior()
    : mCanDeployOverdrive(true), mTiltDeployBand(false), mFillsDeployBand(false),
      mRequireAllCodas(false), mCanFreestyleGems(false), mHasSolos(false),
      mStreakType("default"), mMaxMultiplier(2) {}

void PlayerBehavior::SetCanDeployOverdrive(bool b) { mCanDeployOverdrive = b; }

void PlayerBehavior::SetTiltDeploysBandEnergy(bool b) { mTiltDeployBand = b; }

void PlayerBehavior::SetFillsDeployBandEnergy(bool b) { mFillsDeployBand = b; }

void PlayerBehavior::SetRequireAllCodaLanes(bool b) { mRequireAllCodas = b; }

void PlayerBehavior::SetStreakType(Symbol symbol) { mStreakType = symbol; }

void PlayerBehavior::SetMaxMultiplier(int multiplier) { mMaxMultiplier = multiplier; }

void PlayerBehavior::SetCanFreestyleBeforeGems(bool b) { mCanFreestyleGems = b; }

void PlayerBehavior::SetHasSolos(bool b) { mHasSolos = b; }
