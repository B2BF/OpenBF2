// Ghidra headless post-script: decompile a list of PE functions by virtual address.
//
//   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
//       -scriptPath tools/tracks/ghidra -postScript DecompPE.java <list> <outdir> [timeout_s]
//
// <list>: one virtual address per line (hex 0x...). Writes <outdir>/0x%08X.c and appends one line
// per function to <outdir>/decompile-status.tsv: address, ok|fail|nofunc, seconds, message.
// Addresses already present in <outdir> are skipped, so an interrupted run resumes.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class DecompPE extends GhidraScript {
  public void run() throws Exception {
    String[] a = getScriptArgs();
    java.util.List<String> lines = Files.readAllLines(Paths.get(a[0]));
    Path out = Paths.get(a[1]);
    Files.createDirectories(out);
    int timeout = a.length > 2 ? Integer.parseInt(a[2]) : 120;
    DecompInterface d = new DecompInterface();
    d.openProgram(currentProgram);
    Path status = out.resolve("decompile-status.tsv");
    for (String l : lines) {
      l = l.trim();
      if (l.isEmpty() || l.startsWith("#")) continue;
      long va = Long.decode(l);
      String stem = String.format("0x%08X", va);
      Path file = out.resolve(stem + ".c");
      if (Files.exists(file)) continue;
      Function f = getFunctionAt(toAddr(va));
      String st, msg = "";
      long t0 = System.currentTimeMillis();
      String hdr = String.format("// %s %s  %s%n// Ghidra decompile (evidence cache): a structural hint. The listing (.s) is the source of truth.%n",
                                 currentProgram.getName(), stem, f == null ? "?" : f.getName(true));
      if (f == null) {
        st = "nofunc";
        msg = "no function at " + stem;
      } else {
        DecompileResults r = d.decompileFunction(f, timeout, monitor);
        if (r != null && r.getDecompiledFunction() != null) {
          st = "ok";
          Files.write(file, (hdr + r.getDecompiledFunction().getC()).getBytes(StandardCharsets.UTF_8));
        } else {
          st = "fail";
          msg = r == null ? "null result" : String.valueOf(r.getErrorMessage()).replaceAll("\\s+", " ").trim();
          Files.write(file, (hdr + "// DECOMPILE FAILED: " + msg + "\n").getBytes(StandardCharsets.UTF_8));
        }
      }
      double secs = (System.currentTimeMillis() - t0) / 1000.0;
      Files.write(status, String.format("%s\t%s\t%.1f\t%s%n", stem, st, secs, msg).getBytes(StandardCharsets.UTF_8),
                  StandardOpenOption.CREATE, StandardOpenOption.APPEND);
    }
    d.dispose();
  }
}
