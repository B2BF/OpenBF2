#!/usr/bin/env python3
"""[TEMP] Build synthetic SWF observers against real client objects and headers.

The supplied build must already contain the SwiffPlayer and plugin-kit archives.
No game, renderer device, content, sockets or engine entry is executed.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--bitmap-cache", action="store_true",
                        help="Check generated-image registry ownership and pixel replay")
    parser.add_argument("--pre-change-render", action="store_true",
                        help="Compile HEAD's render wrappers to check the regression fails")
    parser.add_argument("--pre-change-enumerate", action="store_true",
                        help="Use HEAD's object enumeration body to check common dispatch fails")
    parser.add_argument("--pre-change-character", action="store_true",
                        help="Use HEAD's character bodies to check arguments/context recovery fails")
    parser.add_argument("--pre-change-super", action="store_true",
                        help="Use HEAD's class binding to check borrowed context recovery fails")
    parser.add_argument("--pre-change-sprite-setter", action="store_true",
                        help="Use HEAD's sprite setter to check text conversion order fails")
    parser.add_argument("--pre-change-destruction", action="store_true",
                        help="Use implicit environment member destruction to check release order fails")
    parser.add_argument("--pre-change-text-graph", action="store_true",
                        help="Use HEAD's text visitors to check member graph traversal fails")
    parser.add_argument("--pre-change-matrix", action="store_true",
                        help="Use HEAD's matrices to check cancellation precision fails")
    parser.add_argument("--pre-change-text-properties", action="store_true",
                        help="Use HEAD's text getter to check in-place String output fails")
    parser.add_argument("--pre-change-text-setter", action="store_true",
                        help="Use HEAD's text setter to check numeric and ownership behavior fails")
    parser.add_argument("--archive-only", action="store_true",
                        help="Use production objects from the completed build archive")
    parser.add_argument("--sanitize", action="store_true",
                        help="Instrument the selected production TUs and observer with AddressSanitizer")
    parser.add_argument("--font-raster", action="store_true",
                        help="Compile the production font TU inside its private-buffer observer")
    parser.add_argument("--pre-change-font", action="store_true",
                        help="Use HEAD's font TU as the expected-failure raster control")
    parser.add_argument("--text-layout", action="store_true",
                        help="Include the production text TU to inspect private layout state")
    parser.add_argument("--pre-change-text-layout", action="store_true",
                        help="Use HEAD's formatter bodies as the expected-failure layout control")
    parser.add_argument("--pre-change-text-lifetime", action="store_true",
                        help="Use HEAD's sprite/list teardown for the stale-parent failure control")
    parser.add_argument("--pre-change-text-numeric", action="store_true",
                        help="Retain profile changes but restore the old glyph baseline expression")
    parser.add_argument("--pre-change-log", action="store_true",
                        help="Use HEAD's logging TU to check full-buffer termination differs")
    parser.add_argument("--pre-change-vm-drag", action="store_true",
                        help="Use HEAD's drag opcode to check callback conversion order fails")
    parser.add_argument("--pre-change-init-actions", action="store_true",
                        help="Use HEAD's sprite load to check queued timeline isolation fails")
    args = parser.parse_args()
    if args.bitmap_cache and (args.text_layout or args.font_raster):
        parser.error("--bitmap-cache requires its own observer run")
    if args.pre_change_text_layout and not args.text_layout:
        parser.error("--pre-change-text-layout requires --text-layout")
    if args.pre_change_text_lifetime and not args.text_layout:
        parser.error("--pre-change-text-lifetime requires --text-layout")
    if args.pre_change_text_numeric and (not args.text_layout or args.pre_change_text_layout):
        parser.error("--pre-change-text-numeric requires its own --text-layout run")
    if args.text_layout and (args.archive_only or args.font_raster):
        parser.error("--text-layout requires its own included production TU")
    if args.pre_change_log and (args.text_layout or args.font_raster):
        parser.error("--pre-change-log requires the main observer")
    if args.pre_change_font and not args.font_raster:
        parser.error("--pre-change-font requires --font-raster")
    if args.font_raster and args.archive_only:
        parser.error("--font-raster compiles the included production font TU")
    if args.sanitize and args.archive_only:
        parser.error("--sanitize requires compiling the production TUs")
    if args.archive_only and (args.pre_change_render or args.pre_change_enumerate
                              or args.pre_change_character or args.pre_change_super
                              or args.pre_change_destruction or args.pre_change_text_graph
                              or args.pre_change_matrix or args.pre_change_text_properties
                              or args.pre_change_text_setter or args.pre_change_sprite_setter
                              or args.pre_change_log or args.pre_change_vm_drag
                              or args.pre_change_init_actions):
        parser.error("--archive-only cannot use a pre-change source snapshot")
    root = Path(__file__).resolve().parents[2]
    output = args.out_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    build = args.build_dir.resolve()
    entries = json.loads((build / "compile_commands.json").read_text())

    def run(command, name):
        result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        (output / (name + ".log")).write_text(result.stdout)
        print(name, result.returncode, flush=True)
        if result.returncode:
            print(result.stdout[-6000:])
            raise SystemExit(result.returncode)

    objects = []
    sources = ["gameswf_action.cpp", "gameswf_impl.cpp", "gameswf_render.cpp", "gameswf_types.cpp",
               "gameswf_shape.cpp", "gameswf_styles.cpp", "gameswf_timers.cpp",
               "gameswf_button.cpp", "gameswf_text.cpp", "gameswf_morph2.cpp", "gameswf_log.cpp"]
    if args.archive_only:
        sources = []
    fixture = "swf_font_raster_runtime.cpp" if args.font_raster else "swf_iface_runtime.cpp"
    if args.bitmap_cache:
        fixture = "swf_bitmap_cache_runtime.cpp"
    if args.text_layout:
        fixture = "swf_text_layout_runtime.cpp"
        sources = ["gameswf_impl.cpp", "gameswf_dlist.cpp"] if args.sanitize or args.pre_change_text_lifetime else []
    if args.font_raster:
        sources = []
    for name in sources + [fixture]:
        entry_name = ("gameswf_text.cpp" if args.text_layout else "gameswf_fontlib.cpp" if args.font_raster else "gameswf_action.cpp") if name == fixture else name
        entry = next(e for e in entries if Path(e["file"]).name == entry_name)
        command = shlex.split(entry["command"])
        if Path(command[0]).name == "ccache":
            command.pop(0)
        obj = output / (name + ".o")
        command[command.index("-o") + 1] = str(obj)
        source = root / "Code/Tests" / fixture if name == fixture else Path(entry["file"])
        if args.pre_change_text_lifetime and name in ("gameswf_impl.cpp", "gameswf_dlist.cpp"):
            snapshot = output / name
            snapshot.write_bytes(subprocess.check_output(
                ["git", "show", "HEAD:Code/BF2/External/gameswf/" + name], cwd=root))
            command.extend(["-iquote", str(source.parent)])
            source = snapshot
        if name == "gameswf_log.cpp" and args.pre_change_log:
            snapshot = output / name
            snapshot.write_bytes(subprocess.check_output(
                ["git", "show", "HEAD:Code/BF2/External/gameswf/" + name], cwd=root))
            command.extend(["-iquote", str(source.parent)])
            source = snapshot
        if args.pre_change_text_layout:
            snapshot = output / "gameswf_text.cpp"
            snapshot.write_bytes(subprocess.check_output(
                ["git", "show", "HEAD:Code/BF2/External/gameswf/gameswf_text.cpp"], cwd=root))
            command.extend(["-iquote", str(root / "Code/BF2/External/gameswf"),
                            '-DBF2_SWF_TEXT_SOURCE="' + str(snapshot) + '"'])
        if args.pre_change_text_numeric:
            current = (root / "Code/BF2/External/gameswf/gameswf_text.cpp").read_text()
            start = current.index("\t\t\trec.m_style.m_y_offset = bf2x87::store(", current.index("void\tformat_text_glyphs()"))
            end = current.index("\n\t\t\trec.m_style.m_text_height", start)
            snapshot = output / "gameswf_text.cpp"
            snapshot.write_text(current[:start] + "\t\t\trec.m_style.m_y_offset = m_def->m_text_height\n"
                                "\t\t\t\t+ (m_def->m_font->get_leading() - m_def->m_font->get_descent()) * scale;"
                                + current[end:])
            command.extend(["-iquote", str(root / "Code/BF2/External/gameswf"),
                            '-DBF2_SWF_TEXT_SOURCE="' + str(snapshot) + '"'])
        if args.pre_change_font:
            snapshot = output / "gameswf_fontlib.cpp"
            snapshot.write_bytes(subprocess.check_output(
                ["git", "show", "HEAD:Code/BF2/External/gameswf/gameswf_fontlib.cpp"], cwd=root))
            command.extend(["-iquote", str(root / "Code/BF2/External/gameswf"),
                            '-DBF2_SWF_FONTLIB_SOURCE="' + str(snapshot) + '"'])
        if (name == "gameswf_text.cpp" and args.pre_change_text_graph
                or name == "gameswf_types.cpp" and args.pre_change_matrix):
            snapshot = output / name
            snapshot.write_bytes(subprocess.check_output(
                ["git", "show", "HEAD:Code/BF2/External/gameswf/" + name], cwd=root))
            command.extend(["-iquote", str(source.parent)])
            source = snapshot
        if name == "gameswf_text.cpp" and (args.pre_change_text_properties or args.pre_change_text_setter):
            previous = subprocess.check_output(
                ["git", "show", "HEAD:Code/BF2/External/gameswf/gameswf_text.cpp"], cwd=root).decode()
            current = source.read_text()

            def getter(text, signature):
                start = text.index(signature)
                opening = text.index("{", start)
                depth = 1
                end = opening + 1
                while depth:
                    depth += (text[end] == "{") - (text[end] == "}")
                    end += 1
                return start, end

            signature = "bool\tdice_set_member(" if args.pre_change_text_setter else "bool\tdice_get_member("
            start, end = getter(current, signature)
            old_start, old_end = getter(previous, signature)
            snapshot = output / name
            snapshot.write_text(current[:start] + previous[old_start:old_end] + current[end:])
            command.extend(["-iquote", str(source.parent)])
            source = snapshot
        if name == "gameswf_render.cpp" and args.pre_change_render:
            snapshot = output / name
            snapshot.write_bytes(subprocess.check_output(
                ["git", "show", "HEAD:Code/BF2/External/gameswf/gameswf_render.cpp"], cwd=root))
            command.extend(["-iquote", str(source.parent)])
            source = snapshot
        if name == "gameswf_impl.cpp" and (args.pre_change_character or args.pre_change_super
                                           or args.pre_change_sprite_setter or args.pre_change_init_actions):
            previous = subprocess.check_output(
                ["git", "show", "HEAD:Code/BF2/External/gameswf/gameswf_impl.cpp"], cwd=root).decode()
            current = source.read_text()

            def body(text, signature):
                start = text.index(signature, text.index("struct sprite_instance : public character"))
                opening = text.index("{", start)
                depth = 1
                end = opening + 1
                while depth:
                    depth += (text[end] == "{") - (text[end] == "}")
                    end += 1
                return start, end

            signatures = (["void\tbind_class_prototype("]
                          if args.pre_change_super or args.pre_change_character else [])
            if args.pre_change_character:
                signatures.append("virtual bool\tget_member(")
            if args.pre_change_sprite_setter:
                signatures.append("virtual void\tset_member(")
            if args.pre_change_init_actions:
                signatures.append("virtual void\ton_event_load(")
            for signature in signatures:
                start, end = body(current, signature)
                old_start, old_end = body(previous, signature)
                current = current[:start] + previous[old_start:old_end] + current[end:]
            snapshot = output / name
            snapshot.write_text(current)
            command.extend(["-iquote", str(source.parent)])
            source = snapshot
        if name == "gameswf_action.cpp" and args.pre_change_vm_drag:
            current = source.read_text()
            previous = subprocess.check_output(
                ["git", "show", "HEAD:Code/BF2/External/gameswf/gameswf_action.cpp"], cwd=root).decode()
            start = current.index("\t\t\t\tcase 0x27:")
            end = current.index("\t\t\t\tcase 0x28:", start)
            old_start = previous.index("\t\t\t\tcase 0x27:")
            old_end = previous.index("\t\t\t\tcase 0x28:", old_start)
            snapshot = output / name
            snapshot.write_text(current[:start] + previous[old_start:old_end] + current[end:])
            command.extend(["-iquote", str(source.parent)])
            source = snapshot
        if name == "gameswf_action.cpp" and args.pre_change_enumerate:
            previous = subprocess.check_output(
                ["git", "show", "HEAD:Code/BF2/External/gameswf/gameswf_action.cpp"], cwd=root).decode()
            current = source.read_text()

            def body(text):
                start = text.index("void\tas_object::enumerate(")
                opening = text.index("{", start)
                depth = 1
                end = opening + 1
                while depth:
                    depth += (text[end] == "{") - (text[end] == "}")
                    end += 1
                return start, end

            start, end = body(current)
            old_start, old_end = body(previous)
            snapshot = output / name
            snapshot.write_text(current[:start] + previous[old_start:old_end] + current[end:])
            command.extend(["-iquote", str(source.parent)])
            source = snapshot
        if name == "gameswf_action.cpp" and args.pre_change_destruction:
            current = source.read_text()
            start = current.index("as_environment::~as_environment()")
            opening = current.index("{", start)
            end = current.index("\n\t}", opening) + len("\n\t}")
            # [TEMP] The old implicit destructor ran only member destructors.
            snapshot = output / name
            snapshot.write_text(current[:opening] + "{}" + current[end:])
            command.extend(["-iquote", str(source.parent)])
            source = snapshot
        command[command.index(entry["file"])] = str(source)
        if args.sanitize:
            command.extend(["-fsanitize=address", "-fno-omit-frame-pointer"])
        run(command, name)
        objects.append(str(obj))
    executable = output / "swf_iface_runtime"
    compiler = shlex.split(entries[0]["command"])
    if Path(compiler[0]).name == "ccache":
        compiler.pop(0)
    run([compiler[0], "-arch", "x86_64", "-Wl,-dead_strip",
         "-Wl,-map," + str(output / "link.map"), *objects,
         str(build / "libbf2_swiffplayer_static.a"),
         str(build / "libbf2_plugin_kit_static.a"),
         str(build / "libbf2_expat_static.a"),
         "-lz", "-framework", "CoreFoundation", "-o", str(executable)]
        + (["-fsanitize=address"] if args.sanitize else []), "link")
    run([str(executable)], "runtime")
    print((output / "runtime.log").read_text(), end="")
    if args.font_raster or args.text_layout or args.bitmap_cache:
        return
    unadapted = subprocess.run([str(executable), "--missing-async-input"], cwd=root,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (output / "missing-async-input.log").write_text(unadapted.stdout)
    if unadapted.returncode != 0 or "[FRONTIER]" in unadapted.stdout:
        raise SystemExit("missing-async-input did not read Enter as up")
    print("missing-async-input: Enter reads up without a host adapter", flush=True)
    for case in ("enumerate-null", "enumerate-function", "text-tab-invalid", "new-method-c-function"):
        fault_environment = None
        if args.sanitize:
            fault_environment = dict(os.environ)
            fault_environment["ASAN_OPTIONS"] = (fault_environment.get("ASAN_OPTIONS", "")
                                                + ":handle_segv=0:handle_sigbus=0")
        malformed = subprocess.run([str(executable), "--" + case], cwd=root,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                   env=fault_environment)
        (output / (case + ".log")).write_text(malformed.stdout)
        if malformed.returncode not in (-10, -11):
            raise SystemExit(case + " did not preserve the retail unchecked fault")
        print(case + (": unchecked index write faults" if case == "text-tab-invalid"
                      else ": unchecked dispatch faults"), flush=True)


if __name__ == "__main__":
    main()
