// Per function: entry \t size \t name \t comma-separated direct call targets in address order
// (CALL instructions with a flow reference to a function entry; thunks resolved).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.nio.file.*;
public class ExportCalls extends GhidraScript {
  public void run() throws Exception {
    Path out = Paths.get(getScriptArgs()[0]);
    Files.createDirectories(out);
    Listing l = currentProgram.getListing();
    FunctionManager fm = currentProgram.getFunctionManager();
    try (PrintWriter w = new PrintWriter(Files.newBufferedWriter(out.resolve("calls.tsv")))) {
      for (Function f : fm.getFunctions(true)) {
        StringBuilder sb = new StringBuilder();
        InstructionIterator it = l.getInstructions(f.getBody(), true);
        while (it.hasNext()) {
          Instruction ins = it.next();
          if (!ins.getFlowType().isCall()) continue;
          for (Address a : ins.getFlows()) {
            Function c = fm.getFunctionAt(a);
            if (c == null) continue;
            if (c.isThunk()) { Function t = c.getThunkedFunction(true); if (t != null) c = t; }
            if (sb.length() > 0) sb.append(',');
            sb.append("0x").append(c.getEntryPoint());
          }
        }
        String nm = f.getName(true);
        try { nm = f.getSymbol().getName(true); } catch (Exception e) {}
        w.printf("0x%s\t%d\t%s\t%s%n", f.getEntryPoint(), f.getBody().getNumAddresses(), nm, sb);
      }
    }
  }
}
