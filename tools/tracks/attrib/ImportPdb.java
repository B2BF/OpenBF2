// Headless pre-script. Select the supplied local PDB without a symbol-server lookup.
import ghidra.app.script.GhidraScript;
import ghidra.app.plugin.core.analysis.PdbUniversalAnalyzer;

public class ImportPdb extends GhidraScript {
    public void run() throws Exception {
        PdbUniversalAnalyzer.setPdbFileOption(currentProgram, new java.io.File(getScriptArgs()[0]));
        setAnalysisOption(currentProgram, "PDB Universal", "true");
        setAnalysisOption(currentProgram, "PDB Universal.Import Source Line Info", "true");
    }
}
