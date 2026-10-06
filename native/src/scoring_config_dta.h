// rb3-xenon native (W16-PJ) -- the REAL RB3 solo-award and coda scoring config.
//
// PROVENANCE (do not hand-edit): verbatim `(solo ...)` and `(coda ...)` blocks of
// Rock Band 3's shipped config/scoring.dta, taken from
//   /home/free/code/milohax/rb3/orig-assets/extracted/config/scoring.dta
// (`(solo` at line 176, `(coda` at line 586). Neither block uses a macro.
//
// WHY: the drivers now link the real src/band3/game/SongDB.cpp, whose
// SongDB::RunMultiplayerAnalyzer -> MultiplayerAnalyzer::PostLoad reads
//   AddCodas : SystemConfig("scoring", "coda")->FindFloat("point_rate")
//              -- unconditionally, even when the song has no coda;
//   AddSolos : Scoring::GetSoloAward -> (scoring (solo <track>|default (awards)))
//              -- for every solo section on a configured track.
// The hand-written driver configs carried neither, so they are spliced in next to
// the real (crowd ...) block (crowd_config_dta.h), inside (scoring ...).
#pragma once

static const char *kRealSoloConfigDta =
R"DTA(
(solo
   (default
      (awards
         (0 0 failed_solo)
         (60 5 bad_solo)
         (70 10 okay_solo)
         (80 20 solid_solo)
         (90 30 great_solo)
         (95 50 awesome_solo)
         (100 100 perfect_solo))
      (reward 1.0)
      (penalty 1.0))
   (real_guitar
      (awards
         (0 0 failed_solo)
         (35 15 bad_solo)
         (55 35 okay_solo)
         (65 55 solid_solo)
         (75 75 great_solo)
         (85 100 awesome_solo)
         (100 150 perfect_solo))
      (reward 1)
      (penalty 0.8))
   (real_bass
      (awards
         (0 0 failed_solo)
         (35 15 bad_solo)
         (55 35 okay_solo)
         (65 55 solid_solo)
         (75 75 great_solo)
         (85 100 awesome_solo)
         (100 150 perfect_solo))
      (reward 1)
      (penalty 0.8))
   (tambourine
      (awards
         (0 0 tamb_rating_1)
         (1 5 tamb_rating_2)
         (40 10 tamb_rating_3)
         (60 20 tamb_rating_4)
         (80 50 tamb_rating_5)
         (100 100 tamb_rating_6))))
)DTA";

static const char *kRealCodaConfigDta =
R"DTA(
(coda
   (point_rate 100)
   (max_mash_ms 1500))
)DTA";
