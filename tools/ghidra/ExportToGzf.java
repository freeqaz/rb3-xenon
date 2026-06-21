import ghidra.app.script.GhidraScript;
import ghidra.framework.model.DomainFile;
import java.io.File;
// args[0] = output gzf path. Packs the CURRENT program's DomainFile to a gzf.
public class ExportToGzf extends GhidraScript {
    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        File out = new File(a[0]);
        DomainFile df = currentProgram.getDomainFile();
        println("Packing "+currentProgram.getName()+" -> "+out.getAbsolutePath());
        df.packFile(out, monitor);
        println("DONE packFile size="+out.length());
    }
}
