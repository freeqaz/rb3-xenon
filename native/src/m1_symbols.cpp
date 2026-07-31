// M1 Symbol-global definitions (native only). AUTO-DERIVED from the
// rb3-song link's undefined `extern Symbol X;' references.
//
// The matched X360 build never ODR-uses these globals (every use is a
// function-local `static Symbol X("X")' shadow -- the RB3_HANDLE_LOCAL_STATIC
// lever). Native code paths that reference the true globals need real defs.
// They cannot be static-init `Symbol X("X")' (the ctor needs gStringTable,
// built by Symbol::Init()), so define them default-null and intern in
// InitM1Symbols(), called from the driver AFTER Symbol::Init().
#include "utl/Symbol.h"

Symbol add_recent_song;
Symbol album_art;
Symbol album_name;
Symbol album_track_number;
Symbol alt_dirs;
Symbol alternate_path;
Symbol anim_tempo;
Symbol artist;
Symbol band;
Symbol band_fail_cue;
Symbol bank;
Symbol base_points;
Symbol bass;
Symbol bonus;
Symbol cheat_toggle_max_song_count;
Symbol disc_update;
Symbol dlc;
Symbol drum;
Symbol drum_bank;
Symbol encoding;
Symbol extra_authoring;
Symbol fake;
Symbol genre;
Symbol get_fake_songs_allowed;
Symbol get_max_song_count;
Symbol get_meta_data;
Symbol guide_pitch_volume;
Symbol guitar;
Symbol has_license;
Symbol has_part;
Symbol has_song;
Symbol has_upgrade;
Symbol id;
Symbol init_msg;
Symbol is_demo;
Symbol is_download;
Symbol is_ugc;
// is_ugc_plus: BandSongMetadata's retail-only `is_ugc_plus' handler arm
// (BandSongMetadata.cpp:589) references the Symbols3.h global. It was added
// after this file was derived, so the rb3-song link had an undefined
// reference to it -- see the regeneration note at the bottom of this file.
Symbol is_ugc_plus;
Symbol keys;
Symbol latin1;
Symbol length_ms;
Symbol master;
Symbol max_song_count;
Symbol max_song_count_debug;
Symbol midi_file;
Symbol mute_win_cues;
Symbol name;
Symbol none;
Symbol num_rank_tiers;
Symbol num_vocal_parts;
Symbol part_plays_in_song;
Symbol rank;
Symbol rank_tier;
Symbol rank_tier_for_song;
Symbol rank_tier_token;
Symbol rating;
Symbol rb1_dlc;
Symbol rb3_dlc;
Symbol real_bass;
Symbol real_bass_tuning;
Symbol real_guitar;
Symbol real_guitar_tuning;
Symbol set_fake_songs_allowed;
Symbol solo;
Symbol song_file_path;
Symbol song_groupings;
Symbol song_key;
Symbol song_length;
Symbol song_lengths;
Symbol song_mgr;
Symbol song_mgr_full;
Symbol song_name;
Symbol song_path;
Symbol song_scroll_speed;
Symbol song_select;
Symbol song_tonality;
Symbol sync_shared_songs;
Symbol title;
Symbol tuning_offset_cents;
Symbol tutorial;
Symbol ugc;
Symbol ugc_plus;
Symbol upgrade_midi_file;
Symbol vocal_gender;
Symbol vocal_percussion;
Symbol vocals;
Symbol vocal_tonic_note;
Symbol year_recorded;
Symbol year_released;

void InitM1Symbols() {
    add_recent_song = Symbol("add_recent_song");
    album_art = Symbol("album_art");
    album_name = Symbol("album_name");
    album_track_number = Symbol("album_track_number");
    alt_dirs = Symbol("alt_dirs");
    alternate_path = Symbol("alternate_path");
    anim_tempo = Symbol("anim_tempo");
    artist = Symbol("artist");
    band = Symbol("band");
    band_fail_cue = Symbol("band_fail_cue");
    bank = Symbol("bank");
    base_points = Symbol("base_points");
    bass = Symbol("bass");
    bonus = Symbol("bonus");
    cheat_toggle_max_song_count = Symbol("cheat_toggle_max_song_count");
    disc_update = Symbol("disc_update");
    dlc = Symbol("dlc");
    drum = Symbol("drum");
    drum_bank = Symbol("drum_bank");
    encoding = Symbol("encoding");
    extra_authoring = Symbol("extra_authoring");
    fake = Symbol("fake");
    genre = Symbol("genre");
    get_fake_songs_allowed = Symbol("get_fake_songs_allowed");
    get_max_song_count = Symbol("get_max_song_count");
    get_meta_data = Symbol("get_meta_data");
    guide_pitch_volume = Symbol("guide_pitch_volume");
    guitar = Symbol("guitar");
    has_license = Symbol("has_license");
    has_part = Symbol("has_part");
    has_song = Symbol("has_song");
    has_upgrade = Symbol("has_upgrade");
    id = Symbol("id");
    init_msg = Symbol("init_msg");
    is_demo = Symbol("is_demo");
    is_download = Symbol("is_download");
    is_ugc = Symbol("is_ugc");
    is_ugc_plus = Symbol("is_ugc_plus");
    keys = Symbol("keys");
    latin1 = Symbol("latin1");
    length_ms = Symbol("length_ms");
    master = Symbol("master");
    max_song_count = Symbol("max_song_count");
    max_song_count_debug = Symbol("max_song_count_debug");
    midi_file = Symbol("midi_file");
    mute_win_cues = Symbol("mute_win_cues");
    name = Symbol("name");
    none = Symbol("none");
    num_rank_tiers = Symbol("num_rank_tiers");
    num_vocal_parts = Symbol("num_vocal_parts");
    part_plays_in_song = Symbol("part_plays_in_song");
    rank = Symbol("rank");
    rank_tier = Symbol("rank_tier");
    rank_tier_for_song = Symbol("rank_tier_for_song");
    rank_tier_token = Symbol("rank_tier_token");
    rating = Symbol("rating");
    rb1_dlc = Symbol("rb1_dlc");
    rb3_dlc = Symbol("rb3_dlc");
    real_bass = Symbol("real_bass");
    real_bass_tuning = Symbol("real_bass_tuning");
    real_guitar = Symbol("real_guitar");
    real_guitar_tuning = Symbol("real_guitar_tuning");
    set_fake_songs_allowed = Symbol("set_fake_songs_allowed");
    solo = Symbol("solo");
    song_file_path = Symbol("song_file_path");
    song_groupings = Symbol("song_groupings");
    song_key = Symbol("song_key");
    song_length = Symbol("song_length");
    song_lengths = Symbol("song_lengths");
    song_mgr = Symbol("song_mgr");
    song_mgr_full = Symbol("song_mgr_full");
    song_name = Symbol("song_name");
    song_path = Symbol("song_path");
    song_scroll_speed = Symbol("song_scroll_speed");
    song_select = Symbol("song_select");
    song_tonality = Symbol("song_tonality");
    sync_shared_songs = Symbol("sync_shared_songs");
    title = Symbol("title");
    tuning_offset_cents = Symbol("tuning_offset_cents");
    tutorial = Symbol("tutorial");
    ugc = Symbol("ugc");
    ugc_plus = Symbol("ugc_plus");
    upgrade_midi_file = Symbol("upgrade_midi_file");
    vocal_gender = Symbol("vocal_gender");
    vocal_percussion = Symbol("vocal_percussion");
    vocals = Symbol("vocals");
    vocal_tonic_note = Symbol("vocal_tonic_note");
    year_recorded = Symbol("year_recorded");
    year_released = Symbol("year_released");
}

// REGENERATING THIS FILE
// ----------------------
// This list is derived from the rb3-song link, so it goes stale whenever a
// decomp lane adds a handler arm that references a new Symbols3.h global (the
// X360 build never links, so nothing there catches it). To refresh:
//
//   cmake --build native/build --target rb3-song 2>&1 \
//     | grep -oP "undefined reference to .\K[a-z0-9_]+(?=')" | sort -u
//
// Every name that appears is a `extern Symbol' global with no definition in
// the link; add it to both the definition list and InitM1Symbols() above.
