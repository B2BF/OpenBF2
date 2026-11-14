// Headless post-script: preserve function bodies, all symbol aliases and PDB source maps.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.sourcemap.*;
import java.io.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class ExportPdb extends GhidraScript {
    private String cell(String s) { return s.replace('\t', ' ').replace('\n', ' '); }
    private PrintWriter writer(Path out, String file) throws IOException {
        return new PrintWriter(Files.newBufferedWriter(out.resolve(file), StandardCharsets.UTF_8));
    }
    public void run() throws Exception {
        if (!currentProgram.getLanguageID().toString().equals("x86:LE:32:default"))
            throw new IOException("expected PE i386, got " + currentProgram.getLanguageID());
        Path out = Paths.get(getScriptArgs()[0]);
        Files.createDirectories(out);
        try (PrintWriter w = writer(out, "metadata.tsv")) {
            w.println("image\tarchitecture\tbase\tsha256");
            w.printf("%s\t%s\t0x%08X\t%s%n", cell(currentProgram.getName()),
                currentProgram.getLanguageID(), currentProgram.getImageBase().getOffset(),
                currentProgram.getExecutableSHA256());
        }
        int count = 0, lines = 0;
        try (PrintWriter w = writer(out, "functions.tsv");
             PrintWriter ranges = writer(out, "ranges.tsv")) {
            w.println("entry\tsize\tthunk\tname\tnamespace\tprototype\tsymbol_source");
            ranges.println("entry\tstart\tend");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                w.printf("0x%08X\t%d\t%d\t%s\t%s\t%s\t%s%n", f.getEntryPoint().getOffset(),
                    f.getBody().getNumAddresses(), f.isThunk() ? 1 : 0, cell(f.getName()),
                    cell(f.getParentNamespace().isGlobal() ? "" : f.getParentNamespace().getName(true)),
                    cell(f.getPrototypeString(false, false)), f.getSymbol().getSource());
                for (AddressRange r : f.getBody().getAddressRanges())
                    ranges.printf("0x%08X\t0x%08X\t0x%08X%n", f.getEntryPoint().getOffset(),
                        r.getMinAddress().getOffset(), r.getMaxAddress().getOffset());
                count++;
            }
        }
        try (PrintWriter w = writer(out, "symbols.tsv")) {
            w.println("address\tname\tsource\ttype");
            SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
            while (it.hasNext()) {
                Symbol s = it.next();
                if (s.getAddress().isMemoryAddress())
                    w.printf("0x%08X\t%s\t%s\t%s%n", s.getAddress().getOffset(),
                        cell(s.getName(true)), s.getSource(), s.getSymbolType());
            }
        }
        try (PrintWriter w = writer(out, "lines.tsv")) {
            w.println("address\tlength\tfile\tline");
            SourceMapEntryIterator it = currentProgram.getSourceFileManager()
                .getSourceMapEntryIterator(currentProgram.getMinAddress(), true);
            while (it.hasNext()) {
                SourceMapEntry e = it.next();
                w.printf("0x%08X\t%d\t%s\t%d%n", e.getBaseAddress().getOffset(), e.getLength(),
                    cell(e.getSourceFile().getPath()), e.getLineNumber());
                lines++;
            }
        }
        println("EXPORT-PDB image=" + currentProgram.getName() + " architecture=" +
            currentProgram.getLanguageID() + " base=" + currentProgram.getImageBase() +
            " functions=" + count + " source_maps=" + lines);
        if (lines == 0) throw new IOException("PDB source line maps are required");
    }
}
