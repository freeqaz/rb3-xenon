import java.io.*;
import java.net.URL;
import java.util.*;

import com.google.gson.*;

import generic.lsh.vector.LSHVectorFactory;
import ghidra.app.script.GhidraScript;
import ghidra.features.bsim.query.*;
import ghidra.features.bsim.query.client.Configuration;
import ghidra.features.bsim.query.description.*;
import ghidra.features.bsim.query.protocol.*;
import ghidra.program.model.listing.*;

// Plain BSim H2-DB query baseline (no seed propagation).
// args: [0]=bsimURL (file:/...rb3wii.bsim) [1]=out.json [2]=maxPerFn [3]=simThresh [4]=signifThresh
public class BSimQueryToJson extends GhidraScript {
    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        String dburl = a[0];
        String out = a[1];
        int maxPer = a.length>2?Integer.parseInt(a[2]):5;
        double sim = a.length>3?Double.parseDouble(a[3]):0.0;
        double signif = a.length>4?Double.parseDouble(a[4]):0.0;

        URL url = BSimClientFactory.deriveBSimURL(dburl);
        FunctionDatabase db = BSimClientFactory.buildClient(url, false);
        if (!db.initialize()) {
            printerr("DB init failed: "+db.getLastError());
            return;
        }
        LSHVectorFactory vf = db.getLSHVectorFactory();

        // Generate signatures for all functions in currentProgram (RB3Xenon dest)
        GenSignatures gensig = new GenSignatures(false);
        gensig.setVectorFactory(vf);
        gensig.openProgram(currentProgram, null, null, null, null, null);
        FunctionManager fm = currentProgram.getFunctionManager();
        gensig.scanFunctions(fm.getFunctions(true), fm.getFunctionCount(), monitor);
        DescriptionManager manager = gensig.getDescriptionManager();
        println("Signed "+manager.numFunctions()+" functions for query");

        QueryNearest query = new QueryNearest();
        query.manage = manager;
        query.thresh = sim;
        query.signifthresh = signif;
        query.max = maxPer;
        ResponseNearest resp = query.execute(db);
        if (resp == null) { printerr("query failed: "+db.getLastError()); db.close(); return; }
        println("Query returned results for "+resp.result.size()+" functions");

        List<Map<String,Object>> rows = new ArrayList<>();
        for (SimilarityResult sr : resp.result) {
            FunctionDescription base = sr.getBase();
            String dva = "0x"+Long.toHexString(base.getAddress());
            List<Map<String,Object>> tops = new ArrayList<>();
            for (SimilarityNote note : sr) {
                FunctionDescription fd = note.getFunctionDescription();
                Map<String,Object> tm = new LinkedHashMap<>();
                tm.put("wii_name", fd.getFunctionName());
                tm.put("src_va", "0x"+Long.toHexString(fd.getAddress()));
                tm.put("sim", note.getSimilarity());
                tm.put("signif", note.getSignificance());
                tops.add(tm);
            }
            if (tops.isEmpty()) continue;
            Map<String,Object> row = new LinkedHashMap<>();
            row.put("dst_va", dva);
            Function f = fm.getFunctionAt(currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(base.getAddress()));
            row.put("dst_size", f!=null?(int)f.getBody().getNumAddresses():0);
            row.put("top", tops);
            rows.add(row);
        }
        db.close();
        try (Writer w = new FileWriter(out)) { new GsonBuilder().create().toJson(rows, w); }
        println("Wrote "+rows.size()+" query rows -> "+out);
    }
}
