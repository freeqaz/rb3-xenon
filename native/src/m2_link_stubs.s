# M2 (MIDI parser) native link stubs.
#
# Weak no-op definitions for off-the-MIDI-path engine symbols that the newly
# wired midi/ TUs (and their transitive vtable/typeinfo refs) pull in but that
# are not exercised by the MidiReader -> MidiParserMgr -> MidiParser::Poll flow.
# Populated after the first link surfaces undefined symbols.
#
# Weak so a real decompiled body overrides them if one gets wired later.
.text

# float DisplayEvents(DataEventList*, float, float)
# Referenced only by MidiParser::OnDebugDraw (debug-draw handler, off the parse/
# Poll path). The real body lives in midi/DisplayEvents.cpp, which pulls in the
# rndobj draw stack; stub it to 0.0f here. Returns float -> xmm0.
.weak _Z13DisplayEventsP13DataEventListff
.type _Z13DisplayEventsP13DataEventListff,@function
_Z13DisplayEventsP13DataEventListff:
    xorps %xmm0, %xmm0
    ret
