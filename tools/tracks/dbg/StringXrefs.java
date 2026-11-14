// For each defined string, list referencing functions: func_entry \t func_name \t ref_addr \t string_addr \t string
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.nio.file.*;
public class StringXrefs extends GhidraScript {
  public void run() throws Exception {
    Path out = Paths.get(getScriptArgs()[0]);
    Files.createDirectories(out);
    Listing l = currentProgram.getListing();
    ReferenceManager rm = currentProgram.getReferenceManager();
    try (PrintWriter w = new PrintWriter(Files.newBufferedWriter(out.resolve("strxrefs.tsv")))) {
      for (Data d : l.getDefinedData(true)) {
        if (!d.hasStringValue()) continue;
        String s = String.valueOf(d.getValue()).replace("\t"," ").replace("\n","\\n").replace("\r","");
        for (Reference r : rm.getReferencesTo(d.getAddress())) {
          Function f = getFunctionContaining(r.getFromAddress());
          w.printf("%s\t%s\t%s\t%s\t%s%n", f==null?"-":"0x"+f.getEntryPoint(), f==null?"-":f.getName(true),
                   "0x"+r.getFromAddress(), "0x"+d.getAddress(), s);
        }
      }
    }
  }
}
