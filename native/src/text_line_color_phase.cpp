// rb3-xenon native -- A15: RndText::UpdateLineColor recolours the line it is
// given, as retail does.
//
// THE REFERENCE is band.exe (TU5, base 0x82000000), fn_82457BA0 (0x174),
// ?UpdateLineColor@RndText@@QAAXIABVColor@Hmx@@PA_N@Z. It copies the line
// (memcpy 0x78), has no early-out, walks the line's bytes from line.startIdx
// to min(endIdx, mFixedLength) decoding UTF-8, and for each character whose
// defining font exists writes the colour into quad charIdx*4 of that font's
// mesh, charIdx starting at line.startIdx and advancing once per character.
// It never writes the colour back into the line.
//
// WHY THAT INDEX IS RIGHT ON NATIVE TOO. ApplyLineText (shared, no arm) writes a
// line's quads starting at line.startIdx*4 in each font's mesh, and startIdx is
// the line's BYTE offset into mText. So a line's first quad is startIdx*4 no
// matter what earlier lines hold.
//
// WHAT THE OLD NATIVE ARM DID. It based the index on "characters of this font
// in mText[0, startIdx)" times 4. That equals startIdx*4 only while every
// earlier byte is a one-byte character of this font. A two-byte UTF-8
// character in an earlier line makes the base too small, so the recolour
// lands on an EARLIER line's quads.
//
// The case: a real font from the ark (CharDefined reads the font's character
// map, so a synthetic RndFont cannot answer it). Three lines are added the way
// LyricPlate does (SetFixedLength, SetText(""), AddLineUTF8): A holds a
// two-byte character (e.g. U+00E9, 3 characters in 5 bytes), B and C are ASCII.
// UpdateLineColor(C, red):
//   text-linecolor-setup           the font loaded and the byte ranges are as built
//   text-linecolor-target-red      every quad of C is red
//   text-linecolor-others-intact   no other vert changed colour (A, B)
// The old native arm based C at 5 characters, not 7 bytes: it recoloured B.
// Run in a forked child: a crash is a failed gate, not a dead runner.

#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Font.h"
#include "rndobj/Mesh.h"
#include "rndobj/Text.h"
#include "utl/FilePath.h"

#include <cstdarg>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

typedef void (*GateFn)(const char *, bool, const char *);

namespace {
    GateFn gGate;

    void Gate(const char *name, bool pass, const char *fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        gGate(name, pass, buf);
    }

    struct TextPeek : RndText {
        static unsigned int Start(RndText *t, int i) {
            return static_cast<TextPeek *>(t)->mLines[i].startIdx;
        }
        static unsigned int End(RndText *t, int i) {
            return static_cast<TextPeek *>(t)->mLines[i].endIdx;
        }
    };

    enum {
        kFont = 1 << 0,     // a font with a defined two-byte character loaded
        kLines = 1 << 1,    // three lines added, A longer in bytes than characters
        kRed = 1 << 2,      // every quad of C is red
        kIntact = 1 << 3,   // no other vert changed
        kRan = 1 << 4,      // the child reached the end
    };

    bool Same(const Hmx::Color &a, const Hmx::Color &b) {
        return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
    }

    // UTF-8 for a code point below U+0800.
    void Utf8(unsigned short c, char *out) {
        if (c < 0x80) {
            out[0] = (char)c;
            out[1] = 0;
        } else {
            out[0] = (char)(0xC0 | (c >> 6));
            out[1] = (char)(0x80 | (c & 0x3F));
            out[2] = 0;
        }
    }

    const char *kFonts[] = { "pentatonic", "default", "stainless", "pentatonic_bold" };

    int LineColorChild() {
        int bits = 0;
        // rb3-milo registers RndFontBase but not RndFont (milo_object_factories.cpp);
        // register retail's Font factory (Rnd::PreInit's RndFont::Init) in this
        // child only, so the font milos load as RndFont.
        RndFont::Init();
        RndFont3d::Init();
        static ObjDirPtr<ObjectDir> dirs[sizeof(kFonts) / sizeof(*kFonts)];
        RndFont *font = nullptr;
        unsigned short wide = 0;
        const char *fontName = "";
        for (int n = 0; n < (int)(sizeof(kFonts) / sizeof(*kFonts)) && !font; n++) {
            char path[128];
            snprintf(path, sizeof(path), "ui/resource/fonts/gen/%s.milo_xbox", kFonts[n]);
            dirs[n].LoadFile(FilePath(path), false, true, kLoadFront, false);
            ObjectDir *d = dirs[n].Ptr();
            if (!d)
                continue;
            for (ObjDirItr<RndFont> it(d, true); it && !font; ++it) {
                RndFont *f = &*it;
                if (!f->CharDefined('a') || !f->CharDefined('d'))
                    continue;
                // Prefer U+00E9; otherwise any defined Latin-1/Latin-A letter.
                if (f->CharDefined(0xE9))
                    wide = 0xE9;
                else
                    for (unsigned short c = 0xC0; c < 0x180 && !wide; c++)
                        if (f->CharDefined(c))
                            wide = c;
                if (wide) {
                    font = f;
                    fontName = kFonts[n];
                }
            }
        }
        if (!font) {
            fprintf(stderr, "  [text-linecolor] no loaded font defines a two-byte character\n");
            return bits;
        }
        bits |= kFont;

        char w[4];
        Utf8(wide, w);
        char line0[16];
        snprintf(line0, sizeof(line0), "%sa%s", w, w); // 3 characters, 5 bytes
        const char *line1 = "ab";
        const char *line2 = "cd";

        RndText *text = Hmx::Object::New<RndText>();
        text->SetFont(font);
        text->SetFixedLength(64);
        text->SetText("");
        text->ReserveLines(4);
        Hmx::Color white(1, 1, 1, 1);
        Hmx::Color red(1, 0, 0, 1);
        RndText::Style style(font, 1.0f, 0.0f, white, 0.0f);
        Transform xfm;
        xfm.Reset();
        bool needSync = false;
        int i0 = text->AddLineUTF8(String(line0), xfm, style, nullptr, &needSync, -1);
        int i1 = text->AddLineUTF8(String(line1), xfm, style, nullptr, &needSync, -1);
        int i2 = text->AddLineUTF8(String(line2), xfm, style, nullptr, &needSync, -1);
        std::vector<RndMesh *> meshes;
        text->GetMeshes(meshes);
        // SetText("") leaves an empty line 0 (Lyric.cpp's LyricPlate starts the same
        // way), so the three lines come back as 1, 2, 3.
        bool order = i0 >= 0 && i1 == i0 + 1 && i2 == i1 + 1 && i2 < text->NumLines();
        unsigned int s0 = order ? TextPeek::Start(text, i0) : 0, e0 = order ? TextPeek::End(text, i0) : 0;
        unsigned int s1 = order ? TextPeek::Start(text, i1) : 0, e1 = order ? TextPeek::End(text, i1) : 0;
        unsigned int s2 = order ? TextPeek::Start(text, i2) : 0, e2 = order ? TextPeek::End(text, i2) : 0;
        fprintf(stderr,
                "  [text-linecolor] font %s:%s U+%04X; lines %d %d %d of %d; bytes [%u,%u) "
                "[%u,%u) [%u,%u); %d mesh(es)\n",
                fontName, font->Name(), wide, i0, i1, i2, text->NumLines(), s0, e0, s1, e1, s2,
                e2, (int)meshes.size());
        if (!order || meshes.size() != 1 || !meshes[0])
            return bits;
        // The case only discriminates when line 0 has more bytes than characters.
        if (e0 - s0 != 5 || s1 != e0 || e1 - s1 != 2 || s2 != e1 || e2 - s2 != 2)
            return bits;
        bits |= kLines;

        RndMesh *mesh = meshes[0];
        std::vector<Hmx::Color> before;
        for (int i = 0; i < (int)mesh->Verts().size(); i++)
            before.push_back(mesh->Verts()[i].color);

        text->UpdateLineColor(i2, red, &needSync);

        int nverts = mesh->Verts().size();
        unsigned int lo = s2 * 4, hi = (s2 + 2) * 4; // C is two characters
        int notRed = 0, changed = 0, firstChanged = -1;
        for (int i = 0; i < nverts; i++) {
            const Hmx::Color &c = mesh->Verts()[i].color;
            if ((unsigned)i >= lo && (unsigned)i < hi) {
                if (!Same(c, red))
                    notRed++;
            } else if (i < (int)before.size() && !Same(c, before[i])) {
                if (firstChanged < 0)
                    firstChanged = i;
                changed++;
            }
        }
        fprintf(stderr,
                "  [text-linecolor] line %d verts [%u, %u): %d not red; %d other vert(s) "
                "changed, first at vert %d (quad %d)\n",
                i2, lo, hi, notRed, changed, firstChanged, firstChanged < 0 ? -1 : firstChanged / 4);
        if (hi <= (unsigned)nverts && notRed == 0)
            bits |= kRed;
        if (changed == 0)
            bits |= kIntact;
        bits |= kRan;
        return bits;
    }
}

int RunTextLineColorPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== A15: RndText::UpdateLineColor is retail's body (band.exe 0x82457BA0) ===\n");
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid == 0) {
        alarm(60);
        _exit(LineColorChild());
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool exited = WIFEXITED(status);
    int bits = exited ? WEXITSTATUS(status) : 0;
    const char *how = exited ? "exited" : "crashed";
    Gate("text-linecolor-setup", exited && (bits & kFont) && (bits & kLines),
         "a real font with a two-byte character; lines A (5 bytes, 3 chars), B, C "
         "(child %s, bits 0x%02x)",
         how, bits);
    Gate("text-linecolor-target-red", exited && (bits & kRan) && (bits & kRed),
         "UpdateLineColor(C) recolours quads from C's startIdx*4 (retail charIdx = startIdx)");
    Gate("text-linecolor-others-intact", exited && (bits & kRan) && (bits & kIntact),
         "UpdateLineColor(C) leaves lines A and B unchanged");
    return 0;
}
