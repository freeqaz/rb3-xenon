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
#include "os/File.h"
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
#include <map>
#include <string>
#include <vector>

extern DataArray *gSystemConfig;

typedef void (*GateFn)(const char *, bool, const char *);

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

} // namespace

int RunW16TWPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== W16-TW phase: unentered in-scope rows on shipped data (continued) ===\n");
    MakePathChecks();
    LocaleChecks();
    SaveObjectsChecks();
    return gRan;
}
