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
// with no engine code. tools/native_health.sh does that for every Boot() driver.
//
// W16-UG: after the config, Boot() also registers the script functions retail's
// boot registers that the shipped config calls (RegisterScriptFuncs below), so
// a driver reaches `frac` and `stagekit_present` the way the console does.
// rb3-midi and rb3-song boot through here too (Boot(dir): they take the disc
// directory on the command line).
//
// RB3_DATAFUNCS_DUMP=<path> writes, at exit, the name of every C++ script
// function registered (gDataFuncs), one per line, sorted. tools/
// retail_script_funcs.py compares that with what retail registers.
//
#include "retail_boot_macros.h"

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataFunc.h"
#include "obj/DataUtl.h"
#include "os/Archive.h"
#include "os/File.h"
#include "os/StageKit.h"
#include "os/System.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
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

    // The name of every registered C++ script function, sorted, one per line.
    inline void DumpDataFuncs() {
        const char *path = getenv("RB3_DATAFUNCS_DUMP");
        if (!path || !*path)
            return;
        std::vector<std::string> v;
        for (std::map<Symbol, DataFunc *>::iterator it = gDataFuncs.begin();
             it != gDataFuncs.end(); ++it)
            v.push_back(it->first.Str());
        std::sort(v.begin(), v.end());
        FILE *f = fopen(path, "wb");
        if (!f)
            return;
        for (size_t i = 0; i < v.size(); i++)
            fprintf(f, "%s\n", v[i].c_str());
        fclose(f);
    }

    // The two places retail's boot registers script functions that the shipped
    // config calls, after the config is read:
    //   1. SystemInit's StageKitInit (retail 0x825113E0 -> 0x82522608): the
    //      thirteen stagekit_* functions (os/StageKit.cpp). config/
    //      midi_parsers.dta asks {stagekit_present}; the venue world's enter and
    //      exit and ui/game.dta's game_over call {stagekit_reset}; beatmatcher.dta's
    //      beat callbacks call {stagekit_left_right} while $stagekit is set.
    //   2. UIManager::Init's Handle(init) on the `ui` type def (retail 0x827E0690),
    //      which runs (ui (init ...)) from config/band_keep.dta. Its {func ...}
    //      commands define script functions, among them frac (ui/global.dta:237,
    //      called by config/player_net.dta:53). There is no UIManager natively,
    //      so only the {func ...} commands run, in order; the other commands
    //      (new panels, set globals, ...) need the UI and are skipped and counted.
    // RB3_SCRIPT_FUNC_DROP=stagekit|ui_init skips one (tools/native_health.sh
    // --selftest, to show the unhandled-call gate goes red without it).
    inline int RegisterScriptFuncs() {
        const char *drop = getenv("RB3_SCRIPT_FUNC_DROP");
        bool dropStageKit = drop && strstr(drop, "stagekit");
        bool dropUi = drop && strstr(drop, "ui_init");
        if (!dropStageKit)
            StageKitInit();
        DataArray *ui = gSystemConfig->FindArray("ui", false);
        DataArray *init = ui ? ui->FindArray("init", false) : nullptr;
        if (!init) {
            printf("  [FAIL] retail-script-funcs -- no (ui (init ...)) in the config\n");
            return 1;
        }
        static const Symbol func("func");
        int funcs = 0, others = 0;
        for (int i = 1; i < init->Size(); i++) {
            const DataNode &n = init->Node(i);
            DataArray *cmd = n.Type() == kDataCommand ? n.UncheckedArray() : nullptr;
            if (cmd && cmd->Size() > 1 && cmd->Node(0).Type() == kDataSymbol
                && cmd->Sym(0) == func) {
                if (!dropUi)
                    cmd->Execute();
                funcs++;
            } else {
                others++;
            }
        }
        bool ok = funcs > 0;
        printf("  [%s] retail-script-funcs -- StageKitInit: %s; (ui (init ...)): %d {func} "
               "command(s) %s, %d other command(s) skipped (need the UI)\n",
               ok ? "PASS" : "FAIL", dropStageKit ? "DROPPED" : "13 stagekit_* registered",
               funcs, dropUi ? "DROPPED" : "run", others);
        return ok ? 0 : 1;
    }

    // Mounts the disc and installs retail's post-SystemInit config as
    // gSystemConfig. Call after DataInit() (the config's #autorun blocks define
    // script functions with `func`) and RetailBootMacros::Define(). Prints
    // `[PASS]/[FAIL] retail-config` (and `config-dump` when asked). Returns 0
    // when the config is installed, 1 when a read failed, 2 when there is no disc
    // to read it from; a driver exits with that code when it is nonzero.
    inline int Boot(const char *dataDir = nullptr) {
        std::string dir = dataDir && *dataDir ? std::string(dataDir) : DataDir();
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
        const char *fdump = getenv("RB3_DATAFUNCS_DUMP");
        if (fdump && *fdump)
            atexit(DumpDataFuncs);
        return RegisterScriptFuncs();
    }

} // namespace RetailSystemConfig
