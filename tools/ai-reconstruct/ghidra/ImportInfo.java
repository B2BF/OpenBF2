// Ghidra headless post-script: print the program's image base and function count (sanity check
// after import). Used by `evidence.py ghidra-project --create`.
import ghidra.app.script.GhidraScript;
public class ImportInfo extends GhidraScript {
  public void run() throws Exception {
    println("EVIDENCE-IMPORT-INFO image_base=" + currentProgram.getImageBase()
            + " functions=" + currentProgram.getFunctionManager().getFunctionCount());
  }
}
