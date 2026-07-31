// rb3-xenon native M12 -- the REAL RB3 crowd-rating config.
//
// PROVENANCE (do not hand-edit): this is the verbatim `(crowd ...)` block of
// Rock Band 3's shipped config/scoring.dta, taken from
//   /home/free/code/milohax/rb3/orig-assets/extracted/config/scoring.dta
// (the `(crowd` block, ~line 222 onward), with the four difficulty macros
// expanded exactly as Milo's DTA preprocessor expands them per
//   /home/free/code/milohax/rb3/orig-assets/extracted/config/macros.dta:306-313
//     kDifficultyEasy -> 0   kDifficultyMedium -> 1
//     kDifficultyHard -> 2   kDifficultyExpert -> 3
//
// That expansion is why Scoring::GetCrowdConfig's `mConfig->FindArray("crowd")
// ->FindArray(diff)` correctly binds DataArray::FindArray(INT): after macro
// expansion the per-difficulty keys really are integers 0..3. A future lane must
// NOT "fix" that call to use DifficultyToSym() -- it is already right, and the
// symbol form would fail to resolve.
//
// These are genuine shipped Harmonix tuning values, not invented ones, so the
// crowd meter in main_crowd.cpp behaves as it does in the retail game.
#pragma once

static const char *kRealCrowdConfigDta =
R"DTA(
(crowd
   (0
      (default
         (note_weight 1.25e-2)
         (range 0.4 0.9)
         (free_ride 0.4)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (drum
         (note_weight 1.0e-2)
         (range 0.25 0.9)
         (free_ride 0.4)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (vocals
         (note_weight 2.0e-2)
         (range 0.2 0.75)
         (free_ride 0.4)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.1)
         (initial_display_level 0.8333))
      (real_guitar
         (note_weight 2.5e-2)
         (range 5.0e-2 0.8)
         (free_ride 0.4)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (real_bass
         (note_weight 2.5e-2)
         (range 5.0e-2 0.8)
         (free_ride 0.4)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (real_keys
         (note_weight 1.25e-2)
         (range 0.4 0.85)
         (free_ride 0.4)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333)))
   (1
      (default
         (note_weight 1.0e-2)
         (range 0.6 0.9)
         (free_ride 0.1)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (drum
         (note_weight 1.0e-2)
         (range 0.6 0.95)
         (free_ride 0.1)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (vocals
         (note_weight 1.25e-2)
         (range 0.3 0.75)
         (free_ride 0.1)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.1)
         (initial_display_level 0.8333))
      (real_guitar
         (note_weight 1.7e-2)
         (range 0.3 0.8)
         (free_ride 0.1)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (real_bass
         (note_weight 1.7e-2)
         (range 0.3 0.8)
         (free_ride 0.1)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (real_keys
         (note_weight 1.0e-2)
         (range 0.4 0.85)
         (free_ride 0.1)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333)))
   (2
      (default
         (note_weight 8.0e-3)
         (range 0.65 0.9)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (drum
         (note_weight 8.0e-3)
         (range 0.7 0.95)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (vocals
         (note_weight 1.0e-2)
         (range 0.35 0.75)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.1)
         (initial_display_level 0.8333))
      (real_guitar
         (note_weight 1.0e-2)
         (range 0.45 0.8)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (real_bass
         (note_weight 1.0e-2)
         (range 0.45 0.8)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (real_keys
         (note_weight 8.0e-3)
         (range 0.45 0.85)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333)))
   (3
      (default
         (note_weight 8.0e-3)
         (range 0.65 0.9)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (drum
         (note_weight 6.0e-3)
         (range 0.75 0.95)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (vocals
         (note_weight 8.0e-3)
         (range 0.4 0.75)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.13)
         (initial_display_level 0.8333))
      (real_guitar
         (note_weight 8.0e-3)
         (range 0.45 0.8)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (real_guitar
         (note_weight 8.0e-3)
         (range 0.45 0.8)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333))
      (real_keys
         (note_weight 8.0e-3)
         (range 0.45 0.85)
         (free_ride 0.0)
         (great_level 0.95)
         (okay_level 0.66)
         (bad_level 0.33)
         (warning_level 0.15)
         (lose_level 1.0e-2)
         (phrase_weight 0.2)
         (initial_display_level 0.8333)))
   (weights
      (0
         (1.0))
      (1
         (1.0))
      (2
         (0.5 0.5))
      (3
         (0.33 0.33 0.34))
      (4
         (0.25 0.25 0.25 0.25)))
   (drum_slot_weights
      (basic
         (1.0 1.0 0.33 0.33 0.33))
      (drums0
         (1.0 1.0 0.33 0.33 0.33))
      (drums0d
         (1.0 0.33 1.0 0.33 0.33))
      (drums0dnoflip
         (1.0 0.33 1.0 0.33 0.33))
      (drums1
         (1.0 1.0 0.33 0.33 0.33))
      (drums1a
         (1.0 1.0 0.33 0.33 0.33))
      (drums1easy
         (1.0 1.0 0.33 0.33 0.33))
      (drums1easynokick
         (1.0 1.0 0.33 0.33 0.33))
      (drums1d
         (1.0 0.33 1.0 0.33 0.33))
      (drums1dnoflip
         (1.0 0.33 1.0 0.33 0.33))
      (drums2
         (1.0 1.0 0.33 0.33 0.33))
      (drums2a
         (1.0 1.0 0.33 0.33 0.33))
      (drums2easy
         (1.0 1.0 0.33 0.33 0.33))
      (drums2easynokick
         (1.0 1.0 0.33 0.33 0.33))
      (drums2easysnareonly
         (1.0 1.0 0.33 0.33 0.33))
      (drums2d
         (1.0 0.33 1.0 0.33 0.33))
      (drums2dnoflip
         (1.0 0.33 1.0 0.33 0.33))
      (drums3
         (1.0 1.0 0.33 0.33 0.33))
      (drums3a
         (1.0 1.0 0.33 0.33 0.33))
      (drums3d
         (1.0 0.33 1.0 0.33 0.33))
      (drums3dnoflip
         (1.0 0.33 1.0 0.33 0.33))
      (drums3easy
         (1.0 1.0 0.33 0.33 0.33))
      (drums3easynokick
         (1.0 1.0 0.33 0.33 0.33))
      (drums4
         (1.0 1.0 0.33 0.33 0.33))
      (drums4a
         (1.0 1.0 0.33 0.33 0.33))
      (drums4d
         (1.0 0.33 1.0 0.33 0.33))
      (drums4dnoflip
         (1.0 0.33 1.0 0.33 0.33))
      (drums4easynokick
         (1.0 1.0 0.33 0.33 0.33))
      (default_weights basic))
   (multiplayer_lose_level 0.2)
   (save_level 0.8333)
   (time_to_return_from_brink 3.5)
   (vocals_to_return_from_brink 2)
   (crowd_loss_per_sec 4.0e-2))
)DTA";
