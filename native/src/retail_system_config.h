#pragma once
//
// W16-UD: the system config the Xbox retail game holds after SystemInit, read off
// the shipped disc image.
//
// Retail boots as App::App -> SystemPreInit(argc, argv, "config/band_preinit_keep.dta")
// and later SystemInit("config/band_keep.dta") (App::App 0x82270E68, the `bl
// SystemInit` at 0x82271014; tools/retail_boot_config.py `macros` decodes the
// name off the image). Between them the config is built like this:
//
//   PreInitSystem  BeginDataRead(); gSystemConfig = DataReadFile(preinit);
//                  DataVariable("syscfg") = gSystemConfig;
//   InitSystem     sys = DataReadFile("config/band_keep.dta");
//                  DataMergeTags(sys, gSystemConfig); DataReplaceTags(sys, gSystemConfig);
//                  gSystemConfig = sys; StripEditorData(); FinishDataRead();
//
// so SystemConfig("scoring"), ("beatmatcher"), the macro table (TRACK_SYMBOLS,
// kDifficulty*, ...) and everything else the game layer reads come from
// config/gen/*.dtb under retail's boot macros.
//
// Before W16-UD the ten drivers that take no disc (rb3-gem, -hit, -score,
// -score2, -score3, -score4, -vocal, -vocal2, -harmony, -crowd) built that
// config from DTA text compiled into the driver: hand-written (beatmatcher ...)
// and (scoring ...) blocks plus two headers cut from a host TEXT extraction
// (crowd_config_dta.h, scoring_config_dta.h), and a hand-rolled TRACK_SYMBOLS
// macro. W16-UC's file audit cannot see a compiled-in config. Boot() replaces
// all of it: it mounts the disc, reads the two files through the real engine
// (DataReadFile + the real InitSystem), and installs the result.
//
// The disc is found at $RB3_ASSETS, else ~/code/milohax/rb3/orig-assets/xbox-zip
// (the default tools/native_health.sh and tools/retail_boot_config.py use).
// There is NO fallback: without the disc a driver stops with rc 2 rather than
// run on a config no console reads.
//
// RB3_CONFIG_DUMP=<path> writes the installed config, then the macro table, in
// tools/retail_boot_config.py's canonical form; `retail_boot_config.py check
// <path> --full` compares it with the config it rebuilds from the .dtb files
// with no engine code. tools/native_health.sh does that for all ten.
//
#include "retail_boot_macros.h"

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataUtl.h"
#include "os/Archive.h"
#include "os/File.h"
#include "os/System.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <vector>

extern DataArray *gSystemConfig;             // os/System.cpp
extern void NativeSetDataDir(const char *dir); // platform/File_Native.cpp
extern void NativeArchiveInit();               // platform/System_Native.cpp

namespace RetailSystemConfig {

    static const char *const kPreinitConfig = "config/band_preinit_keep.dta";
    static const char *const kSystemConfig = "config/band_keep.dta";

    inline std::string DataDir() {
        const char *env = getenv("RB3_ASSETS");
        if (env && *env)
            return env;
        const char *home = getenv("HOME");
        return std::string(home ? home : "") + "/code/milohax/rb3/orig-assets/xbox-zip";
    }

    // The macro table as `m NAME` + value lines, sorted by name, in
    // retail_boot_config.py's dump_macros form.
    inline void DumpMacros(FILE *f) {
        std::vector<std::pair<std::string, DataArray *> > v;
        for (std::map<Symbol, DataArray *>::iterator it = gMacroTable.begin();
             it != gMacroTable.end(); ++it) {
            if (it->second)
                v.push_back(std::make_pair(std::string(it->first.Str()), it->second));
        }
        std::sort(v.begin(), v.end());
        RetailBootMacros::Names nm;
        fputs("# macro table after the read\n", f);
        for (size_t i = 0; i < v.size(); i++) {
            fprintf(f, "m %s\n", v[i].first.c_str());
            RetailBootMacros::DumpNodes(f, v[i].second, 1, nm);
        }
    }

    // Mounts the disc and installs retail's post-SystemInit config as
    // gSystemConfig. Call after DataInit() (the config's #autorun blocks define
    // script functions with `func`) and RetailBootMacros::Define(). Prints
    // `[PASS]/[FAIL] retail-config` (and `config-dump` when asked). Returns 0
    // when the config is installed, 1 when a read failed, 2 when there is no disc
    // to read it from; a driver exits with that code when it is nonzero.
    inline int Boot() {
        std::string dir = DataDir();
        std::string hdr = dir + "/gen/main_xbox.hdr";
        struct stat st;
        if (stat(hdr.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
            printf("  [FAIL] retail-config -- no disc image at %s (no gen/main_xbox.hdr); "
                   "set RB3_ASSETS. There is no compiled-in fallback.\n",
                   dir.c_str());
            return 2;
        }
        NativeSetDataDir(dir.c_str());
        SetUsingCD(true); // every relative read below goes to the archive
        NativeArchiveInit();
        if (!TheArchive) {
            printf("  [FAIL] retail-config -- the archive at %s did not mount\n", dir.c_str());
            return 1;
        }

        std::string boot = RetailBootMacros::TableNames();
        // PreInitSystem's read (os/System.cpp; retail 0x82510BB8).
        BeginDataRead();
        gSystemConfig = DataReadFile(kPreinitConfig, true);
        if (!gSystemConfig) {
            FinishDataRead();
            printf("  [FAIL] retail-config -- %s did not read\n", kPreinitConfig);
            return 1;
        }
        DataVariable("syscfg") = DataNode(gSystemConfig, kDataArray);
        int preSections = gSystemConfig->Size();
        // The real InitSystem: read, DataMergeTags, DataReplaceTags,
        // StripEditorData, FinishDataRead.
        InitSystem(kSystemConfig);
        bool ok = gSystemConfig && gSystemConfig->FindArray("scoring", false)
            && gSystemConfig->FindArray("beatmatcher", false);
        printf("  [%s] retail-config -- %s (%d sections) + %s off %s: %d top-level sections\n",
               ok ? "PASS" : "FAIL", kPreinitConfig, preSections, kSystemConfig, dir.c_str(),
               gSystemConfig ? gSystemConfig->Size() : -1);
        if (!ok)
            return 1;

        const char *dumpPath = getenv("RB3_CONFIG_DUMP");
        if (dumpPath && *dumpPath) {
            FILE *f = fopen(dumpPath, "wb");
            bool dumped = f != nullptr;
            if (f) {
                fprintf(f, "# boot macros: %s\n", boot.c_str());
                RetailBootMacros::Names nm;
                RetailBootMacros::DumpNodes(f, gSystemConfig, 0, nm);
                DumpMacros(f);
                dumped = fclose(f) == 0;
            }
            printf("  [%s] config-dump -- %s\n", dumped ? "PASS" : "FAIL", dumpPath);
            if (!dumped)
                return 1;
        }
        return 0;
    }

} // namespace RetailSystemConfig
