// List all defined strings and sample raw memory for strings
// Usage: analyzeHeadless ... -postScript StringSearch.java
//@category Search

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class StringSearch extends GhidraScript {
    @Override
    public void run() throws Exception {
        // List memory blocks
        printf("Memory Blocks:\n");
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            printf("  %s: %s - %s (0x%x bytes, r=%b w=%b x=%b)\n",
                block.getName(),
                block.getStart(),
                block.getEnd(),
                block.getSize(),
                block.isRead(),
                block.isWrite(),
                block.isExecute());
        }

        // Count and show defined strings
        printf("\nDefined strings (first 50):\n");
        DataIterator dataIterator = currentProgram.getListing().getDefinedData(true);
        int count = 0;
        int total = 0;
        while (dataIterator.hasNext()) {
            Data data = dataIterator.next();
            if (data.hasStringValue()) {
                total++;
                if (count < 50) {
                    Object val = data.getValue();
                    if (val != null) {
                        String str = val.toString();
                        if (str.length() > 60) str = str.substring(0, 57) + "...";
                        printf("  %s: %s\n", data.getAddress(), str);
                        count++;
                    }
                }
            }
        }
        printf("\nTotal defined strings: %d\n", total);

        // Sample raw memory in .rdata
        printf("\nRaw ASCII scan in .rdata (first 20):\n");
        MemoryBlock rdata = currentProgram.getMemory().getBlock(".rdata");
        if (rdata != null) {
            byte[] bytes = new byte[100];
            Address addr = rdata.getStart();
            int found = 0;
            while (addr.compareTo(rdata.getEnd()) < 0 && found < 20) {
                try {
                    currentProgram.getMemory().getBytes(addr, bytes);
                    int len = 0;
                    for (int i = 0; i < bytes.length && bytes[i] >= 0x20 && bytes[i] < 0x7f; i++) {
                        len++;
                    }
                    if (len >= 8 && bytes[len] == 0) {
                        String str = new String(bytes, 0, len);
                        printf("  %s: \"%s\"\n", addr, str);
                        found++;
                        addr = addr.add(len + 1);
                    } else {
                        addr = addr.add(1);
                    }
                } catch (Exception e) {
                    addr = addr.add(1);
                }
            }
        }
    }
}
