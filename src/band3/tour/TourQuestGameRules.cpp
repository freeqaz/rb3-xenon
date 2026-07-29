#include "tour/TourQuestGameRules.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/Symbol.h"

TourQuestGameRules::TourQuestGameRules() : mModifier() {}
TourQuestGameRules::~TourQuestGameRules() {}

void TourQuestGameRules::Init(const DataArray *i_pConfig) {
    MILO_ASSERT(i_pConfig, 23);
    TourGameRules::Init(i_pConfig);
    // Retail spells `modifiers` as a function-local static Symbol (guard word
    // 0x82CBEB60 bit 0 + its own ??__F clear at 0x82365DD0), NOT the
    // utl/Symbols.h global the rb3-Wii dev oracle uses.  Declaration position
    // is load-bearing: the guard test lands AFTER the base Init call.
    static Symbol modifiers("modifiers");
    mModifier.Init(i_pConfig->FindArray(modifiers, false));
}
