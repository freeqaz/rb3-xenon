#include "meta_band/CharSync.h"
#include "bandobj/BandCharDesc.h"
#include "bandobj/BandDirector.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "meta_band/BandUI.h"
#include "meta_band/CharCache.h"
#include "meta_band/CharData.h"
#include "meta_band/ClosetMgr.h"
#include "meta_band/OvershellPanel.h"
#include "meta_band/PrefabMgr.h"
#include "meta_band/ProfileMessages.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SessionMgr.h"
#include "movie/Splash.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "ui/UI.h"
#include "ui/UIScreen.h"
#include "utl/Std.h"
#include <algorithm>
#include <list>
#include <vector>

CharSync *TheCharSync;

void CharSync::Init(BandUserMgr *mgr) {
    BandUserMgr *pMgr;
    if (mgr) {
        pMgr = mgr;
    } else {
        pMgr = TheBandUserMgr;
    }
    TheCharSync = new CharSync(pMgr);
    TheCharSync->SetName("char_sync", ObjectDir::Main());
}

CharSync::CharSync(BandUserMgr *mgr) : mUserMgr(mgr) {
    MILO_ASSERT(mUserMgr, 0x35);
    TheProfileMgr.AddSink(this, PrimaryProfileChangedMsg::Type());
    TheProfileMgr.AddSink(this, ProfileChangedMsg::Type());
}

CharSync::~CharSync() {
    TheProfileMgr.RemoveSink(this, ProfileChangedMsg::Type());
    TheProfileMgr.RemoveSink(this, PrimaryProfileChangedMsg::Type());
}

void CharSync::UpdateCharCache() {
    OvershellPanel *overshell = TheBandUI.GetOvershell();
    MILO_ASSERT(overshell, 0x47);

    if (overshell->InSong() || TheSplasher)
        return;

    std::vector<CharData *> data48;
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    PrefabMgr *prefabMgr = PrefabMgr::GetPrefabMgr();
    for (int i = 0; i < 4; i++) {
        CharData *charDesc = 0;
        if (profile) {
            const StandIn &standIn = profile->GetStandIn(i);
            if (standIn.IsNone())
                charDesc = 0;
            else if (standIn.IsPrefabCharacter()) {
                charDesc = prefabMgr->GetPrefab(standIn.GetName());
            } else {
                MILO_ASSERT(standIn.IsCustomCharacter(), 0x7E);
                charDesc = profile->GetCharFromGuid(standIn.GetGuid());
                MILO_ASSERT(charDesc, 0x81);
            }
        }
        data48.push_back(charDesc);
    }

    std::vector<CharData *> prefabsBySlot;
    for (int i = 0; i < 4; i++) {
        CharData *target = prefabMgr->GetDefaultPrefab(i);
        if (std::find(data48.begin(), data48.end(), target) == data48.end()) {
            prefabsBySlot.push_back(target);
        } else
            prefabsBySlot.push_back(0);
    }

    std::vector<BandUser *> users58;
    mUserMgr->GetParticipatingBandUsers(users58);
    for (int i = 0; i < users58.size(); i++) {
        CharData *target = users58[i]->GetChar();
        std::vector<CharData *>::iterator it48 =
            std::find(data48.begin(), data48.end(), target);
        if (it48 != data48.end())
            *it48 = 0;
        std::vector<CharData *>::iterator it50 =
            std::find(prefabsBySlot.begin(), prefabsBySlot.end(), target);
        if (it50 != prefabsBySlot.end())
            *it50 = 0;
    }

    std::list<CharData *> data60;
    std::list<CharData *> prefabBackups;
    for (int i = 0; i < 4; i++) {
        BandUser *curUser = mUserMgr->GetUserFromSlot(i);
        if (curUser && curUser->HasChar()) {
            if (prefabsBySlot[i]) {
                prefabBackups.push_back(prefabsBySlot[i]);
                prefabsBySlot[i] = 0;
            }
            if (data48[i]) {
                data60.push_back(data48[i]);
                data48[i] = 0;
            }
        } else if (data48[i] && prefabsBySlot[i]) {
            prefabBackups.push_back(prefabsBySlot[i]);
            prefabsBySlot[i] = 0;
        }
    }

    for (int n = 0; n < 4; n++) {
        bool inCloset = false;
        std::vector<BandCharDesc *> descs70;
        BandUser *curUser = mUserMgr->GetUserFromSlot(n);
        if (curUser) {
            if (ClosetMgr::GetClosetMgr()->GetUser() == curUser)
                continue;
        }
        if (n == 0) {
            if (ClosetMgr::GetClosetMgr()->InNoUserMode())
                continue;
        }
        if (curUser && curUser->HasChar()) {
            inCloset = TheCharCache->GetCharacter(n)->InCloset();
            descs70.push_back(curUser->GetChar()->GetBandCharDesc());
        } else {
            CharData *npc;
            if (data48[n]) {
                npc = data48[n];
                MILO_ASSERT(!prefabsBySlot[n], 0x114);
            } else {
                if (!data60.empty()) {
                    npc = data60.front();
                    data60.pop_front();
                    if (prefabsBySlot[n]) {
                        prefabBackups.push_back(prefabsBySlot[n]);
                    }
                } else {
                    if (prefabsBySlot[n]) {
                        npc = prefabsBySlot[n];
                    } else {
                        MILO_ASSERT(!prefabBackups.empty(), 0x128);
                        npc = prefabBackups.front();
                        prefabBackups.pop_front();
                    }
                }
            }
            MILO_ASSERT(npc, 0x12D);
            descs70.push_back(npc->GetBandCharDesc());
        }
        TheCharCache->Request(n, descs70, inCloset, false);
    }
}

DataNode CharSync::OnMsg(const PrimaryProfileChangedMsg &) {
    UpdateCharCache();
    return 1;
}

DataNode CharSync::OnMsg(const ProfileChangedMsg &msg) {
    BandProfile *p = msg.GetProfile();
    if (p) {
        LocalBandUser *u = p->GetAssociatedLocalBandUser();
        if (TheSessionMgr->HasUser(u)) {
            CharData *data = p->GetLastCharUsed();
            if (data && TheBandUserMgr->IsCharAvailable(data)) {
                u->SetChar(data);
            }
        }
    }
    UpdateCharCache();
    return 1;
}

BEGIN_HANDLERS(CharSync)
    HANDLE_ACTION_STATIC(update_char_cache, UpdateCharCache())
    HANDLE_MESSAGE(PrimaryProfileChangedMsg)
    HANDLE_MESSAGE(ProfileChangedMsg)
    HANDLE_CHECK(0x15F)
END_HANDLERS
