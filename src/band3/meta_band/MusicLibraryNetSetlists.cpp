#include "meta_band/MusicLibraryNetSetlists.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/ProfileMgr.h"
#include "net_band/RockCentral.h"
#include "net_band/RockCentralMsgs.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "rndobj/Utl.h"
#include "utl/BufStream.h"
#include "utl/NetCacheMgr.h"
#include "utl/Std.h"
#include "utl/Symbol.h"

MusicLibraryNetSetlists::MusicLibraryNetSetlists()
    : mFailed(0), mSucceeded(0), unk48(0), mPendingSetlistArt(0), unk50(gNullStr),
      mSetlistArtLoader(0) {}

MusicLibraryNetSetlists::~MusicLibraryNetSetlists() {
    CleanUp();
    DeleteAll(unk20);
    DeleteAll(unk28);
}

void MusicLibraryNetSetlists::Poll() {
    if (mSetlistArtLoader) {
        MILO_ASSERT(mPendingSetlistArt, 0x2C);
        if (mSetlistArtLoader->IsLoaded()) {
            RndBitmap bmap;
            BufStream bs(
                mSetlistArtLoader->GetBuffer(), mSetlistArtLoader->GetSize(), true
            );
            bmap.Load(bs);
            SwapDxtEndianness(&bmap);
            bmap.SetMip(nullptr);
            mPendingSetlistArt->SetBitmap(bmap, nullptr, false);
            TheNetCacheMgr->DeleteNetCacheLoader(mSetlistArtLoader);
            mSetlistArtLoader = nullptr;
            FinishGettingSetlistArt(true);
        } else if (mSetlistArtLoader->HasFailed()) {
            TheNetCacheMgr->DeleteNetCacheLoader(mSetlistArtLoader);
            mSetlistArtLoader = nullptr;
            FinishGettingSetlistArt(false);
        }
    }
}

void MusicLibraryNetSetlists::RefreshSetlists() {
    MILO_ASSERT(!mFailed && !mSucceeded, 0x4F);
    std::vector<BandProfile *> profiles = TheProfileMgr.GetSignedInProfiles();
    unk48 = false;
    TheRockCentral.GetAllSonglists(profiles, mDataResults, this);
}

void MusicLibraryNetSetlists::RefreshArchivedBattles() {
    std::vector<BandProfile *> profiles = TheProfileMgr.GetSignedInProfiles();
    unk48 = true;
    TheRockCentral.GetAllSonglists(profiles, mDataResults, this);
}

void MusicLibraryNetSetlists::CleanUp() {
    mFailed = false;
    mSucceeded = false;
    TheRockCentral.CancelOutstandingCalls(this);
    DeleteAll(unk20);
    DeleteAll(unk28);
    mDataResults.Clear();
    CleanUpArt();
    FOREACH (it, mSetlists) {
        RELEASE(it->unk4);
    }
    mSetlists.clear();
}

DataNode MusicLibraryNetSetlists::OnMsg(const RockCentralOpCompleteMsg &msg) {
    if (!mPendingSetlistArt) {
        if (msg.Success()) {
            mDataResults.Update(nullptr);
            ParseDataResultsIntoSetlists(unk48);
            mDataResults.Clear();
            if (!unk48) {
                RefreshArchivedBattles();
                return 1;
            }
            mSucceeded = true;
        } else {
            DeleteAll(unk20);
            DeleteAll(unk28);
            mDataResults.Clear();
            mFailed = true;
        }
        TheMusicLibrary->RebuildAndSortSetlists();
    } else
        FinishGettingSetlistArt(msg.Success());
    return 1;
}

void MusicLibraryNetSetlists::FinishGettingSetlistArt(bool b1) {
    if (b1 && mPendingSetlistArt->Height() && mPendingSetlistArt->Width()) {
        mSetlists.push_back(SetlistArtRecord());
        SetlistArtRecord &rec = mSetlists.back();
        rec.unk0 = unk50;
        rec.unk4 = mPendingSetlistArt;
        mPendingSetlistArt = nullptr;
        while (mSetlists.size() > 15) {
            mSetlists.erase(mSetlists.begin());
        }
        TheMusicLibrary->SetlistArtFinished();
    } else {
        RELEASE(mPendingSetlistArt);
    }
}

void MusicLibraryNetSetlists::ParseDataResultsIntoSetlists(bool archived) {
    std::vector<NetSavedSetlist *> &setlists = archived ? unk28 : unk20;
    DeleteAll(setlists);
    MILO_LOG("Setlists from net:\n");
    mDataResults.Print(TheDebug);
    MILO_LOG("\n");
    FOREACH (it, mDataResults.mDataResultList) {
        DataNode node;
        DataResult &result = *it;
        result.GetDataResultValue("title", node);
        String title(node.Str(nullptr));
        result.GetDataResultValue("desc", node);
        String desc(node.Str(nullptr));
        result.GetDataResultValue("type", node);
        int type = node.Int(nullptr);
        bool validInstr =
            (type == 1 || type == 1001 || type == 2 || type == 1002);
        String artUrl(gNullStr);
        if (validInstr) {
            if (result.GetDataResultValue("art_url", node)) {
                artUrl = node.Str(nullptr);
            }
        }
        NetSavedSetlist *setlist = nullptr;
        switch (type) {
        case 0:
        case 1: {
            result.GetDataResultValue("owner", node);
            String owner(node.Str(nullptr));
            result.GetDataResultValue("guid", node);
            String guid(node.Str(nullptr));
            MILO_ASSERT(!archived, 0xFC);
            setlist = new NetSavedSetlist(
                SavedSetlist::kSetlistFriend, title.c_str(), desc.c_str(), validInstr,
                owner.c_str(), artUrl.c_str(), guid.c_str()
            );
            break;
        }
        case 2: {
            MILO_ASSERT(!archived, 0x108);
            setlist = new NetSavedSetlist(
                SavedSetlist::kSetlistHarmonix, title.c_str(), desc.c_str(), validInstr,
                gNullStr, artUrl.c_str(), gNullStr
            );
            break;
        }
        case 1000:
        case 1001: {
            result.GetDataResultValue("owner", node);
            String owner(node.Str(nullptr));
            result.GetDataResultValue("id", node);
            int id = node.Int(nullptr);
            result.GetDataResultValue("valid_instr", node);
            int scoreType = node.Int(nullptr);
            int secondsLeft = 0;
            if (!archived) {
                result.GetDataResultValue("seconds_left", node);
                secondsLeft = node.Int(nullptr);
            }
            SavedSetlist::SetlistType battleType = archived
                ? SavedSetlist::kBattleFriendArchived
                : SavedSetlist::kBattleFriend;
            setlist = new BattleSavedSetlist(
                id, (ScoreType)scoreType, battleType, title.c_str(), validInstr,
                desc.c_str(), owner.c_str(), artUrl.c_str(), secondsLeft
            );
            break;
        }
        case 1002: {
            result.GetDataResultValue("id", node);
            int id = node.Int(nullptr);
            result.GetDataResultValue("valid_instr", node);
            int scoreType = node.Int(nullptr);
            int secondsLeft = 0;
            if (!archived) {
                result.GetDataResultValue("seconds_left", node);
                secondsLeft = node.Int(nullptr);
            }
            SavedSetlist::SetlistType battleType = archived
                ? SavedSetlist::kBattleHarmonixArchived
                : SavedSetlist::kBattleHarmonix;
            setlist = new BattleSavedSetlist(
                id, (ScoreType)scoreType, battleType, title.c_str(), validInstr,
                desc.c_str(), nullptr, artUrl.c_str(), secondsLeft
            );
            break;
        }
        default:
            MILO_FAIL("Bad setlist type from RockCentral!\n");
            break;
        }
        MILO_ASSERT(setlist, 0x15C);
        int i = 0;
        while (result.GetDataResultValue(MakeString("s_id%03i", i), node)) {
            setlist->AddSong(node.Int(nullptr));
            result.GetDataResultValue(MakeString("s_name%03i", i), node);
            setlist->AddSongTitle(node.Str(nullptr));
            i++;
        }
        bool keep = true;
        if (!setlist->GetOwnerOnlineID()->IsInvalid()
            && !ThePlatformMgr.CanSeeUserCreatedContent(setlist->GetOwnerOnlineID())) {
            keep = false;
        }
        if (keep && !setlist->mSongs.empty()) {
            setlists.push_back(setlist);
        } else {
            RELEASE(setlist);
        }
    }
}

// The original assertion string referenced "kArchivedHarmonix", which is no
// longer a member of SavedSetlist::SetlistType (renamed to
// kBattleHarmonixArchived). Aliasing here lets MILO_ASSERT preserve the
// original stringified message byte-for-byte while the comparison still
// resolves to the correct enum value.
#define kArchivedHarmonix kBattleHarmonixArchived

void MusicLibraryNetSetlists::RefreshSetlistArt() {
    MILO_ASSERT(mSucceeded, 0x18E);
    SortNode *node = TheMusicLibrary->GetHighlightedNode();
    SetlistSortNode *ssn = dynamic_cast<SetlistSortNode *>(node);
    MILO_ASSERT(ssn, 0x192);
    SavedSetlist *setlist = ssn->mSetlistRecord->mSetlist;
    NetSavedSetlist *nss = dynamic_cast<NetSavedSetlist *>(setlist);
    MILO_ASSERT(nss, 0x195);
    if (unk50 == node->GetToken()) return;
    if (!nss->GetOwnerOnlineID()->IsInvalid()
        && !ThePlatformMgr.CanSeeUserCreatedContent(nss->GetOwnerOnlineID()))
        return;
    CleanUpArt();
    MILO_ASSERT(!mPendingSetlistArt, 0x1A2);
    MILO_ASSERT(!mSetlistArtLoader, 0x1A3);
    mPendingSetlistArt = Hmx::Object::New<RndTex>();
    unk50 = node->GetToken();
    if (nss->unk44) {
        mSetlistArtLoader =
            TheNetCacheMgr->AddNetCacheLoader(nss->GetArtUrl(), (NetLoaderPos)0);
        if (!mSetlistArtLoader) {
            RELEASE(mPendingSetlistArt);
        }
    } else {
        MILO_ASSERT(nss->GetType() != SavedSetlist::kSetlistHarmonix, 0x1B6);
        MILO_ASSERT(nss->GetType() != SavedSetlist::kBattleHarmonix, 0x1B7);
        MILO_ASSERT(nss->GetType() != SavedSetlist::kArchivedHarmonix, 0x1B8);
        SavedSetlist::SetlistType type = nss->GetType();
        switch (type) {
        case SavedSetlist::kSetlistFriend:
        case SavedSetlist::kSetlistHarmonix:
            TheRockCentral.GetSetlistArt(
                nss->mGuid.c_str(), mPendingSetlistArt, this, 0
            );
            break;
        case SavedSetlist::kBattleHarmonix:
        case SavedSetlist::kBattleFriend:
        case SavedSetlist::kBattleHarmonixArchived:
        case SavedSetlist::kBattleFriendArchived: {
            BattleSavedSetlist *bss = dynamic_cast<BattleSavedSetlist *>(setlist);
            MILO_ASSERT(bss, 0x1C9);
            TheRockCentral.GetBattleArt(bss->mID, mPendingSetlistArt, this, 0);
            break;
        }
        default:
            MILO_FAIL("Bad SetlistType %i in RefreshSetlistArt!", type);
            break;
        }
    }
}

#undef kArchivedHarmonix

bool MusicLibraryNetSetlists::IsSetlistArtReady(Symbol s) const {
    FOREACH (it, mSetlists) {
        if (s == it->unk0) {
            return true;
        }
    }
    return false;
}

RndTex *MusicLibraryNetSetlists::GetSetlistArt(Symbol s) const {
    FOREACH (it, mSetlists) {
        if (s == it->unk0) {
            return it->unk4;
        }
    }
    MILO_FAIL("No setlist art matching id sym \"%s\"!", s);
    return nullptr;
}

void MusicLibraryNetSetlists::CleanUpArt() {
    if (mPendingSetlistArt) {
        TheRockCentral.CancelOutstandingCalls(this);
    }
    if (mSetlistArtLoader) {
        TheNetCacheMgr->DeleteNetCacheLoader(mSetlistArtLoader);
        mSetlistArtLoader = nullptr;
    }
    RELEASE(mPendingSetlistArt);
}

BEGIN_HANDLERS(MusicLibraryNetSetlists)
    HANDLE_MESSAGE(RockCentralOpCompleteMsg)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x1FE)
END_HANDLERS