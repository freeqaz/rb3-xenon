#include "obj/ObjMacros.h"
#include "meta_band/SetlistSortByLocation.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SavedSetlist.h"
#include "meta_band/SongRecord.h"
#include "meta/Profile.h"
#include "meta/Sorting.h"
#include "os/Debug.h"
#include "utl/MakeString.h"
#include "utl/Str.h"

#include <string.h>

LocationCmp::LocationCmp(
    SavedSetlist::SetlistType type, const char *owner, const char *cmp, int id,
    const char *name
)
    : mCmp(cmp), mSetlistType(type), mOwnerName(owner), mId(id), mName(name) {
    switch (mSetlistType) {
    case SavedSetlist::kBattleHarmonix:
    case SavedSetlist::kBattleFriend:
    case SavedSetlist::kBattleHarmonixArchived:
    case SavedSetlist::kBattleFriendArchived:
        mField20 = 0;
        break;
    case SavedSetlist::kSetlistFriend:
    case SavedSetlist::kSetlistHarmonix:
    case SavedSetlist::kSetlistLocal:
        mField20 = 1;
        break;
    case SavedSetlist::kSetlistInternal:
        mField20 = 2;
        break;
    default:
        MILO_FAIL("Bad SetlistType in LocationCmp::LocationCmp!");
        break;
    }

    switch (mSetlistType) {
    case SavedSetlist::kBattleFriend:
    case SavedSetlist::kBattleFriendArchived: {
        MILO_ASSERT(owner, 0x55);
        Profile *profile = TheProfileMgr.GetPrimaryProfile();
        if (profile) {
            bool eq = strcmp(profile->GetName(), owner) == 0;
            if (eq) {
                mField24 = 0;
                break;
            }
        }
        mField24 = 2;
        break;
    }
    case SavedSetlist::kSetlistHarmonix:
    case SavedSetlist::kBattleHarmonix:
    case SavedSetlist::kBattleHarmonixArchived:
        mField24 = 1;
        break;
    case SavedSetlist::kSetlistFriend:
        mField24 = 2;
        break;
    case SavedSetlist::kSetlistLocal:
    case SavedSetlist::kSetlistInternal:
        mField24 = 0;
        break;
    default:
        MILO_FAIL("Bad SetlistType in LocationCmp::LocationCmp!");
        break;
    }
}

int LocationCmp::Compare(const SongSortCmp *cmp, SongNodeType nodeType) const {
    LocationCmp *loc = (LocationCmp *)cmp;
    MILO_ASSERT(cmp, 0x73);
    switch (nodeType) {
    case kNodeShortcut:
    case kNodeHeader:
        return mField20 - loc->mField20;
    case kNodeSetlist:
        if (mField20 == loc->mField20) {
            if ((mId > 0) == (loc->mId > 0)) {
                if (mField24 == loc->mField24) {
                    switch (mField20) {
                    case 0: {
                        if (mField24 == 2) {
                            MILO_ASSERT(mOwnerName, 0x8E);
                            MILO_ASSERT(loc->mOwnerName, 0x8F);
                            int ownerCmp =
                                AlphaKeyStrCmp(mOwnerName, loc->mOwnerName, false);
                            if (ownerCmp != 0)
                                return ownerCmp;
                        }
                        int idCmp = mId - loc->mId;
                        if (idCmp != 0)
                            return idCmp;
                        int cmpCmp = AlphaKeyStrCmp(mCmp, loc->mCmp, false);
                        if (cmpCmp != 0)
                            return cmpCmp;
                        return AlphaKeyStrCmp(mName.c_str(), loc->mName.c_str(), false);
                    }
                    case 1: {
                        if (mField24 == 2) {
                            MILO_ASSERT(mOwnerName, 0xA8);
                            MILO_ASSERT(loc->mOwnerName, 0xA9);
                            int ownerCmp =
                                AlphaKeyStrCmp(mOwnerName, loc->mOwnerName, false);
                            if (ownerCmp != 0)
                                return ownerCmp;
                        }
                        return AlphaKeyStrCmp(mCmp, loc->mCmp, false);
                    }
                    case 2:
                        return AlphaKeyStrCmp(mCmp, loc->mCmp, false);
                    default:
                        MILO_FAIL("Bad SetlistHeaderType in LocationCmp::Compare!");
                        return 0;
                    }
                }
                return mField24 - loc->mField24;
            }
            return loc->mId - mId;
        }
        return mField20 - loc->mField20;
    default:
        MILO_FAIL("invalid type of node comparison.\n");
        return 0;
    }
}

Symbol LocationCmp::SetlistHeaderTypeToSym(SetlistHeaderType type) {
    // Retail builds these three as guarded function-local statics (guard word
    // 0x82DFF750 bits 0/1/2, three matching 32-byte ??__F), NOT the
    // utl/Symbols.h globals of the same name.
    static Symbol setlist_header_battles("setlist_header_battles");
    static Symbol setlist_header_custom("setlist_header_custom");
    static Symbol setlist_header_internal("setlist_header_internal");
    switch (type) {
    case kHeaderBattles:
        return setlist_header_battles;
    case kHeaderCustom:
        return setlist_header_custom;
    case kHeaderInternal:
        return setlist_header_internal;
    default:
        MILO_FAIL("Bad SetlistHeaderType in LocationCmp::Compare!");
        return gNullStr;
    }
}

ShortcutNode *SetlistSortByLocation::NewShortcutNode(SetlistSortNode *node) const {
    SavedSetlist::SetlistType type = node->GetSetlistRecord()->GetSetlist()->GetType();
    LocationCmp *cmp = new LocationCmp(type, gNullStr, gNullStr, 0, gNullStr);
    Symbol sym = LocationCmp::SetlistHeaderTypeToSym(
        (LocationCmp::SetlistHeaderType)cmp->mField20
    );
    return new ShortcutNode(cmp, sym, true);
}

HeaderSortNode *SetlistSortByLocation::NewHeaderNode(SetlistSortNode *node) const {
    SavedSetlist::SetlistType type = node->GetSetlistRecord()->GetSetlist()->GetType();
    LocationCmp *cmp = new LocationCmp(type, gNullStr, gNullStr, 0, gNullStr);
    Symbol sym = LocationCmp::SetlistHeaderTypeToSym(
        (LocationCmp::SetlistHeaderType)cmp->mField20
    );
    return new HeaderSortNode(cmp, sym, true);
}

ShortcutNode *SetlistSortByLocation::NewShortcutNode(FunctionSortNode *node) const {
    LocationCmp *cmp = new LocationCmp(
        SavedSetlist::kBattleHarmonix, gNullStr, gNullStr, 0, gNullStr
    );
    Symbol sym = LocationCmp::SetlistHeaderTypeToSym(
        (LocationCmp::SetlistHeaderType)cmp->mField20
    );
    return new ShortcutNode(cmp, sym, true);
}

HeaderSortNode *SetlistSortByLocation::NewHeaderNode(FunctionSortNode *node) const {
    LocationCmp *cmp = new LocationCmp(
        SavedSetlist::kBattleHarmonix, gNullStr, gNullStr, 0, gNullStr
    );
    Symbol sym = LocationCmp::SetlistHeaderTypeToSym(
        (LocationCmp::SetlistHeaderType)cmp->mField20
    );
    return new HeaderSortNode(cmp, sym, true);
}

SetlistSortNode *SetlistSortByLocation::NewSetlistNode(SetlistRecord *record) const {
    BattleSavedSetlist *battle =
        dynamic_cast<BattleSavedSetlist *>(record->GetSetlist());
    int id = battle ? battle->mBattleTimeLeft : 0;
    NetSavedSetlist *net = dynamic_cast<NetSavedSetlist *>(record->GetSetlist());
    const char *name = gNullStr;
    if (battle) {
        name = MakeString("%i", battle->mID);
    } else if (net) {
        name = net->mGuid.c_str();
    }
    LocationCmp *cmp = new LocationCmp(
        record->GetSetlist()->GetType(), record->GetOwner(),
        record->GetSetlist()->GetTitle(), id, name
    );
    return new SetlistSortNode(cmp, record);
}

FunctionSortNode *SetlistSortByLocation::NewFunctionNode(Symbol sym) const {
    LocationCmp *cmp = new LocationCmp(
        SavedSetlist::kBattleHarmonix, gNullStr, gNullStr, 0, gNullStr
    );
    return new FunctionSortNode(
        cmp, false, sym, gNullStr, (const char *)0, (const char *)0
    );
}

SubheaderSortNode *SetlistSort::NewSubheaderNode(SetlistSortNode *node) const {
    MILO_FAIL(__FUNCTION__);
    return nullptr;
}
