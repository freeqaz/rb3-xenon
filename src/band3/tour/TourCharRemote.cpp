#include "tour/TourCharRemote.h"
#include "bandobj/BandCharDesc.h"
#include "game/BandUserMgr.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "tour/TourChar.h"

TourCharRemote::TourCharRemote() {}

TourCharRemote::~TourCharRemote() {}

void TourCharRemote::SyncLoad(BinStream &bs) {
    bs >> mName;
    bs >> mGuid;
    mBandCharDesc->Load(bs);
}

RndTex *TourCharRemote::GetTexAtPatchIndex(int i, bool b) const {
    BandUser *user = TheBandUserMgr->GetUserWithChar(this);
    MILO_ASSERT(user, 0x38);
    if (b && !ThePlatformMgr.CanSeeUserCreatedContent(user->GetOnlineID()))
        return nullptr;
    else
        for (int n = 0; n < unk4c.size(); n++) {
            // Operand order is load-bearing and VERIFIED on retail bytes:
            // retail word 30 of this body is 0x7F07F000 = `cmpw cr6,r7,r30`
            // (r7 = the loaded index byte, r30 = i). The Wii oracle spells
            // this `i == unk4c[n].index`, which emits 0x7F1E3800 --
            // `cmpw cr6,r30,r7`, the operands swapped -- and was the ONLY
            // non-relocation difference in the whole TourCharRemote port.
            // Writing the member first reproduces retail exactly (row now
            // 172/172 B at fuzzy 100).
            if (unk4c[n].index == i) {
                return unk4c[n].patch;
            }
        }
    return nullptr;
}

BEGIN_HANDLERS(TourCharRemote)
    HANDLE_SUPERCLASS(TourChar)
    HANDLE_CHECK(0x4A)
END_HANDLERS
