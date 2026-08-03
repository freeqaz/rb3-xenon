// rb3-xenon native M6 — symbol/message/singleton definitions for the REAL
// scoring TUs (Performer/Player/Scoring/Stats/Band). These game Symbol and
// Message globals (from utl/Symbols*.h, utl/Messages*.h) are referenced only by
// the message-handler (BEGIN_HANDLERS) blocks the scoring path never executes,
// and the singleton pointers are all off the scoring path. Default-constructed
// (never interned/used here) so the object graph links. X360 unaffected.
#include "utl/Symbol.h"
#include "obj/Msg.h"

class Game; class GameConfig; class SongDB; class BandUserMgr;
class NetSession; class GamePanel; class BandDirector;
#include "net/Net.h"

// --- game Symbol globals (off-path; handler-only) ---
Symbol accumulated_score;
Symbol active_player;
Symbol band;
Symbol band_energy;
Symbol band_performer;
Symbol can_deploy;
Symbol change_difficulty;
Symbol clear_score_histories;
Symbol crowd_rating;
Symbol crowd_rating_active;
Symbol current_notes_hit_fraction;
Symbol current_streak;
Symbol deploy;
Symbol deploy_count;
Symbol deploy_if_possible;
Symbol difficulty;
Symbol disable_phrase_bonus;
Symbol disconnected_at_start;
Symbol display_crowd_rating;
Symbol empty_band_energy;
Symbol enabled_state;
Symbol enable_phrase_bonus;
Symbol enable_swings;
Symbol enable_time;
Symbol enter_coda;
Symbol fail;
Symbol failed_deploy;
Symbol fill_band_energy;
Symbol fill_hit_count;
Symbol finalize_stats;
Symbol force_deploy;
Symbol get_deploy_failed;
Symbol get_double_harmony_hit;
Symbol get_double_harmony_total;
Symbol get_multiplier_active;
Symbol get_saved_count;
Symbol get_singer_count;
Symbol get_singer_ranked_part;
Symbol get_singer_ranked_percentage;
Symbol get_song_num_vocal_parts;
Symbol get_times_failed;
Symbol get_triple_harmony_hit;
Symbol get_triple_harmony_total;
Symbol get_user;
Symbol get_vocal_part_percentage;
Symbol in_freestyle_section;
Symbol instrument;
Symbol intro;
Symbol is_deploying;
Symbol is_net;
Symbol longest_multiplier_ms;
Symbol longest_streak;
Symbol lose;
Symbol main_performer;
Symbol max_multiplier;
Symbol multiplier;
Symbol notes_hit;
Symbol notes_hit_fraction;
Symbol notes_per_streak;
Symbol num_active_players;
Symbol on_game_lost;
Symbol percent_complete;
Symbol perfect_solo_with_solo_buttons;
Symbol player_name;
Symbol popup_help;
Symbol progress_ms;
Symbol raw_crowd_rating;
Symbol remote_already_saved;
Symbol remote_deploy;
Symbol remote_enabled_state;
Symbol remote_fail_unison_phrase;
Symbol remote_finished_song;
Symbol remote_hit_last_unison_gem;
Symbol remote_streak;
Symbol remote_tracker_deploy;
Symbol remote_tracker_end_deploy_streak;
Symbol remote_tracker_end_streak;
Symbol remote_tracker_focus;
Symbol remote_tracker_player_display;
Symbol remote_tracker_player_progress;
Symbol remote_tracker_section_complete;
Symbol remote_update_crowd;
Symbol remote_update_energy;
Symbol remote_update_score;
Symbol reset_controller;
Symbol save_all;
Symbol saved_count;
Symbol score;
Symbol send_net_gameplay_msg;
Symbol send_net_gameplay_msg_to_player;
Symbol send_remote_stats;
Symbol set_band_energy;
Symbol set_crowd_meter_active;
Symbol set_crowd_rating;
Symbol set_crowd_rating_active;
Symbol set_energy_automatically;
Symbol set_multiplier_active;
Symbol set_permanent_overdrive;
Symbol solo_percentage;
Symbol star_power_meter;
Symbol stats_finalized;
Symbol strummed_down;
Symbol strummed_up;
Symbol total_stars;
Symbol track;
Symbol update_lefty_flip;
Symbol update_vocal_style;
Symbol was_never_bad;
Symbol win;

// --- game Message globals (off-path; handler-only) ---
Message deploy_msg{Symbol()};
Message enable_player_msg{Symbol()};
Message send_finished_song_msg{Symbol()};
Message send_streak_msg{Symbol()};
Message send_update_energy_msg{Symbol()};
Message send_update_score_msg{Symbol()};

// --- game singletons (off the scoring path) ---
Game *TheGame = 0;
GameConfig *TheGameConfig = 0;
SongDB *TheSongDB = 0;
BandUserMgr *TheBandUserMgr = 0;
NetSession *TheNetSession = 0;
GamePanel *TheGamePanel = 0;
BandDirector *TheBandDirector = 0;
Net TheNet;

// ══════════════════════════════════════════════════════════════════════════
// ⛔ X8 DEFECT FIX -- THESE 109 GLOBALS WERE DEAD DISPATCH KEYS.
//
// Every `Symbol foo;` above is DEFAULT-CONSTRUCTED, i.e. the NULL symbol
// (Symbol.h:16, mStr = gNullStr). The comment they were added under says they
// are "dispatch keys only ... never reached on the load path". That is false,
// and it cost this lane a whole debugging pass:
//
//   obj/ObjMacros.h:184  #define HANDLE_ACTION(symbol, action)
//                            if (sym == symbol) { (action); return 0; }
//
// -- this arm of the macro compares the incoming message symbol against THE
// GLOBAL ITSELF, not against a string literal. A null global therefore matches
// NOTHING, and every BEGIN_HANDLERS entry keyed on one silently falls through
// to HANDLE_CHECK and reports "unhandled msg".
//
// MEASURED: BandWardrobe::SetVenueDir -> SyncPlayMode() ->
// mModeSink->Handle(sync_play_mode_msg) produced
//   "BandConfiguration (...small_club_01_base.milo) unhandled msg: sync_play_mode"
// so the venue's authored band-slot transforms were never applied and all four
// members stayed at char/main/main.milo's defaults, with rc=0 and no warning.
// A dead dispatch key fails SILENTLY -- it is indistinguishable from "the
// message was never sent".
//
// retail defines these in src/system/utl/Symbols*.cpp as
//     Symbol sync_play_mode("sync_play_mode");
// (rb3-Wii oracle rb3/src/system/utl/Symbols.cpp:960). rb3-xenon ships the
// Symbols*.h HEADERS but no corresponding .cpp, which is why they had to be
// hand-defined here at all.
//
// Interned in a FUNCTION rather than at static-init, deliberately: the Symbol
// ctor (utl/Symbol.cpp) dereferences gStringTable, which does not exist until
// Symbol::Init() -> PreInit(). Static-init construction would be a null deref
// or an ordering lottery. Call this AFTER Symbol::Init().
// ══════════════════════════════════════════════════════════════════════════
void InternSymbolGlobals_M6Symbols() {
    accumulated_score = Symbol("accumulated_score");
    active_player = Symbol("active_player");
    band = Symbol("band");
    band_energy = Symbol("band_energy");
    band_performer = Symbol("band_performer");
    can_deploy = Symbol("can_deploy");
    change_difficulty = Symbol("change_difficulty");
    clear_score_histories = Symbol("clear_score_histories");
    crowd_rating = Symbol("crowd_rating");
    crowd_rating_active = Symbol("crowd_rating_active");
    current_notes_hit_fraction = Symbol("current_notes_hit_fraction");
    current_streak = Symbol("current_streak");
    deploy = Symbol("deploy");
    deploy_count = Symbol("deploy_count");
    deploy_if_possible = Symbol("deploy_if_possible");
    difficulty = Symbol("difficulty");
    disable_phrase_bonus = Symbol("disable_phrase_bonus");
    disconnected_at_start = Symbol("disconnected_at_start");
    display_crowd_rating = Symbol("display_crowd_rating");
    empty_band_energy = Symbol("empty_band_energy");
    enabled_state = Symbol("enabled_state");
    enable_phrase_bonus = Symbol("enable_phrase_bonus");
    enable_swings = Symbol("enable_swings");
    enable_time = Symbol("enable_time");
    enter_coda = Symbol("enter_coda");
    fail = Symbol("fail");
    failed_deploy = Symbol("failed_deploy");
    fill_band_energy = Symbol("fill_band_energy");
    fill_hit_count = Symbol("fill_hit_count");
    finalize_stats = Symbol("finalize_stats");
    force_deploy = Symbol("force_deploy");
    get_deploy_failed = Symbol("get_deploy_failed");
    get_double_harmony_hit = Symbol("get_double_harmony_hit");
    get_double_harmony_total = Symbol("get_double_harmony_total");
    get_multiplier_active = Symbol("get_multiplier_active");
    get_saved_count = Symbol("get_saved_count");
    get_singer_count = Symbol("get_singer_count");
    get_singer_ranked_part = Symbol("get_singer_ranked_part");
    get_singer_ranked_percentage = Symbol("get_singer_ranked_percentage");
    get_song_num_vocal_parts = Symbol("get_song_num_vocal_parts");
    get_times_failed = Symbol("get_times_failed");
    get_triple_harmony_hit = Symbol("get_triple_harmony_hit");
    get_triple_harmony_total = Symbol("get_triple_harmony_total");
    get_user = Symbol("get_user");
    get_vocal_part_percentage = Symbol("get_vocal_part_percentage");
    in_freestyle_section = Symbol("in_freestyle_section");
    instrument = Symbol("instrument");
    intro = Symbol("intro");
    is_deploying = Symbol("is_deploying");
    is_net = Symbol("is_net");
    longest_multiplier_ms = Symbol("longest_multiplier_ms");
    longest_streak = Symbol("longest_streak");
    lose = Symbol("lose");
    main_performer = Symbol("main_performer");
    max_multiplier = Symbol("max_multiplier");
    multiplier = Symbol("multiplier");
    notes_hit = Symbol("notes_hit");
    notes_hit_fraction = Symbol("notes_hit_fraction");
    notes_per_streak = Symbol("notes_per_streak");
    num_active_players = Symbol("num_active_players");
    on_game_lost = Symbol("on_game_lost");
    percent_complete = Symbol("percent_complete");
    perfect_solo_with_solo_buttons = Symbol("perfect_solo_with_solo_buttons");
    player_name = Symbol("player_name");
    popup_help = Symbol("popup_help");
    progress_ms = Symbol("progress_ms");
    raw_crowd_rating = Symbol("raw_crowd_rating");
    remote_already_saved = Symbol("remote_already_saved");
    remote_deploy = Symbol("remote_deploy");
    remote_enabled_state = Symbol("remote_enabled_state");
    remote_fail_unison_phrase = Symbol("remote_fail_unison_phrase");
    remote_finished_song = Symbol("remote_finished_song");
    remote_hit_last_unison_gem = Symbol("remote_hit_last_unison_gem");
    remote_streak = Symbol("remote_streak");
    remote_tracker_deploy = Symbol("remote_tracker_deploy");
    remote_tracker_end_deploy_streak = Symbol("remote_tracker_end_deploy_streak");
    remote_tracker_end_streak = Symbol("remote_tracker_end_streak");
    remote_tracker_focus = Symbol("remote_tracker_focus");
    remote_tracker_player_display = Symbol("remote_tracker_player_display");
    remote_tracker_player_progress = Symbol("remote_tracker_player_progress");
    remote_tracker_section_complete = Symbol("remote_tracker_section_complete");
    remote_update_crowd = Symbol("remote_update_crowd");
    remote_update_energy = Symbol("remote_update_energy");
    remote_update_score = Symbol("remote_update_score");
    reset_controller = Symbol("reset_controller");
    save_all = Symbol("save_all");
    saved_count = Symbol("saved_count");
    score = Symbol("score");
    send_net_gameplay_msg = Symbol("send_net_gameplay_msg");
    send_net_gameplay_msg_to_player = Symbol("send_net_gameplay_msg_to_player");
    send_remote_stats = Symbol("send_remote_stats");
    set_band_energy = Symbol("set_band_energy");
    set_crowd_meter_active = Symbol("set_crowd_meter_active");
    set_crowd_rating = Symbol("set_crowd_rating");
    set_crowd_rating_active = Symbol("set_crowd_rating_active");
    set_energy_automatically = Symbol("set_energy_automatically");
    set_multiplier_active = Symbol("set_multiplier_active");
    set_permanent_overdrive = Symbol("set_permanent_overdrive");
    solo_percentage = Symbol("solo_percentage");
    star_power_meter = Symbol("star_power_meter");
    stats_finalized = Symbol("stats_finalized");
    strummed_down = Symbol("strummed_down");
    strummed_up = Symbol("strummed_up");
    total_stars = Symbol("total_stars");
    track = Symbol("track");
    update_lefty_flip = Symbol("update_lefty_flip");
    update_vocal_style = Symbol("update_vocal_style");
    was_never_bad = Symbol("was_never_bad");
    win = Symbol("win");
}
