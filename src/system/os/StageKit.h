#pragma once

// The Xbox 360 Rock Band Stage Kit driver: fog, strobe and four LED banks
// (0 blue, 1 green, 2 yellow, 3 red), sent to the pad whose controller type is
// `stagekit_xbox` through JoypadStageKitSetRaw. Retail keeps it in one TU at
// 0x82521B30..0x825227E8 with no symbols; every name here is ours.
//
// SystemInit calls StageKitInit (0x825113E0), which registers the thirteen
// stagekit_* script functions; SystemPoll ends with StageKitPoll.
// BandDirector::SetFog and LightPreset::Keyframe::ApplyStageKit drive it from C++.

bool StageKitConnected(); // 0x82521C80
void StageKitSetFog(bool on); // 0x82521D80
void StageKitSetStrobe(int setting); // 0x82522028
void StageKitSetLedState(int bank, int state); // 0x82521B98
void StageKitSetLedPattern(int bank, int pattern); // 0x82521BF0
void StageKitUpdateLeds(); // 0x82521E20
void StageKitReset(); // 0x82521CB0
void StageKitPoll(); // 0x82521ED0
void StageKitInit(); // 0x82522608
