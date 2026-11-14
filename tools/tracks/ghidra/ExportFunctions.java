// Ghidra headless post-script: export the function list and the RTTI vftables of a PE image.
//
//   analyzeHeadless <projdir> <proj> -import <image> -postScript ExportFunctions.java <outdir>
//   analyzeHeadless <projdir> <proj> -process <name> -noanalysis -readOnly
//       -scriptPath tools/tracks/ghidra -postScript ExportFunctions.java <outdir>
//
// Writes into <outdir>:
//   functions.tsv  entry (0x%08X), size (body bytes), thunk (0|1), name, namespace ("" = global)
//   vftables.tsv   vftable address, class (RTTI namespace), slot index, target (0x%08X)
//
// A vftable is every symbol named "vftable" (the MSVC RTTI analyzer names them
// <Class>::vftable). Its slots are read as 32-bit pointers until a word that is not the entry
// of a function, or the start of the next labelled data.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class ExportFunctions extends GhidraScript {
  public void run() throws Exception {
    Path out = Paths.get(getScriptArgs()[0]);
    Files.createDirectories(out);
    FunctionManager fm = currentProgram.getFunctionManager();
    try (PrintWriter w = new PrintWriter(Files.newBufferedWriter(out.resolve("functions.tsv"),
                                                                 StandardCharsets.UTF_8))) {
      w.println("entry\tsize\tthunk\tname\tnamespace");
      for (Function f : fm.getFunctions(true)) {
        Namespace ns = f.getParentNamespace();
        String nsName = (ns == null || ns.isGlobal()) ? "" : ns.getName(true);
        w.printf("0x%08X\t%d\t%d\t%s\t%s%n", f.getEntryPoint().getOffset(),
                 f.getBody().getNumAddresses(), f.isThunk() ? 1 : 0, f.getName(), nsName);
      }
    }
    SymbolTable st = currentProgram.getSymbolTable();
    Memory mem = currentProgram.getMemory();
    int count = 0;
    try (PrintWriter w = new PrintWriter(Files.newBufferedWriter(out.resolve("vftables.tsv"),
                                                                 StandardCharsets.UTF_8))) {
      w.println("vftable\tclass\tslot\ttarget");
      SymbolIterator it = st.getSymbols("vftable");
      while (it.hasNext()) {
        Symbol s = it.next();
        Address a = s.getAddress();
        String cls = s.getParentNamespace().getName(true);
        for (int slot = 0; slot < 4096; slot++) {
          Address p = a.add(4L * slot);
          if (slot > 0 && st.getPrimarySymbol(p) != null) break;
          long v;
          try { v = mem.getInt(p) & 0xFFFFFFFFL; } catch (MemoryAccessException e) { break; }
          Address t;
          try { t = toAddr(v); } catch (Exception e) { break; }
          if (t == null || fm.getFunctionAt(t) == null) break;
          w.printf("0x%08X\t%s\t%d\t0x%08X%n", a.getOffset(), cls, slot, v);
        }
        count++;
      }
    }
    println("EXPORT-FUNCTIONS functions=" + fm.getFunctionCount() + " vftables=" + count);
  }
}
