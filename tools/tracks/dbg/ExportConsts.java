// Per function: entry \t comma-separated constant fingerprints: for every data reference from the
// body to an initialized non-executable address, the 4-byte value as float ("f<value>") and the
// 8-byte value as double ("d<value>") when finite and "plausible" (|v| in [1e-6, 1e7]); plus
// immediate scalar operands >= 0x100 that are not addresses ("i<hex>").
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.scalar.Scalar;
import java.io.*;
import java.nio.file.*;
import java.util.*;
public class ExportConsts extends GhidraScript {
  static boolean plausible(double v) { double a = Math.abs(v); return !Double.isNaN(v) && !Double.isInfinite(v) && a >= 1e-6 && a <= 1e7; }
  public void run() throws Exception {
    Path out = Paths.get(getScriptArgs()[0]);
    Listing l = currentProgram.getListing();
    Memory mem = currentProgram.getMemory();
    FunctionManager fm = currentProgram.getFunctionManager();
    try (PrintWriter w = new PrintWriter(Files.newBufferedWriter(out.resolve("consts.tsv")))) {
      for (Function f : fm.getFunctions(true)) {
        TreeSet<String> fp = new TreeSet<>();
        InstructionIterator it = l.getInstructions(f.getBody(), true);
        while (it.hasNext()) {
          Instruction ins = it.next();
          for (Reference r : ins.getReferencesFrom()) {
            if (!r.getReferenceType().isData()) continue;
            Address a = r.getToAddress();
            MemoryBlock b = mem.getBlock(a);
            if (b == null || b.isExecute() || !b.isInitialized()) continue;
            try {
              float fv = Float.intBitsToFloat(mem.getInt(a));
              if (plausible(fv)) fp.add("f" + fv);
              double dv = Double.longBitsToDouble(mem.getLong(a));
              if (plausible(dv)) fp.add("d" + (float) dv);
            } catch (Exception e) {}
          }
          for (int i = 0; i < ins.getNumOperands(); i++) {
            for (Object o : ins.getOpObjects(i)) {
              if (o instanceof Scalar) {
                long v = ((Scalar) o).getUnsignedValue();
                if (v >= 0x100 && v < 0x01000000 && ins.getMnemonicString().matches("(?i)(mov|cmp|push|add|sub|and|or|test|imul)")) {
                  fp.add("i" + Long.toHexString(v));
                }
              }
            }
          }
        }
        w.printf("0x%s\t%s%n", f.getEntryPoint(), String.join(",", fp));
      }
    }
  }
}
