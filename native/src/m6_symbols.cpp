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
