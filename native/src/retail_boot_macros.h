#pragma once
//
// W16-UA: the DTA macros the Xbox retail game defines before it reads any config.
//
// Retail boots as App::App -> SystemPreInit(argc, argv, "config/band_preinit_keep.dta")
// (src/App.cpp). Two functions put entries in the DTA macro table before that file
// is read, in this order:
//
//   1. SystemPreInit (0x82510EC8) calls PlatformMgr::RegionInit (0x8251BD28).
//      XGetGameRegion() 0xFF, and every non-European code, select NA, and
//      SetRegion defines "REGION_" + upper("na") = REGION_NA.
//   2. PreInitSystem (0x82510BB8) defines HX_XBOX, HX_WIN, HX_NG and _SHIP, each
//      as (1), then one macro per `-define` option (a retail boot passes none),
//      then BeginDataRead() and DataReadFile(config).
//
// DataInit runs between the two and does not touch the macro table. The macros
// stay defined for the rest of the run, so every later DataReadFile sees them
// too. tools/retail_boot_config.py derives the same five names from the retail
// image (call order in SystemPreInit, Symbol(const char *) arguments to
// DataSetMacro in PreInitSystem); kNames below must equal its output, and the
// native health run checks that it does through DumpConfig's header line.
//
// The 18 native drivers hand-roll their bring-up instead of calling
// PreInitSystem (see boot_invariants.h), so before W16-UA none of them defined
// these macros. The shipped .dtb files test HX_XBOX in 30 files and _SHIP in 24,
// so each driver read a config no console reads: rb3-render's joypad section had
// no `controllers` block (JoypadInitCommon aborts on it) but did have the dev
// `breed_data_string_mappings`, `ui` had `cheat_init`, there was a `hostnames`
// section, and songs.dta had eight extra test songs. Every driver that reads
// DTA now calls Define() before its first read.
//
// Negative control: RB3_BOOT_MACRO_DROP=<name>[,<name>...] leaves those names out
// and nothing else. tools/native_health.sh --selftest uses it to show the gates
// go red when any one macro is missing. It is never set otherwise.
//
#include "obj/Data.h"
#include "obj/DataUtl.h"
#include "obj/Object.h"
#include "utl/Symbol.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

extern std::map<Symbol, DataArray *> gMacroTable; // obj/DataUtl.cpp
extern std::map<Symbol, DataNode> gDataVars;       // obj/DataNode.cpp
extern std::map<Symbol, DataFunc *> gDataFuncs;    // obj/DataFunc.cpp

namespace RetailBootMacros {

    // Retail order: RegionInit's region macro, then PreInitSystem's four.
    static const char *const kNames[] = { "REGION_NA", "HX_XBOX", "HX_WIN", "HX_NG", "_SHIP" };
    static const int kCount = sizeof(kNames) / sizeof(kNames[0]);

    inline bool Dropped(const char *name) {
        const char *env = getenv("RB3_BOOT_MACRO_DROP");
        if (!env || !*env)
            return false;
        std::string all(env);
        size_t p = 0;
        while (p <= all.size()) {
            size_t c = all.find(',', p);
            if (c == std::string::npos)
                c = all.size();
            if (all.compare(p, c - p, name) == 0 && strlen(name) == c - p)
                return true;
            p = c + 1;
        }
        return false;
    }

    // The defined (non-null) entries of the macro table, sorted by name and
    // space-separated.
    inline std::string TableNames() {
        std::vector<std::string> v;
        for (std::map<Symbol, DataArray *>::iterator it = gMacroTable.begin();
             it != gMacroTable.end(); ++it) {
            if (it->second)
                v.push_back(it->first.Str());
        }
        std::sort(v.begin(), v.end());
        std::string s;
        for (size_t i = 0; i < v.size(); i++) {
            if (i)
                s += ' ';
            s += v[i];
        }
        return s;
    }

    // Defines the five macros as retail's boot does, then checks the macro table
    // holds exactly them. Call it before the first DTA read, after Symbol::Init.
    // Prints one `[PASS]`/`[FAIL] boot-macros` line and returns the failure count.
    inline int Define() {
        // SystemPreInit -> PlatformMgr::RegionInit -> SetRegion(kRegionNA).
        if (!Dropped(kNames[0])) {
            DataArrayPtr region(1);
            DataSetMacro(kNames[0], region);
        }
        // PreInitSystem: one shared (1) for all four, as retail's DataArrayPtr ptr(1).
        DataArrayPtr ptr(1);
        for (int i = 1; i < kCount; i++) {
            if (!Dropped(kNames[i]))
                DataSetMacro(kNames[i], ptr);
        }

        std::vector<std::string> want(kNames, kNames + kCount);
        std::sort(want.begin(), want.end());
        std::string wantStr;
        for (size_t i = 0; i < want.size(); i++) {
            if (i)
                wantStr += ' ';
            wantStr += want[i];
        }
        std::string got = TableNames();
        bool ok = got == wantStr;
        printf("  [%s] boot-macros — macro table before the first config read: %s%s%s\n",
               ok ? "PASS" : "FAIL", got.empty() ? "(empty)" : got.c_str(),
               ok ? "" : "; retail defines ", ok ? "" : wantStr.c_str());
        return ok ? 0 : 1;
    }

    // ---------------------------------------------------------------------
    // Canonical dump, line for line the format tools/retail_boot_config.py
    // writes: the boot macro line, then one node per line, two spaces of indent
    // per array level.

    inline void Escape(FILE *f, const char *s) {
        for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
            if (*p == '"' || *p == '\\')
                fprintf(f, "\\%c", *p);
            else if (*p >= 0x20 && *p < 0x7F)
                fputc(*p, f);
            else
                fprintf(f, "\\x%02x", *p);
        }
    }

    struct Names {
        std::map<const DataNode *, const char *> vars;
        std::map<DataFunc *, const char *> funcs;
        Names() {
            for (std::map<Symbol, DataNode>::iterator it = gDataVars.begin();
                 it != gDataVars.end(); ++it)
                vars[&it->second] = it->first.Str();
            for (std::map<Symbol, DataFunc *>::iterator it = gDataFuncs.begin();
                 it != gDataFuncs.end(); ++it)
                funcs[it->second] = it->first.Str();
        }
    };

    inline void DumpNodes(FILE *f, const DataArray *a, int depth, const Names &nm) {
        for (int i = 0; i < a->Size(); i++) {
            const DataNode &n = a->Node(i);
            for (int d = 0; d < depth; d++)
                fputs("  ", f);
            switch (n.Type()) {
            case kDataInt:
                fprintf(f, "i %d\n", n.UncheckedInt());
                break;
            case kDataFloat:
                fprintf(f, "f %.9g\n", (double)n.UncheckedFloat());
                break;
            case kDataSymbol:
                fprintf(f, "s %s\n", n.UncheckedStr());
                break;
            case kDataString:
                fputs("t \"", f);
                Escape(f, n.LiteralStr());
                fputs("\"\n", f);
                break;
            case kDataVar: {
                std::map<const DataNode *, const char *>::const_iterator it =
                    nm.vars.find(n.UncheckedVar());
                fprintf(f, "v %s\n", it != nm.vars.end() ? it->second : "?");
                break;
            }
            case kDataFunc: {
                std::map<DataFunc *, const char *>::const_iterator it =
                    nm.funcs.find(n.UncheckedFunc());
                fprintf(f, "fn %s\n", it != nm.funcs.end() ? it->second : "?");
                break;
            }
            case kDataObject: {
                Hmx::Object *o = n.UncheckedObj();
                fprintf(f, "o %s\n", o ? o->Name() : "");
                break;
            }
            case kDataUnhandled:
                fputs("u\n", f);
                break;
            case kDataArray:
            case kDataCommand:
            case kDataProperty: {
                const char *oc = n.Type() == kDataArray ? "()"
                    : n.Type() == kDataCommand          ? "{}"
                                                        : "[]";
                fprintf(f, "%c\n", oc[0]);
                DumpNodes(f, n.UncheckedArray(), depth + 1, nm);
                for (int d = 0; d < depth; d++)
                    fputs("  ", f);
                fprintf(f, "%c\n", oc[1]);
                break;
            }
            default:
                fprintf(f, "? %d\n", (int)n.Type());
                break;
            }
        }
    }

    // The canonical text of one array's nodes (no header line).
    inline std::string Canonical(const DataArray *a) {
        char *buf = nullptr;
        size_t len = 0;
        FILE *f = open_memstream(&buf, &len);
        if (!f)
            return std::string();
        Names nm;
        DumpNodes(f, a, 0, nm);
        fclose(f);
        std::string s(buf, len);
        free(buf);
        return s;
    }

    // bootMacros is TableNames() taken just before the config was read.
    inline bool DumpConfig(const DataArray *cfg, const std::string &bootMacros,
                           const char *path) {
        FILE *f = fopen(path, "wb");
        if (!f)
            return false;
        fprintf(f, "# boot macros: %s\n", bootMacros.c_str());
        Names nm;
        DumpNodes(f, cfg, 0, nm);
        fclose(f);
        return true;
    }

} // namespace RetailBootMacros
