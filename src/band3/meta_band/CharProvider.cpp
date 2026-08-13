#include "meta_band/CharProvider.h"
#include "game/BandUser.h"
#include "meta_band/AppLabel.h"
#include "meta_band/BandProfile.h"
#include "meta_band/CharData.h"
#include "meta_band/PrefabMgr.h"
#include "meta_band/ProfileMgr.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "tour/TourCharLocal.h"
#include "ui/UILabel.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include <algorithm>

CharProvider::CharProvider(LocalBandUser *u, bool b1, bool b2)
    : unk20(u), unk2c(b1), unk2d(b2), unk30(-1) {}

CharProvider::~CharProvider() { Clear(); }

void CharProvider::Reload(LocalBandUser *user) {
    unk20 = user;
    Clear();
    if (unk2c) {
        static Symbol char_createnew("char_createnew");
        mCharacters.push_back(
            CharacterEntry(kCharacterEntryNew, char_createnew, 0, 0, false)
        );
    } else {
        static Symbol none("none");
        mCharacters.push_back(CharacterEntry(kCharacterEntryNone, none, 0, 0, false));
    }
    BandProfile *userprofile = TheProfileMgr.GetProfileForUser(unk20);
    if (userprofile)
        AddCharactersFromProfile(userprofile);
    else {
        std::vector<BandProfile *> profiles = TheProfileMgr.GetSignedInProfiles();
        for (std::vector<BandProfile *>::iterator it = profiles.begin(); it < profiles.end();
             ++it)
            AddCharactersFromProfile(*it);
    }
    static Symbol char_prefab("char_prefab");
    mCharacters.push_back(CharacterEntry(kCharacterEntryHeader, char_prefab, 0, 0, false)
    );

    CharData *chardata = unk20->GetChar();
    if (chardata && !chardata->IsCustomizable() && !unk2d) {
        PrefabChar *pPrefab = dynamic_cast<PrefabChar *>(chardata);
        MILO_ASSERT(pPrefab, 0x54);
        mCharacters.push_back(CharacterEntry(
            kCharacterEntryPrefab, pPrefab->GetPrefabName(), pPrefab, 0, true
        ));
    }
    int numchars = mCharacters.size();
    std::vector<PrefabChar *> prefabs;

    if (unk2d) {
        PrefabMgr::GetPrefabMgr()->GetPrefabs(prefabs);
    } else
        PrefabMgr::GetPrefabMgr()->GetAvailablePrefabs(prefabs);
    for (int i = 0; i < prefabs.size(); i++) {
        PrefabChar *pChar = prefabs[i];
        int standinidx;
        if (!userprofile || !unk2d
            || (standinidx = userprofile->GetCharacterStandinIndex(pChar)) == -1
            || standinidx == unk30) {
            mCharacters.push_back(CharacterEntry(
                kCharacterEntryPrefab, pChar->GetPrefabName(), pChar, 0, false
            ));
        }
    }
    int endidx = mCharacters.size();
    CompareCharacters cmp;
    std::sort(mCharacters.begin() + numchars, mCharacters.begin() + endidx, cmp);
}

void CharProvider::AddCharactersFromProfile(BandProfile *profile) {
    MILO_ASSERT(profile, 0x7D);
    std::vector<TourCharLocal *> chars;
    if (unk2d) {
        profile->GetAvailableStandins(unk30, chars);
    } else
        profile->GetAvailableCharacters(chars);
    CharData *pCurrentChar = unk20->GetChar();
    bool haschar = false;
    if (pCurrentChar && pCurrentChar->IsCustomizable()) {
        TourCharLocal *pLocalChar = dynamic_cast<TourCharLocal *>(pCurrentChar);
        MILO_ASSERT(pLocalChar, 0x8F);
        haschar = profile->HasChar(pLocalChar);
    }
    if (!chars.size() && !haschar)
        return;
    else {
        static Symbol char_header("char_header");
        mCharacters.push_back(
            CharacterEntry(kCharacterEntryHeader, char_header, 0, profile, false)
        );
        if (haschar && !unk2d) {
            MILO_ASSERT(pCurrentChar, 0xA0);
            MILO_ASSERT(pCurrentChar->IsCustomizable(), 0xA1);
            mCharacters.push_back(
                CharacterEntry(kCharacterEntryCustom, "", pCurrentChar, 0, true)
            );
        }
        int numchars = mCharacters.size();
        for (std::vector<TourCharLocal *>::iterator it = chars.begin(); it != chars.end();
             ++it) {
            TourCharLocal *pCharacter = *it;
            MILO_ASSERT(pCharacter, 0xAD);
            mCharacters.push_back(
                CharacterEntry(kCharacterEntryCustom, "", pCharacter, 0, false)
            );
        }
        int endidx = mCharacters.size();
        CompareCharacters cmp;
        std::sort(mCharacters.begin() + numchars, mCharacters.begin() + endidx, cmp);
    }
}

void CharProvider::Clear() { mCharacters.clear(); }

void CharProvider::Text(int, int data, UIListLabel *slot, UILabel *label) const {
    const CharacterEntry &entry = mCharacters[data];
    AppLabel *pLabel = dynamic_cast<AppLabel *>(label);
    MILO_ASSERT(pLabel, 0xC1);
    if (slot->Matches("name")) {
        switch (entry.mType) {
        case kCharacterEntryNone:
        case kCharacterEntryNew:
        case kCharacterEntryPrefab:
            pLabel->SetTextToken(entry.unk8);
            break;
        case kCharacterEntryCustom:
            pLabel->SetFromCharacter(entry.unk4);
            break;
        default:
            pLabel->SetTextToken(gNullStr);
            break;
        }
    } else {
        if (slot->Matches("header_name") && entry.mType == 4) {
            static Symbol char_header("char_header");
            if (entry.unk8 == char_header) {
                pLabel->SetTokenFmt(entry.unk8, entry.unkc->GetName());
            } else
                pLabel->SetTextToken(entry.unk8);
        } else
            pLabel->SetTextToken(gNullStr);
    }
}

RndMat *CharProvider::Mat(int, int data, UIListMesh *slot) const {
    const CharacterEntry &entry = mCharacters[data];
    if (entry.unk10 && slot->Matches("currentchar_bg")) {
        return slot->DefaultMat();
    } else if ((entry.mType == kCharacterEntryHeader && slot->Matches("header_bg"))
               || (entry.mType != kCharacterEntryHeader && slot->Matches("bg"))) {
        return slot->DefaultMat();
    } else
        return nullptr;
}

Symbol CharProvider::DataSymbol(int data) const {
    MILO_ASSERT_RANGE(data, 0, NumData(), 0x102);
    return mCharacters[data].unk8;
}

bool CharProvider::IsActive(int data) const {
    if (mCharacters.empty())
        return false;
    else {
        MILO_ASSERT_RANGE(data, 0, mCharacters.size(), 0x117);
        CharacterEntry entry = mCharacters[data];
        return entry.mType != kCharacterEntryHeader;
    }
}

CharData *CharProvider::GetCharData(int idx) {
    MILO_ASSERT_RANGE(idx, 0, mCharacters.size(), 0x11F);
    CharacterEntry entry = mCharacters[idx];
    CharData *ret = entry.unk4;
    if (entry.mType == kCharacterEntryNew || entry.mType == kCharacterEntryHeader)
        ret = nullptr;
    return ret;
}

bool CharProvider::IsIndexNewChar(int idx) {
    if (mCharacters.empty())
        return false;
    else {
        MILO_ASSERT_RANGE(idx, 0, mCharacters.size(), 0x134);
        CharacterEntry entry = mCharacters[idx];
        return entry.mType == kCharacterEntryNew;
    }
}

bool CharProvider::IsIndexNone(int idx) {
    if (mCharacters.empty())
        return false;
    else {
        MILO_ASSERT_RANGE(idx, 0, mCharacters.size(), 0x141);
        CharacterEntry entry = mCharacters[idx];
        return entry.mType == kCharacterEntryNone;
    }
}

bool CharProvider::IsIndexCustomChar(int idx) {
    if (mCharacters.empty())
        return false;
    else {
        MILO_ASSERT_RANGE(idx, 0, mCharacters.size(), 0x14E);
        CharacterEntry entry = mCharacters[idx];
        return entry.mType == kCharacterEntryCustom;
    }
}

bool CharProvider::IsIndexPrefab(int idx) {
    if (mCharacters.empty())
        return false;
    else {
        MILO_ASSERT_RANGE(idx, 0, mCharacters.size(), 0x15B);
        CharacterEntry entry = mCharacters[idx];
        return entry.mType == kCharacterEntryPrefab;
    }
}

int CharProvider::GetDefaultCharIndex() const {
    int index = 0;
    for (int i = 0; i < NumData(); i++) {
        if (mCharacters[i].unk10) {
            index = i;
            break;
        }
    }
    return index;
}

BEGIN_HANDLERS(CharProvider)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x176)
END_HANDLERS
