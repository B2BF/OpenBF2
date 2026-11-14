// Ghidra headless post-script: decompile a list of ELF functions.
//
//   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
//       -scriptPath tools/ai-reconstruct/ghidra -postScript DecompList.java <list> <outdir> [timeout_s] [suffix]
//
// <list>: one ELF virtual address per line (hex 0x...), optionally followed by a tab and the
// output stem (default 0xADDR). The Ghidra address is image base + ELF address (the ELF images
// link at 0; Ghidra rebases ia-32 to 0x10000 and x86-64 to 0x100000), so the script works for
// both images. Writes <outdir>/<stem><suffix> (suffix default ".c") and appends one line per
// function to <outdir>/decompile-status.tsv: stem, ok|fail|nofunc, seconds, message.
// A function missing from the program is created at the address (not saved: -readOnly).
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class DecompList extends GhidraScript {
  public void run() throws Exception {
    String[] a = getScriptArgs();
    java.util.List<String> lines = Files.readAllLines(Paths.get(a[0]));
    Path out = Paths.get(a[1]);
    int timeout = a.length > 2 ? Integer.parseInt(a[2]) : 120;
    String suffix = a.length > 3 ? a[3] : ".c";
    long base = currentProgram.getImageBase().getOffset();
    DecompInterface d = new DecompInterface();
    d.openProgram(currentProgram);
    Path status = out.resolve("decompile-status.tsv");
    for (String l : lines) {
      l = l.trim();
      if (l.isEmpty() || l.startsWith("#")) continue;
      String[] p = l.split("\t");
      long elf = Long.decode(p[0]);
      String stem = p.length > 1 ? p[1] : String.format("0x%X", elf);
      Address ad = toAddr(elf + base);
      Function f = getFunctionAt(ad);
      if (f == null) { createFunction(ad, null); f = getFunctionAt(ad); }
      String st, msg = "";
      long t0 = System.currentTimeMillis();
      if (f == null) {
        st = "nofunc";
        msg = "no function at " + ad;
      } else {
        DecompileResults r = d.decompileFunction(f, timeout, monitor);
        String hdr = String.format("// ELF 0x%X  Ghidra %s  %s%n// Ghidra decompile (evidence cache). The listing (.s) is the source of truth.%n",
                                   elf, ad, f.getName(true));
        if (r != null && r.getDecompiledFunction() != null) {
          st = "ok";
          Files.write(out.resolve(stem + suffix), (hdr + r.getDecompiledFunction().getC()).getBytes(StandardCharsets.UTF_8));
        } else {
          st = "fail";
          msg = r == null ? "null result" : String.valueOf(r.getErrorMessage()).replaceAll("\\s+", " ").trim();
          Files.write(out.resolve(stem + suffix), (hdr + "// DECOMPILE FAILED: " + msg + "\n").getBytes(StandardCharsets.UTF_8));
        }
      }
      double secs = (System.currentTimeMillis() - t0) / 1000.0;
      Files.write(status, String.format("%s\t%s\t%.1f\t%s%n", stem + suffix, st, secs, msg).getBytes(StandardCharsets.UTF_8),
                  StandardOpenOption.CREATE, StandardOpenOption.APPEND);
    }
    d.dispose();
  }
}
