// NOTE FOR rb3-xenon:
// This script is kept for REFERENCE ONLY. It is the DC3 leaked-map workflow.
// rb3-xenon has NO leaked .map file (ham_xbox_r.map does not exist for RB3).
//
// For rb3-xenon function identification, use tools/fingerprint_match.py instead:
//   python3 tools/fingerprint_match.py autoid    # string/callee cross-ref matching
//   python3 tools/fingerprint_match.py report    # print identification table
//
// This file would be useful if a RB3 .map file were ever recovered.
// ============================================================================

// Import symbols from a Microsoft linker .map file into Ghidra.
//
// Three-pass import:
//   Pass 1: Create function objects at code addresses where auto-analysis missed them
//   Pass 2: Rename auto-generated FUN_ names to the real mangled symbol names
//   Pass 3: Apply full demangled signatures (calling convention, return type, all params)
//
// Works in both GUI (Script Manager) and headless mode:
//   GUI:     Run from Script Manager, pick the .map file when prompted
//   Headless: analyzeHeadless ... -postScript ImportMapFile.java /path/to/ham_xbox_r.map
//
//@author freeqaz (MiloHax)
//@category Import

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.app.util.demangler.DemanglerOptions;
import ghidra.app.util.demangler.microsoft.MicrosoftDemangler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.io.*;
import java.util.*;
import java.util.regex.*;

public class ImportMapFile extends GhidraScript {

    // Parsed symbol entry from the map file
    private static class MapSymbol {
        String mangled;
        long address;
        String type; // "f" = function, "i" = inlined, etc.

        MapSymbol(String mangled, long address, String type) {
            this.mangled = mangled;
            this.address = address;
            this.type = type;
        }
    }

    @Override
    public void run() throws Exception {
        // Get map file path - from script args (headless) or file chooser (GUI)
        File mapFile;
        String[] args = getScriptArgs();
        if (args.length > 0) {
            mapFile = new File(args[0]);
        } else {
            mapFile = askFile("Select MSVC Linker Map File", "Import");
        }

        if (mapFile == null || !mapFile.exists()) {
            printerr("Map file not found: " + (mapFile != null ? mapFile.getPath() : "null") + "\n");
            return;
        }

        printf("=== ImportMapFile: %s ===\n\n", mapFile.getName());

        // Parse map file
        List<MapSymbol> symbols = parseMapFile(mapFile);
        printf("Parsed %d symbols from map file\n\n", symbols.size());

        if (symbols.isEmpty()) return;

        // Pass 1: Create function objects
        int[] pass1 = createFunctions(symbols);
        printf("\nPass 1 (create functions): %d created, %d already existed, %d skipped\n\n",
            pass1[0], pass1[1], pass1[2]);

        // Pass 2: Rename FUN_ auto-names to real symbol names
        int[] pass2 = renameSymbols(symbols);
        printf("\nPass 2 (rename symbols): %d renamed, %d labels added, %d skipped\n\n",
            pass2[0], pass2[1], pass2[2]);

        // Pass 3: Apply demangled signatures
        int[] pass3 = applySignatures(symbols);
        printf("\nPass 3 (demangle signatures): %d applied, %d partial, %d failed, %d skipped\n\n",
            pass3[0], pass3[1], pass3[2], pass3[3]);

        printf("=== Import complete ===\n");
    }

    /**
     * Parse the "Publics by Value" section of an MSVC linker .map file.
     *
     * Format:
     *   0005:00000000  ?asciiDigitToHex@@YAED@Z  82330000 f  keygen_xbox.obj
     *   section:offset  mangled_name              address  type  object_file
     */
    private List<MapSymbol> parseMapFile(File mapFile) throws IOException {
        // Match: section:offset  symbol  address  [type]  [object]
        Pattern pattern = Pattern.compile(
            "\\s*([0-9a-fA-F]+):[0-9a-fA-F]+\\s+(\\S+)\\s+([0-9a-fA-F]{8})\\s+(\\S?).*");

        List<MapSymbol> symbols = new ArrayList<>();
        BufferedReader reader = new BufferedReader(new FileReader(mapFile));
        String line;
        boolean inPublics = false;

        while ((line = reader.readLine()) != null) {
            if (line.contains("Publics by Value")) {
                inPublics = true;
                continue;
            }
            if (!inPublics) continue;

            // Stop at next section header (empty line after symbols block)
            if (inPublics && line.trim().isEmpty() && !symbols.isEmpty()) {
                // Map files have blank lines between sections; keep going
                // until we hit a non-matching line after seeing symbols
                continue;
            }

            Matcher matcher = pattern.matcher(line);
            if (matcher.matches()) {
                String section = matcher.group(1);
                String symbolName = matcher.group(2);
                String addrStr = matcher.group(3);
                String type = matcher.group(4);

                try {
                    long addr = Long.parseLong(addrStr, 16);
                    symbols.add(new MapSymbol(symbolName, addr, type));
                } catch (NumberFormatException e) {
                    // Skip malformed addresses
                }
            }
        }
        reader.close();
        return symbols;
    }

    /**
     * Pass 1: Create Function objects at addresses where Ghidra auto-analysis
     * didn't detect a function. Many map file entries point to valid code that
     * Ghidra skipped.
     */
    private int[] createFunctions(List<MapSymbol> symbols) throws Exception {
        printf("Pass 1: Creating function objects...\n");

        FunctionManager fm = currentProgram.getFunctionManager();
        Memory memory = currentProgram.getMemory();
        int created = 0, existed = 0, skipped = 0;

        for (int i = 0; i < symbols.size(); i++) {
            if (monitor.isCancelled()) break;

            MapSymbol sym = symbols.get(i);
            // Only create functions for "f" type (skip data, inlined, etc.)
            if (!"f".equals(sym.type)) {
                skipped++;
                continue;
            }

            Address addr = toAddr(sym.address);
            if (addr == null || !memory.contains(addr)) {
                skipped++;
                continue;
            }

            if (fm.getFunctionAt(addr) != null) {
                existed++;
                continue;
            }

            CreateFunctionCmd cmd = new CreateFunctionCmd(addr);
            if (cmd.applyTo(currentProgram)) {
                created++;
            } else {
                skipped++;
            }

            if ((i + 1) % 10000 == 0) {
                printf("  Progress: %d/%d symbols processed\n", i + 1, symbols.size());
            }
        }
        return new int[]{created, existed, skipped};
    }

    /**
     * Pass 2: Rename auto-generated symbol names (FUN_XXXXXXXX) with real
     * mangled names from the map file. If the function already has a non-auto
     * name, adds the map name as a secondary label instead.
     */
    private int[] renameSymbols(List<MapSymbol> symbols) throws Exception {
        printf("Pass 2: Renaming symbols...\n");

        FunctionManager fm = currentProgram.getFunctionManager();
        Memory memory = currentProgram.getMemory();
        int renamed = 0, labeled = 0, skipped = 0;

        for (int i = 0; i < symbols.size(); i++) {
            if (monitor.isCancelled()) break;

            MapSymbol sym = symbols.get(i);
            Address addr = toAddr(sym.address);
            if (addr == null || !memory.contains(addr)) {
                skipped++;
                continue;
            }

            Function func = fm.getFunctionAt(addr);
            if (func != null) {
                String name = func.getName();
                boolean isAutoName = name.startsWith("FUN_")
                    || name.startsWith("Function_")
                    || name.startsWith("thunk_FUN_");

                if (isAutoName) {
                    // Clean up any duplicate labels at this address first
                    Symbol[] existingSyms = currentProgram.getSymbolTable().getSymbols(addr);
                    for (Symbol existing : existingSyms) {
                        if (existing.getName().equals(sym.mangled)
                                && existing != func.getSymbol()) {
                            existing.delete();
                        }
                    }

                    try {
                        func.setName(sym.mangled, SourceType.IMPORTED);
                        renamed++;
                    } catch (Exception e) {
                        skipped++;
                    }
                } else {
                    // Function has a real name already; add map name as label
                    try {
                        currentProgram.getSymbolTable().createLabel(
                            addr, sym.mangled, SourceType.IMPORTED);
                        labeled++;
                    } catch (Exception e) {
                        skipped++;
                    }
                }
            } else {
                // No function at this address (data symbol, etc.) — create label
                try {
                    currentProgram.getSymbolTable().createLabel(
                        addr, sym.mangled, SourceType.IMPORTED);
                    labeled++;
                } catch (Exception e) {
                    skipped++;
                }
            }

            if ((i + 1) % 10000 == 0) {
                printf("  Progress: %d/%d symbols processed\n", i + 1, symbols.size());
            }
        }
        return new int[]{renamed, labeled, skipped};
    }

    /**
     * Pass 3: Apply full demangled signatures from MSVC mangled names.
     * Uses Ghidra's MicrosoftDemangler to parse mangled names into complete
     * function signatures (calling convention, return type, all parameter types).
     *
     * This is the most impactful pass — it transforms the decompiler output from
     * "void FUN_82345678(undefined4 param_1)" to
     * "void __thiscall CharBones::PoseMeshes(CharBones *this)"
     */
    private int[] applySignatures(List<MapSymbol> symbols) throws Exception {
        printf("Pass 3: Applying demangled signatures...\n");

        FunctionManager fm = currentProgram.getFunctionManager();
        Memory memory = currentProgram.getMemory();

        MicrosoftDemangler demangler = new MicrosoftDemangler();
        DemanglerOptions options = new DemanglerOptions();
        options.setApplySignature(true);
        options.setApplyCallingConvention(true);

        int applied = 0, partial = 0, failed = 0, skipped = 0;

        for (int i = 0; i < symbols.size(); i++) {
            if (monitor.isCancelled()) break;

            MapSymbol sym = symbols.get(i);

            // Only demangle MSVC-mangled names (start with ?)
            // Skip string literals (??_C@), RTTI (??_R), and non-mangled names
            if (!sym.mangled.startsWith("?")
                    || sym.mangled.startsWith("??_C@")
                    || sym.mangled.startsWith("??_R")) {
                skipped++;
                continue;
            }

            Address addr = toAddr(sym.address);
            if (addr == null || !memory.contains(addr)) {
                skipped++;
                continue;
            }

            Function func = fm.getFunctionAt(addr);
            if (func == null) {
                skipped++;
                continue;
            }

            try {
                var demangled = demangler.demangle(sym.mangled);
                if (demangled == null) {
                    failed++;
                    continue;
                }

                try {
                    demangled.applyTo(currentProgram, addr, options, monitor);
                    applied++;
                } catch (Exception e) {
                    // applyTo can fail on type conflicts — count as partial
                    partial++;
                }
            } catch (Exception e) {
                failed++;
            }

            if ((i + 1) % 10000 == 0) {
                printf("  Progress: %d/%d symbols processed (%d applied so far)\n",
                    i + 1, symbols.size(), applied);
            }
        }
        return new int[]{applied, partial, failed, skipped};
    }
}
