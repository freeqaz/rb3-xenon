#pragma once
#include "game/BandUserMgr.h"
#include "os/Debug.h"
#include "tour/TourPerformer.h"
#include <hash_map>

// Retail RB3-X360 uses Harmonix `hash_map<Symbol,int>` here where the rb3-Wii
// dev decomp approximated std::map -- ChooseQuestFilters/CheatCycleSetlist call
// the container's default ctor OUT OF LINE (??0?$hash_map@VSymbol@@H...) and
// InqSongsInFilterData indexes it through the hashtable helpers.  Same finding
// as meta_band/AccomplishmentProgress.h.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

class TourPerformerLocal : public TourPerformerImpl {
public:
    TourPerformerLocal(BandUserMgr &);
    virtual ~TourPerformerLocal();
    virtual DataNode Handle(DataArray *, bool);
    virtual void SyncSave(BinStream &, unsigned int) const;
    virtual bool IsLocal() const { return 1; }
    virtual bool HasSyncPermission() const { return 1; }
    virtual void SyncLoad(BinStream &, unsigned int) { MILO_ASSERT(false, 35); }
    virtual void CompleteQuest();

    void MakeDirty();
    void SelectVenue();
    void ClearCurrentQuest();
    void ClearCurrentQuestFilter();
    void SetCurrentQuest(Symbol);
    void SetCurrentQuestFilter(Symbol, TourSetlistType);
    Symbol ChooseRandomQuestForGroupAndTier(Symbol, int);
    bool InqSongsInFilterData(Symbol, std::hash_map<Symbol, int> &, std::hash_map<Symbol, int> &);
    Symbol GetRandomArtistFromMap(const std::hash_map<Symbol, int> &, int);
    Symbol
    GetRandomQuestFilter(TourProgress *, int, const std::hash_map<Symbol, int> &, const std::hash_map<Symbol, int> &);
    Symbol GetRandomFixedSetlist(TourProgress *, int, Symbol);
    void ChooseQuestFilters();
    bool SanityCheckFilterAgainstType(Symbol, Symbol);
    int SanityCheckQuestFilters();
    void InitializeNextGig();
    void CheatCycleChallenge();
    void CheatCycleSetlist();
};