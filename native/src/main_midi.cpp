// rb3-xenon native — M2 MIDI parsing driver.
//
// Exercises the REAL Milo MIDI cluster end to end on a real RB-format notes.mid:
//
//   1. MidiReader          — parses the .mid (file/track headers, var-len deltas,
//                            running status, meta/text/sysex events, tempo/timesig).
//                            Driven twice: once with a lightweight CountingReceiver
//                            for raw per-track stats, once with the real
//                            MidiParserMgr.
//   2. MidiParserMgr       — the real MidiReceiver: note-on/off pairing
//      + MidiParser::ParseAll   (CreateNote), text bracket-parsing (ParseText ->
//                            DataReadString), and per-track dispatch to the
//                            MidiParsers, whose ParseAll runs the typedef parser
//                            scripts and fills each DataEventList.
//   3. MidiParser::Poll    — driven over a synthetic advancing beat
//      + DataEventList::NextEvent  (TheTaskMgr.SetSecondsAndBeat). Fired events are
//                            observed through the real DataEventList cursor.
//
// Real engine code paths throughout — no reimplementations. The one genuine hole
// is MsgSource (undecompiled; only a native no-op Export shim exists), so Poll's
// broadcast has no sink side effect to observe; instead we read the real event
// cursor that Poll advances via NextEvent.

#include "midi/MidiParserMgr.h"
#include "midi/MidiParser.h"
#include "midi/Midi.h" // MidiReader / MidiReceiver
#include "midi/MidiConstants.h"
#include "midi/DataEventList.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "obj/Msg.h" // MsgSource / AddSink (the Poll broadcast target)
#include "obj/Task.h"
#include "utl/FileStream.h"
#include "utl/Symbol.h"
#include "utl/TimeConversion.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern void InitMakeString();
extern DataArray *gSystemConfig; // src/system/os/System.cpp
void DataInit();                 // src/system/obj/Data.cpp

// SystemConfig("beatmatcher") must resolve to a valid array (MidiParserMgr's ctor
// + FinishLoad look up (beatmatcher (midi_parsers ...))). We give it an empty
// (beatmatcher) so midi_parsers is absent -> the mgr constructs no parsers of its
// own, and we construct them directly below (they self-register in
// MidiParser::Parsers(), which is exactly what the mgr dispatches over).
static DataArray *MakeSystemConfig() {
    DataArray *bm = new DataArray(1);
    bm->Node(0) = DataNode(Symbol("beatmatcher"));
    DataArray *cfg = new DataArray(1);
    cfg->Node(0) = DataNode(bm, kDataArray);
    return cfg;
}

// ------------------------------------------------------------------ Stage 1 --
// Lightweight MidiReceiver: raw per-track event tallies straight off MidiReader.
class CountingReceiver : public MidiReceiver {
public:
    struct TrackStat {
        std::string name;
        int noteOns = 0;
        int noteOffs = 0;
        int text = 0;
        int lyric = 0;
        int otherMsgs = 0;
    };
    std::vector<TrackStat> tracks;

    void OnNewTrack(int) override { tracks.push_back(TrackStat()); }
    void OnEndOfTrack() override {}
    void OnAllTracksRead() override {}
    void OnMidiMessage(int, unsigned char status, unsigned char, unsigned char) override {
        if (tracks.empty())
            return;
        switch (MidiGetType(status)) {
        case kNoteOn: tracks.back().noteOns++; break;
        case kNoteOff: tracks.back().noteOffs++; break;
        default: tracks.back().otherMsgs++; break;
        }
    }
    void OnText(int, const char *text, unsigned char type) override {
        if (tracks.empty())
            return;
        if (type == kTrackname)
            tracks.back().name = text ? text : "";
        else if (type == kLyricEvent)
            tracks.back().lyric++;
        else if (type == kTextEvent)
            tracks.back().text++;
    }
};

// Build a MidiReader over a real file. MIDI is big-endian; FileStream defaults to
// big-endian (BinStream(lilEndian=false)), which is what MidiReader asserts.
static void ReadMidiRaw(const char *path, CountingReceiver &rcvr) {
    FileStream fs(path, FileStream::kRead, false);
    MidiReader reader(fs, rcvr, path);
    reader.ReadAllTracks();
}

// ------------------------------------------------------------------ Stage 2 --
// Construct a real MidiParser with a typedef parsed from a DTA snippet. The
// MidiParser ctor pushes itself onto MidiParser::Parsers(), so MidiParserMgr will
// dispatch parsed events to it by track name.
static MidiParser *MakeParser(const char *name, const char *typeDefDta) {
    MidiParser *p = new MidiParser();
    p->SetName(name, ObjectDir::Main());
    DataArray *td = DataReadString(typeDefDta);
    p->SetTypeDef(td);
    td->Release();
    return p;
}

// ------------------------------------------------------------------ Sink ----
// A real Hmx::Object that registers as a global sink on the events_parser
// (a MidiParser IS-A MsgSource). When MidiParser::Poll fires an event it calls
// MsgSource::Export(msg,false), which broadcasts through mSinks -> Sink::Export
// -> obj->Handle(msg,false). This probe's Handle is that receiving handler:
// with the real (no-longer-shimmed) MsgSource bodies the broadcast lands here.
class EventProbeSink : public Hmx::Object {
public:
    int received = 0;
    DataNode Handle(DataArray *msg, bool /*warn*/) {
        String s;
        msg->Print(s, kDataArray, false);
        printf("    >>> SINK received broadcast: %s\n", s.c_str());
        received++;
        return DataNode(kDataUnhandled, 0);
    }
};

static void PrintFiredEvents(const char *label, MidiParser *p, float beat, int fromIdx) {
    DataEventList *ev = p->Events();
    int to = ev->CurIndex();
    for (int i = fromIdx; i < to; i++) {
        const DataEvent &e = ev->Event(i);
        String msg;
        if (e.Msg())
            e.Msg()->Print(msg, kDataArray, false);
        printf("    [beat %-8.3f] %-16s fired @start=%-8.3f end=%-8.3f  msg=%s\n", beat,
               label, e.Start(), e.End(), e.Msg() ? msg.c_str() : "(null)");
    }
}

int main(int argc, char **argv) {
    const char *defMid =
        "/home/free/code/milohax/onyx/songs-cort/hurt/pills/notes.mid";
    const char *midPath = (argc >= 2) ? argv[1] : defMid;

    // --- Minimal engine boot (mirrors the M0/M1 harness order) ---
    InitMakeString();
    Symbol::Init();
    DataInit();                     // data funcs + script engine
    ObjectDir::PreInit(256, 4096);  // sMainDir + DataSetThis(main)
    gSystemConfig = MakeSystemConfig();

    printf("=== rb3-xenon native M2: MIDI parser ===\n");
    printf("mid: %s\n\n", midPath);

    // === Stage 1: raw MidiReader parse =====================================
    printf("--- Stage 1: MidiReader raw parse ---\n");
    CountingReceiver counter;
    ReadMidiRaw(midPath, counter);
    printf("  %-2s  %-18s %6s %6s %6s %6s %6s\n", "#", "track", "on", "off", "text",
           "lyric", "other");
    int totOn = 0, totText = 0, totLyric = 0;
    for (size_t i = 0; i < counter.tracks.size(); i++) {
        const CountingReceiver::TrackStat &t = counter.tracks[i];
        printf("  %-2zu  %-18s %6d %6d %6d %6d %6d\n", i,
               t.name.empty() ? "(unnamed)" : t.name.c_str(), t.noteOns, t.noteOffs,
               t.text, t.lyric, t.otherMsgs);
        totOn += t.noteOns;
        totText += t.text;
        totLyric += t.lyric;
    }
    printf("  MidiReader read %zu track(s); %d note-ons, %d text, %d lyric events.\n\n",
           counter.tracks.size(), totOn, totText, totLyric);

    // === Stage 2: MidiParserMgr + MidiParser::ParseAll =====================
    printf("--- Stage 2: MidiParserMgr dispatch + MidiParser::ParseAll ---\n");

    // Real parsers, directly constructed (self-register in MidiParser::Parsers()).
    // events_parser : EVENTS text track  -> add each event array
    // note_parser   : PART BASS notes    -> add each note event array
    // Track names with a space are Milo DTA single-quoted symbols ('PART BASS');
    // double-quotes would make a String and mismatch the Symbol track_name field.
    MidiParser *eventsParser =
        MakeParser("events_parser",
                   "(track_name EVENTS) (text {$this add_message $mp.data})");
    MidiParser *noteParser =
        MakeParser("note_parser",
                   "(track_name 'PART BASS') (midi {$this add_message $mp.data})");

    // Real receiver. gems=null -> gem parsing skipped; note/text parsing still runs.
    MidiParserMgr mgr(nullptr, Symbol("nativetest"));

    {
        FileStream fs(midPath, FileStream::kRead, false);
        MidiReader reader(fs, mgr, midPath);
        reader.ReadAllTracks();
    }
    mgr.FinishLoad();

    printf("  Parsed DataEventLists:\n");
    printf("    events_parser (EVENTS text): %d events\n",
           eventsParser->Events()->Size());
    printf("    note_parser   (PART BASS)  : %d events\n", noteParser->Events()->Size());

    // Dump a sample of the events_parser list.
    {
        DataEventList *ev = eventsParser->Events();
        int n = ev->Size();
        int show = n < 12 ? n : 12;
        printf("  events_parser sample (first %d of %d):\n", show, n);
        for (int i = 0; i < show; i++) {
            const DataEvent &e = ev->Event(i);
            String msg;
            if (e.Msg())
                e.Msg()->Print(msg, kDataArray, false);
            printf("    start=%-9.3f end=%-9.3f  %s\n", e.Start(), e.End(),
                   e.Msg() ? msg.c_str() : "(null)");
        }
    }

    // Sample of the note_parser list (msg = (0 <midi note number>)).
    {
        DataEventList *ev = noteParser->Events();
        int n = ev->Size();
        int show = n < 8 ? n : 8;
        printf("  note_parser sample (first %d of %d):\n", show, n);
        for (int i = 0; i < show; i++) {
            const DataEvent &e = ev->Event(i);
            String msg;
            if (e.Msg())
                e.Msg()->Print(msg, kDataArray, false);
            printf("    start=%-9.3f end=%-9.3f  %s\n", e.Start(), e.End(),
                   e.Msg() ? msg.c_str() : "(null)");
        }
    }

    // === Stage 3: MidiParser::Poll over a synthetic advancing beat =========
    printf("\n--- Stage 3: MidiParser::Poll over advancing beat ---\n");

    // Reset each parser cursor to the start so Poll re-fires from beat 0.
    eventsParser->Reset(-1.0e30f);
    noteParser->Reset(-1.0e30f);

    // Register a real sink on the events_parser. MidiParser::Poll -> Export ->
    // MsgSource::Sink::Export -> sink->Handle. Global sink (null event) in
    // kHandle mode receives every broadcast message.
    EventProbeSink *probe = new EventProbeSink();
    eventsParser->AddSink(probe);
    printf("  Registered EventProbeSink as a global sink on events_parser.\n");

    // Find the last event beat so we drive the clock across the whole track.
    float lastBeat = 0.0f;
    for (int i = 0; i < eventsParser->Events()->Size(); i++)
        lastBeat = Max(lastBeat, eventsParser->Events()->Event(i).Start());
    for (int i = 0; i < noteParser->Events()->Size(); i++)
        lastBeat = Max(lastBeat, noteParser->Events()->Event(i).Start());

    int firedEvents = 0, firedNotes = 0;
    const float step = 4.0f; // advance one measure (4 beats) per poll tick
    for (float beat = 0.0f; beat <= lastBeat + step; beat += step) {
        // Synthetic beat clock: this is what MidiParser::Poll reads.
        TheTaskMgr.SetSecondsAndBeat(BeatToSeconds(beat), beat, false);

        int evFrom = eventsParser->Events()->CurIndex();
        eventsParser->Poll();
        PrintFiredEvents("events", eventsParser, beat, evFrom);
        firedEvents += eventsParser->Events()->CurIndex() - evFrom;

        int ntFrom = noteParser->Events()->CurIndex();
        noteParser->Poll();
        int ntFired = noteParser->Events()->CurIndex() - ntFrom;
        if (ntFired > 0)
            printf("    [beat %-8.3f] notes            fired %d note event(s) this window\n",
                   beat, ntFired);
        firedNotes += ntFired;
    }

    printf("\n  Poll drove the beat clock 0 -> %.1f (step %.1f).\n", lastBeat + step, step);
    printf("  events_parser fired %d event(s); note_parser fired %d note event(s).\n",
           firedEvents, firedNotes);
    printf("  EventProbeSink received %d broadcast(s) via MsgSource::Export.\n",
           probe->received);
    if (probe->received == firedEvents && firedEvents > 0)
        printf("  OK: every fired events_parser event reached the sink handler.\n");
    else if (probe->received > 0)
        printf("  Sink saw %d of %d events (mode/handler routing).\n", probe->received,
               firedEvents);
    else
        printf("  WARNING: sink received nothing — broadcast did not reach the sink.\n");
    printf("\nDone.\n");
    return 0;
}
