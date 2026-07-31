// rb3-xenon native — M2 MIDI parsing driver, now reading STRAIGHT OUT OF THE ARK.
//
// M14 (lane CD-3): the archive stopped being a demo and became the filesystem.
// Before this, every driver but rb3-ark took loose files on the command line;
// this one now mounts the real RB3 .ark and parses a disc song's notes.mid with
// no extraction step anywhere in the pipeline.
//
// WHY THIS NEEDED ALMOST NO PLUMBING — the interesting part:
//   FileStream's ctor (utl/FileStream.cpp:5) maps kRead -> NewFile(file, 2), and
//   os/File.cpp:646 hands mode-2 opens of a NON-LOCAL path to ArkFile whenever
//   UsingCD() is true. MidiReader is built on FileStream. So the entire MIDI
//   cluster reads through the archive the moment the driver calls SetUsingCD(true)
//   — no change to MidiReader, FileStream, or any engine file. The mount is three
//   calls; everything below it was already ark-capable and nobody had pointed it
//   at an ark.
//   ★ SetUsingCD(true) is load-bearing and NOT optional: without it NewFile falls
//   through to AsyncFile and tries the archive-relative path on the host FS, which
//   fails in a way that looks like a missing file rather than a misconfiguration.
//
// Pipeline exercised (all real engine code, no reimplementations):
//   1. Archive/ArkFile  — index lookup + multi-ark offset walk + read.
//   2. MidiReader       — file/track headers, var-len deltas, running status,
//                         meta/text/sysex, tempo/timesig. Driven twice: with a
//                         CountingReceiver for raw stats, then with the real
//                         MidiParserMgr.
//   3. MidiParserMgr    — note-on/off pairing, MidiParser::ParseAll (CreateNote),
//                         text bracket-parsing, per-track dispatch.
//   4. MidiParser::Poll — over a synthetic advancing beat, observed through the
//                         real DataEventList cursor + a real MsgSource sink.
//
// HOW THE CRITERION IS MADE FALSIFIABLE (three independent axes, not one):
//   (a) BYTES, externally adjudicated. --dump writes what ArkFile returned so
//       coreutils sha256sum/cmp can judge it. The reference (--reference) is
//       produced by a DIFFERENT implementation in a DIFFERENT language:
//       native/tools/ark_extract.py re-derives the v6 .hdr, the Rand2 keystream,
//       the string heap and the cumulative multi-ark walk from the format alone.
//       That tool is itself calibrated against a third, third-party
//       implementation (arkhelper's bulk extraction), and all three agree.
//   (b) STRUCTURE, independently parsed. This file walks the raw MThd/MTrk chunk
//       headers itself and gates the engine's MidiReader track count against that
//       independent walk. The engine's own opinion of its parse is never the
//       criterion — if MidiReader desynced on a track length, the counts diverge.
//   (c) NEGATIVE CONTROLS THAT ACTUALLY FIRE, both executed and reported:
//         --corrupt  flips one bit of the ark-read buffer  => byte gates FAIL, rc=1.
//         pointing --mid at a real NON-MIDI archive member (songs/gen/songs.dtb)
//                    => the MThd gate FAILS, rc=1, and it does so CLEANLY because
//                       the header is validated BEFORE MidiReader is invoked
//                       (MILO_* are live natively; feeding garbage to MidiReader
//                       would abort with SIGABRT and a core, which is precisely
//                       the "reported PASS while dumping core" failure mode this
//                       project has already shipped once).
//
// Exit codes: 0 = every gate passed. 1 = at least one gate failed, or setup failed.
//
// Usage:
//   rb3-midi <dataDir> [arkMidPath] [--reference <file>] [--dump <file>] [--corrupt]
//   rb3-midi <dataDir> --list
//   rb3-midi --loose <path>          (legacy: parse a loose .mid, no archive)

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
#include "os/Archive.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/FileStream.h"
#include "utl/Symbol.h"
#include "utl/TimeConversion.h"

#include "ark_verify.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern void InitMakeString();
extern DataArray *gSystemConfig; // src/system/os/System.cpp
void DataInit();                 // src/system/obj/Data.cpp
// native/src/platform/File_Native.cpp
extern void NativeSetDataDir(const char *dir);
// native/src/platform/System_Native.cpp
extern void NativeArchiveInit();

using arkverify::Gates;

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

// ----------------------------------------------------- independent SMF walk --
// A deliberately naive, self-contained Standard-MIDI-File structural parse of the
// RAW bytes. It shares nothing with MidiReader: it just reads the 14-byte MThd
// and then walks <4-byte type><4-byte big-endian length> chunk headers.
//
// This is the reference for gate (b). Its whole value is that it can DISAGREE
// with the engine — a track-count match between two parsers that share no code
// is evidence; MidiReader agreeing with itself would not be.
struct SmfInfo {
    bool headerOk = false;
    int format = -1;
    int declaredTracks = -1; // ntrks field in MThd
    int division = -1;
    int mtrkChunks = 0; // MTrk chunks actually present
    bool walkClean = false; // chunk walk consumed the file exactly
};

static unsigned Be16(const unsigned char *p) { return ((unsigned)p[0] << 8) | p[1]; }
static unsigned Be32(const unsigned char *p) {
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}

static SmfInfo WalkSmf(const std::vector<unsigned char> &b) {
    SmfInfo info;
    if (b.size() < 14 || memcmp(b.data(), "MThd", 4) != 0)
        return info;
    if (Be32(b.data() + 4) != 6)
        return info;
    info.headerOk = true;
    info.format = (int)Be16(b.data() + 8);
    info.declaredTracks = (int)Be16(b.data() + 10);
    info.division = (int)Be16(b.data() + 12);

    size_t pos = 8 + Be32(b.data() + 4); // skip MThd payload
    while (pos + 8 <= b.size()) {
        unsigned len = Be32(b.data() + pos + 4);
        if (memcmp(b.data() + pos, "MTrk", 4) == 0)
            info.mtrkChunks++;
        // Overflow-safe advance: len is untrusted data.
        if (len > b.size() - (pos + 8))
            return info; // truncated/!clean — walkClean stays false
        pos += 8 + len;
    }
    info.walkClean = (pos == b.size());
    return info;
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

// Build a MidiReader over `path`. In ark mode `path` is ARCHIVE-RELATIVE and this
// FileStream resolves through NewFile -> ArkFile; in loose mode it is a host path.
// MIDI is big-endian; FileStream defaults to big-endian, which MidiReader asserts.
static void ReadMidiRaw(const char *path, CountingReceiver &rcvr) {
    FileStream fs(path, FileStream::kRead, false);
    MidiReader reader(fs, rcvr, path);
    reader.ReadAllTracks();
}

// ------------------------------------------------------------------ Stage 2 --
static MidiParser *MakeParser(const char *name, const char *typeDefDta) {
    MidiParser *p = new MidiParser();
    p->SetName(name, ObjectDir::Main());
    DataArray *td = DataReadString(typeDefDta);
    p->SetTypeDef(td);
    td->Release();
    return p;
}

// ------------------------------------------------------------------ Sink ----
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

// --list support. Archive::Enumerate takes a plain function pointer with no user
// data, so the tally lives here.
static int gListed = 0;
static void ListCb(const char *path, const char *name) {
    printf("  %s/%s\n", path, name);
    gListed++;
}

int main(int argc, char **argv) {
    const char *dataDir = nullptr;
    const char *midPath = "songs/20thcenturyboy/20thcenturyboy.mid";
    const char *refPath = nullptr;
    const char *dumpPath = nullptr;
    const char *loosePath = nullptr;
    bool corrupt = false;
    bool doList = false;

    std::vector<const char *> pos;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--corrupt") == 0) {
            corrupt = true;
        } else if (strcmp(argv[i], "--list") == 0) {
            doList = true;
        } else if (strcmp(argv[i], "--reference") == 0 && i + 1 < argc) {
            refPath = argv[++i];
        } else if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            dumpPath = argv[++i];
        } else if (strcmp(argv[i], "--loose") == 0 && i + 1 < argc) {
            loosePath = argv[++i];
        } else {
            pos.push_back(argv[i]);
        }
    }

    const bool arkMode = (loosePath == nullptr);
    if (arkMode) {
        if (pos.empty()) {
            fprintf(stderr,
                    "usage: %s <dataDir> [arkMidPath] [--reference <f>] "
                    "[--dump <f>] [--corrupt]\n"
                    "       %s <dataDir> --list\n"
                    "       %s --loose <path>\n",
                    argv[0], argv[0], argv[0]);
            return 1;
        }
        dataDir = pos[0];
        if (pos.size() >= 2)
            midPath = pos[1];
    } else {
        midPath = loosePath;
    }

    Gates g;

    // --- Minimal engine boot (mirrors the M0/M1 harness order) ---
    InitMakeString();
    Symbol::Init();
    DataInit();                     // data funcs + script engine
    ObjectDir::PreInit(256, 4096);  // sMainDir + DataSetThis(main)
    gSystemConfig = MakeSystemConfig();

    printf("=== rb3-xenon native M14: MIDI parsed straight out of the .ark ===\n");
    printf("mode : %s\n", arkMode ? "ARCHIVE" : "loose file (legacy)");
    if (arkMode)
        printf("data : %s\n", dataDir);
    printf("mid  : %s\n", midPath);
    if (refPath)
        printf("ref  : %s\n", refPath);

    std::vector<unsigned char> arkBytes;

    if (arkMode) {
        // --- mount ---------------------------------------------------------
        NativeSetDataDir(dataDir);
        SetUsingCD(true); // ★ must precede any NewFile(); see the header comment.
        NativeArchiveInit();
        if (!TheArchive) {
            fprintf(stderr, "FATAL: TheArchive is null after NativeArchiveInit()\n");
            return 1;
        }

        if (doList) {
            printf("\n--- .mid members under songs/ ---\n");
            // ★ Milo glob dialect, NOT fnmatch: os/File.cpp:207 FileMatchInternal
            // stops at '/' for '*' but not for '&'. So '*' matches within ONE path
            // segment and '&' is the cross-segment wildcard. "*.mid" silently
            // matches NOTHING here (Enumerate compares against the full
            // "path/name"), which reads as an empty archive rather than a bad
            // pattern -- measured 0 hits where the independent parser found 635.
            TheArchive->Enumerate("songs", ListCb, true, "&.mid");
            printf("  %d .mid member(s)\n", gListed);
            return 0;
        }

        // --- where does the engine think this file lives? -------------------
        int arkNum = 0, fileSize = 0, ucSize = 0;
        unsigned long long byteOff = 0;
        bool found = TheArchive->GetFileInfo(
            FileMakePath(".", midPath), arkNum, byteOff, fileSize, ucSize
        );
        printf("\nArchive::GetFileInfo(%s) -> %s\n", midPath,
               found ? "found" : "NOT FOUND");
        if (found) {
            printf("  ark part      : %d (%s)\n", arkNum,
                   TheArchive->GetArkfileName(arkNum));
            printf("  offset in part: %llu (0x%llx)\n", byteOff, byteOff);
            printf("  size          : %d\n", fileSize);
            printf("  uncompressed  : %d %s\n", ucSize,
                   ucSize == 0 ? "(0 = stored uncompressed)" : "(COMPRESSED!)");
        }
        printf("\n--- gates: archive ---\n");
        g.Check("archive-lookup", found,
                found ? "" : "file not present in archive index");
        if (!found)
            return g.Finish();

        // --- read the bytes THROUGH the engine's ArkFile ---------------------
        File *f = NewFile(midPath, 2); // mode 2 == kRead
        if (!f) {
            g.Check("open-through-ArkFile", false, "NewFile returned null");
            return g.Finish();
        }
        g.Check("open-through-ArkFile", true, "");

        int size = f->Size();
        arkBytes.resize((size_t)size);
        int total = 0;
        while (total < size) {
            int got = f->Read(arkBytes.data() + total, size - total);
            if (got <= 0) break;
            total += got;
        }
        bool fullRead = (total == size) && !f->Fail();
        {
            char d[128];
            snprintf(d, sizeof(d), "read %d of %d bytes%s", total, size,
                     f->Fail() ? ", ArkFile::Fail() set" : "");
            g.Check("full-read", fullRead, d);
        }
        delete f;

        if (corrupt && !arkBytes.empty()) {
            printf("  [note] --corrupt: flipping one bit of byte 0 (NEGATIVE CONTROL)\n");
            arkBytes[0] ^= 0x01;
        }

        if (dumpPath) {
            FILE *out = fopen(dumpPath, "wb");
            if (out) {
                fwrite(arkBytes.data(), 1, arkBytes.size(), out);
                fclose(out);
                printf("  [note] wrote ark-read bytes to %s for EXTERNAL verification\n",
                       dumpPath);
            } else {
                printf("  [note] could not write dump to %s\n", dumpPath);
            }
        }

        // --- byte identity vs an independently-produced reference ------------
        if (refPath) {
            std::vector<unsigned char> refBytes;
            bool refOk = arkverify::ReadWholeFilePosix(refPath, refBytes);
            g.Check("reference-readable", refOk, refPath);

            char arkHex[80] = { 0 }, refHex[80] = { 0 };
            arkverify::Sha256Hex(arkBytes.data(), arkBytes.size(), arkHex);
            if (refOk)
                arkverify::Sha256Hex(refBytes.data(), refBytes.size(), refHex);

            printf("\n  sha256(ark read via ArkFile) = %s  (%zu bytes)\n", arkHex,
                   arkBytes.size());
            printf("  sha256(reference on disk)    = %s  (%zu bytes)\n",
                   refOk ? refHex : "<unreadable>", refBytes.size());

            bool sizeOk = refOk && (arkBytes.size() == refBytes.size());
            g.Check("size-match", sizeOk, "");
            g.Check("sha256-match", refOk && strcmp(arkHex, refHex) == 0, "");
            g.Check("byte-for-byte memcmp",
                    sizeOk
                        && memcmp(arkBytes.data(), refBytes.data(), arkBytes.size()) == 0,
                    "");
        } else {
            printf("\n  [note] no --reference given; byte-identity gates skipped\n");
        }
    } else {
        // Loose mode: read the bytes with POSIX I/O so the structural gates below
        // still apply.
        if (!arkverify::ReadWholeFilePosix(midPath, arkBytes)) {
            fprintf(stderr, "FATAL: cannot read %s\n", midPath);
            return 1;
        }
        printf("\n--- gates ---\n");
    }

    // --- structural gates: an INDEPENDENT parse of the same bytes -------------
    // Validated BEFORE MidiReader runs. MILO_* asserts are live in the native
    // build, so handing a non-MIDI to MidiReader would abort with a core rather
    // than fail a gate; this ordering is what makes the non-MIDI negative control
    // exit cleanly at rc=1.
    SmfInfo smf = WalkSmf(arkBytes);
    printf("\n--- gates: structure (independent SMF walk, shares no code with MidiReader) ---\n");
    {
        char d[160];
        snprintf(d, sizeof(d), "MThd format=%d ntrks=%d division=%d", smf.format,
                 smf.declaredTracks, smf.division);
        g.Check("valid-MThd", smf.headerOk, smf.headerOk ? d : "not a Standard MIDI File");
    }
    if (!smf.headerOk) {
        printf("\n  Refusing to hand a non-MIDI stream to MidiReader (its MILO_* "
               "asserts are live natively and would abort).\n");
        return g.Finish();
    }
    {
        char d[160];
        snprintf(d, sizeof(d), "%d MTrk chunk(s) walked, declared %d, walk %s",
                 smf.mtrkChunks, smf.declaredTracks, smf.walkClean ? "clean" : "RAGGED");
        g.Check("chunk-walk == MThd ntrks",
                smf.walkClean && smf.mtrkChunks == smf.declaredTracks, d);
    }

    // === Stage 1: raw MidiReader parse — THROUGH THE ARK ====================
    printf("\n--- Stage 1: MidiReader raw parse (FileStream -> NewFile -> ArkFile) ---\n");
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
    printf("  MidiReader read %zu track(s); %d note-ons, %d text, %d lyric events.\n",
           counter.tracks.size(), totOn, totText, totLyric);

    printf("\n--- gates: engine parse vs independent walk ---\n");
    {
        char d[160];
        snprintf(d, sizeof(d), "MidiReader %zu vs independent walk %d",
                 counter.tracks.size(), smf.mtrkChunks);
        g.Check("MidiReader track count == independent walk",
                (int)counter.tracks.size() == smf.mtrkChunks, d);
    }
    {
        char d[96];
        snprintf(d, sizeof(d), "%d note-on(s) decoded from the archive stream", totOn);
        g.Check("note-ons > 0", totOn > 0, d);
    }

    // === Stage 2: MidiParserMgr + MidiParser::ParseAll =====================
    printf("\n--- Stage 2: MidiParserMgr dispatch + MidiParser::ParseAll ---\n");

    MidiParser *eventsParser =
        MakeParser("events_parser",
                   "(track_name EVENTS) (text {$this add_message $mp.data})");
    MidiParser *noteParser =
        MakeParser("note_parser",
                   "(track_name 'PART BASS') (midi {$this add_message $mp.data})");

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

    eventsParser->Reset(-1.0e30f);
    noteParser->Reset(-1.0e30f);

    EventProbeSink *probe = new EventProbeSink();
    eventsParser->AddSink(probe);
    printf("  Registered EventProbeSink as a global sink on events_parser.\n");

    float lastBeat = 0.0f;
    for (int i = 0; i < eventsParser->Events()->Size(); i++)
        lastBeat = Max(lastBeat, eventsParser->Events()->Event(i).Start());
    for (int i = 0; i < noteParser->Events()->Size(); i++)
        lastBeat = Max(lastBeat, noteParser->Events()->Event(i).Start());

    int firedEvents = 0, firedNotes = 0;
    const float step = 4.0f; // advance one measure (4 beats) per poll tick
    for (float beat = 0.0f; beat <= lastBeat + step; beat += step) {
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

    printf("\n--- gates: parser pipeline ---\n");
    {
        char d[128];
        snprintf(d, sizeof(d), "%d EVENTS-track event(s) parsed out of the archive",
                 eventsParser->Events()->Size());
        g.Check("events_parser produced events", eventsParser->Events()->Size() > 0, d);
    }
    {
        char d[128];
        snprintf(d, sizeof(d), "%d of %d fired event(s) reached the sink",
                 probe->received, firedEvents);
        g.Check("every fired event reached the MsgSource sink",
                firedEvents > 0 && probe->received == firedEvents, d);
    }

    return g.Finish();
}
