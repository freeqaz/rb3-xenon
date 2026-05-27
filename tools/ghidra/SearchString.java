// Search for strings and show which functions reference them.
//
// GUI:      Script Manager > Search > SearchString (prompts for pattern)
// Headless: analyzeHeadless ... -postScript SearchString.java <pattern>
//
//@author freeqaz (MiloHax)
//@category Search

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;

import ghidra.program.model.symbol.ReferenceIterator;

import java.util.*;

public class SearchString extends GhidraScript {
    @Override
    public void run() throws Exception {
        // Get pattern from script args (headless) or GUI prompt
        String pattern;
        String[] args = getScriptArgs();
        if (args.length > 0) {
            pattern = args[0];
        } else {
            pattern = askString("Search Strings", "Search pattern (case-insensitive substring):");
        }

        if (pattern == null || pattern.isEmpty()) {
            printerr("No pattern provided.\n");
            return;
        }

        String patternLower = pattern.toLowerCase();
        printf("Searching for: \"%s\"\n\n", pattern);

        ReferenceManager refMgr = currentProgram.getReferenceManager();

        // Search defined strings and show xrefs for each match
        printf("=== Defined Strings ===\n");
        DataIterator dataIterator = currentProgram.getListing().getDefinedData(true);
        int count = 0;

        while (dataIterator.hasNext()) {
            if (monitor.isCancelled()) break;

            Data data = dataIterator.next();
            if (!data.hasStringValue()) continue;

            Object val = data.getValue();
            if (val == null) continue;

            String str = val.toString();
            if (!str.toLowerCase().contains(patternLower)) continue;

            count++;
            if (count > 200) {
                printf("\n... truncated at 200 matches. Narrow your search.\n");
                break;
            }

            // Truncate long strings for display
            String display = str.length() > 80 ? str.substring(0, 77) + "..." : str;
            printf("\n  %s: \"%s\"\n", data.getAddress(), display);

            // Find xrefs to this string
            ReferenceIterator refs = refMgr.getReferencesTo(data.getAddress());
            boolean hasRefs = false;
            Set<String> seen = new HashSet<>();
            int refCount = 0;
            while (refs.hasNext()) {
                Reference ref = refs.next();
                hasRefs = true;
                Function func = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                String funcName = func != null ? func.getName(true) : "?";
                String key = funcName + "@" + ref.getFromAddress();
                if (seen.add(key)) {
                    printf("    <- %s (%s)\n", funcName, ref.getFromAddress());
                    refCount++;
                    if (refCount >= 10) {
                        printf("    ... (more refs truncated)\n");
                        break;
                    }
                }
            }
            if (!hasRefs) {
                printf("    (no references)\n");
            }
        }

        printf("\n%d matching defined strings\n", Math.min(count, 200));

        // Raw memory search for strings Ghidra didn't identify
        printf("\n=== Raw Memory Matches ===\n");
        byte[] searchBytes = pattern.getBytes("US-ASCII");
        Memory mem = currentProgram.getMemory();
        Address found = mem.findBytes(mem.getMinAddress(), searchBytes, null, true, monitor);
        int rawCount = 0;

        while (found != null && rawCount < 50) {
            if (monitor.isCancelled()) break;

            // Skip if this address is already a defined string (shown above)
            Data existing = currentProgram.getListing().getDefinedDataAt(found);
            if (existing != null && existing.hasStringValue()) {
                found = mem.findBytes(found.add(1), searchBytes, null, true, monitor);
                continue;
            }

            // Read surrounding context to show the full string
            byte[] ctx = new byte[120];
            try {
                // Walk backwards to find string start (up to 40 bytes)
                Address start = found;
                for (int back = 1; back <= 40; back++) {
                    Address prev = found.subtract(back);
                    if (!mem.contains(prev)) break;
                    byte b = mem.getByte(prev);
                    if (b == 0 || b < 0x20 || b >= 0x7f) break;
                    start = prev;
                }

                int len = 0;
                mem.getBytes(start, ctx);
                for (int i = 0; i < ctx.length; i++) {
                    if (ctx[i] >= 0x20 && ctx[i] < 0x7f) {
                        len++;
                    } else {
                        break;
                    }
                }
                if (len >= 4) {
                    String str = new String(ctx, 0, len);
                    printf("  %s: \"%s\"\n", start, str);
                    rawCount++;
                }
            } catch (Exception e) {}

            found = mem.findBytes(found.add(1), searchBytes, null, true, monitor);
        }
        printf("\n%d raw memory matches (not identified as strings by Ghidra)\n", rawCount);
    }
}
