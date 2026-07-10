#include "bandobj/BandWardrobe.h"
#include "bandobj/BandCharDesc.h"
#include "bandobj/BandDirector.h"
#include "bandobj/BandRetargetVignette.h"
#include "char/CharInterest.h"
#include "char/CharLipSync.h"
#include "char/CharLipSyncDriver.h"
#include "char/CharFaceServo.h"
#include "char/CharWeightSetter.h"
#include "char/CharServoBone.h"
#include "obj/DataUtl.h"
#include "obj/ObjMacros.h"
#include "obj/Utl.h"
#include "rndobj/TransProxy.h"
#include "rndobj/Wind.h"
#include <algorithm>
#include "decomp.h"
#include "utl/Symbols.h"
#include "utl/Messages.h"

#define DIM(x) (sizeof(x) / sizeof((x)[0]))

BandWardrobe *TheBandWardrobe;

INIT_REVS(BandWardrobe);

const char *FlagString(int flags) {
    const char *flagstrs[] = { "FR",
                               "FD",
                               "FB",
                               "FS",
                               "MR",
                               "MD",
                               "MB",
                               "MS",
                               "Required",
                               "Dircut",
                               "Intro",
                               "FinaleArena",
                               "FinaleBigClub",
                               "FinaleFestival",
                               "FemaleGenreSplit",
                               "MaleGenreSplit" };
    const char **ptr;
    char *str = (char *)MakeString(
        "                                                                         "
    );
    ptr = flagstrs;
    char *strptr = str;
    int i5 = 0;
    int i1;
    goto loop_check;
loop_body:
    if (flags & i1) {
        if (strptr != str) {
            *strptr++ = '|';
        }
        strcpy(strptr, *ptr);
        strptr += strlen(*ptr);
    }
    ptr++;
    i5++;
loop_check:
    i1 = 1 << i5;
    if (i1 <= 0x8000) goto loop_body;
    *strptr = 0;
    return str;
}

const char *gGenres[4] = { "rocker", "dramatic", "banger", "spazz" };

int GetGenreGenderFlags(Symbol s1, Symbol s2) {
    int gv = 0;
    for (int i = 0; i < 4; i++) {
        if (s1 == gGenres[i]) {
            gv = 1 << i;
            break;
        }
    }
    MILO_ASSERT(gv, 0x62);
    return s2 == "female" ? gv : gv << 4;
}

Symbol BandWardrobe::GetCoopMode(BandCamShot *shot) {
    static const char *modes[] = { "coop_bg", "coop_bk", "coop_gk" };
    int allModes = 0x700000;
    int shotModes = shot->Flags() & 0x700000;
    Symbol curMode = GetPlayMode();
    const char *const *modePtr = &modes[2];
    const char *modeName;
    int curBit = 0;
    int i;
    for (i = 2; i >= 0; i--, modePtr--) {
        modeName = *modePtr;
        int bit = 0x100000 << i;
        if (curMode == modeName) {
            curBit = bit;
        }
        DataArray *remap = GetRemap(modeName);
        if (remap->FindArray(shot->Category(), false)) {
            allModes &= ~bit;
        }
    }
    shotModes &= allModes;
    if (curBit & shotModes) {
        return curMode;
    }
    for (int i = 0; i < 3; i++) {
        if ((0x100000 << i) & shotModes) {
            return Symbol(modes[i]);
        }
    }
    TheDebug.Notify(MakeString("%s is not valid for any modes", PathName(shot)));
    for (int i = 0; i < 3; i++) {
        if ((0x100000 << i) & allModes) {
            return Symbol(modes[i]);
        }
    }
    MILO_FAIL(
        "%s: category %s is excluded from all play modes!",
        PathName(shot),
        shot->Category()
    );
    return Symbol("coop_bg");
}

int BandWardrobe::GetShotFlags(CamShot *shot) {
    int flags = 0x100;
    if (shot) {
        const char *cat = shot->Category().Str();
        if (strncmp(cat, "directed_", 9) == 0) {
            flags |= 0x200;
            const DataNode *prop = shot->Property("free_dircuts", false);
            if (prop) {
                DataArray *proparr = prop->Array();
                for (int i = 0; i < proparr->Size(); i++) {
                    if (shot->Category() == proparr->Sym(i)) {
                        flags &= ~0x200;
                        break;
                    }
                }
            }
        } else {
            if (strstr(cat, "INTRO"))
                flags |= 0x400;
            else if (strstr(cat, "WIN_FINALE")) {
                const char *venuestr = unk78.Str();
                if (strstr(venuestr, "arena"))
                    flags = 0x800;
                else if (strstr(venuestr, "big_club"))
                    flags = 0x1000;
                else if (strstr(venuestr, "festival"))
                    flags = 0x2000;
            }
        }
    }
    if (flags & 0x600) {
        int camflags = shot->Flags();
        int u7 = camflags >> 8 & 0xF;
        if (camflags & 2)
            flags |= u7;
        if (camflags & 1)
            flags |= u7 << 4;
    } else
        flags |= 0xFF;
    return flags;
}

int BandWardrobe::TargetNames::FindTarget(Symbol s) const {
    if (!s.Null()) {
        for (int i = 0; i < 4; i++) {
            if (names[i] == s)
                return i;
        }
    }
    return -1;
}

bool BandWardrobe::DemandLoad() const {
    return LOADMGR_EDITMODE || !TheBandDirector || !mDemandLoad.Null();
}

void BandWardrobe::Init() { Register(); }

BandWardrobe::BandWardrobe()
    : unk8(0), unk14(0), unk20(this, 0), mCurNames(&mVenueNames), mVenueDir(0),
      mGenre(gGenres[0]), mTempo("medium"), mModeSink(this, 0), mShotSetPlayMode(1),
      mPlayShot5(0), mDemandLoad("") {
    static DataNode &bandwardrobe = DataVariable("bandwardrobe");
    if (TheBandWardrobe)
        MILO_WARN("Trying to make > 1 BandWardrobe, which should be single");
    bandwardrobe = DataNode(this);
    TheBandWardrobe = this;
}

BandWardrobe::~BandWardrobe() {
    if (TheBandWardrobe == this) {
        static DataNode &bandwardrobe = DataVariable("bandwardrobe");
        bandwardrobe = DataNode((Hmx::Object *)0);
        TheBandWardrobe = 0;
    }
}

void BandWardrobe::SetDir(ObjectDir *dir) {
    for (int i = 0; i < 4; i++)
        mTargets[i]->SetShowing(true);
    mVenueDir = dir;
    RndWind *worldwind = GetCharacter(0)->Find<RndWind>("world.wind", true);
    if (worldwind) {
        RndWind *wind = dir->Find<RndWind>("Wind.wind", false);
        if (wind)
            worldwind->SetWindOwner(wind);
        else {
            worldwind->SetWindOwner(worldwind);
            worldwind->Zero();
        }
    }
    SyncInterestObjects();
    for (int i = 0; i < 4; i++) {
        BandCharacter *bc = mTargets[i];
        if (bc) {
            bc->SetFocusInterest(0, 0);
            bc->EnableBlinks(true, false);
        }
    }
    SyncTransProxies();
}

void BandWardrobe::SetVenueDir(ObjectDir *dir) {
    static const char *genders[2] = { "male", "female" };
    mCurNames = &mVenueNames;
    SetDir(dir);
    SyncPlayMode();
    SetContexts("venue");
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 2; j++) {
            Character *thechar = dir->Find<Character>(
                MakeString("crowd_%s%02d", genders[j], i + 1), false
            );
            if (thechar) {
                ObjectDir *gendir =
                    thechar->Find<ObjectDir>(MakeString("%s_base", genders[j]), true);
                thechar->Driver()->SetClips(gendir);
            }
        }
    }
}

DataArray *BandWardrobe::GetRemap(Symbol s) {
    static DataArray *arr = SystemConfig("objects", "BandWardrobe");
    return arr->FindArray(MakeString("%s_remap", s), true);
}

DataArray *BandWardrobe::GetGroupArray(BandCharDesc::CharInstrumentType ty) {
    if (ty == BandCharDesc::kNumInstruments) {
        MILO_WARN("Trying to get group array for no instrument");
    }
    static DataArray *arr = SystemConfig("objects", "BandWardrobe", "anim_groups");
    return arr->Array(ty + 1);
}

void BandWardrobe::StartVenueShot(BandCamShot *shot) {
    if (!TheBandDirector) {
        Symbol coopmode = GetCoopMode(shot);
        if (mShotSetPlayMode)
            SetPlayMode(coopmode, shot);
        else if (coopmode != GetPlayMode()) {
            MILO_WARN(
                "%s is not a valid camera for current play mode %s",
                shot->Name(),
                GetPlayMode()
            );
        }
    }
    for (int i = 0; i < 4; i++) {
        BandCharacter *bc = mTargets[i];
        if (bc) {
            bc->unk5a3 = true;
            bc->unk5a2 = true;
        }
    }
}

Symbol BandWardrobe::GetPlayMode() {
    static DataNode &pm = DataVariable("band.play_mode");
    return pm.Sym();
}

void BandWardrobe::SetSongInfo(Symbol s1, Symbol s2) {
    mTempo = s1;
    mVocalGender = s2;
}

void BandWardrobe::SetSongAnimGenre(Symbol s) { mGenre = s; }

void BandWardrobe::SetPlayMode(Symbol s, BandCamShot *shot) {
    static DataNode &pm = DataVariable("band.play_mode");
    pm = DataNode(s);
    bool b1 = false;
    if (LOADMGR_EDITMODE || !TheBandDirector || !mDemandLoad.Null()) {
        b1 = true;
    }
    if (b1 && !unk78.Null()) {
        LoadMainCharacters(shot);
        SetContexts("venue");
        SyncTransProxies();
        SyncPlayMode();
        if (TheBandDirector && !shot)
            TheBandDirector->HarvestDircuts();
    }
}

void BandWardrobe::SyncPlayMode() {
    if (mModeSink) {
        static Message sync_play_mode_msg("sync_play_mode");
        mModeSink->Handle(sync_play_mode_msg, true);
    }
}

void BandWardrobe::SyncInterestObjects() {
    ObjPtrList<CharInterest, ObjectDir> clist(this, kObjListNoNull);
    for (ObjDirItr<CharInterest> it(mVenueDir, true); it; ++it) {
        clist.push_back(it);
    }
    for (int i = 0; i < 4; i++) {
        BandCharacter *bc = mTargets[i];
        if (bc)
            bc->SetInterestObjects(clist, 0);
    }
}

void BandWardrobe::SyncTransProxies() {
    for (ObjDirItr<RndTransProxy> it(mVenueDir, true); it != 0; ++it) {
        const char *thisname = it->Name();
        for (int i = 0; i < 4; i++) {
            const char *name = mCurNames->names[i].Str();
            if (*name) {
                const char *str = strstr(thisname, name);
                if (str) {
                    it->SetProxy(mTargets[i]);
                    break;
                }
            }
        }
    }
}

bool BandWardrobe::AllCharsLoaded() {
    for (int i = 0; i < 4; i++) {
        BandCharacter *bc = mTargets[i];
        if (bc && bc->IsLoading())
            return false;
    }
    return true;
}

bool BandWardrobe::DircutRecurse(BandCamShot *shot, int i) {
    for (ObjVector<BandCamShot::Target>::iterator it = shot->mTargets.begin();
         it != shot->mTargets.end();
         ++it) {
        if (!it->mAnimGroup.Null()) {
            BandCharacter *bc = FindTarget(it->mTarget, mVenueNames);
            if (bc) {
                if (!AddDircut(bc, shot, it->mAnimGroup, i))
                    return false;
            }
        }
    }
    for (ObjPtrList<BandCamShot, ObjectDir>::iterator it = shot->mNextShots.begin();
         it != shot->mNextShots.end();
         ++it) {
        if (!DircutRecurse(*it, i))
            return false;
    }
    return true;
}

void BandWardrobe::ClearDircuts() {
    for (int i = 0; i < 4; i++)
        mTargets[i]->ClearDircuts();
}

bool BandWardrobe::AddDircut(BandCamShot *shot) {
    if (shot)
        return DircutRecurse(shot, GetShotFlags(shot));
    else
        return true;
}

void BandWardrobe::SendMessage(Symbol s1, Symbol s2, bool b) {
    static Message msg("");
    if (s1 == "mic")
        s1 = "vocal";
    for (int i = 0; i < 4; i++) {
        if (strstr(mVenueNames.names[i].Str(), s1.Str())) {
            msg.SetType(s2);
            if (b)
                mTargets[i]->HandleType(msg);
            else
                mTargets[i]->Handle(msg, true);
        }
    }
}

const int gInstFocus[] = { 0x20000, 0x8000, 0x10000, 0x40000, 0x80000 };

bool BandWardrobe::ValidGenreGender(CamShot *shot) {
    int flags = shot->Flags();
    if ((flags & 0xF03) == 0xF03)
        return true;
    else {
        if (!PowerOf2(flags & 0xF8000)) {
            MILO_FAIL("%s has bad focus flags", PathName(shot));
        }
        int instnum;
        for (instnum = 0; instnum < 4; instnum++) {
            if (flags & gInstFocus[instnum])
                break;
        }
        Symbol instsym = BandCharDesc::GetInstrumentSym(instnum);
        if (instsym == "mic")
            instsym = "vocals";
        instsym = MakeString("player_%s0", instsym);
        int shotflags = GetShotFlags(shot);
        int genderflags = 0;
        BandCharacter *bc = FindTarget(instsym, mVenueNames);
        if (bc)
            genderflags = GetGenreGenderFlags(mGenre, bc->mGender);
        return (shotflags & genderflags) & 0xFF;
    }
}

BandCharacter *BandWardrobe::FindTarget(Symbol s, const TargetNames &names) {
    int idx = names.FindTarget(s);
    if (idx != -1)
        return mTargets[idx];
    else
        return 0;
}

BandCharacter *BandWardrobe::FindTarget(Symbol s) {
    MILO_ASSERT(mCurNames, 0x3E2);
    return FindTarget(s, *mCurNames);
}

DECOMP_FORCEACTIVE(BandWardrobe, "0")

int BandWardrobe::GetInstrumentForTarget(Symbol mode, int i) {
    if (mode == "coop_bk") {
        int arr[4] = { 1, 2, 3, 4 };
        return arr[i];
    } else if (mode == "coop_gk") {
        int arr[4] = { 0, 2, 3, 4 };
        return arr[i];
    } else {
        MILO_ASSERT(mode == "coop_bg", 0x420);
        int arr[4] = { 1, 2, 3, 0 };
        return arr[i];
    }
}

void BandWardrobe::LoadCharacters(Symbol s, bool b) {
    unk78 = s;
    unk7c = b;
    LoadMainCharacters(0);
}

int InstrumentIndex(std::vector<Symbol> &syms, Symbol s) {
    int idx = 0;
    for (; idx < syms.size(); idx++) {
        if (s == syms[idx])
            break;
    }
    return idx;
}

Symbol GrabInstrument(std::vector<Symbol> &syms, Symbol s) {
    int idx = InstrumentIndex(syms, s);
    if (idx == syms.size())
        idx = 0;
    s = syms[idx];
    syms.erase(syms.begin() + idx);
    return s;
}

const char *PrefabSuffix(char *c) {
    static const char *names[2] = { "_male", "_female" };
    const char *found;
    int i = 0;
    do {
        found = strstr(c, names[i]);
        if (found) {
            return (found + strlen(found) == c + strlen(c)) ? found : NULL;
        }
        i++;
    } while (i < 2);
    return NULL;
}

DataNode BandWardrobe::GetUserTrack(int i) {
    static Message msg("get_user_track", DataNode(0));
    msg[0] = DataNode(i);
    return HandleType(msg);
}
__declspec(noinline) const char *_outline_Str(Symbol *_obj) {
    return _obj->Str();
}

void BandWardrobe::LoadMainCharacters(BandCamShot *shot) {
    MILO_ASSERT(DemandLoad() || !shot, 0x45C);
    HandleType(on_loading_characters_msg);
    Symbol playmode = GetPlayMode();
    int instOrderEnd = 5;
    Symbol gender = female;
    if (shot) {
        int shotflags = GetShotFlags(shot);
        if ((shotflags & 0xFF) != 0xFF) {
            int flags = shot->Flags();
            int ff = flags & 0xF8000;
            MILO_ASSERT(PowerOf2(ff), 0x479);
            for (int i = 0; i < 5; i++) {
                if (ff == gInstFocus[i]) {
                    instOrderEnd = i;
                    if (shotflags & 0xF0) {
                        gender = "male";
                    }
                    break;
                }
            }
            int gflags = shotflags >> 4;
            if (shotflags & 0xF)
                gflags = shotflags;
            for (int i = 0; i < 4; i++) {
                if (gflags & 1 << i) {
                    mGenre = gGenres[i];
                    break;
                }
            }
        }
    }
    bool usePrefabs = LOADMGR_EDITMODE;
    if (!usePrefabs) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 2; j++) {
                if (GetPrefab(i, j)) usePrefabs = true;
            }
        }
    }
    if (usePrefabs) {
        for (int i = 0; i < 4; i++) {
            BandCharDesc *prefab = 0;
            for (int j = 0; j < 2 && !prefab; j++) {
                prefab = GetPrefab(i, j);
            }
            int forceInst = GetInstrumentForTarget(playmode, i);
            bool needOverride = false;
            bool genderMismatch = false;
            if (forceInst == instOrderEnd && prefab) needOverride = true;
            if (needOverride && prefab->mGender != gender) genderMismatch = true;
            if (genderMismatch) {
                char buf[256];
                strcpy(buf, prefab->Name());
                prefab = 0;
                for (int j = 0; j < 2; j++) {
                    BandCharDesc *p = GetPrefab(i, j);
                    if (p && p->mGender == gender) {
                        prefab = p;
                        break;
                    }
                }
                if (!prefab) {
                    char *suffix = (char *)PrefabSuffix(buf);
                    if (suffix) {
                        strcpy(suffix + 1, _outline_Str(&gender));
                        prefab = BandCharDesc::FindPrefab(buf, false);
                    }
                }
            }
            if (!prefab) {
                Symbol plat = PlatformSymbol(TheLoadMgr.GetPlatform());
                if (plat == "pc") plat = "xbox";
                Symbol gen2;
                if (forceInst == instOrderEnd) {
                    gen2 = gender;
                } else {
                    char *gs;
                    if ((i & 1)) {
                        gs = "male";
                    } else {
                        gs = "female";
                    }
                    gen2 = Symbol(gs);
                }
                prefab = BandCharDesc::FindPrefab(
                    MakeString("%s_budget_%s", gen2, plat), true
                );
                if (!prefab) {
                    MILO_WARN("could not find fallback prefab");
                }
            }
            BandCharacter *bchar = mTargets[i];
            bchar->SetPrefab(prefab);
            Symbol instSym = BandCharDesc::GetInstrumentSym(forceInst);
            bchar->SetInstrumentType(instSym);
            bchar->Enter();
        }
    } else {
        std::vector<Symbol> syms(4);
        for (int i = 0; i < 4; i++) {
            syms[i] = BandCharDesc::GetInstrumentSym(GetInstrumentForTarget(playmode, i));
        }
        int forcedTargets[4];
        int forcedCount = 0;
        for (int i = 0; i < 4; i++) {
            DataNode tracknode = GetUserTrack(i);
            Symbol inst = "none";
            if (tracknode.Type() != kDataUnhandled) {
                if (InstrumentIndex(syms, tracknode.Sym()) == syms.size()) {
                    forcedTargets[forcedCount++] = i;
                    continue;
                }
                inst = GrabInstrument(syms, tracknode.Sym());
            }
            mTargets[i]->SetInstrumentType(inst);
        }
        for (int i = 0; i < forcedCount; i++) {
            int target = forcedTargets[i];
            Symbol instOrder[5] = { "guitar", "bass", "mic", "drum", "keyboard" };
            unsigned int j;
            for (j = 0; j < 5; j++) {
                if (InstrumentIndex(syms, instOrder[j]) != syms.size())
                    break;
            }
            MILO_ASSERT(j != DIM(instOrder), 0x512);
            mTargets[target]->SetInstrumentType(GrabInstrument(syms, instOrder[j]));
        }
        if (InstrumentIndex(syms, mic) != syms.size()) {
            for (int i = 0; i < 4; i++) {
                BandCharacter *bchar = mTargets[(i + 2) % 4];
                bool ok = false;
                if (bchar->mInstrumentType == "none") {
                    bool genderOk = true;
                    if (bchar->mGender != mVocalGender && mVocalGender != gNullStr) {
                        genderOk = false;
                    }
                    if (genderOk) ok = true;
                }
                if (ok) {
                    GrabInstrument(syms, mic);
                    bchar->SetInstrumentType(mic);
                    break;
                }
            }
        }
        for (int i = 0; i < 4; i++) {
            BandCharacter *bchar = mTargets[i];
            if (bchar->mInstrumentType == "none") {
                bchar->SetInstrumentType(GrabInstrument(syms, Symbol("none")));
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        BandCharacter *bchar = mTargets[i];
        Symbol inst = bchar->mInstrumentType;
        BandCharDesc::OutfitPiece *piece = bchar->mInstruments.GetPiece(inst);
        if (piece->mName.Null()) {
            MILO_WARN(
                "NOTIFY: %s (%s) has no %s\n",
                PathName(bchar),
                mVenueNames.names[i],
                inst
            );
            BandCharDesc::CharInstrumentType type = BandCharDesc::GetInstrumentFromSym(inst);
            switch (type) {
                case BandCharDesc::kGuitar:
                    piece->mName = "kelly02_triburst";
                    break;
                case BandCharDesc::kBass:
                    piece->mName = "mb4_triburst";
                    break;
                case BandCharDesc::kDrum:
                    piece->mName = "generic_zebra";
                    break;
                case BandCharDesc::kMic:
                    piece->mName = "e935_resource";
                    break;
                case BandCharDesc::kKeyboard:
                    piece->mName = "m50_resource";
                    break;
                default:
                    MILO_WARN("hey, we shouldn't be here");
                    break;
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        BandCharacter *bchar = mTargets[i];
        Symbol inst = bchar->mInstrumentType;
        BandCharDesc::GetInstrumentFromSym(inst);
        if (inst == "none") inst = "vocals";
        mVenueNames.names[i] = MakeString("player_%s0", inst);
    }
    StartClipLoads(false, shot);
}

void BandWardrobe::StartClipLoads(bool b, BandCamShot *shot) {
    if (shot) {
        ClearDircuts();
        AddDircut(shot);
        b = true;
    }
    for (int i = 0; i < 4; i++) {
        if (b)
            mTargets[i]->SetTempoGenreVenue(mTempo, mGenre, unk78.Str());
        else
            mTargets[i]->SetTempoGenreVenue(Symbol(), Symbol(), unk78.Str());
        DataArray *mac = DataGetMacro("HX_SYSTEST");
        if (!mac && TheBandDirector->IsMusicVideo()) {
            BandCharDesc *desc = Hmx::Object::New<BandCharDesc>();
            desc->mHead.mHide = true;
            mTargets[i]->CopyCharDesc(desc);
            delete desc;
            b = false;
        } else
            mTargets[i]->StartLoad(unk7c, mTargets[i]->mInCloset, false);
    }
    FileMerger *merger = Dir()->Find<FileMerger>("crowd_clips.fm", false);
    if (merger) {
        static Message msg("load_tempo", DataNode(0), DataNode(0), DataNode(0));
        msg[0] = DataNode(b ? mTempo : Symbol());
        msg[1] = DataNode(unk7c);
        merger->HandleType(msg);
    }
}

void BandWardrobe::SetContexts(Symbol s) {
    for (int i = 0; i < 4; i++) {
        mTargets[i]->SetContext(s);
    }
}

#define kNumTargets 4

BandCharacter *BandWardrobe::GetCharacter(int which) const {
    MILO_ASSERT(which >= 0 && which < kNumTargets, 0x5AC);
    return mTargets[which];
}

DECOMP_FORCEACTIVE(BandWardrobe, "Bandcharacter is not target")

bool BandWardrobe::AddDircut(BandCharacter *bchar, BandCamShot *shot, Symbol cat, int ff) {
    if (!bchar)
        MILO_FAIL("BandWardrobe::AddDircut character is NULL");
    Symbol animinst = BandCharDesc::GetAnimInstrument(bchar->mInstrumentType);
    DataArray *grouparr =
        GetGroupArray(BandCharDesc::GetInstrumentFromSym(animinst));
    int flag = -1;
    for (int i = 0; i < grouparr->Size(); i++) {
        if (cat == grouparr->Array(i)->Sym(0)) {
            flag = grouparr->Array(i)->Int(1);
            break;
        }
    }
    if (flag == -1) {
        MILO_NOTIFY_ONCE(
            "%s could not find directed cut group %s for inst %s",
            PathName(shot),
            cat,
            animinst
        );
        return true;
    }
    if (flag & 0x100)
        return true;
    if ((flag & 0x800) || (flag & 0x1000) || (flag & 0x2000))
        return true;
    if ((ff & 0x400) && !(flag & 0x400)) {
        MILO_WARN("%s intro camera looking for non-intro anim group", PathName(shot));
        return true;
    }
    int genderflags = GetGenreGenderFlags(mGenre, bchar->mGender);
    if (genderflags != (genderflags & flag)) {
        MILO_WARN(
            "%s can't load %s, group is %s, character is %s",
            PathName(shot),
            cat,
            FlagString(flag),
            FlagString(genderflags)
        );
        return true;
    }
    return bchar->AddDircut(cat, mGenre, flag);
}

void BandWardrobe::SelectExtra(FileMerger::Merger &merger) {
    FilePathTracker tracker(FileRoot());
    ObjectDir *dir = merger.mDir;
    if (!dir)
        return;
    DataNode node = dir->PropertyArray("proxies");
    DataArray *proparr = node.Array();
    for (std::list<Symbol>::iterator it = unk2c.begin(); it != unk2c.end(); ++it) {
        Symbol cur = *it;
        for (int i = 0; i < proparr->Size(); i++) {
            if (cur == proparr->Sym(i)) {
                unk2c.erase(it);
                unk2c.push_back(cur);
                merger.SetSelected(FilePath(MakeString("char/extras/%s.milo", cur)), false);
                return;
            }
        }
    }
    MILO_FAIL("Couldn't find match!");
}

void BandWardrobe::LoadPrefabPrefs() {
    if (LOADMGR_EDITMODE) {
        for (int i = 0; i < 4; i++) {
            BandCharDesc *desc = 0;
            for (int j = 0; j < 2 && !desc; j++) {
                desc = GetPrefab(i, j);
            }
            if (desc) {
                BandCharacter *bchar = mTargets[i];
                bchar->SetInstrumentType(bchar->mInstrumentType);
                if (bchar->SetPrefab(desc)) {
                    bchar->StartLoad(false, false, false);
                }
            }
        }
    }
}

BandCharDesc *BandWardrobe::GetPrefab(int target, int variation) {
    MILO_ASSERT(target < kNumTargets && target >= 0, 0x6AF);
    MILO_ASSERT(variation < 2 && target >= 0, 0x6B0);
    if (!mDemandLoad.Null()) {
        char buf[256];
        Symbol plat = PlatformSymbol(TheLoadMgr.GetPlatform());
        if (plat == "pc") plat = Symbol("xbox");
        strcpy(buf, MakeString("%s_%s", plat, mDemandLoad));
        if (variation == 1) {
            char *suffix = (char *)PrefabSuffix(buf);
            const char *found = strstr(suffix, "female");
            strcpy(suffix + 1, found ? "male" : "female");
        }
        return BandCharDesc::FindPrefab(buf, false);
    } else {
        Symbol prefabsym = MakeString("milopref_prefab%d_%c", target, variation + 'a');
        Symbol findsym =
            DataVarExists(prefabsym) ? DataVariable(prefabsym).Sym() : Symbol();
        if (!findsym.Null())
            return BandCharDesc::FindPrefab(findsym.Str(), false);
        else
            return 0;
    }
}

BEGIN_SAVES(BandWardrobe)
    SAVE_REVS(5, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mGenre;
    bs << mTempo;
    bs << mVocalGender;
    bs << GetPlayMode();
    bs << mShotSetPlayMode;
    bs << mPlayShot5;
END_SAVES

BEGIN_LOADS(BandWardrobe)
    LOAD_REVS(bs)
    ASSERT_REVS(5, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    if (gRev != 0) {
        Symbol s;
        bs >> mGenre;
        bs >> mTempo;
        if (gRev > 4)
            bs >> mVocalGender;
        bs >> s;
        bs >> mShotSetPlayMode;
        bs >> mPlayShot5;
        if (gRev == 2 || gRev == 3) {
            Symbol s2;
            bs >> s2;
            if (gRev > 2) {
                bs >> s2;
                bs >> s2;
                bs >> s2;
            }
        }
        SetPlayMode(s, 0);
    }
    if (Dir()) {
        for (int i = 0; i < 4; i++) {
            mTargets[i] = Dir()->Find<BandCharacter>(MakeString("player%d", i), false);
        }
    }
END_LOADS

BEGIN_COPYS(BandWardrobe)
    COPY_SUPERCLASS(Hmx::Object)
END_COPYS

BEGIN_HANDLERS(BandWardrobe)
    HANDLE(find_target, OnFindTarget)
    HANDLE_ACTION(start_venue_shot, StartVenueShot(_msg->Obj<BandCamShot>(2)))
    HANDLE(enter_venue, OnEnterVenue)
    HANDLE(unload_venue, OnUnloadVenue)
    HANDLE(enter_closet, OnEnterCloset)
    HANDLE(enter_vignette, OnEnterVignette)
    HANDLE(select_extras, OnSelectExtras)
    HANDLE(on_extra_loaded, OnExtraLoaded)
    HANDLE_EXPR(chars_dir, Dir())
    HANDLE_EXPR(get_character, GetCharacter(_msg->Int(2)))
    HANDLE(list_venue_anim_groups, OnListVenueAnimGroups)
    HANDLE(sort_targets, OnSortTargets)
    HANDLE_EXPR(prefabs_list, ObjectList(BandCharDesc::GetPrefabs(), "BandCharDesc", true))
    HANDLE(get_matching_dude, OnGetMatchingDude)
    HANDLE(list_interest_objects, OnGetCurrentInterests)
    HANDLE(enable_debug_interests, OnEnableDebugInterests)
    HANDLE_ACTION(load_prefab_prefs, LoadPrefabPrefs())
    HANDLE_ACTION(sync_interests, SyncInterestObjects())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x73A)
END_HANDLERS

DataNode BandWardrobe::OnFindTarget(DataArray *da) {
    if (LOADMGR_EDITMODE && da->Size() > 3) {
        StartVenueShot(da->Obj<BandCamShot>(3));
    }
    return DataNode(FindTarget(da->Sym(2), *mCurNames));
}

DataNode BandWardrobe::OnEnterVenue(DataArray *da) {
    MILO_ASSERT(!TheBandDirector, 0x750);
    ObjectDir *dir = da->Obj<ObjectDir>(2);
    MILO_ASSERT(dir, 0x752);
    LoadCharacters(dir->Name(), false);
    SetVenueDir(dir);
    return DataNode(0);
}

DataNode BandWardrobe::OnUnloadVenue(DataArray *da) {
    for (int i = 0; i < 4; i++) {
        BandCharacter *bc = GetCharacter(i);
        if (bc) {
            bc->ClearDircuts();
            bc->SetTempoGenreVenue(Symbol(), Symbol(), "");
            bc->SetInstrumentType(Symbol());
            bc->StartLoad(false, bc->mInCloset, true);
        }
    }
    return DataNode(0);
}

DataNode BandWardrobe::OnGetCurrentInterests(DataArray *da) {
    int playerIdx = da->Int(2);
    MILO_ASSERT(playerIdx < kNumTargets, 0x772);
    if (mTargets[playerIdx])
        return mTargets[playerIdx]->OnGetCurrentInterests(0);
    else {
        DataArray *arr = new DataArray(1);
        arr->Node(0) = DataNode(Symbol());
        DataNode ret(arr, kDataArray);
        arr->Release();
        return DataNode(ret);
    }
}

DataNode BandWardrobe::OnEnableDebugInterests(DataArray *da) {
    int playerIdx = da->Int(2);
    bool i3 = da->Int(3);
    MILO_ASSERT(playerIdx < kNumTargets, 0x785);
    if (mTargets[playerIdx]) {
        mTargets[playerIdx]->SetDebugDrawInterestObjects(i3);
    }
    return DataNode(0);
}

DataNode BandWardrobe::OnEnterCloset(DataArray *da) {
    ObjectDir *dir = da->Obj<ObjectDir>(2);
    MILO_ASSERT(dir, 0x795);
    if (dir) {
        int i3 = da->Int(3);
        if (i3 != -1) {
            mCurNames = &mClosetNames;
            SetContexts("closet");
            CharDriver *driver = mTargets[i3]->Driver();
            if (driver) {
                driver->SetClips(dir->Find<ObjectDir>("clips", false));
                for (int i = 0; i < 4; i++) {
                    mClosetNames.names[i] = i == i3 ? "closet_character" : "";
                }
                SetDir(dir);
                for (int i = 0; i < 4; i++) {
                    mTargets[i]->SetShowing(i == i3);
                }
            }
        }
    }
    return DataNode(0);
}

int BandWardrobe::MostImportantHuman(const SlotInfo *info) {
    int best = -1;
    for (int i = 0; i < 4; i++) {
        if (info[i].hint == -1) {
            if (best != -1) {
                bool better;
                if (info[i].human != info[best].human) {
                    better = info[i].human;
                } else {
                    better = info[i].score < info[best].score;
                }
                if (!better)
                    continue;
            }
            best = i;
        }
    }
    return best;
}

DataNode BandWardrobe::OnGetMatchingDude(DataArray *da) {
    BandCharacter *target = da->Obj<BandCharacter>(2);
    for (int i = 0; i < 4; i++) {
        BandCharacter *bc = GetCharacter(i);
        if (bc) {
            bool found = false;
            bool check = false;
            if (bc && bc->Driver() && bc != target)
                check = true;
            if (check) {
                Symbol a = BandCharDesc::GetAnimInstrument(target->mInstrumentType);
                Symbol b = BandCharDesc::GetAnimInstrument(bc->mInstrumentType);
                if (b == a)
                    found = true;
            }
            if (found)
                return DataNode(bc);
        }
    }
    return DataNode((Hmx::Object *)0);
}

void BandWardrobe::InstrumentMatch(
    int *scores, const SlotInfo *info, int hint, int &bestScore, int &bestSlot, int &bestHint
) {
    for (int i = 0; i < 4; i++) {
        if (info[i].hint == -1) {
            BandCharDesc::CharInstrumentType it =
                BandCharDesc::GetInstrumentFromSym(info[i].inst);
            MILO_ASSERT(it >= 0 && it < BandCharDesc::kNumInstruments, 0x813);
            int score = scores[it];
            bool nonZero = (unsigned int)(-score | score) >> 31;
            bool match = nonZero;
            if (nonZero) {
                int diff = score - info[i].human;
                if (diff < bestScore) {
                    bestScore = diff;
                    match = true;
                } else {
                    match = false;
                }
            }
            if (match) {
                bestSlot = i;
                bestHint = hint;
            }
        }
    }
}

int BandWardrobe::FindBestScoringHint(Symbol *hints, SlotInfo *info, int &outSlot) {
    static const int scores[5][5] = {
        { 8, 12, 0, 0, 16 },
        { 14, 10, 0, 0, 16 },
        { 0, 0, 2, 0, 0 },
        { 0, 0, 0, 4, 0 },
        { 16, 16, 0, 0, 6 },
    };
    int bestScore = 10000;
    outSlot = -1;
    int result = -1;
    for (int i = 0; i < 4; i++) {
        Symbol hint = hints[i];
        if (hint == done)
            continue;
        if (hint == "customize") {
            bool ok;
            if (bestScore > 0) {
                bestScore = 0;
                ok = true;
            } else {
                ok = false;
            }
            if (!ok)
                continue;
            int _tmp0 = HandleType(get_customize_slot_msg).Int();
            outSlot = _tmp0;
            result = i;
        } else if (strncmp("importance", hint.Str(), 10) == 0) {
            int score = hint.Str()[10] - 0x20;
            bool ok;
            if (score < bestScore) {
                bestScore = score;
                ok = true;
            } else {
                ok = false;
            }
            if (!ok)
                continue;
            outSlot = -1;
            result = i;
        } else if (hint.Null()) {
            int score = i + 0x15;
            bool ok;
            if (score < bestScore) {
                bestScore = score;
                ok = true;
            } else {
                ok = false;
            }
            if (!ok)
                continue;
            outSlot = -1;
            result = i;
        } else if (hint == "slot") {
            int score = i + 0x15;
            bool ok;
            if (score < bestScore) {
                bestScore = score;
                ok = true;
            } else {
                ok = false;
            }
            if (!ok)
                continue;
            outSlot = i;
            result = i;
        } else {
            BandCharDesc::CharInstrumentType type =
                BandCharDesc::GetInstrumentFromSym(hint);
            if (type == 5) {
                MILO_FAIL("Bad hint value %s got in here!", hint);
            } else {
                InstrumentMatch(
                    (int *)scores[type], info, i, bestScore, outSlot, result
                );
            }
        }
    }
    return result;
}

DataNode BandWardrobe::OnEnterVignette(DataArray *da) {
    static const char *player_names[4] = {
        "player0", "player1", "player2", "player3"
    };
    WorldDir *worldDir = da->Obj<WorldDir>(2);
    mCurNames = &mVignetteNames;
    LoadPrefabPrefs();
    SetContexts(Symbol("vignette"));
    ObjectDir *charsDir = dynamic_cast<ObjectDir *>(worldDir->FindObject("clips", false));
    if (charsDir) {
        for (ObjDirItr<Character> it(worldDir, true); it; ++it) {
            CharDriver *driver = it->Driver();
            const char *name = it->Name();
            if (!driver) {
                TheDebug.Notify(MakeString(
                    "%s has no main.drv, not valid character", PathName(it)
                ));
            } else {
                if (strstr(name, "extra")) {
                    driver->SetClipType(vignette);
                    it->BoneServo()->SetClipType(vignette);
                    ObjectDir *visemes = dynamic_cast<ObjectDir *>(
                        it->FindObject("vignette_visemes", false)
                    );
                    if (visemes) {
                        CharLipSyncDriver *lsd = dynamic_cast<CharLipSyncDriver *>(
                            it->FindObject("vignette.lipdrv", false)
                        );
                        if (lsd)
                            lsd->SetClips(visemes);
                        CharFaceServo *fs = dynamic_cast<CharFaceServo *>(
                            it->FindObject("face.faceservo", false)
                        );
                        if (fs)
                            fs->SetClips(visemes);
                    }
                    CharWeightSetter *ws = dynamic_cast<CharWeightSetter *>(
                        it->FindObject("venue.weight", false)
                    );
                    if (ws)
                        ws->SetWeight(0.0f);
                    CharLipSyncDriver *lsd2 = dynamic_cast<CharLipSyncDriver *>(
                        it->FindObject("vignette.lipdrv", false)
                    );
                    if (lsd2) {
                        CharLipSync *ls = dynamic_cast<CharLipSync *>(
                            charsDir->FindObject(MakeString("%s.lipsync", name), false)
                        );
                        lsd2->SetLipSync(ls);
                        lsd2->Sync();
                    }
                }
                it->Driver()->SetClips(charsDir);
            }
        }
        if (TheLoadMgr.EditMode()) {
            for (int i = 0; i < 4; i++) {
                mVignetteNames.names[i] = Symbol(player_names[i]);
            }
        } else {
            static Message msg("get_slot_info", DataNode(0));
            SlotInfo info[4];
            for (int i = 0; i < 4; i++)
                info[i].inst = Symbol();
            bool hasBass = false;
            for (int i = 0; i < 4; i++) {
                info[i].hint = -1;
                msg[0] = DataNode(i);
                DataArray *result = HandleType(msg).Array();
                info[i].human = result->Int(0) != 0;
                info[i].inst = result->Sym(1);
                info[i].score = 1.0f - result->Float(2);
                if (info[i].inst == "bass")
                    hasBass = true;
            }
            if (info[1].inst.Null())
                info[1].inst = Symbol("drum");
            if (info[2].inst.Null())
                info[2].inst = Symbol("mic");
            Symbol fallback(hasBass ? "guitar" : "bass");
            if (info[0].inst.Null())
                info[0].inst = fallback;
            if (info[3].inst.Null())
                info[3].inst = fallback;
            Symbol hints[4];
            for (int i = 0; i < 4; i++)
                hints[i] = Symbol();
            Hmx::Object *hintsDir = worldDir->FindObject("player_hints.obj", false);
            if (hintsDir) {
                for (int i = 0; i < 4; i++) {
                    const DataNode *prop = hintsDir->Property(
                        Symbol(MakeString("player%d_hint", i)), false
                    );
                    if (prop)
                        hints[i] = prop->Sym();
                }
            }
            int slot;
            int idx;
            while ((idx = FindBestScoringHint(hints, info, slot)) != -1) {
                if (slot == -1)
                    slot = MostImportantHuman(info);
                hints[idx] = Symbol("done");
                info[slot].hint = idx;
                mVignetteNames.names[slot] = Symbol(player_names[idx]);
            }
        }
    }
    SetDir(worldDir);
    BandRetargetVignette *brv = dynamic_cast<BandRetargetVignette *>(
        worldDir->FindObject("BandRetargetVignette.brv", false)
    );
    if (brv)
        brv->EnterDir();
    if (charsDir) {
        for (int i = 0; i < 4; i++) {
            BandCharacter *bc = TheBandWardrobe->GetCharacter(i);
            Symbol name = mVignetteNames.names[i];
            CharLipSyncDriver *lsd = dynamic_cast<CharLipSyncDriver *>(
                bc->FindObject("vignette.lipdrv", false)
            );
            if (lsd) {
                CharLipSync *ls = dynamic_cast<CharLipSync *>(
                    charsDir->FindObject(MakeString("%s.lipsync", name), false)
                );
                lsd->SetLipSync(ls);
                lsd->Sync();
            }
        }
    }
    return DataNode(0);
}

void BandWardrobe::SyncVignetteInterest(int playerIdx) {
    MILO_ASSERT(playerIdx < kNumTargets, 0x876);
    BandCharacter *bc = FindTarget(mCurNames->names[playerIdx], *mCurNames);
    if (bc) {
        bc->Character::SetFocusInterest(mPlayerForcedFocuses[playerIdx], 0);
    }
}

void BandWardrobe::SyncEnableBlinks(int playerIdx) {
    MILO_ASSERT(playerIdx < kNumTargets, 0x883);
    BandCharacter *bc = FindTarget(mCurNames->names[playerIdx], *mCurNames);
    if (bc) {
        bc->EnableBlinks(mPlayerEnableBlinks[playerIdx], false);
    }
}

void BandWardrobe::ForceBlink(int playerIdx) {
    MILO_ASSERT(playerIdx < kNumTargets, 0x891);
    BandCharacter *bc = FindTarget(mCurNames->names[playerIdx], *mCurNames);
    if (bc) {
        bc->ForceBlink();
    }
}

DataNode BandWardrobe::OnListVenueAnimGroups(DataArray *da) {
    MILO_ASSERT(TheLoadMgr.EditMode(), 0x947);
    BandCamShot *shot = da->Obj<BandCamShot>(3);
    StartVenueShot(shot);
    Symbol sym = da->Sym(2);
    BandCharacter *bchar = FindTarget(sym, mVenueNames);
    if (bchar) {
        return bchar->ListAnimGroups(GetShotFlags(shot));
    } else {
        Character *chr = mVenueDir->Find<Character>(sym.Str(), false);
        if (chr)
            return DataNode(0);
        else {
            DataArray *arr = new DataArray(1);
            arr->Node(0) = DataNode(Symbol());
            DataNode ret(arr, kDataArray);
            arr->Release();
            return DataNode(ret);
        }
    }
}

DataNode BandWardrobe::OnExtraLoaded(DataArray *da) {
    Character *c1 = da->Obj<Character>(2);
    Character *c2 = da->Obj<Character>(3);
    if (c1 && c2) {
        c2->CopyBoundingSphere(c1);
        c2->SetShowing(false);
    }
    return DataNode(0);
}

DataNode BandWardrobe::OnSelectExtras(DataArray *da) {
    FileMerger *merger = da->Obj<FileMerger>(2);
    if (unk20 != merger) {
        ObjectDir *mergerdir = merger->Dir();
        merger->Clear();
        merger->Mergers().clear();
        unk20 = merger;
        unk2c.clear();
        for (ObjectDir::Entry *e = mergerdir->HashTable().Begin(); e != 0;
             e = mergerdir->HashTable().Next(e)) {
            Hmx::Object *o = e->obj;
            if (o->Type() == extras && o->ClassName() == "Character") {
                FileMerger::Merger m(unk20);
                m.mName = o->Name();
                m.mDir = dynamic_cast<ObjectDir *>(o);
                m.mSubdirs = (MergeFilter::Subdirs)3;
                unk20->Mergers().push_back(m);
                DataNode propnode = o->PropertyArray(proxies);
                DataArray *proparr = propnode.Array();
                for (int i = 0; i < proparr->Size(); i++) {
                    unk2c.push_back(proparr->Sym(i));
                }
            }
        }
        unk2c.sort();
        unk2c.unique();
        int size = unk2c.size();
        for (std::list<Symbol>::iterator it = unk2c.begin(); it != unk2c.end(); ++it) {
            int rint = RandomInt(0, size--);
            std::list<Symbol>::iterator cur = it;
            while (rint-- != 0)
                ++cur;
            std::swap<Symbol>(*cur, *it);
        }
    }
    for (int i = 0; i < merger->Mergers().size(); i++) {
        SelectExtra(merger->Mergers()[i]);
    }
    std::sort(
        merger->Mergers().begin(),
        merger->Mergers().end(),
        FileMerger::Merger::SortBySelected()
    );
    return DataNode(0);
}

int NodeCmp(const void *a, const void *b) {
    DataNode *na = (DataNode *)a;
    DataNode *nb = (DataNode *)b;
    const char *stra = na->Str();
    const char *strb = nb->Str();
    bool hasA = strstr(stra, ".tp") != 0;
    bool hasB = strstr(strb, ".tp") != 0;
    if (hasA == hasB) {
        return stricmp(stra, strb);
    } else
        return hasA ? -1 : 1;
}

DataNode BandWardrobe::OnSortTargets(DataArray *da) {
    DataArray *arr = da->Array(2);
    qsort(&arr->Node(0), arr->Size(), 8, NodeCmp);
    return DataNode(0);
}

BEGIN_PROPSYNCS(BandWardrobe)
    SYNC_PROP(genre, mGenre)
    SYNC_PROP(tempo, mTempo)
    SYNC_PROP(vocal_gender, mVocalGender)
    SYNC_PROP_SET(play_mode, GetPlayMode(), SetPlayMode(_val.Sym(), 0))
    SYNC_PROP(shot_set_play_mode, mShotSetPlayMode)
    SYNC_PROP(play_shot_5, mPlayShot5)
    SYNC_PROP_MODIFY(
        player0_forced_focus, mPlayerForcedFocuses[0], SyncVignetteInterest(0)
    )
    SYNC_PROP_MODIFY(
        player1_forced_focus, mPlayerForcedFocuses[1], SyncVignetteInterest(1)
    )
    SYNC_PROP_MODIFY(
        player2_forced_focus, mPlayerForcedFocuses[2], SyncVignetteInterest(2)
    )
    SYNC_PROP_MODIFY(
        player3_forced_focus, mPlayerForcedFocuses[3], SyncVignetteInterest(3)
    )
    SYNC_PROP_MODIFY(player0_enable_blinks, mPlayerEnableBlinks[0], SyncEnableBlinks(0))
    SYNC_PROP_MODIFY(player1_enable_blinks, mPlayerEnableBlinks[1], SyncEnableBlinks(1))
    SYNC_PROP_MODIFY(player2_enable_blinks, mPlayerEnableBlinks[2], SyncEnableBlinks(2))
    SYNC_PROP_MODIFY(player3_enable_blinks, mPlayerEnableBlinks[3], SyncEnableBlinks(3))
    SYNC_PROP_SET(player0_force_blink, 0, if (_val.Int()) ForceBlink(0))
    SYNC_PROP_SET(player1_force_blink, 0, if (_val.Int()) ForceBlink(1))
    SYNC_PROP_SET(player2_force_blink, 0, if (_val.Int()) ForceBlink(2))
    SYNC_PROP_SET(player3_force_blink, 0, if (_val.Int()) ForceBlink(3))
    SYNC_PROP(demand_load, mDemandLoad)
    SYNC_PROP(dir, mVenueDir)
END_PROPSYNCS
