#pragma once
// Unification shim: rb3-Wii canonically defines BOTH DataEvent and
// DataEventList in a single header (midi/DataEvent.h). Our tree split them, and
// the engine implementation (midi/DataEventList.cpp) plus its consumers
// (MidiParser.h, DisplayEvents.h, HamMaster.cpp) follow midi/DataEventList.h's
// layout. To avoid a C2011 duplicate-definition clash when a TU pulls in both
// (e.g. TrainerPanel.cpp includes DataEvent.h and, via MidiParser.h,
// DataEventList.h), DataEvent.h now forwards to the canonical DataEventList.h.
#include "midi/DataEventList.h"
