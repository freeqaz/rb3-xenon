// rb3-xenon native -- W16-TW: more in-scope rows native compiles but no target entered.
//
// docs/decomp/CAMPAIGN_STATE_2026-10-07c.md section 6, lever 2, continued from
// W16-TS (docs/decomp/W16TS_UNENTERED_ROWS_SHIPPED_DATA_GATES_2026-10-07.md). Of
// the 69 behaviour-class rows no target entered, W16-TS took 9; this phase takes
// the next largest that shipped data can drive, in rb3-render's default mode:
//
//   * Locale::Init over the shipped config's locale section and
//     ui/locale/eng/locale_keep.dta;
//   * FileMakePath over a table of paths whose answers were read off the
//     retail body (fn_82516B10);
//   * DirLoader::SaveObjects over shipped milos, against the shipped file's own
//     bytes (shipped milos were written by the retail-era SaveObjects).
//
// THE REFERENCE. Expected values come from the shipped files read directly, or
// from the retail body read off the retail asm and written down here, never from
// the code under test. A fixture gate checks each fixture first.

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/DataUtl.h"
#include "os/File.h"
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "os/System.h"
#include "utl/FilePath.h"
#include "utl/Locale.h"
#include "utl/MemStream.h"
#include "ui/PanelDir.h"

#include <zlib.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

extern DataArray *gSystemConfig;

typedef void (*GateFn)(const char *, bool, const char *);

// ===================================================== joypad back end ==
// rb3-render links the weak ReadSingleJoypad / requestBreedWrite from
// dta_link_stubs.s, which report "no pad", so JoypadPollCommon never sees a
// controller. These strong definitions replace them for rb3-render only. They
// stay inert ("no pad", write refused) unless the W16-TW phase arms a script.
namespace w16tw {
struct PadFrame {
    int type = 0;
    unsigned int buttons = 0;
    signed char lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
};
PadFrame gFrame[4];
bool gScripted = false;
std::vector<std::pair<int, std::vector<unsigned char>>> gBreed;
}

int ReadSingleJoypad(int pad, unsigned int *buttons, char *lx, char *ly, char *rx, char *ry,
                     char *lt, char *rt, float *, float *, unsigned char *) {
    if (!w16tw::gScripted || pad < 0 || pad >= 4)
        return 0;
    const w16tw::PadFrame &f = w16tw::gFrame[pad];
    *buttons = f.buttons;
    *lx = f.lx;
    *ly = f.ly;
    *rx = f.rx;
    *ry = f.ry;
    *lt = f.lt;
    *rt = f.rt;
    return f.type;
}

bool requestBreedWrite(int pad, unsigned char *packet) {
    if (!w16tw::gScripted)
        return false;
    w16tw::gBreed.push_back({ pad, std::vector<unsigned char>(packet, packet + 0x14) });
    return true;
}


namespace {

GateFn gGate = nullptr;
char gBuf[1024];
int gRan = 0;

void Gate(const char *name, bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Gate(const char *name, bool ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gBuf, sizeof(gBuf), fmt, ap);
    va_end(ap);
    gGate(name, ok, gBuf);
    gRan++;
}

// ================================================================ Locale ==
// The shipped config/band_keep.dta carries `(locale ../ui/locale/eng/locale_keep.dta)`
// (read off config/gen/band_keep.dtb). rb3-render cannot read band_keep (it pulls
// ui/dev_only/selvenue.dta, absent from the disc; see main_render.cpp's
// StandUpConfig), so the section is rebuilt here with the same symbol and the same
// owning file. The owning file matters: Init resolves each entry with
// FileMakePath(FileGetPath(cfg->File()), entry), so the entry is relative to
// config/.
const char *kLocaleCfgFile = "config/band_keep.dta";
const char *kLocaleEntry = "../ui/locale/eng/locale_keep.dta";
const char *kLocaleFile = "ui/locale/eng/locale_keep.dta";

// The preinit half rb3-render reads already holds an EMPTY `(locale)` (from
// system/run/config/default.dta). On the console InitSystem then reads
// band_keep.dta as the new config and DataMergeTags the preinit tags into it
// (os/System.cpp, retail 0x82510A08), so band_keep's populated section is the one
// Locale::Init sees. The fixture swaps that node in and puts the old one back.
int gLocaleIdx = -1;
DataNode gLocaleSaved;

DataArray *InstallLocaleSection(bool &installed) {
    installed = false;
    gLocaleIdx = -1;
    for (int i = 0; i < gSystemConfig->Size(); i++) {
        DataNode &n = gSystemConfig->Node(i);
        if (n.Type() == kDataArray && n.Array()->Size() > 0
            && n.Array()->Type(0) == kDataSymbol && n.Array()->Sym(0) == Symbol("locale")) {
            gLocaleIdx = i;
            break;
        }
    }
    DataArray *sec = new DataArray(2);
    sec->Node(0) = Symbol("locale");
    sec->Node(1) = Symbol(kLocaleEntry);
    sec->SetFileLine(Symbol(kLocaleCfgFile), 1);
    if (gLocaleIdx >= 0) {
        gLocaleSaved = gSystemConfig->Node(gLocaleIdx);
        printf("  preinit config's own locale section has %d entr%s (%s); replaced for the "
               "phase\n",
               gLocaleSaved.Array()->Size() - 1,
               gLocaleSaved.Array()->Size() == 2 ? "y" : "ies", gLocaleSaved.Array()->File());
        gSystemConfig->Node(gLocaleIdx) = DataNode(sec, kDataArray);
    } else {
        int n = gSystemConfig->Size();
        gSystemConfig->Resize(n + 1);
        gSystemConfig->Node(n) = DataNode(sec, kDataArray);
        gLocaleIdx = n;
    }
    sec->Release();
    installed = true;
    return SystemConfig()->FindArray("locale", false);
}

void RemoveLocaleSection() {
    if (gLocaleIdx < 0)
        return;
    if (gLocaleSaved.Type() == kDataArray)
        gSystemConfig->Node(gLocaleIdx) = gLocaleSaved;
    else
        gSystemConfig->Resize(gLocaleIdx);
    gLocaleSaved = DataNode();
    gLocaleIdx = -1;
}

void LocaleChecks() {
    printf("\n=== W16-TW: Locale::Init over %s ===\n", kLocaleFile);
    bool installed = false;
    DataArray *sec = InstallLocaleSection(installed);

    // Reference: the shipped file read directly. Locale::Init's retail body
    // (fn_827C9AF8) walks files from last to first and entries from last to
    // first, numbering chunks upward, and sorts with FastSort<3> (symbol word,
    // then that number), so for a repeated symbol the LAST definition in file
    // order is the one kept.
    DataArray *raw = DataReadFile(kLocaleFile, true);
    std::map<std::string, std::string> ref;
    int entries = 0, redefined = 0, malformed = 0;
    if (raw) {
        for (int i = 0; i < raw->Size(); i++) {
            DataNode &n = raw->Node(i);
            if (n.Type() != kDataArray) {
                malformed++;
                continue;
            }
            DataArray *e = n.Array();
            if (e->Size() < 2 || e->Type(0) != kDataSymbol
                || (e->Type(1) != kDataString && e->Type(1) != kDataSymbol)) {
                malformed++;
                continue;
            }
            entries++;
            std::string key = e->Sym(0).Str();
            if (ref.count(key))
                redefined++;
            ref[key] = e->Str(1);
        }
    }
    bool fixtureOk = sec && sec->Size() == 2 && raw && entries > 1000 && malformed == 0
        && strcmp(FileGetPath(sec->File()), "config") == 0;
    Gate("tw-locale-fixture", fixtureOk,
         "section %s (file %s, path '%s'%s); %s: %d entries, %zu distinct, %d redefined, "
         "%d malformed",
         sec ? "present" : "MISSING", sec ? sec->File() : "-",
         sec ? FileGetPath(sec->File()) : "-", installed ? ", installed here" : "",
         kLocaleFile, entries, ref.size(), redefined, malformed);
    if (raw)
        raw->Release();
    if (!fixtureOk) {
        if (installed)
            RemoveLocaleSection();
        return;
    }

    Locale loc;
    loc.Init();

    int wrong = 0, missing = 0;
    std::string firstBad;
    for (std::map<std::string, std::string>::const_iterator it = ref.begin();
         it != ref.end(); ++it) {
        const char *got = loc.Localize(Symbol(it->first.c_str()), false);
        if (!got) {
            missing++;
            if (firstBad.empty())
                firstBad = it->first + " missing";
        } else if (it->second != got) {
            wrong++;
            if (firstBad.empty())
                firstBad = it->first + " = '" + got + "', file says '" + it->second + "'";
        }
    }
    // FindDataIndex's binary search needs the table strictly ascending under the
    // comparison it uses (natively: the interned string pointer).
    int unsorted = 0;
    for (int i = 1; i < loc.mSize; i++)
        if (!(loc.mSymTable[i - 1].Str() < loc.mSymTable[i].Str()))
            unsorted++;
    int flagged = 0;
    for (int i = 0; i < loc.mSize; i++)
        if (loc.mUploadedFlags[i])
            flagged++;
    const char *absent = loc.Localize(Symbol("w16tw_no_such_locale_token"), false);
    const char *empty = loc.Localize(Symbol(), false);
    bool ok = loc.mNumFilesLoaded == 1 && loc.mSize == (int)ref.size() && wrong == 0
        && missing == 0 && unsorted == 0 && flagged == 0 && absent == nullptr && empty
        && *empty == '\0' && loc.mFile == Symbol(kLocaleEntry);
    Gate("tw-locale-init", ok,
         "files %d (want 1), size %d (want %zu distinct), %d wrong, %d missing%s%s, %d out of "
         "order, %d upload flags set, absent token -> %s, null token -> '%s', mFile '%s'",
         loc.mNumFilesLoaded, loc.mSize, ref.size(), wrong, missing, firstBad.empty() ? "" : ": ",
         firstBad.c_str(), unsorted, flagged, absent ? absent : "null",
         empty ? empty : "(null)", loc.mFile.Str());

    loc.Terminate();
    bool clean = loc.mSize == 0 && !loc.mSymTable && !loc.mStrTable && !loc.mUploadedFlags
        && !loc.mStringData && loc.mNumFilesLoaded == 0;
    Gate("tw-locale-terminate", clean, "after Terminate: size %d, tables %s, files %d",
         loc.mSize, (loc.mSymTable || loc.mStrTable) ? "LEFT" : "freed", loc.mNumFilesLoaded);
    if (installed)
        RemoveLocaleSection();
}

// ========================================================== FileMakePath ==
// Expected answers worked by hand from the retail body fn_82516B10:
//   * file's drive (text before ':') is stripped from file and re-prefixed;
//   * an absolute or empty file ignores root but keeps root's drive;
//   * otherwise "%s/%s" of root and file;
//   * FileNormalizePath (fn_825164F0): '\\' -> '/', everything else tolower;
//   * strtok on '/' after the drive; a component whose first byte is '.' is
//     DROPPED unless it is exactly "..", which pops the previous component
//     unless there is none or it also starts with '.', in which case it is kept
//     (retail: lbz/cmplwi 0x2e at .L_82411BC4, the third-byte test, then the
//     stack test against r1+0x50);
//   * an empty result is "/" for a rooted path, else ".".
struct MakePathCase {
    const char *root;
    const char *file;
    const char *want;
};
const MakePathCase kMakePath[] = {
    { "config", "../ui/locale/eng/locale_keep.dta", "ui/locale/eng/locale_keep.dta" },
    { "ui/track", "gen/trackpanel.milo_xbox", "ui/track/gen/trackpanel.milo_xbox" },
    { "Songs\\Foo", "Gen\\Foo.MID", "songs/foo/gen/foo.mid" },
    { "a/b", "/abs/x", "/abs/x" },
    { "a/b", "\\abs\\x", "/abs/x" },
    { "a", "", "." },
    { "/a", "..", "/" },
    { "..", "../x", "../../x" },
    { "a/./b", ".hidden/c", "a/b/c" },
    { "a", "..x/y", "a/y" },
    { "a/", "b/", "a/b" },
    { "", "x", "/x" },
    { "dvd:/root", "x/y", "dvd:/root/x/y" },
    { "dvd:root", "/abs", "dvd:/abs" },
    { "x", "game:\\Data\\F.dta", "game:/data/f.dta" },
    { "devkit:\\locale", "eng\\locale_keep.dta", "devkit:/locale/eng/locale_keep.dta" },
    { "a/b/c", "../../../../d", "../d" },
};

void MakePathChecks() {
    printf("\n=== W16-TW: FileMakePath ===\n");
    int bad = 0;
    for (const MakePathCase &c : kMakePath) {
        const char *got = FileMakePath(c.root, c.file);
        bool ok = got && strcmp(got, c.want) == 0;
        if (!ok) {
            bad++;
            printf("  WRONG  FileMakePath(\"%s\", \"%s\") = \"%s\", want \"%s\"\n", c.root,
                   c.file, got ? got : "(null)", c.want);
        }
    }
    // Both arguments may alias the function's own static buffer; retail copies
    // the aliased one aside first (the two range tests at the top of the body).
    std::string r1 = FileMakePath(FileMakePath("a", "b"), "c");
    std::string r2 = FileMakePath("r", FileMakePath("x", "y"));
    if (r1 != "a/b/c") {
        bad++;
        printf("  WRONG  root aliasing the buffer: \"%s\", want \"a/b/c\"\n", r1.c_str());
    }
    if (r2 != "r/x/y") {
        bad++;
        printf("  WRONG  file aliasing the buffer: \"%s\", want \"r/x/y\"\n", r2.c_str());
    }
    Gate("tw-makepath", bad == 0, "%zu table cases + 2 aliasing cases: %d wrong",
         sizeof(kMakePath) / sizeof(kMakePath[0]), bad);
}

// ======================================================= SaveObjects ==
// The shipped milo, decompressed here without DirLoader or ChunkStream: a
// little-endian header (magic, header size, chunk count, max chunk), then the
// chunk sizes; magic 0xCDBEDEAF chunks are raw deflate unless bit 24 of the size
// marks them stored.
bool ReadShippedMilo(const char *path, std::vector<unsigned char> &out) {
    out.clear();
    File *f = NewFile(path, 2);
    if (!f)
        return false;
    int size = f->Size();
    std::vector<unsigned char> b(size > 0 ? size : 0);
    int got = size > 0 ? f->Read(b.data(), size) : 0;
    delete f;
    if (size < 16 || got != size)
        return false;
    auto le = [&](size_t o) {
        return (unsigned)b[o] | (unsigned)b[o + 1] << 8 | (unsigned)b[o + 2] << 16
            | (unsigned)b[o + 3] << 24;
    };
    unsigned magic = le(0), hdr = le(4), n = le(8);
    if ((magic != 0xCDBEDEAF && magic != 0xCABEDEAF) || 16 + 4 * n > b.size())
        return false;
    size_t p = hdr;
    for (unsigned i = 0; i < n; i++) {
        unsigned s = le(16 + 4 * i);
        bool stored = magic == 0xCABEDEAF || (s & 0x01000000);
        s &= 0xFFFFFF;
        if (p + s > b.size())
            return false;
        if (stored) {
            out.insert(out.end(), b.begin() + p, b.begin() + p + s);
        } else {
            z_stream z;
            memset(&z, 0, sizeof(z));
            if (inflateInit2(&z, -15) != Z_OK)
                return false;
            z.next_in = b.data() + p;
            z.avail_in = s;
            unsigned char tmp[0x10000];
            int rc;
            do {
                z.next_out = tmp;
                z.avail_out = sizeof(tmp);
                rc = inflate(&z, Z_NO_FLUSH);
                out.insert(out.end(), tmp, tmp + (sizeof(tmp) - z.avail_out));
            } while (rc == Z_OK);
            inflateEnd(&z);
            if (rc != Z_STREAM_END)
                return false;
        }
        p += s;
    }
    return true;
}

// The milo header, as SaveObjects writes it: rev, class, name, hash size,
// string size, object count, then class and name per object (big-endian).
struct MiloHeader {
    unsigned rev = 0;
    std::string cls, name;
    unsigned hash = 0, str = 0;
    std::vector<std::pair<std::string, std::string>> objs;
    size_t end = 0;
};

bool ParseHeader(const unsigned char *d, size_t n, MiloHeader &h) {
    size_t p = 0;
    auto be = [&](unsigned &v) {
        if (p + 4 > n)
            return false;
        v = (unsigned)d[p] << 24 | (unsigned)d[p + 1] << 16 | (unsigned)d[p + 2] << 8
            | d[p + 3];
        p += 4;
        return true;
    };
    auto str = [&](std::string &s) {
        unsigned len;
        if (!be(len) || p + len > n)
            return false;
        s.assign((const char *)d + p, len);
        p += len;
        return true;
    };
    unsigned cnt;
    if (!be(h.rev) || !str(h.cls) || !str(h.name) || !be(h.hash) || !be(h.str) || !be(cnt))
        return false;
    for (unsigned i = 0; i < cnt; i++) {
        std::pair<std::string, std::string> o;
        if (!str(o.first) || !str(o.second))
            return false;
        h.objs.push_back(o);
    }
    h.end = p;
    return true;
}

int CountMarks(const unsigned char *d, size_t n) {
    int c = 0;
    for (size_t i = 0; i + 4 <= n; i++)
        if (d[i] == 0xAD && d[i + 1] == 0xDE && d[i + 2] == 0xAD && d[i + 3] == 0xDE)
            c++;
    return c;
}

const char *kSaveMilos[] = {
    "ui/gen/colors_default.milo_xbox",
    "ui/resource/gen/color.milo_xbox",
    "ui/resource/gen/star_display.milo_xbox",
    "ui/main/gen/attract_overlay.milo_xbox",
};

void SaveObjectsChecks() {
    printf("\n=== W16-TW: DirLoader::SaveObjects over shipped milos ===\n");
    for (const char *path : kSaveMilos) {
        std::string tagStr = FileGetBase(path); // FileGetBase returns a static buffer
        const char *tag = tagStr.c_str();
        std::vector<unsigned char> shipped;
        MiloHeader sh;
        bool readOk = ReadShippedMilo(path, shipped)
            && ParseHeader(shipped.data(), shipped.size(), sh);
        ObjDirPtr<ObjectDir> dir;
        dir.LoadFile(FilePath(path), false, false, kLoadFront, false);
        int live = 0;
        if (dir.Ptr())
            for (ObjDirItr<Hmx::Object> it(dir.Ptr(), false); it != nullptr; ++it)
                if (it != dir.Ptr())
                    live++;
        bool fixtureOk = readOk && dir.Ptr() && live == (int)sh.objs.size();
        Gate(MakeString("tw-save-fixture-%s", tag), fixtureOk,
             "%s: shipped %zu bytes, rev %u, %s '%s', %zu objects; loaded %s with %d objects",
             path, shipped.size(), sh.rev, sh.cls.c_str(), sh.name.c_str(), sh.objs.size(),
             dir.Ptr() ? dir->ClassName().Str() : "NOTHING", live);
        if (!fixtureOk)
            continue;

        if (PanelDir *pd = dynamic_cast<PanelDir *>(dir.Ptr()))
            printf("  %s as loaded: cam %s, env %s, test event '%s', constraint %d, "
                   "target %s, parent %s\n",
                   tag, pd->mCam ? pd->mCam->Name() : "(null)",
                   pd->mEnv ? pd->mEnv->Name() : "(null)", pd->mTestEvent.Str(),
                   (int)pd->TransConstraint(), pd->mTarget ? pd->mTarget->Name() : "(null)",
                   pd->TransParent() ? pd->TransParent()->Name() : "(null)");
        // Load-context fields. ObjectDir::PreLoad (rev > 10) resolves the saved
        // current camera with FindObject(name, true) and, when that fails while
        // mCurViewportID is 7, resets the id to 0. attract_overlay was saved in
        // the tool with mCurViewportID 7 and camera "[ui.cam]", which does not
        // resolve in a standalone load, so the expected stream is the shipped
        // one with that rule applied: id 0 and an empty camera name. The edit is
        // made only where the shipped bytes are exactly what this says.
        std::vector<unsigned char> expect = shipped;
        const char *edit = "none";
        {
            static const unsigned char kCam[] = { 0, 0, 0, 8, '[', 'u', 'i', '.', 'c', 'a', 'm', ']' };
            size_t camAt = std::string::npos;
            for (size_t i = sh.end; i + sizeof(kCam) <= expect.size(); i++)
                if (memcmp(&expect[i], kCam, sizeof(kCam)) == 0) {
                    camAt = i;
                    break;
                }
            ObjectDir *od = dir.Ptr();
            if (camAt != std::string::npos && od->mCurCam == nullptr
                && od->mCurViewportID == 0) {
                // mCurViewportID is the int just before the inline-proxy byte,
                // which precedes the proxy file name, the two subdir vectors, the
                // inline type byte and the next-name string (all empty here).
                size_t vpAt = camAt - 22;
                static const unsigned char kSeven[] = { 0, 0, 0, 7 };
                if (memcmp(&expect[vpAt], kSeven, 4) == 0) {
                    memset(&expect[vpAt], 0, 4);
                    expect.erase(expect.begin() + camAt + 4, expect.begin() + camAt + 12);
                    expect[camAt + 3] = 0;
                    edit = "viewport 7 -> 0, camera \"[ui.cam]\" -> \"\"";
                }
            }
        }
        if (strcmp(edit, "none") != 0)
            printf("  %s: expected stream = shipped with the load rule applied (%s)\n", tag, edit);
        MemStream ms(false);
        DirLoader::SaveObjects(ms, dir.Ptr());
        const unsigned char *d = (const unsigned char *)ms.Buffer();
        size_t n = ms.Size();
        if (const char *dumpDir = getenv("W16TW_DUMP")) {
            FILE *df = fopen(MakeString("%s/%s.saved", dumpDir, tag), "wb");
            if (df) {
                fwrite(d, 1, n, df);
                fclose(df);
            }
        }
        MiloHeader ours;
        bool parsed = ParseHeader(d, n, ours);
        size_t firstDiff = 0;
        size_t common = std::min(n, expect.size());
        while (firstDiff < common && d[firstDiff] == expect[firstDiff])
            firstDiff++;
        bool same = n == expect.size() && firstDiff == n;
        int listBad = 0;
        if (parsed)
            for (size_t i = 0; i < ours.objs.size() && i < sh.objs.size(); i++)
                if (ours.objs[i] != sh.objs[i])
                    listBad++;
        int marksOurs = CountMarks(d, n), marksShipped = CountMarks(shipped.data(), shipped.size());
        printf("  %s: ours %zu bytes, shipped %zu, first difference at %zu; header rev %u "
               "'%s' '%s' hash %u/%u str %u/%u; marks %d/%d\n",
               tag, n, shipped.size(), firstDiff, ours.rev, ours.cls.c_str(), ours.name.c_str(),
               ours.hash, sh.hash, ours.str, sh.str, marksOurs, marksShipped);
        bool headerOk = parsed && ours.rev == 0x1C && ours.rev == sh.rev && ours.cls == sh.cls
            && ours.objs.size() == sh.objs.size() && listBad == 0
            && marksOurs == (int)sh.objs.size() + 1;
        Gate(MakeString("tw-save-header-%s", tag), headerOk,
             "rev %u (shipped %u), class %s (shipped %s), %zu objects (shipped %zu), %d "
             "listed out of shipped order, %d end marks (want %zu: the dir and each object)",
             ours.rev, sh.rev, ours.cls.c_str(), sh.cls.c_str(), ours.objs.size(),
             sh.objs.size(), listBad, marksOurs, sh.objs.size() + 1);
        Gate(MakeString("tw-save-bytes-%s", tag), same,
             "re-saved stream %s the %s stream (%zu vs %zu bytes, first difference at %zu)",
             same ? "equals" : "DIFFERS FROM",
             strcmp(edit, "none") ? "expected (shipped + load rule)" : "shipped", n,
             expect.size(), firstDiff);
    }
}


// ======================================================= JoypadPollCommon ==
// Driven through the strong back end above on pad 0. Each expectation comes
// from the shipped config (built below as the console builds it) or from the
// retail body 0x82526A00 as ported: messages are exported button by button in
// ascending bit order, ups before downs within a bit, connect after the buttons
// and disconnect after the releases; a drum's DUp/DDown downs are dropped while
// its whole cymbal mask is held; the EEPROM state machine sends a 0xAD 0xDE
// header packet with a 0x55 0xAA fill, then data chunks [offset, 0, total, len,
// bytes...].
struct JoyEvent {
    std::string type;
    int a, b, pad;
    bool operator==(const JoyEvent &o) const {
        return type == o.type && a == o.a && b == o.b && pad == o.pad;
    }
};
std::vector<JoyEvent> gJoy;

struct JoySink : public Hmx::Object {
    DataNode Handle(DataArray *m, bool) override {
        if (m && m->Size() >= 5 && m->Type(1) == kDataSymbol) {
            std::string t = m->Sym(1).Str();
            if ((t == "button_down" || t == "button_up") && m->Size() >= 6)
                gJoy.push_back({ t, m->Int(3), m->Int(4), m->Int(5) });
            else if (t == "joypad_connect")
                gJoy.push_back({ t, m->Int(3), m->Int(4), -1 });
        }
        return DataNode(kDataUnhandled, 0);
    }
};

std::string EventsText(const std::vector<JoyEvent> &v) {
    std::string s;
    for (const JoyEvent &e : v) {
        char b[96];
        if (e.type == "joypad_connect")
            snprintf(b, sizeof(b), "connect(%d,type %d)", e.a, e.b);
        else
            snprintf(b, sizeof(b), "%s(btn %d,act %d,pad %d)",
                     e.type == "button_down" ? "down" : "up", e.a, e.b, e.pad);
        s += (s.empty() ? "" : " ") + std::string(b);
    }
    return s.empty() ? "(none)" : s;
}

// The controllers entry the shipped config gives this pad type: the entries whose
// (detect ...) carries (type <padType>).
DataArray *gJoyCfg = nullptr;

DataArray *ControllerEntryFor(int padType, Symbol &name) {
    DataArray *ctl = gJoyCfg->FindArray("controllers", false);
    if (!ctl)
        return nullptr;
    for (int i = 1; i < ctl->Size(); i++) {
        if (ctl->Type(i) != kDataArray)
            continue;
        DataArray *e = ctl->Array(i);
        DataArray *det = e->FindArray("detect", false);
        DataArray *ty = det ? det->FindArray("type", false) : nullptr;
        if (ty && ty->Size() > 1 && ty->Int(1) == padType) {
            name = e->Sym(0);
            return e;
        }
    }
    return nullptr;
}

int CfgInt(DataArray *e, const char *key, int def) {
    DataArray *a = e ? e->FindArray(key, false) : nullptr;
    return a && a->Size() > 1 ? a->Int(1) : def;
}

int ExpectAction(Symbol ctype, int btn) {
    DataArray *bm = gJoyCfg->FindArray("button_meanings");
    DataArray *a = bm->FindArray(ctype, false);
    DataArray *b = a ? a->FindArray(btn, false) : nullptr;
    return b ? b->Int(1) : 0;
}

void Poll(int type, unsigned int buttons, signed char lx = 0, signed char lt = 0) {
    w16tw::gFrame[0].type = type;
    w16tw::gFrame[0].buttons = buttons;
    w16tw::gFrame[0].lx = lx;
    w16tw::gFrame[0].lt = lt;
    gJoy.clear();
    JoypadPollCommon();
}

std::vector<JoyEvent> Edges(const char *kind, unsigned int bits, Symbol ctype) {
    std::vector<JoyEvent> v;
    for (int b = 0; b < kPad_NumButtons; b++)
        if (bits & (1u << b))
            v.push_back({ kind, b, ExpectAction(ctype, b), 0 });
    return v;
}

void JoypadChecks() {
    printf("\n=== W16-TW: JoypadPollCommon over the shipped joypad config ===\n");
    // The joypad section as the console builds it. PreInitSystem (retail
    // 0x82510BB8) defines HX_XBOX, HX_WIN, HX_NG and _SHIP before reading
    // config/band_preinit_keep.dta; that file's (joypad #joypad.dta) is the
    // game's config/joypad.dta, and its trailing #merge of
    // system/run/config/default.dta DataMergeTags the system joypad.dta into it
    // (tags the game lacks are added; the game's own tags, e.g. its empty
    // `ignore`, win). rb3-render reads the config with none of those macros
    // defined, so its section lacks the HX_XBOX `controllers` block; this
    // fixture rebuilds the section the same way with them defined.
    const char *kMacros[] = { "HX_XBOX", "HX_WIN", "HX_NG", "_SHIP" };
    DataArray *savedMacro[4];
    for (int i = 0; i < 4; i++) {
        savedMacro[i] = DataGetMacro(kMacros[i]);
        if (savedMacro[i])
            savedMacro[i]->AddRef();
        DataArray *one = new DataArray(1);
        DataSetMacro(kMacros[i], one);
        one->Release();
    }
    DataArray *game = DataReadFile("config/joypad.dta", true);
    DataArray *sys = DataReadFile("../../system/run/config/joypad.dta", true);
    for (int i = 0; i < 4; i++) {
        DataSetMacro(kMacros[i], savedMacro[i]);
        if (savedMacro[i])
            savedMacro[i]->Release();
    }
    DataArray *cfg = nullptr;
    if (game && sys) {
        cfg = new DataArray(game->Size() + 1);
        cfg->Node(0) = Symbol("joypad");
        for (int i = 0; i < game->Size(); i++)
            cfg->Node(i + 1) = game->Node(i);
        DataArray *sysSec = new DataArray(sys->Size() + 1);
        sysSec->Node(0) = Symbol("joypad");
        for (int i = 0; i < sys->Size(); i++)
            sysSec->Node(i + 1) = sys->Node(i);
        DataMergeTags(cfg, sysSec);
        sysSec->Release();
    }
    if (game)
        game->Release();
    if (sys)
        sys->Release();
    gJoyCfg = cfg;
    Symbol gName, dName;
    const int G = kJoypadXboxHxGuitarRb2, D = kJoypadXboxDrums;
    DataArray *gEnt = cfg ? ControllerEntryFor(G, gName) : nullptr;
    DataArray *dEnt = cfg ? ControllerEntryFor(D, dName) : nullptr;
    DataArray *ign = cfg ? cfg->FindArray("ignore", false) : nullptr;
    bool fixtureOk = gEnt && dEnt && ign && ign->Size() == 1
        && CfgInt(dEnt, "is_drum", 0) == 1 && cfg->FindArray("button_meanings", false);
    Gate("tw-joy-fixture", fixtureOk,
         "game joypad.dta merged with system joypad.dta under HX_XBOX: %s; guitar type %d -> "
         "'%s', drum type %d -> '%s' (is_drum %d, cymbal_mask %d); pad ignore list %s (the "
         "game's empty one wins the merge)",
         cfg ? "built" : "MISSING", G, gName.Str(), D, dName.Str(), CfgInt(dEnt, "is_drum", -1),
         CfgInt(dEnt, "cymbal_mask", -1), !ign ? "MISSING" : ign->Size() == 1 ? "empty" : "NOT empty");
    if (!fixtureOk) {
        if (cfg)
            cfg->Release();
        gJoyCfg = nullptr;
        return;
    }

    JoypadData saved = *JoypadGetPadData(0);
    JoypadInitCommon(cfg);
    JoySink *sink = new JoySink();
    JoypadSubscribe(sink);
    w16tw::gScripted = true;
    JoypadData *pd = JoypadGetPadData(0);

    // ---- guitar ----
    unsigned int gIgnore = 0;
    if (DataArray *ia = gEnt->FindArray("ignore", false))
        for (int i = 1; i < ia->Size(); i++)
            gIgnore |= 1u << ia->Int(i);
    bool analog = CfgInt(gEnt, "has_analog_sticks", 0) != 0;
    bool translate = CfgInt(gEnt, "translate_sticks", 0) != 0;
    int bad = 0;
    std::string detail;
    auto expect = [&](const char *step, const std::vector<JoyEvent> &want) {
        if (!(gJoy == want)) {
            bad++;
            printf("  WRONG  %s: got %s\n         want %s\n", step, EventsText(gJoy).c_str(),
                   EventsText(want).c_str());
        }
    };
    Poll(G, 0);
    expect("guitar connect", { { "joypad_connect", 1, G, -1 } });
    bool typed = pd->mConnected && pd->mType == G && pd->mControllerType == gName
        && pd->mIgnoreButtonMask == gIgnore;
    if (!typed) {
        bad++;
        printf("  WRONG  after connect: connected %d type %d controller '%s' (want '%s'), "
               "ignore mask 0x%x (want 0x%x)\n",
               pd->mConnected, (int)pd->mType, pd->mControllerType.Str(), gName.Str(),
               pd->mIgnoreButtonMask, gIgnore);
    }
    unsigned int press = (1u << kPad_Xbox_A) | (1u << kPad_Xbox_B);
    unsigned int ignBit = 0;
    for (int b = 0; b < kPad_NumButtons && !ignBit; b++)
        if ((gIgnore & (1u << b)) && !(press & (1u << b)))
            ignBit = 1u << b;
    unsigned int stickBits = analog && translate ? 1u << kPad_LStickRight : 0;
    unsigned int held = (press | ignBit | stickBits) & ~gIgnore;
    Poll(G, press | ignBit, 127, 64);
    expect("guitar press A+B (+ an ignored bit, LX full right, LT half)",
           Edges("button_down", held, gName));
    float wantStick = analog ? 1.0f : 0.0f;
    if (pd->mButtons != held || pd->mSticks[0][0] != wantStick
        || pd->mTriggers[0] != 64 / 127.0f) {
        bad++;
        printf("  WRONG  pad state: buttons 0x%x (want 0x%x), LX %g (want %g), LT %g (want %g)\n",
               pd->mButtons, held, pd->mSticks[0][0], wantStick, pd->mTriggers[0],
               64 / 127.0f);
    }
    Poll(G, press | ignBit, 127, 64);
    expect("guitar hold", {});
    Poll(G, 1u << kPad_Xbox_B);
    expect("guitar release A (and the stick)", Edges("button_up", held & ~(1u << kPad_Xbox_B), gName));
    Poll(0, 0);
    {
        std::vector<JoyEvent> want = Edges("button_up", 1u << kPad_Xbox_B, gName);
        want.push_back({ "joypad_connect", 0, 0, -1 });
        expect("guitar unplugged", want);
    }
    if (pd->mConnected || pd->mType != kJoypadNone) {
        bad++;
        printf("  WRONG  after unplug: connected %d type %d\n", pd->mConnected, (int)pd->mType);
    }
    Poll(0, 0);
    expect("nothing plugged", {});
    Gate("tw-joy-guitar", bad == 0,
         "%s: connect, press with an ignored bit and a translated stick, hold, release, "
         "unplug, idle: %d wrong (has_analog_sticks %d, translate_sticks %d, ignore 0x%x)",
         gName.Str(), bad, analog, translate, gIgnore);

    // ---- drums: the cymbal-mask DUp/DDown rule ----
    bad = 0;
    Poll(D, 0);
    expect("drums connect", { { "joypad_connect", 1, D, -1 } });
    unsigned int dIgnore = 0;
    if (DataArray *ia = dEnt->FindArray("ignore", false))
        for (int i = 1; i < ia->Size(); i++)
            dIgnore |= 1u << ia->Int(i);
    unsigned int cym = CfgInt(dEnt, "cymbal_mask", 0);
    unsigned int dp = (cym | (1u << kPad_DUp) | (1u << kPad_DDown) | (1u << kPad_Start))
        & ~dIgnore;
    Poll(D, dp);
    expect("drums cymbal + DUp + DDown + Start (DUp/DDown downs dropped)",
           Edges("button_down", dp & ~((1u << kPad_DUp) | (1u << kPad_DDown)), dName));
    Poll(D, 0);
    expect("drums release all (every up reported, DUp/DDown included)",
           Edges("button_up", dp, dName));
    Poll(D, (1u << kPad_DUp) & ~dIgnore);
    expect("drums DUp alone (no cymbal held: reported)",
           Edges("button_down", (1u << kPad_DUp) & ~dIgnore, dName));
    Poll(D, 0);
    gJoy.clear();
    Gate("tw-joy-drums", bad == 0,
         "%s: cymbal_mask 0x%x held drops DUp/DDown downs, releases all report, DUp alone "
         "reports: %d wrong",
         dName.Str(), cym, bad);

    // ---- EEPROM ("breed data") write state machine ----
    bad = 0;
    w16tw::gBreed.clear();
    for (int i = 0; i < 0x10; i++)
        pd->mEepromData[i] = (unsigned char)(0x10 + i);
    memset(pd->mEepromPacket, 0, sizeof(pd->mEepromPacket));
    pd->mEepromTotalBytes = 10;
    pd->mEepromBytesLeft = 10;
    pd->mEepromChunkSize = 4;
    pd->mEepromWriteState = 0;
    pd->mEepromWriteDone = false;
    std::vector<std::vector<unsigned char>> want;
    want.push_back({ 0xAD, 0xDE, 0, 0, 0x55, 0xAA, 0x55, 0xAA });
    for (int off = 0; off < 10; off += 4) {
        int len = std::min(4, 10 - off);
        std::vector<unsigned char> pk = { (unsigned char)off, 0, 10, (unsigned char)len };
        for (int j = 0; j < len; j++)
            pk.push_back((unsigned char)(0x10 + off + j));
        want.push_back(pk);
    }
    std::vector<int> states;
    for (int frame = 0; frame < 12; frame++) {
        Poll(D, 0);
        states.push_back(pd->mEepromWriteState);
        if (pd->mEepromWriteState == 1 || pd->mEepromWriteState == 3)
            pd->mEepromWriteDone = true; // the console's write-response callback
    }
    if (w16tw::gBreed.size() != want.size()) {
        bad++;
        printf("  WRONG  %zu packets sent, want %zu\n", w16tw::gBreed.size(), want.size());
    }
    for (size_t k = 0; k < want.size() && k < w16tw::gBreed.size(); k++) {
        const std::vector<unsigned char> &got = w16tw::gBreed[k].second;
        bool ok = w16tw::gBreed[k].first == 0
            && std::equal(want[k].begin(), want[k].end(), got.begin());
        if (k > 0) // a data packet's tail is cleared (memset of the 0x10 data bytes)
            for (size_t j = want[k].size(); j < 0x14; j++)
                ok = ok && got[j] == 0;
        if (!ok) {
            bad++;
            std::string g, w;
            for (size_t j = 0; j < want[k].size(); j++) {
                char b[4];
                snprintf(b, sizeof(b), "%02x", got[j]);
                g += b;
                snprintf(b, sizeof(b), "%02x", want[k][j]);
                w += b;
            }
            printf("  WRONG  packet %zu: %s, want %s\n", k, g.c_str(), w.c_str());
        }
    }
    if (pd->mEepromBytesLeft != 0) {
        bad++;
        printf("  WRONG  %d bytes left after the run\n", pd->mEepromBytesLeft);
    }
    // timeout: a write never acknowledged moves state 1 -> 4 when the counter runs out
    pd->mEepromBytesLeft = 2;
    pd->mEepromWriteState = 1;
    pd->mEepromWriteDone = false;
    pd->mEepromTimeout = 3;
    Poll(D, 0);
    Poll(D, 0);
    int s2 = pd->mEepromWriteState;
    Poll(D, 0);
    int s3 = pd->mEepromWriteState;
    if (s2 != 1 || s3 != 4) {
        bad++;
        printf("  WRONG  timeout: state %d after 2 polls (want 1), %d after 3 (want 4)\n", s2, s3);
    }
    std::string st;
    for (int x : states)
        st += std::to_string(x);
    Gate("tw-joy-eeprom", bad == 0,
         "10 bytes in 4-byte chunks: %zu packets (header + 3 data), states %s, timeout 1 -> 4 "
         "on the third unanswered poll: %d wrong",
         w16tw::gBreed.size(), st.c_str(), bad);
    pd->mEepromBytesLeft = 0;
    pd->mEepromWriteState = 0;

    // ---- restore ----
    Poll(0, 0);
    w16tw::gScripted = false;
    JoypadUnsubscribe(sink);
    delete sink;
    JoypadTerminateCommon();
    *JoypadGetPadData(0) = saved;
    gJoyCfg = nullptr;
    cfg->Release();
}
} // namespace

int RunW16TWPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== W16-TW phase: unentered in-scope rows on shipped data (continued) ===\n");
    MakePathChecks();
    LocaleChecks();
    SaveObjectsChecks();
    JoypadChecks();
    return gRan;
}
