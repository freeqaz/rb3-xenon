import java.io.*;
import java.util.*;
import com.google.gson.*;
import com.google.gson.reflect.TypeToken;
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.framework.model.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.Scalar;

// currentProgram = dest (default.xex). Source program path arg[1] = /wii. arg[0]=spotcheck.json arg[2]=out.json
public class SpotCheck extends GhidraScript {
    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        Gson g = new Gson();
        List<Map<String,Object>> items;
        try (Reader r = new FileReader(a[0])) {
            items = g.fromJson(r, new TypeToken<List<Map<String,Object>>>(){}.getType());
        }
        DomainFile sdf = findFile(getProjectRootFolder(), a[1].replaceFirst("^/",""));
        Program src = (Program) sdf.getDomainObject(this, true, false, monitor);

        List<Map<String,Object>> out = new ArrayList<>();
        for (Map<String,Object> it : items) {
            long dva = Long.parseLong(((String)it.get("dst_va")).replaceFirst("0x",""),16);
            long sva = Long.parseLong(((String)it.get("src_va")).replaceFirst("0x",""),16);
            Function df = funcAt(currentProgram, dva);
            Function sf = funcAt(src, sva);
            Map<String,Object> row = new LinkedHashMap<>();
            row.put("dst_va", it.get("dst_va"));
            row.put("wii_name", it.get("wii_name"));
            row.put("sim", it.get("sim"));
            row.put("dst_size", df!=null?(int)df.getBody().getNumAddresses():0);
            row.put("src_size", sf!=null?(int)sf.getBody().getNumAddresses():0);
            row.put("dst_calls", calleeNames(currentProgram, df));
            row.put("src_calls", calleeNames(src, sf));
            row.put("dst_strings", strings(currentProgram, df));
            row.put("src_strings", strings(src, sf));
            out.add(row);
        }
        try (Writer w = new FileWriter(a[2])) { new GsonBuilder().setPrettyPrinting().create().toJson(out, w); }
        println("Wrote "+out.size()+" spotcheck rows -> "+a[2]);
    }
    private Function funcAt(Program p, long va){
        Address ad = p.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        return p.getFunctionManager().getFunctionAt(ad);
    }
    private List<String> calleeNames(Program p, Function f){
        List<String> r = new ArrayList<>();
        if (f==null) return r;
        for (Function c : f.getCalledFunctions(monitor)) {
            String n = c.getName(true);
            r.add(n);
        }
        Collections.sort(r);
        return r;
    }
    private List<String> strings(Program p, Function f){
        List<String> r = new ArrayList<>();
        if (f==null) return r;
        ReferenceManager rm = p.getReferenceManager();
        Listing lst = p.getListing();
        for (Address a : f.getBody().getAddresses(true)) {
            for (Reference ref : rm.getReferencesFrom(a)) {
                Data d = lst.getDataAt(ref.getToAddress());
                if (d!=null && d.hasStringValue()) {
                    Object v = d.getValue();
                    if (v!=null) r.add(v.toString());
                }
            }
        }
        return r;
    }
    private DomainFile findFile(DomainFolder folder, String name){
        DomainFile f = folder.getFile(name);
        if (f!=null) return f;
        for (DomainFolder s : folder.getFolders()){ DomainFile rr=findFile(s,name); if(rr!=null) return rr; }
        return null;
    }
}
