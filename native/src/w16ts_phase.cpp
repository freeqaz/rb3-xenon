// rb3-xenon native -- W16-TS: in-scope rows native linked but no target entered.
//
// docs/decomp/CAMPAIGN_STATE_2026-10-07c.md section 6, lever 2: of the in-scope
// behaviour-class rows native compiles, 69 / 49,512 B were entered by no target
// (W16-TN's native_runtime_rank run). This phase runs the largest of them, and
// the four the coordinator named, in rb3-render's default mode, after W16-TM:
//
//   * BandSongMetadata::HasPart(Symbol, bool) and SongSortMgr::DoesSongMatchFilter
//     over every disc song in the shipped songs/songs.dta;
//   * ModifierMgr::IsActive / IsHidden / IsModifierActive over the shipped
//     config/modifiers.dta;
//   * BandDirector::OnFileLoaded over a shipped song milo
//     (songs/20thcenturyboy/gen/20thcenturyboy.milo) and over no milo at all;
//   * UIStats::MaybePublish over UIScreens typed by shipped screen definitions;
//   * SaveLoadManager::SetState's cache/memcard-free transition arms.
//
// THE REFERENCE. Every expected value comes from the shipped files read directly
// (DataReadFile, or this file's own walk of a loaded dir), or from the retail
// transition read off the retail asm and written down here, never from the code
// under test. A fixture gate checks each fixture first, so a later failure points
// at the function under test.

#include "bandobj/BandDirector.h"
#include "bandobj/BandSongPref.h"
#include "char/CharLipSync.h"
#include "char/FileMerger.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/GameMode.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/SongSortMgr.h"
#include "meta_band/SongUpgradeMgr.h"
#include "meta_band/UIStats.h"
#include "net/NetCore.h"
#include "net/Server.h"
#include "obj/DataUtl.h"
#include "os/Joypad.h"
#include "os/OnlineID.h"
#include "ui/UIScreen.h"
#include "utl/DataPointMgr.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Msg.h"
#include "rndobj/PropAnim.h"
#include "rndobj/PropKeys.h"
#include "utl/FilePath.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

extern DataArray *gSystemConfig;

extern Symbol hidden; // w16ts_link_support.cpp

typedef void (*GateFn)(const char *, bool, const char *);

namespace {

GateFn gGate = nullptr;
char gBuf[1024];
int gRan = 0;

void Gate(const char *name, bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Gate(const char *name, bool ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gBuf, sizeof(gBuf), fmt, ap);
    va_end(ap);
    gGate(name, ok, gBuf);
    gRan++;
}

// ======================================================= config splicing ==
// Same construction as W16-TJ's: the section is built from the shipped file the
// game's band_keep.dta #includes, unless an earlier phase already installed it.
DataArray *Splice(const char *name, const char *file) {
    DataArray *have = SystemConfig()->FindArray(name, false);
    if (have)
        return have;
    DataArray *body = DataReadFile(file, true);
    if (!body)
        return nullptr;
    DataArray *sec = new DataArray(1 + body->Size());
    sec->Node(0) = Symbol(name);
    for (int i = 0; i < body->Size(); i++)
        sec->Node(i + 1) = body->Node(i);
    body->Release();
    int n = gSystemConfig->Size();
    gSystemConfig->Resize(n + 1);
    gSystemConfig->Node(n) = DataNode(sec, kDataArray);
    sec->Release();
    return SystemConfig()->FindArray(name, false);
}

// ============================================================ song fixture ==
// One disc song as songs.dta spells it, read without BandSongMetadata.
struct RawSong {
    std::string shortName;
    int id = 0;
    int year = 0;
    int rating = 0;
    std::string artist;
    std::map<std::string, float> rank;
    bool Has(const char *part) const {
        std::map<std::string, float>::const_iterator it = rank.find(part);
        return it != rank.end() && it->second > 0;
    }
};

std::vector<RawSong> gSongs;

DataArray *Sub(DataArray *e, const char *key) {
    for (int i = 1; i < e->Size(); i++) {
        if (e->Type(i) != kDataArray)
            continue;
        DataArray *a = e->Array(i);
        if (a->Size() > 0 && a->Type(0) == kDataSymbol && !strcmp(a->Sym(0).Str(), key))
            return a;
    }
    return nullptr;
}

bool BuildSongs() {
    DataArray *root = DataReadFile("songs/songs.dta", true);
    if (!root)
        return false;
    for (int i = 0; i < root->Size(); i++) {
        if (root->Type(i) != kDataArray)
            continue;
        DataArray *e = root->Array(i);
        if (e->Size() < 2 || e->Type(0) != kDataSymbol)
            continue;
        RawSong s;
        s.shortName = e->Sym(0).Str();
        DataArray *a;
        if ((a = Sub(e, "song_id")))
            s.id = a->Int(1);
        if ((a = Sub(e, "year_released")))
            s.year = a->Int(1);
        if ((a = Sub(e, "rating")))
            s.rating = a->Int(1);
        if ((a = Sub(e, "artist")))
            s.artist = a->Str(1);
        if ((a = Sub(e, "rank"))) {
            for (int k = 1; k < a->Size(); k++) {
                DataArray *r = a->Array(k);
                s.rank[r->Sym(0).Str()] = r->Float(1);
            }
        }
        if (s.id)
            gSongs.push_back(s);
    }
    // The real song manager over the same file (W16-TJ's construction; skipped
    // when TJ's phase already filled it).
    if (!TheSongMgr.GetSongIDFromShortName(gSongs.empty() ? "" : gSongs[0].shortName.c_str(),
                                           false)) {
        TheSongMgr.SongMgr::Init();
        if (!TheSongMgr.mUpgradeMgr)
            TheSongMgr.mUpgradeMgr = new SongUpgradeMgr();
        TheSongMgr.AddSongData(root, nullptr, kLocationRoot);
    }
    root->Release();
    int found = 0;
    for (const RawSong &s : gSongs) {
        if (TheSongMgr.GetSongIDFromShortName(s.shortName.c_str(), false) == s.id
            && TheSongMgr.Data(s.id))
            found++;
    }
    Gate("ts-songs", !gSongs.empty() && found == (int)gSongs.size(),
         "songs.dta: %d entries with a song_id; the real BandSongMgr resolves %d of them by "
         "short name to the same id with metadata",
         (int)gSongs.size(), found);
    return found == (int)gSongs.size() && found > 0;
}

BandSongMetadata *Md(const RawSong &s) { return (BandSongMetadata *)TheSongMgr.Data(s.id); }

// HasPart(Symbol, bool): the rank block, except that real_guitar / real_bass go
// to the song's upgrade data first (no disc song has one) and that a session's
// machine manager can veto the part (none exists here: TheSessionMgr is null).
void HasPartChecks() {
    static const char *parts[] = { "drum", "guitar", "bass", "vocals", "keys",
                                   "real_keys", "real_guitar", "real_bass", "band" };
    int bad = 0, n = 0, yes = 0;
    for (const RawSong &s : gSongs) {
        BandSongMetadata *md = Md(s);
        for (const char *p : parts) {
            bool want = s.Has(p);
            for (int b = 0; b < 2; b++) {
                bool got = md->HasPart(Symbol(p), b != 0);
                n++;
                if (got != want) {
                    if (bad < 4)
                        printf("  %s %s (%d): HasPart=%d, songs.dta rank %.0f\n",
                               s.shortName.c_str(), p, b, got, s.rank.count(p) ? s.rank.at(p) : -1.f);
                    bad++;
                }
            }
            yes += want;
        }
    }
    // A part absent from the rank block reads false on both sides.
    bool absent = !Md(gSongs[0])->HasPart(Symbol("not_a_part"), false);
    Gate("hp-rank", bad == 0 && n > 0 && absent,
         "%d (song, part, flag) queries over %d songs x 9 parts: HasPart = songs.dta rank > 0 "
         "(%d parts ranked), %d wrong; an unranked symbol reads false",
         n, (int)gSongs.size(), yes, bad);
}

// DoesSongMatchFilter. The reference ANDs each active filter over songs.dta's
// own fields: decade from year_released, "rating_N" from rating, the artist
// string, has_part_yes/no from the real_guitar/real_bass ranks, from the keys /
// real_keys ranks, a required part from its rank, and the excluded-id list.

std::string DecadeOf(int year) {
    char b[16];
    snprintf(b, sizeof(b), "the%is", year - year % 10);
    return b;
}

void FilterChecks() {
    // DoesSongMatchFilter reads no member (only TheSongMgr), so the manager is
    // zeroed storage: its ctor builds the nine sort objects, whose TUs this
    // target does not link.
    SongSortMgr *mgr = (SongSortMgr *)calloc(1, sizeof(SongSortMgr));
    // Pick filter values that split the song list: the most common decade, a
    // rating present in the list, the first song's artist.
    std::map<std::string, int> decades, ratings;
    for (const RawSong &s : gSongs) {
        decades[DecadeOf(s.year)]++;
        char b[16];
        snprintf(b, sizeof(b), "rating_%i", s.rating);
        ratings[b]++;
    }
    std::string decade, decade2, rating;
    int best = 0;
    for (auto &d : decades)
        if (d.second > best) {
            best = d.second;
            decade = d.first;
        }
    for (auto &d : decades)
        if (d.first != decade && (decade2.empty() || d.second > decades[decade2]))
            decade2 = d.first;
    best = 0;
    for (auto &r : ratings)
        if (r.second > best) {
            best = r.second;
            rating = r.first;
        }
    std::string artist = gSongs[0].artist;

    struct Case {
        const char *name;
        SongSortMgr::SongFilter f;
        std::function<bool(const RawSong &)> want;
    };
    std::vector<Case> cases;
    auto add = [&](const char *name, std::function<void(SongSortMgr::SongFilter &)> build,
                   std::function<bool(const RawSong &)> want) {
        Case c;
        c.name = name;
        build(c.f);
        c.want = want;
        cases.push_back(c);
    };
    add("empty filter", [](SongSortMgr::SongFilter &) {}, [](const RawSong &) { return true; });
    add("decade", [&](SongSortMgr::SongFilter &f) { f.filters[kFilterDecade].insert(decade.c_str()); },
        [&](const RawSong &s) { return DecadeOf(s.year) == decade; });
    add("two decades",
        [&](SongSortMgr::SongFilter &f) {
            f.filters[kFilterDecade].insert(decade.c_str());
            f.filters[kFilterDecade].insert(decade2.c_str());
        },
        [&](const RawSong &s) { return DecadeOf(s.year) == decade || DecadeOf(s.year) == decade2; });
    add("rating", [&](SongSortMgr::SongFilter &f) { f.filters[kFilterRating].insert(rating.c_str()); },
        [&](const RawSong &s) {
            char b[16];
            snprintf(b, sizeof(b), "rating_%i", s.rating);
            return rating == b;
        });
    add("decade and rating",
        [&](SongSortMgr::SongFilter &f) {
            f.filters[kFilterDecade].insert(decade.c_str());
            f.filters[kFilterRating].insert(rating.c_str());
        },
        [&](const RawSong &s) {
            char b[16];
            snprintf(b, sizeof(b), "rating_%i", s.rating);
            return DecadeOf(s.year) == decade && rating == b;
        });
    add("artist (filter 9)", [&](SongSortMgr::SongFilter &f) { f.filters[9].insert(artist.c_str()); },
        [&](const RawSong &s) { return s.artist == artist; });
    add("pro guitar yes",
        [](SongSortMgr::SongFilter &f) { f.filters[kFilterProGuitar].insert("has_part_yes"); },
        [](const RawSong &s) { return s.Has("real_guitar") || s.Has("real_bass"); });
    add("pro guitar no",
        [](SongSortMgr::SongFilter &f) { f.filters[kFilterProGuitar].insert("has_part_no"); },
        [](const RawSong &s) { return !(s.Has("real_guitar") || s.Has("real_bass")); });
    add("keys yes", [](SongSortMgr::SongFilter &f) { f.filters[kFilterKeys].insert("has_part_yes"); },
        [](const RawSong &s) { return s.Has("keys") || s.Has("real_keys"); });
    static const TrackType req[] = { kTrackDrum, kTrackGuitar, kTrackBass, kTrackVocals, kTrackKeys,
                                     kTrackRealKeys, kTrackRealGuitar, kTrackRealBass };
    static const char *reqSym[] = { "drum", "guitar", "bass", "vocals", "keys",
                                    "real_keys", "real_guitar", "real_bass" };
    for (int k = 0; k < 8; k++) {
        TrackType t = req[k];
        const char *p = reqSym[k];
        add(p, [t](SongSortMgr::SongFilter &f) { f.requiredTrackType = t; },
            [p](const RawSong &s) { return s.Has(p); });
    }
    std::vector<int> excl;
    for (size_t i = 0; i < gSongs.size(); i += 3)
        excl.push_back(gSongs[i].id);
    add("excluded ids + keys",
        [&](SongSortMgr::SongFilter &f) {
            f.excludedSongs = excl;
            f.requiredTrackType = kTrackKeys;
        },
        [&](const RawSong &s) {
            return std::find(excl.begin(), excl.end(), s.id) == excl.end() && s.Has("keys");
        });

    int bad = 0, n = 0, split = 0;
    for (Case &c : cases) {
        int match = 0, cb = 0;
        for (const RawSong &s : gSongs) {
            bool got = mgr->DoesSongMatchFilter(s.id, &c.f, gNullStr);
            bool want = c.want(s);
            match += want;
            n++;
            if (got != want) {
                if (cb < 2)
                    printf("  %s: %s got %d want %d\n", c.name, s.shortName.c_str(), got, want);
                cb++;
            }
        }
        if (match > 0 && match < (int)gSongs.size())
            split++;
        printf("  filter %-20s %3d of %d songs match, %d wrong\n", c.name, match,
               (int)gSongs.size(), cb);
        bad += cb;
    }
    bool nullOk = mgr->DoesSongMatchFilter(gSongs[0].id, nullptr, gNullStr);
    Gate("sf-filters", bad == 0 && split >= 8 && nullOk,
         "%d filters x %d songs (%d filters split the list): DoesSongMatchFilter = the AND of "
         "each active filter over songs.dta's year/rating/artist/rank, %d wrong; a null filter "
         "matches",
         (int)cases.size(), (int)gSongs.size(), split, bad);
    free(mgr);
}

// ============================================================== modifiers ==
// config/modifiers.dta: (modifiers (name flag ...) ...). The list provider
// holds the entries without custom_location; IsModifierUnlocked is constant
// true in RB3 retail (its only body), so IsActive and IsHidden do not depend
// on the entry, and IsModifierActive reads default_enabled until toggled.
void ModifierChecks() {
    DataArray *sec = Splice("modifiers", "config/modifiers.dta");
    DataArray *list = sec ? sec->FindArray("modifiers", false) : nullptr;
    std::vector<std::string> all, listed;
    std::set<std::string> enabled;
    for (int i = 1; list && i < list->Size(); i++) {
        DataArray *m = list->Array(i);
        std::string name = m->Sym(0).Str();
        bool custom = false;
        for (int k = 1; k < m->Size(); k++) {
            if (!strcmp(m->Sym(k).Str(), "custom_location"))
                custom = true;
            if (!strcmp(m->Sym(k).Str(), "default_enabled"))
                enabled.insert(name);
        }
        all.push_back(name);
        if (!custom)
            listed.push_back(name);
    }
    ModifierMgr *saved = TheModifierMgr;
    TheModifierMgr = nullptr;
    ModifierMgr *mm = list ? new ModifierMgr() : nullptr;
    bool fixture = mm && mm->NumData() == (int)listed.size() && all.size() > listed.size();
    for (int i = 0; fixture && i < mm->NumData(); i++)
        fixture = listed[i] == mm->DataSymbol(i).Str();
    Gate("mm-fixture", fixture,
         "modifiers.dta: %d modifiers, %d without custom_location, %d default_enabled; the "
         "ModifierMgr lists %d in file order",
         (int)all.size(), (int)listed.size(), (int)enabled.size(), mm ? mm->NumData() : -1);
    if (!fixture) {
        TheModifierMgr = saved;
        return;
    }
    int listBad = 0;
    for (int i = 0; i < mm->NumData(); i++) {
        if (!mm->IsActive(i))
            listBad++;
        if (mm->IsHidden(i))
            listBad++;
    }
    Gate("mm-list", listBad == 0,
         "%d list rows: IsActive true and IsHidden false on every row (every modifier is "
         "unlocked), %d wrong",
         mm->NumData(), listBad);
    int actBad = 0, toggled = 0;
    for (const std::string &n : all) {
        bool want = enabled.count(n) != 0;
        if (mm->IsModifierActive(Symbol(n.c_str())) != want)
            actBad++;
        mm->ToggleModifierEnabled(Symbol(n.c_str()));
        if (mm->IsModifierActive(Symbol(n.c_str())) == want)
            actBad++;
        DataNode viaHandle = mm->Handle(Message("is_modifier_active", Symbol(n.c_str())), true);
        if ((viaHandle.Int() != 0) == want)
            actBad++;
        mm->ToggleModifierEnabled(Symbol(n.c_str()));
        toggled++;
    }
    Gate("mm-active", actBad == 0 && toggled == (int)all.size(),
         "%d modifiers: IsModifierActive = default_enabled, flips with toggle_modifier_enabled, "
         "and is_modifier_active (Handle) agrees; %d wrong",
         toggled, actBad);
    delete mm;
    TheModifierMgr = saved;
}

// =========================================================== BandDirector ==
// OnFileLoaded is sent by the world's file merger when the song milo arrives
// (world/gen/world.milo: world.fm carries a "song" merger, and shared/
// director.milo holds the BandDirector its keys target by name). The fixture
// builds that shape: a director named "BandDirector" in the main dir, and a
// FileMerger with a "song" merger as its `merger`.
const char *kSongMilo = "songs/20thcenturyboy/gen/20thcenturyboy.milo_xbox";
const char *kIntensity[] = { "mic_intensity", "bass_intensity", "drum_intensity",
                             "guitar_intensity", "keyboard_intensity" };

struct KeyInfo {
    std::string prop;
    int type;
    std::string interp;
    bool isSym;
    bool clamp;
    Hmx::Object *target;
};

std::vector<KeyInfo> WalkKeys(RndPropAnim *a) {
    std::vector<KeyInfo> out;
    for (PropKeys *k : a->PropKeysList()) {
        KeyInfo ki;
        DataArray *p = k->Prop();
        ki.prop = p && p->Size() == 1 && p->Type(0) == kDataSymbol ? p->Sym(0).Str() : "?";
        ki.type = k->KeysType();
        ki.interp = k->InterpHandler().Str();
        SymbolKeys *sk = dynamic_cast<SymbolKeys *>(k);
        ki.isSym = sk != nullptr;
        ki.clamp = sk && sk->mClampToPrevRange;
        ki.target = k->Target();
        out.push_back(ki);
    }
    return out;
}

bool IsIntensity(const std::string &p) {
    for (const char *s : kIntensity)
        if (p == s)
            return true;
    return false;
}

DataNode SendFileLoaded(BandDirector *bd, Symbol which, ObjectDir *dir) {
    return bd->Handle(Message("on_file_loaded", which, DataNode((Hmx::Object *)dir)), true);
}

void BandDirectorChecks() {
    BandSongPref::Init();
    BandDirector *savedBd = TheBandDirector;
    TheBandDirector = nullptr;
    BandDirector *bd = new BandDirector();
    bd->SetName("BandDirector", ObjectDir::Main());
    FileMerger *fm = Hmx::Object::New<FileMerger>();
    fm->SetName("w16ts_world.fm", ObjectDir::Main());
    {
        FileMerger::Merger m(fm);
        m.mName = Symbol("song");
        fm->mMergers.push_back(m);
    }
    bd->mMerger = fm;

    // --- the shipped song milo, walked here (iteration, not Find) ---
    ObjDirPtr<ObjectDir> song;
    song.LoadFile(FilePath(kSongMilo), false, true, kLoadFront, false);
    ObjectDir *dir = song.Ptr();
    RndPropAnim *anim = nullptr;
    BandSongPref *pref = nullptr;
    std::map<std::string, CharLipSync *> lips;
    int objs = 0;
    for (ObjDirItr<Hmx::Object> it(dir, true); dir && it; ++it) {
        objs++;
        if (!strcmp(it->ClassName().Str(), "PropAnim") && !strcmp(it->Name(), "song.anim"))
            anim = dynamic_cast<RndPropAnim *>(&*it);
        else if (!strcmp(it->ClassName().Str(), "BandSongPref"))
            pref = dynamic_cast<BandSongPref *>(&*it);
        else if (!strcmp(it->ClassName().Str(), "CharLipSync"))
            lips[it->Name()] = dynamic_cast<CharLipSync *>(&*it);
    }
    std::vector<KeyInfo> before = anim ? WalkKeys(anim) : std::vector<KeyInfo>();
    int onBd = 0, clampBefore = 0;
    for (const KeyInfo &k : before) {
        onBd += k.target == bd;
        clampBefore += k.clamp;
    }
    static const char *lipNames[] = { "song.lipsync", "part2.lipsync", "part3.lipsync",
                                      "part4.lipsync" };
    bool fixture = dir && anim && pref && lips.size() == 4 && onBd > 0;
    for (const char *n : lipNames)
        fixture = fixture && lips[n];
    Gate("bd-fixture", fixture,
         "%s: %d objects; song.anim with %d keys (%d targeting the BandDirector by name, %d "
         "clamped), BandSongPref %s, %d CharLipSync",
         kSongMilo, objs, (int)before.size(), onBd, clampBefore, pref ? "yes" : "no",
         (int)lips.size());
    if (!fixture) {
        TheBandDirector = savedBd;
        return;
    }

    // --- an on_file_loaded for another merger changes nothing ---
    bd->mEndOfSongSec = 99.0f;
    SendFileLoaded(bd, Symbol("venue"), dir);
    bool otherOk = !bd->mPropAnim && bd->mEndOfSongSec == 99.0f && !bd->mSongPref;

    // --- the song milo ---
    bd->unk110 = true;
    DataNode r = SendFileLoaded(bd, Symbol("song"), dir);
    bool finds = bd->mPropAnim == anim && bd->mSongPref == pref && !bd->unk110
        && bd->mEndOfSongSec == 0.0f && r.Type() == kDataInt && r.Int() == 0;
    for (int i = 0; i < 4; i++)
        finds = finds && bd->mLipSyncs[i] == lips[lipNames[i]];
    Gate("bd-song-finds", otherOk && finds,
         "on_file_loaded venue leaves the director alone (%s); on_file_loaded song caches "
         "song.anim, BandSongPref and song/part2/part3/part4.lipsync from the walked dir, "
         "keeps the shipped anim (no synthesized one), zeroes the end-of-song time: %s",
         otherOk ? "yes" : "NO", finds ? "yes" : "NO");

    std::vector<KeyInfo> after = WalkKeys(anim);
    int clampBad = 0, clamped = 0;
    for (size_t i = 0; i < after.size() && i < before.size(); i++) {
        bool want = before[i].clamp
            || (before[i].isSym && before[i].target == bd && IsIntensity(before[i].prop));
        clamped += after[i].clamp && !before[i].clamp;
        if (after[i].clamp != want) {
            printf("  %s: clamp %d, want %d\n", after[i].prop.c_str(), after[i].clamp, want);
            clampBad++;
        }
    }
    Gate("bd-song-clamp", clampBad == 0 && clamped == 5 && after.size() == before.size(),
         "the shipped anim's %d keys: exactly the five <instrument>_intensity symbol tracks "
         "on the director gain clamp-to-previous-range (%d did), %d wrong",
         (int)after.size(), clamped, clampBad);

    // --- no song milo: the director synthesizes its own song.anim ---
    bd->mEndOfSongSec = 99.0f;
    SendFileLoaded(bd, Symbol("song"), nullptr);
    RndPropAnim *made = bd->mPropAnim;
    bool cleared = made && made != anim && bd->unk110 && !bd->mSongPref
        && bd->mEndOfSongSec == 0.0f;
    for (int i = 0; i < 4; i++)
        cleared = cleared && !bd->mLipSyncs[i];
    FileMerger::Merger *sm = fm->FindMerger(Symbol("song"), false);
    bool owned = made && sm && sm->mLoadedObjects.find(made) != sm->mLoadedObjects.end()
        && !strcmp(made->Name(), "song.anim") && made->Dir() == fm->Dir()
        && !strcmp(made->Type().Str(), "song_anim")
        && made->GetRate() == RndAnimatable::k480_fpb;
    Gate("bd-null-made", cleared && owned,
         "on_file_loaded song with no dir clears every cached find (%s) and makes a song.anim "
         "of type song_anim at 480 fpb in the merger's dir, owned by the song merger (%s)",
         cleared ? "yes" : "NO", owned ? "yes" : "NO");

    // The synthesized anim against the shipped one: the same director tracks,
    // the same key types, the same interp handlers.
    std::map<std::string, KeyInfo> want, got;
    for (const KeyInfo &k : before)
        if (k.target == bd)
            want[k.prop] = k;
    int notOnBd = 0;
    for (const KeyInfo &k : made ? WalkKeys(made) : std::vector<KeyInfo>()) {
        if (k.target != bd)
            notOnBd++;
        got[k.prop] = k;
    }
    int keyBad = notOnBd;
    for (auto &w : want) {
        auto g = got.find(w.first);
        if (g == got.end()) {
            printf("  synthesized anim lacks %s\n", w.first.c_str());
            keyBad++;
        } else if (g->second.type != w.second.type || g->second.interp != w.second.interp
                   || g->second.clamp != IsIntensity(w.first)) {
            printf("  %s: type %d interp '%s' clamp %d; shipped type %d interp '%s'\n",
                   w.first.c_str(), g->second.type, g->second.interp.c_str(), g->second.clamp,
                   w.second.type, w.second.interp.c_str());
            keyBad++;
        }
    }
    for (auto &g : got)
        if (!want.count(g.first)) {
            printf("  synthesized anim has %s, the shipped one does not\n", g.first.c_str());
            keyBad++;
        }
    Gate("bd-null-keys", keyBad == 0 && !got.empty(),
         "the synthesized song.anim's %d tracks vs the shipped 20thcenturyboy song.anim's %d "
         "director tracks: same property set, key types and interp handlers, clamp only on "
         "the five intensities; %d differ",
         (int)got.size(), (int)want.size(), keyBad);

    delete bd;
    TheBandDirector = savedBd;
}

// ================================================== UIStats::MaybePublish ==
// The fixture, every piece real code except the Server:
//   * two UIScreens typed by the shipped ui/splash/splash.dta definitions
//     ({new BandScreen splash_screen ... (gather_uistats FALSE)} and
//     {new BandScreen intro_movie_screen ...}, no gather_uistats); the type is
//     set through Hmx::Object::SetTypeDef, so UIScreen's panel lookup is skipped;
//   * a LocalBandUser on pad 1 (gJoypadData[1].mUser, a core-guitar type with ten
//     EEPROM bytes) that is not participating -- a participating local user would
//     ask PlatformMgr::GetOnlineID, which is XUser-only (bandtrack_link_stubs);
//   * a participating RemoteBandUser on drums with a valid OnlineID;
//   * a GameMode whose mMode is "qp_coop", and TheDataPointMgr's recorder hook.
// THE SERVER is the one fake. Server::Server and Server's vtable live in
// network/net/Server.cpp, which also defines the global gXboxServer (a dynamic
// initializer rb3-render must not run). MaybePublish makes exactly one Server
// call, the virtual IsConnected(); the fake object is Server-sized zeroed
// storage whose vptr points at a table holding, in IsConnected's slot (read off
// &Server::IsConnected, Itanium ABI), Server.h's own inline body
// `mLoginState == 2`, and in every other slot a trap that aborts.
//
// THE REFERENCE is the retail body (fn_8255F9D0), read off the target asm:
//   * one `bl RecordDataPoint` -- only stats/pad_user is ever recorded, and only
//     when it carries more than its name;
//   * mLastControllerType (UIStats+0xe8, walked by r15) is LOADED once per remote
//     user and never stored, so a participating remote user whose controller type
//     is not kControllerNone is re-reported on every publish;
//   * the drop arms: disconnected clears mPublishingPad, a gather_uistats FALSE
//     screen does not; both bump mLastDroppedScreen and rewind the pad log.
// Expected breed strings are formatted here from the joypad bytes this fixture
// wrote; the controller symbol is read from the shipped config/macros.dta.

struct Recorded {
    std::string type;
    std::map<std::string, std::string> pairs;
};
std::vector<Recorded> gRecorded;

void RecordHook(DataPoint &dp, bool) {
    Recorded r;
    r.type = dp.mType.Str();
    for (std::map<Symbol, DataNode>::iterator it = dp.mNameValPairs.begin();
         it != dp.mNameValPairs.end(); ++it) {
        const DataNode &n = it->second;
        std::string v;
        if (n.Type() == kDataInt) {
            char b[32];
            snprintf(b, sizeof(b), "%d", n.Int());
            v = b;
        } else
            v = n.Str();
        r.pairs[it->first.Str()] = v;
    }
    gRecorded.push_back(r);
}

std::string PairsText(const std::map<std::string, std::string> &m) {
    std::string s;
    for (auto &kv : m)
        s += (s.empty() ? "" : " ") + kv.first + "=" + kv.second;
    return s;
}

bool SamePoint(const std::vector<Recorded> &got, const std::map<std::string, std::string> &want) {
    if (got.size() != 1) {
        printf("  recorded %d points, want 1\n", (int)got.size());
        return false;
    }
    if (got[0].type != "stats/pad_user" || got[0].pairs != want) {
        printf("  got  %s {%s}\n  want stats/pad_user {%s}\n", got[0].type.c_str(),
               PairsText(got[0].pairs).c_str(), PairsText(want).c_str());
        return false;
    }
    return true;
}

void FakeServerTrap() {
    fprintf(stderr, "W16-TS fake Server: a virtual other than IsConnected was called\n");
    abort();
}
bool FakeServerIsConnected(Server *self) { return self->mLoginState == 2; } // Server.h:24
void *gFakeServerVtbl[256];
alignas(Server) unsigned char gFakeServerStorage[sizeof(Server)];

Server *MakeFakeServer() {
    bool (Server::*mfp)() = &Server::IsConnected;
    struct {
        uintptr_t ptr;
        ptrdiff_t adj;
    } rep;
    static_assert(sizeof(rep) == sizeof(mfp), "Itanium member pointer");
    memcpy(&rep, &mfp, sizeof(rep));
    if (!(rep.ptr & 1) || rep.adj != 0 || (rep.ptr - 1) / sizeof(void *) >= 256)
        return nullptr;
    for (int i = 0; i < 256; i++)
        gFakeServerVtbl[i] = (void *)&FakeServerTrap;
    gFakeServerVtbl[(rep.ptr - 1) / sizeof(void *)] = (void *)&FakeServerIsConnected;
    memset(gFakeServerStorage, 0, sizeof(gFakeServerStorage));
    void **vptr = (void **)gFakeServerStorage;
    *vptr = gFakeServerVtbl;
    return (Server *)gFakeServerStorage;
}

// {new BandScreen <name> ...} anywhere in a loaded file.
DataArray *FindScreenDef(DataArray *a, const char *name) {
    for (int i = 0; i < a->Size(); i++) {
        if (a->Type(i) != kDataArray && a->Type(i) != kDataCommand)
            continue;
        DataArray *c = a->UncheckedArray(i); // Array() would EXECUTE a command
        if (c->Size() > 2 && c->Type(0) == kDataSymbol && !strcmp(c->Sym(0).Str(), "new")
            && c->Type(2) == kDataSymbol && !strcmp(c->Sym(2).Str(), name))
            return c;
        if (DataArray *r = FindScreenDef(c, name))
            return r;
    }
    return nullptr;
}

std::string Hex2(int v) {
    char b[8];
    snprintf(b, sizeof(b), "%02x", v & 0xff);
    return b;
}

void UIStatsChecks() {
    // A DTB's #defines are applied while it loads (DataArray::Load), so the live
    // macro is copied BEFORE the shipped file is read, and the shipped value is
    // the macro as that read leaves it.
    std::vector<Symbol> liveBefore;
    if (DataArray *m = DataGetMacro("CHAR_INSTRUMENT_SYMBOLS"))
        for (int i = 0; i < m->Size(); i++)
            liveBefore.push_back(m->Sym(i));
    DataArray *splashFile = DataReadFile("ui/splash/splash.dta", true);
    DataArray *macros = DataReadFile("config/macros.dta", true);
    DataArray *splashDef = splashFile ? FindScreenDef(splashFile, "splash_screen") : nullptr;
    DataArray *introDef = splashFile ? FindScreenDef(splashFile, "intro_movie_screen") : nullptr;
    DataArray *shippedSyms = macros ? DataGetMacro("CHAR_INSTRUMENT_SYMBOLS") : nullptr;
    static Symbol gather("gather_uistats");
    DataArray *splashGather = splashDef ? splashDef->FindArray(gather, false) : nullptr;
    DataArray *introGather = introDef ? introDef->FindArray(gather, false) : nullptr;
    bool symsAgree = shippedSyms && shippedSyms->Size() == (int)liveBefore.size()
        && shippedSyms->Size() == kNumControllerTypes + 1; // ... and "none"
    for (int i = 0; symsAgree && i < shippedSyms->Size(); i++)
        symsAgree = shippedSyms->Sym(i) == liveBefore[i];
    Server *fake = MakeFakeServer();

    bool namesFree = !ObjectDir::Main()->Find<Hmx::Object>("splash_screen", false)
        && !ObjectDir::Main()->Find<Hmx::Object>("intro_movie_screen", false);
    bool fixOk = splashDef && introDef && splashGather && splashGather->Int(1) == 0
        && !introGather && symsAgree && fake && namesFree;
    Gate("us-fixture", fixOk,
         "shipped splash.dta: splash_screen %s (gather_uistats %d), intro_movie_screen %s "
         "(gather_uistats %s); CHAR_INSTRUMENT_SYMBOLS shipped %d / live %d entries, %s; "
         "fake Server %s",
         splashDef ? "found" : "MISSING", splashGather ? splashGather->Int(1) : -1,
         introDef ? "found" : "MISSING", introGather ? "PRESENT" : "absent",
         shippedSyms ? shippedSyms->Size() : -1, (int)liveBefore.size(),
         symsAgree ? "agree" : "DIFFER", fake ? "built" : "NOT BUILT");
    if (!fixOk) {
        if (splashFile) splashFile->Release();
        if (macros) macros->Release();
        return;
    }

    // ---- fixture install (everything saved, everything restored) ----
    Server *savedServer = TheNet.mServer;
    GameMode *savedMode = TheGameMode;
    BandUserMgr *savedMgr = TheBandUserMgr;
    DataPointRecordFunc *savedRec = TheDataPointMgr.SetDataPointRecorder(&RecordHook);
    JoypadData *pad1 = JoypadGetPadData(1);
    LocalUser *savedPadUser = pad1->mUser;
    JoypadType savedPadType = pad1->mType;
    unsigned char savedEeprom[0x10];
    memcpy(savedEeprom, pad1->mEepromData, sizeof(savedEeprom));

    GameMode *mode = (GameMode *)calloc(1, sizeof(GameMode));
    mode->mMode = Symbol("qp_coop");
    TheGameMode = mode;
    TheNet.mServer = fake;

    UIScreen *splash = new UIScreen();
    UIScreen *intro = new UIScreen();
    splash->SetName("splash_screen", ObjectDir::Main());
    intro->SetName("intro_movie_screen", ObjectDir::Main());
    splash->Hmx::Object::SetTypeDef(splashDef);
    intro->Hmx::Object::SetTypeDef(introDef);

    LocalBandUser *local = new LocalBandUser();
    RemoteBandUser *remote = new RemoteBandUser();
    pad1->mUser = local;
    pad1->mType = kJoypadXboxCoreGuitar;
    for (int i = 0; i < 10; i++)
        pad1->mEepromData[i] = (unsigned char)(0xa0 + 7 * i);
    remote->mParticipating = true;
    remote->mControllerType = kControllerDrum;
    unsigned long long xuid = 0x0009000012345678ULL;
    remote->mOnlineID->SetXUID(xuid);
    BandUserMgr *mgr = (BandUserMgr *)calloc(1, sizeof(BandUserMgr));
    mgr->mUsers.push_back(local);
    mgr->mUsers.push_back(remote);
    TheBandUserMgr = mgr;

    std::string breed = Hex2(kJoypadXboxCoreGuitar);
    for (int i = 0; i < 10; i++)
        breed += Hex2(0xa0 + 7 * i);
    char xuidText[32];
    snprintf(xuidText, sizeof(xuidText), "%016llx", xuid);
    std::string remoteVal = std::string(shippedSyms->Sym(kControllerDrum).Str()) + ":" + xuidText;

    UIStats *st = new UIStats();
    st->Init();
    auto padLogRewound = [&]() {
        return st->mPadLogCount == 0 && st->mPadLogWritePtr == st->mPadLogBuffer;
    };
    int padNum = local->GetPadNum();

    // P0: disconnected, a gathering screen, with pad events logged.
    st->EventLog(0, 3, 0);
    st->EventLog(0, 3, 1);
    int loggedBefore = st->mPadLogCount;
    st->mPublishingPad = true;
    gRecorded.clear();
    st->MaybePublish(intro);
    int p0Pub = st->mPublishingPad, p0Drop = st->mLastDroppedScreen, p0Rec = (int)gRecorded.size();
    bool p0 = !p0Pub && p0Drop == 1 && padLogRewound() && p0Rec == 0 && loggedBefore == 2;
    // P1: connected, the gather_uistats FALSE screen; mPublishingPad set by hand
    // so the arm's "leave it alone" is visible.
    ((Server *)gFakeServerStorage)->mLoginState = 2;
    st->mPublishingPad = true;
    st->MaybePublish(splash);
    int p1Pub = st->mPublishingPad, p1Drop = st->mLastDroppedScreen, p1Rec = (int)gRecorded.size();
    bool p1 = p1Pub && p1Drop == 2 && padLogRewound() && p1Rec == 0;
    st->mPublishingPad = false;
    Gate("us-drop", p0 && p1 && padNum == 1,
         "disconnected: mPublishingPad %d (want 0), dropped %d (want 1), pad log of %d "
         "rewound, %d recorded; splash_screen (gather_uistats FALSE): mPublishingPad %d "
         "(want 1), dropped %d (want 2), %d recorded; local user on pad %d",
         p0Pub, p0Drop, loggedBefore, p0Rec, p1Pub, p1Drop, p1Rec, padNum);

    // P2: first publish since the drops.
    st->EventLog(1, 5, 0);
    st->EventLog(1, 5, 1);
    st->EventLog(1, 6, 0);
    gRecorded.clear();
    st->MaybePublish(intro);
    std::map<std::string, std::string> want2 = {{"name", "intro_movie_screen"},
                                                {"pad_1", breed},
                                                {"remote_user_0", remoteVal},
                                                {"dropped_screens", "2"}};
    bool p2 = SamePoint(gRecorded, want2) && st->mPublishingPad && st->mLastDroppedScreen == 0
        && padLogRewound() && st->mLastMode == Symbol("qp_coop")
        && st->mLastBreedString[1] == breed.c_str() && st->mLastRemoteID[0] == OnlineID(xuid);
    Gate("us-publish", p2,
         "first publish of intro_movie_screen: one stats/pad_user point {%s}; "
         "mLastDroppedScreen %d (want 0), pad log rewound %d, mLastMode %s",
         gRecorded.empty() ? "" : PairsText(gRecorded[0].pairs).c_str(),
         st->mLastDroppedScreen, padLogRewound() ? 1 : 0, st->mLastMode.Str());

    // P3: nothing changed. Retail never writes mLastControllerType back, so the
    // drummer is reported again; the guitar's breed is not.
    gRecorded.clear();
    st->MaybePublish(intro);
    std::map<std::string, std::string> want3 = {{"name", "intro_movie_screen"},
                                                {"remote_user_0", remoteVal}};
    bool p3 = SamePoint(gRecorded, want3) && st->mLastControllerType[0] == kControllerNone;
    Gate("us-remote-again", p3,
         "second publish, nothing changed: {%s} (want name + remote_user_0 only: the "
         "remote controller type is compared against mLastControllerType[0], which "
         "stays %d)",
         gRecorded.empty() ? "" : PairsText(gRecorded[0].pairs).c_str(),
         st->mLastControllerType[0]);

    // P4/P5: a disconnect drops one screen and clears mPublishingPad, so the next
    // publish resets the per-pad memory and reports the guitar again.
    ((Server *)gFakeServerStorage)->mLoginState = 0;
    gRecorded.clear();
    st->MaybePublish(intro);
    int p4Rec = (int)gRecorded.size(), p4Pub = st->mPublishingPad, p4Drop = st->mLastDroppedScreen;
    bool p4 = p4Rec == 0 && !p4Pub && p4Drop == 1;
    ((Server *)gFakeServerStorage)->mLoginState = 2;
    st->MaybePublish(intro);
    std::map<std::string, std::string> want5 = want2;
    want5["dropped_screens"] = "1";
    bool p5 = SamePoint(gRecorded, want5) && st->mLastDroppedScreen == 0;
    Gate("us-reconnect", p4 && p5,
         "disconnect: %d recorded, mPublishingPad %d, dropped %d; reconnect publish {%s}",
         p4Rec, p4Pub, p4Drop,
         gRecorded.empty() ? "" : PairsText(gRecorded.back().pairs).c_str());

    // ---- restore ----
    st->Terminate();
    delete st;
    TheBandUserMgr = savedMgr;
    std::vector<BandUser *>().swap(mgr->mUsers);
    free(mgr);
    pad1->mUser = savedPadUser;
    pad1->mType = savedPadType;
    memcpy(pad1->mEepromData, savedEeprom, sizeof(savedEeprom));
    delete local;
    delete remote;
    delete splash;
    delete intro;
    TheNet.mServer = savedServer;
    TheGameMode = savedMode;
    free(mode);
    TheDataPointMgr.SetDataPointRecorder(savedRec);
    splashFile->Release();
    macros->Release();
}

} // namespace

int RunW16TSPhase(GateFn gate) {
    gGate = gate;
    hidden = Symbol("hidden");
    printf("\n=== W16-TS phase: unentered in-scope rows on shipped data ===\n");
    if (BuildSongs()) {
        HasPartChecks();
        FilterChecks();
    }
    ModifierChecks();
    BandDirectorChecks();
    UIStatsChecks();
    return gRan;
}
