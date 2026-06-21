import java.io.*;
import java.util.*;

import com.google.gson.*;
import com.google.gson.reflect.TypeToken;

import ghidra.app.script.GhidraScript;
import ghidra.feature.vt.api.BSimProgramCorrelatorFactory;
import ghidra.feature.vt.api.db.VTSessionDB;
import ghidra.feature.vt.api.main.*;
import ghidra.feature.vt.api.util.*;
import ghidra.framework.model.*;
import ghidra.framework.options.ToolOptions;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;

/**
 * VT BSim seed-propagation driver (headless).
 *
 * currentProgram = DEST (RB3Xenon / default.xex), opened for update.
 * SOURCE = the Wii oracle program at project path given by args[1] (e.g. "/wii").
 *
 * args:
 *   [0] seeds.json   (list of {src_va,dst_va,wii_name,stem,...}; src_va=Wii VA, dst_va=Xenon VA)
 *   [1] source program project path (e.g. "/wii")
 *   [2] out.json
 *   [3] seedConfThreshold (double, e.g. 0.0  -- accept all our manual seeds as-is)
 *   [4] implicationThreshold (double, e.g. 0.0)
 */
public class VTSeedPropDriver extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        String seedsPath = a[0];
        String srcProgPath = a[1];
        String outPath = a[2];
        double seedConf = a.length > 3 ? Double.parseDouble(a[3]) : 0.0;
        double impThresh = a.length > 4 ? Double.parseDouble(a[4]) : 0.0;

        Program destProgram = currentProgram; // RB3Xenon
        println("DEST = " + destProgram.getName() + " (" + destProgram.getLanguageID() + ")");

        DomainFile srcDF = getProjectRootFolder().getFile(srcProgPath.replaceFirst("^/", ""));
        if (srcDF == null) {
            // try folder traversal
            srcDF = findFile(getProjectRootFolder(), srcProgPath.replaceFirst("^/", ""));
        }
        if (srcDF == null) {
            printerr("Source program not found at " + srcProgPath);
            return;
        }
        Program srcProgram = (Program) srcDF.getDomainObject(this, true, false, monitor);
        println("SOURCE = " + srcProgram.getName() + " (" + srcProgram.getLanguageID() + ")");

        // Load seeds
        Gson gson = new Gson();
        List<Map<String, Object>> seeds;
        try (Reader r = new FileReader(seedsPath)) {
            seeds = gson.fromJson(r, new TypeToken<List<Map<String, Object>>>() {}.getType());
        }
        println("Loaded " + seeds.size() + " seed pairs");

        VTSession session = null;
        end(true); // end script transaction so VT can take its own locks
        try {
            session = new VTSessionDB("seedprop", srcProgram, destProgram, this);
            DomainFolder folder = getProjectRootFolder();
            if (folder.getFile("seedprop_session") == null) {
                folder.createFile("seedprop_session", session, monitor);
            }

            int tx = session.startTransaction("seed-prop");
            int applied = 0, missingSrc = 0, missingDst = 0;
            try {
                VTAssociationManager manager = session.getAssociationManager();

                // Create a manual match set and add each seed as a match, then ACCEPT it.
                VTMatchSet manualSet = session.getManualMatchSet();
                FunctionManager srcFM = srcProgram.getFunctionManager();
                FunctionManager dstFM = destProgram.getFunctionManager();

                for (Map<String, Object> s : seeds) {
                    Address sAddr = addr(srcProgram, (String) s.get("src_va"));
                    Address dAddr = addr(destProgram, (String) s.get("dst_va"));
                    if (sAddr == null || dAddr == null) continue;
                    Function sf = srcFM.getFunctionAt(sAddr);
                    Function df = dstFM.getFunctionAt(dAddr);
                    if (sf == null) { missingSrc++; continue; }
                    if (df == null) { missingDst++; continue; }

                    VTMatchInfo mi = new VTMatchInfo(manualSet);
                    mi.setSourceAddress(sAddr);
                    mi.setDestinationAddress(dAddr);
                    mi.setAssociationType(VTAssociationType.FUNCTION);
                    mi.setSimilarityScore(new VTScore(1.0));
                    mi.setConfidenceScore(new VTScore(10.0));
                    mi.setSourceLength((int) sf.getBody().getNumAddresses());
                    mi.setDestinationLength((int) df.getBody().getNumAddresses());
                    VTMatch match = manualSet.addMatch(mi);
                    VTAssociation assoc = match.getAssociation();
                    if (assoc.getStatus() != VTAssociationStatus.ACCEPTED) {
                        try { assoc.setAccepted(); applied++; }
                        catch (VTAssociationStatusException e) { /* conflict; skip */ }
                    } else { applied++; }
                }
                println("Seeds applied as ACCEPTED: " + applied +
                        "  (missingSrcFn=" + missingSrc + " missingDstFn=" + missingDst + ")");
                int accepted = countAccepted(session);
                println("Total ACCEPTED associations in session BEFORE correlate: " + accepted);

                // Build BSim correlator with USE_ACCEPTED_MATCHES_AS_SEEDS=true
                BSimProgramCorrelatorFactory factory = new BSimProgramCorrelatorFactory();
                VTOptions opts = factory.createDefaultOptions();
                opts.setBoolean(BSimProgramCorrelatorFactory.USE_ACCEPTED_MATCHES_AS_SEEDS, true);
                opts.setDouble(BSimProgramCorrelatorFactory.SEED_CONF_THRESHOLD, seedConf);
                opts.setDouble(BSimProgramCorrelatorFactory.IMPLICATION_THRESHOLD, impThresh);

                AddressSetView srcSet = srcProgram.getMemory().getLoadedAndInitializedAddressSet();
                AddressSetView dstSet = destProgram.getMemory().getLoadedAndInitializedAddressSet();

                VTProgramCorrelator correlator =
                    factory.createCorrelator(srcProgram, srcSet, destProgram, dstSet, opts);
                println("Running BSimProgramCorrelator (seed propagation)...");
                long t0 = System.currentTimeMillis();
                VTMatchSet results = correlator.correlate(session, monitor);
                long dt = (System.currentTimeMillis() - t0) / 1000;
                println("Correlate done in " + dt + "s. Result matches: " + results.getMatches().size());

                // Export ALL matches across all sets (manual seeds + bsim results)
                exportMatches(session, srcProgram, destProgram, outPath);
            } finally {
                session.endTransaction(tx, true);
            }
        } finally {
            if (session != null) session.release(this);
            // do NOT save the dest program / session (keep dest pristine; we only read)
        }
    }

    private DomainFile findFile(DomainFolder folder, String name) {
        DomainFile f = folder.getFile(name);
        if (f != null) return f;
        for (DomainFolder sub : folder.getFolders()) {
            DomainFile r = findFile(sub, name);
            if (r != null) return r;
        }
        return null;
    }

    private int countAccepted(VTSession session) {
        int n = 0;
        for (VTMatchSet ms : session.getMatchSets()) {
            for (VTMatch m : ms.getMatches()) {
                if (m.getAssociation().getStatus() == VTAssociationStatus.ACCEPTED) n++;
            }
        }
        return n;
    }

    private Address addr(Program p, String hex) {
        if (hex == null) return null;
        try {
            long v = Long.parseLong(hex.replaceFirst("^0x", ""), 16);
            return p.getAddressFactory().getDefaultAddressSpace().getAddress(v);
        } catch (Exception e) { return null; }
    }

    private void exportMatches(VTSession session, Program src, Program dst, String outPath)
            throws IOException {
        FunctionManager sfm = src.getFunctionManager();
        FunctionManager dfm = dst.getFunctionManager();
        List<Map<String, Object>> rows = new ArrayList<>();
        Set<String> seenDst = new HashSet<>();
        for (VTMatchSet ms : session.getMatchSets()) {
            String setName;
            try { setName = ms.getProgramCorrelatorInfo().getName(); }
            catch (Exception e) { setName = "set" + ms.getID(); }
            for (VTMatch m : ms.getMatches()) {
                VTAssociation as = m.getAssociation();
                Address sa = as.getSourceAddress();
                Address da = as.getDestinationAddress();
                Function sf = sfm.getFunctionAt(sa);
                Function df = dfm.getFunctionAt(da);
                Map<String, Object> row = new LinkedHashMap<>();
                row.put("dst_va", "0x" + Long.toHexString(da.getOffset()));
                row.put("src_va", "0x" + Long.toHexString(sa.getOffset()));
                row.put("wii_name", sf != null ? sf.getName(true) : null);
                row.put("dst_size", df != null ? (int) df.getBody().getNumAddresses() : 0);
                row.put("src_size", sf != null ? (int) sf.getBody().getNumAddresses() : 0);
                VTScore sim = m.getSimilarityScore();
                VTScore conf = m.getConfidenceScore();
                row.put("sim", sim != null ? sim.getScore() : null);
                row.put("conf", conf != null ? conf.getScore() : null);
                row.put("status", as.getStatus().toString());
                row.put("matchset", setName);
                rows.add(row);
            }
        }
        try (Writer w = new FileWriter(outPath)) {
            new GsonBuilder().create().toJson(rows, w);
        }
        println("Exported " + rows.size() + " match rows -> " + outPath);
    }
}
