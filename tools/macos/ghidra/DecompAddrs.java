// Decompiles the ELF addresses listed in args[0] (one hex per line, ELF vaddr) into args[1].
//@category BF2
import java.io.*;
import java.nio.file.*;
import java.util.*;
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
public class DecompAddrs extends GhidraScript {
  @Override protected void run() throws Exception {
    String[] a = getScriptArgs();
    List<String> lines = Files.readAllLines(Paths.get(a[0]));
    DecompInterface d = new DecompInterface();
    DecompileOptions o = new DecompileOptions(); o.grabFromProgram(currentProgram);
    d.setOptions(o); d.toggleCCode(true); d.toggleSyntaxTree(false); d.setSimplificationStyle("decompile");
    d.openProgram(currentProgram);
    Address base = currentProgram.getImageBase();
    try (PrintWriter w = new PrintWriter(new FileWriter(a[1]))) {
      for (String l : lines) {
        l = l.trim(); if (l.isEmpty()) continue;
        long elf = Long.parseLong(l.replace("0x",""), 16);
        Address ad = base.add(elf);
        Function f = getFunctionAt(ad);
        if (f == null) { createFunction(ad, null); f = getFunctionAt(ad); }
        w.println("//==== ELF 0x" + Long.toHexString(elf).toUpperCase() + " " + (f == null ? "?" : f.getName(true)));
        if (f == null) { w.println("// no function"); continue; }
        DecompileResults r = d.decompileFunction(f, 120, monitor);
        if (r.decompileCompleted()) w.println(r.getDecompiledFunction().getC()); else w.println("// FAILED " + r.getErrorMessage());
      }
    }
    d.dispose();
  }
}
