#!/bin/bash
# [TEMP] [CHOSEN, not recovered] Owner-run M2 validation and play of the arm64 renderer-ON client.
# Usage: ./run.sh [--content DIR] MODE [MINUTES] [-- extra loopback.py options]
#   preflight   quarantine, signatures, architecture, tools and content (every mode runs it first)
#   checks      the arm64 CPU/GPU checks (no game content needed)
#   gate        the arm64 checks, then the Intel track gate on this machine: the scenarios infantry-range, render-view-dalian
#               and render-view-ocs, then the map2 reload run (Dalian, then operation_clean_sweep
#               after admin.runNextLevel, 300 s). results.txt lists per run the [FRONTIER] count, crash
#               reports, whether the script finished, and for the render views the per-view numbers
#               against the Intel reference views, with a compare PNG for every view. Over the Intel
#               limit is information (the GPU differs), not a failure. About 15 minutes.
#   smoke       scripted loopback play: sandboxed server + client, 16 bots, Dalian, play-combat,
#               200 s, bots spawn-only (the track gate's command)
#   smoke-ai    the same with the full bot AI ticking
#   play [MIN]  server + client with 16 bots and no input script, for hand play (default 10 min)
#   collect     pack results/ and every loopback run into results.tgz, the file to send back
#               (every mode ends with it)
#   all         checks, smoke, smoke-ai (gate is separate)
# DIR is the read-only Battlefield 2 folder (mods/, python/, pylib-2.3.4.zip, ...); default: the
# first of $BF2_CONTENT and the usual "~/Applications/Battlefield 2.app" layouts that exists.
# Ports: 16890 (game) and 29911 (LAN query) on loopback only; 16567 is denied by the sandbox.
# Nothing is written outside this folder; the game folder is cloned (APFS), never changed.
set -uo pipefail
bundle=$(cd "$(dirname "$0")" && pwd -P)
out="$bundle/results"
summary="$bundle/results.txt"
mkdir -p "$out/crash"
[ -f "$summary" ] || : > "$summary"
say() { echo "$*" | tee -a "$summary"; }
die() { echo "run.sh: $*" >&2; echo "run.sh: $*" >> "$summary"; exit 2; }

content=${BF2_CONTENT:-}
mode=
minutes=10
extra=()
while [ $# -gt 0 ]; do
    case "$1" in
        --content) content=${2:?--content needs a folder}; shift 2 ;;
        --content=*) content=${1#--content=}; shift ;;
        --) shift; extra=("$@"); break ;;
        -h|--help) sed -n '2,24p' "$0"; exit 0 ;;
        [0-9]*) minutes=$1; shift ;;
        *) [ -z "$mode" ] || die "unexpected argument: $1"; mode=$1; shift ;;
    esac
done
mode=${mode:-all}
case "$mode" in preflight|checks|gate|smoke|smoke-ai|play|collect|all) ;; *) die "unknown mode $mode (see ./run.sh --help)";; esac
tag=$(cat "$bundle/build-tag.txt" 2>/dev/null || echo arm64)   # package.sh: the out/build/<tag> folder
dry=${BF2_BUNDLE_DRYRUN:-0}    # 1: print the harness commands instead of running them

# ---- preflight --------------------------------------------------------------------------------
need_content=1; need_python=1; py=
case "$mode" in checks|collect) need_content=0; need_python=0;; esac

preflight() {
    local log="$out/preflight.txt" bad=0 f rehearsal_ok=1
    [ "${BF2_BUNDLE_REHEARSAL:-0}" = 1 ] && rehearsal_ok=0   # [TEMP] Intel rehearsal: report only
    : > "$log"
    pf() { echo "$*" >> "$log"; }
    pf "date: $(date -u)"
    pf "host: $(/usr/bin/uname -a)"
    pf "model: $(/usr/sbin/sysctl -n hw.model 2>&1); cpu: $(/usr/sbin/sysctl -n machdep.cpu.brand_string 2>&1); memory: $(( $(/usr/sbin/sysctl -n hw.memsize) / 1073741824 )) GB"
    pf "os: $(/usr/bin/sw_vers -productVersion) ($(/usr/bin/sw_vers -buildVersion))"
    pf "rosetta-translated shell: $(/usr/sbin/sysctl -n sysctl.proc_translated 2>/dev/null || echo n/a)"
    pf "bundle: $bundle"
    head -2 "$bundle/artifacts.txt" >> "$log"
    if [ "$(/usr/bin/uname -m)" != arm64 ]; then
        if [ "${BF2_BUNDLE_REHEARSAL:-0}" = 1 ]; then
            pf "[TEMP] rehearsal on $(/usr/bin/uname -m): the architecture check is skipped"
        else
            die 'run this bundle natively on Apple silicon: /usr/bin/arch -arm64 /bin/bash ./run.sh ...'
        fi
    fi
    # Gatekeeper: a download, AirDrop or Archive Utility copy carries com.apple.quarantine, and a
    # quarantined ad-hoc-signed binary is refused. The bundle is ours: clear it inside the bundle.
    local q
    q=$(/usr/bin/find "$bundle" -xattrname com.apple.quarantine 2>/dev/null | wc -l | tr -d ' ')
    /usr/bin/xattr -dr com.apple.quarantine "$bundle" 2>/dev/null
    pf "quarantine: $q files carried com.apple.quarantine; cleared (now $(/usr/bin/find "$bundle" -xattrname com.apple.quarantine 2>/dev/null | wc -l | tr -d ' '))"
    # Integrity first (a re-sign below changes the hashes).
    if (cd "$bundle" && /usr/bin/shasum -a 256 -c --quiet MANIFEST.sha256) >> "$log" 2>&1; then
        pf 'manifest: every file matches MANIFEST.sha256'
    else
        pf 'manifest: MISMATCH (listed above): incomplete copy?'; bad=$rehearsal_ok
    fi
    # arm64 runs only validly signed code; ad-hoc signatures are enough. Re-sign what fails.
    while IFS= read -r f; do
        f="$bundle/$f"
        if ! /usr/bin/file -b "$f" | grep -q 'arm64'; then pf "NOT arm64: $f"; bad=$rehearsal_ok; fi
        if ! /usr/bin/codesign -v "$f" 2>/dev/null; then
            /usr/bin/codesign --force --sign - "$f" >> "$log" 2>&1
            if /usr/bin/codesign -v "$f" 2>/dev/null; then pf "re-signed (ad hoc): ${f#"$bundle"/}"
            else pf "SIGNATURE INVALID: ${f#"$bundle"/}"; bad=1; fi
        fi
    done < <(sed -n 's/^== //p' "$bundle/artifacts.txt")
    pf "signatures: checked $(grep -c '^== ' "$bundle/artifacts.txt") Mach-O files"
    case "$bundle" in "$HOME/Desktop"*|"$HOME/Documents"*|"$HOME/Downloads"*)
        pf 'note: the bundle is in a privacy-protected folder; macOS may ask Terminal for access';; esac
    [ "$bad" = 0 ] || { cat "$log"; die "preflight failed (results/preflight.txt)"; }

    if [ "$need_python" = 1 ]; then
        for f in /usr/bin/expect /usr/bin/sandbox-exec /usr/sbin/lsof; do
            [ -x "$f" ] || die "$f is missing (needed by the loopback harness)"
        done
        py=${BF2_PYTHON:-}
        # The bundle's own portable Python (package.sh BF2_BUNDLE_PYTHON) comes first, so the M2
        # needs neither the Command Line Tools nor Homebrew. /usr/bin/python3 is only a stub that
        # offers the Command Line Tools download unless they are installed.
        [ -n "$py" ] || { [ -x "$bundle/python/bin/python3" ] && py=$bundle/python/bin/python3; }
        if [ -z "$py" ]; then
            if /usr/bin/xcode-select -p >/dev/null 2>&1; then py=/usr/bin/python3; fi
            for f in /opt/homebrew/bin/python3 /usr/local/bin/python3 \
                     /Library/Frameworks/Python.framework/Versions/Current/bin/python3; do
                [ -n "$py" ] || { [ -x "$f" ] && py=$f; }
            done
        fi
        [ -n "$py" ] || die 'the loopback harness needs Python 3.7 or later: this bundle has no python/ folder;
    pass BF2_PYTHON=/path/to/python3 or install the Command Line Tools (xcode-select --install)'
        "$py" -I -c 'import sys; sys.exit(sys.version_info < (3, 7))' 2>/dev/null \
            || die "$py is not a usable Python 3.7+ (set BF2_PYTHON)"
        pf "python: $py ($("$py" -I -c 'import sys; print(sys.version.split()[0])'))"
    fi

    if [ "$need_content" = 1 ]; then
        local tried=() c
        if [ -z "$content" ]; then
            for c in "$HOME/Applications/Battlefield 2.app/Contents/drive_c/Battlefield 2" \
                     "$HOME/Applications/Battlefield 2.app/Contents/SharedSupport/prefix/drive_c/Battlefield 2" \
                     "$HOME/Applications/Battlefield 2.app/Contents/SharedSupport/prefix/drive_c/Program Files (x86)/EA GAMES/Battlefield 2" \
                     "/Applications/Battlefield 2.app/Contents/drive_c/Battlefield 2" \
                     "/Applications/Battlefield 2.app/Contents/SharedSupport/prefix/drive_c/Battlefield 2"; do
                tried+=("$c")
                [ -d "$c/mods/bf2" ] && { content=$c; break; }
            done
        else
            tried+=("$content")
        fi
        if [ -z "$content" ] || [ ! -d "$content" ]; then
            die "no Battlefield 2 content folder found. Pass it with --content '/path/to/Battlefield 2'
    (the folder that holds mods/, python/ and pylib-2.3.4.zip). Looked in:
$(printf '      %s\n' "${tried[@]}")"
        fi
        content=$(cd "$content" && pwd -P) || die "cannot enter $content"
        for f in mods/bf2/Settings/ServerSettings.con mods/bf2/Settings/Controls.con python/bf2 \
                 pylib-2.3.4.zip mods/bf2/Levels/dalian_plant; do
            [ -e "$content/$f" ] || die "$content is not a Battlefield 2 1.50 folder: $f is missing"
        done
        case "$bundle/" in "$content/"*) die 'copy the bundle outside the game installation';; esac
        case "$content/" in "$bundle/"*) die 'the content folder must not be inside the bundle';; esac
        pf "content (read-only): $content"
        if [ "$(/usr/bin/stat -f %d "$content")" != "$(/usr/bin/stat -f %d "$bundle")" ]; then
            pf 'note: content and bundle are on different volumes: every run copies the content twice'
            pf '      (several GB) instead of cloning it. Move the bundle to the same volume.'
        fi
        pf "free space: $(/bin/df -h "$bundle" | tail -1 | awk '{print $4}')"
    fi
    cat "$log" >> "$summary"
    echo "preflight OK (results/preflight.txt)${content:+; content: $content}${py:+; python: $py}"
}

# ---- checks -----------------------------------------------------------------------------------
failures=0
# step NAME EXPECTED_EXIT TIMEOUT_SECONDS DIR COMMAND...  (log: results/checks/NAME.log)
step() {
    local name=$1 expect=$2 limit=$3 dir=$4; shift 4
    local log="$out/checks/$name.log" rc
    ( cd "$dir" && exec "$@" ) > "$log" 2>&1 &
    local pid=$! waited=0
    while kill -0 "$pid" 2>/dev/null && [ "$waited" -lt "$limit" ]; do sleep 1; waited=$((waited + 1)); done
    if kill -0 "$pid" 2>/dev/null; then
        kill -TERM "$pid" 2>/dev/null; sleep 2; kill -KILL "$pid" 2>/dev/null
        wait "$pid" 2>/dev/null; rc=timeout
    else
        wait "$pid"; rc=$?
    fi
    local verdict=PASS
    [ "$rc" = "$expect" ] || { verdict=FAIL; failures=$((failures + 1)); }
    say "== $verdict $name: exit $rc (expected $expect)"
    tail -12 "$log" | sed 's/^/   | /' >> "$summary"
}

crash_stack() {   # print the triggered thread of an .ips crash report (needs python; optional)
    local py3=${py:-}
    [ -n "$py3" ] || { /usr/bin/xcode-select -p >/dev/null 2>&1 && py3=/usr/bin/python3; }
    [ -n "$py3" ] || { echo "     (no python3: open $(basename "$1") in Console)"; return; }
    "$py3" -I - "$1" 2>&1 <<'PY'
import json, sys
j = json.loads(open(sys.argv[1]).read().split("\n", 1)[1]); e = j.get("exception", {})
print("    ", e.get("type"), e.get("signal"), e.get("subtype", ""))
th = [t for t in j["threads"] if t.get("triggered")][0]; im = j["usedImages"]
for f in th["frames"][:20]:
    n = im[f["imageIndex"]].get("name", "?") if f.get("imageIndex") is not None else "?"
    print("     %-18s %s +%s" % (n, f.get("symbol", "0x%x" % f.get("imageOffset", 0)), f.get("symbolLocation", "")))
PY
}

checks() {
    rm -rf "$out/checks"; mkdir -p "$out/checks"
    local marker="$out/checks/.start" b="$bundle/bin"; : > "$marker"
    say "---- checks ($(date -u '+%H:%M:%S UTC'))"
    step dice_py_check 0 120 "$bundle" "$b/dice_py_check" "$bundle/check-lib"
    step aidll_loader 0 60 "$b" ./bf2_aidll_loader_check
    # The runtime half of the vtable audit gate (the Intel build host only runs the static half).
    step native_vtable_check 0 60 "$b" ./bf2_native_vtable_check
    step meme_dispatch_check 0 60 "$b" ./bf2_meme_dispatch_check
    step gfx_forward 0 60 "$b" ./macos_gfx_forward
    step metal_cpu 0 60 "$b" ./macos_metal_device --cpu
    step metal_gpu 0 120 "$b" /usr/bin/env MTL_DEBUG_LAYER=1 ./macos_metal_device
    # --frontier was the fixed-function vertex abort; since 0.3.0 piece 1 it checks the D3D9 answer.
    step metal_frontier 0 60 "$b" ./macos_metal_device --frontier
    # The remaining frontier mode must print its diagnostic and abort (SIGABRT = 134), never succeed.
    step metal_state-frontier 134 60 "$b" ./macos_metal_device --state-frontier
    sleep 10   # ReportCrash lags a few seconds
    /usr/bin/find "$HOME/Library/Logs/DiagnosticReports" -newer "$marker" \
        \( -name 'bf2*.ips' -o -name 'macos_*.ips' -o -name 'dice_py*.ips' \) -exec cp {} "$out/crash/" \; 2>/dev/null
    say "checks: $failures failed; crash reports: $(ls "$out/crash" | wc -l | tr -d ' ') (the metal state-frontier abort is expected)"
    for f in "$out"/crash/bf2*.ips "$out"/crash/dice_py*.ips; do
        [ -f "$f" ] || continue
        say "   $(basename "$f"):"; crash_stack "$f" >> "$summary"
    done
    rm -f "$marker"
}

# ---- loopback runs ----------------------------------------------------------------------------
loop_status=0
interrupted=0
loopback() {   # loopback NAME OPTIONS...: one harness run, then its summary
    local name=$1; shift
    local log="$out/$name.harness.log" start="$out/.$name.start" rc run
    local cmd=("$py" -I "$bundle/tools/macos/loopback/loopback.py" --build $tag --content "$content" "$@" ${extra[@]+"${extra[@]}"})
    say "---- $name ($(date -u '+%H:%M:%S UTC'))"
    say "   $(printf '%q ' "${cmd[@]}")"
    if [ "$dry" = 1 ]; then say '   (dry run: not started)'; return 0; fi
    : > "$start"
    trap 'interrupted=1' INT
    ( cd "$bundle" && exec "${cmd[@]}" ) 2>&1 | tee -i "$log"
    rc=${PIPESTATUS[0]}
    trap - INT
    run=$(sed -n 's/^Run artifacts: //p' "$log" | tail -1)
    if [ -z "$run" ]; then
        run=$(/usr/bin/find "$bundle/out/loopback" -maxdepth 1 -name 'run-*' -newer "$start" 2>/dev/null | sort | tail -1)
    fi
    # An interrupted harness leaves its server behind: stop it the way the harness does.
    if [ -n "$run" ] && [ -f "$run/server.command.txt" ] && [ ! -f "$run/server.result" ]; then
        : > "$run/server.stop"
        for _ in $(seq 45); do [ -f "$run/server.result" ] && break; sleep 1; done
        /usr/bin/pkill -KILL -f "$bundle/out/build/$tag/bin_macos/bf2( |$)" 2>/dev/null
        say "   the harness stopped early (exit $rc): server stopped"
    fi
    rm -f "$start"
    echo "${run:-}" > "$out/$name.run"
    summarize "$name" "$rc" "$run"
}

# Gate facts for one run, written inside summarize's subshell. Frontier and crash counts are over
# the whole run directory, the way tools/tracks/rend_gate.sh counts them.
gate_details() {   # gate_details NAME RUN_DIR
    local name=$1 run=$2 sc=${1#gate-} n ips
    n=$(grep -rh '\[FRONTIER\]' "$run" 2>/dev/null | wc -l | tr -d ' ')
    ips=$(/usr/bin/find "$run" -name '*.ips' | wc -l | tr -d ' ')
    echo "   gate: frontier lines $n, crash reports $ips"
    local cexit
    cexit=$(sed -n 's/.*exit=\([-0-9]*\).*/\1/p' "$run/client.result" 2>/dev/null)
    case "$sc" in
        map2) if grep -q 'timeout=True' "$run/client.result" 2>/dev/null; then
                  echo "   script finished: not applicable (the run ends on its client time limit)"
              else echo "   script finished: NO (the client exited with exit=${cexit:-?} before its time limit)"; fi;;
        *) if grep -q 'timeout=True' "$run/client.result" 2>/dev/null; then
               echo "   script finished: NO (the client hit its time limit)"
           elif [ "${cexit:-x}" != 0 ]; then
               echo "   script finished: NO (the client exited with exit=${cexit:-?})"
           else echo "   script finished: yes"; fi;;
    esac
    case "$sc" in render-view-*)
        local ref="$bundle/reference/baselines/$sc"
        if [ ! -d "$ref" ]; then
            echo "   no Intel reference views in this bundle (reference/baselines/$sc): nothing to compare"
        else
            echo "   view comparison against the Intel reference (mean |diff| 0-255 after +-4 px alignment;"
            echo "   'FAIL' means over the Intel limit: information only, the GPU differs):"
            "$py" -I "$bundle/tools/macos/loopback/compare_views.py" "$ref" "$run" --scenario "$sc" \
                --all-pngs --out "$out/compare/$sc" 2>&1 | sed 's/^/     /'
            echo "   compare PNGs (reference | M2 | heatmap) for every view: results/compare/$sc/"
        fi;;
    esac
}

summarize() {   # summarize NAME HARNESS_EXIT RUN_DIR
    local name=$1 rc=$2 run=$3 s="$out/$name.summary.txt" verdict=PASS
    (
        echo "== $name: harness exit $rc, run $(basename "${run:-none}")"
        [ -n "$run" ] && [ -d "$run" ] || { echo "   no run directory"; exit 0; }
        local cr sr
        cr=$(cat "$run/client.result" 2>/dev/null || echo 'no client.result')
        sr=$(cat "$run/server.result" 2>/dev/null || echo 'no server.result')
        echo "   client: $cr"
        echo "   server: $sr"
        [ -f "$bundle/reference/intel-client.result" ] && echo "   Intel reference run client (one run, not per scenario): $(cat "$bundle/reference/intel-client.result")"
        local lines=0
        [ -f "$run/client.log" ] && lines=$(grep -c '\[INPUT-SCRIPT\]' "$run/client.log")
        [ -f "$run/input-script.txt" ] && echo "   input script: $lines report lines (Intel gate: $(cat "$bundle/reference/intel-input-script-lines.txt" 2>/dev/null || echo '?'))"
        echo "   present PNGs: $(ls "$run/present" 2>/dev/null | grep -c '\.png$')"
        echo "   crash reports: $(ls "$run" | grep -c '\.ips$')"
        for f in "$run"/*.ips; do [ -f "$f" ] && { echo "   $(basename "$f"):"; crash_stack "$f"; }; done
        grep -h -o '\[FRONTIER\].*' "$run/client.log" "$run/server.pty.log" 2>/dev/null \
            | sed 's/[[:space:]]*$//' | LC_ALL=C sort -u > "$out/$name.frontiers.txt"
        echo "   frontiers reached: $(wc -l < "$out/$name.frontiers.txt" | tr -d ' ')"
        sed 's/^/     /' "$out/$name.frontiers.txt" | cut -c1-200
        if [ -f "$bundle/reference/intel-frontiers.txt" ]; then
            local new
            new=$(grep -v '^#' "$bundle/reference/intel-frontiers.txt" | LC_ALL=C comm -13 - "$out/$name.frontiers.txt")
            echo "   not in the Intel gate run: $( [ -n "$new" ] && echo "$new" | wc -l | tr -d ' ' || echo 0)"
            [ -n "$new" ] && echo "$new" | sed 's/^/     NEW /' | cut -c1-200
        fi
        # arm64 watch list (docs/macos-c-arm64-client.md): x87 emulation, conversions, dispatch.
        echo "   watch-list lines (ExtendedFloat, bf2x87, SIGFPE, pure virtual, EXC_):"
        grep -h -n -i -E 'ExtendedFloat|bf2x87|SIGFPE|floating point exception|pure virtual|EXC_(BAD|ARITH)' \
            "$run/client.log" "$run/server.pty.log" 2>/dev/null | head -10 | cut -c1-200 | sed 's/^/     /'
        echo "   client.log tail:"
        tail -12 "$run/client.log" 2>/dev/null | cut -c1-200 | sed 's/^/     | /'
        case "$name" in gate-*) gate_details "$name" "$run";; esac
    ) > "$s" 2>&1
    case "$name" in gate-*)
        # [FRONTIER] lines, crash reports and an unfinished scenario script make the gate CHECK;
        # the view numbers never do.
        grep -q '   gate: frontier lines 0, crash reports 0' "$s" || verdict=CHECK
        grep -q 'script finished: NO' "$s" && verdict=CHECK
        grep -q '^   client: no client.result' "$s" && verdict=CHECK
        [ "$verdict" = PASS ] || loop_status=1
        sed -i '' "1s/^== /== $verdict /" "$s" ;;
    esac
    case "$name" in smoke*)
        grep -q 'timeout=True' "$run/client.result" 2>/dev/null || verdict=CHECK
        grep -q '^EXIT' "$run/server.result" 2>/dev/null || verdict=CHECK
        ls "$run" 2>/dev/null | grep -q '\.ips$' && verdict=CHECK
        grep -q '     NEW ' "$s" && verdict=CHECK
        [ "$verdict" = PASS ] || loop_status=1
        sed -i '' "1s/^== /== $verdict /" "$s" ;;
    esac
    cat "$s" | tee -a "$summary"
}

gate() {
    local sc
    # The arm64 checks first (no content, a few minutes): they isolate ABI faults such as the
    # member-pointer encoding before the scenarios run.
    checks
    for sc in infantry-range render-view-dalian render-view-ocs; do
        [ "$interrupted" = 1 ] || loopback "gate-$sc" --scenario "$sc"
    done
    [ "$interrupted" = 1 ] || loopback gate-map2 --ai --bots 16 --ai-spawn-only --spawn-time 1 --client-seconds 300 \
        --map2 operation_clean_sweep --server-send 120:admin.runNextLevel --input-script play-combat
}
smoke()    { loopback smoke --ai --bots 16 --ai-spawn-only --input-script play-combat --client-seconds 200; }
smoke_ai() { loopback smoke-ai --ai --bots 16 --input-script play-combat --client-seconds 200; }
play() {
    say "---- hand play: about $minutes minutes. The window opens once Dalian has loaded; play, then"
    say "     leave through the in-game menu (Esc > Quit) or let the time run out. Do not close Terminal."
    loopback play --ai --bots 16 --client-seconds $((minutes * 60 + 300)) --present-png --present-every 3600 \
        --client-env BF2_PRESENT_PNG_MAX=20 --client-env BF2_PLAY_TRACE=1
}

# ---- results.tgz ------------------------------------------------------------------------------
collect() {
    local list="$out/.files" run base
    (cd "$bundle" && {
        printf '%s\0' results.txt artifacts.txt
        find results -type f ! -name '.files' -print0
        for run in out/loopback/run-*; do
            [ -d "$run" ] || continue
            find "$run" -maxdepth 1 -type f -print0
            [ -d "$run/present" ] && find "$run/present" -type f -print0
            # Profiles, logs and settings the game wrote; not its shader cache (.cfx), compiled
            # game Python (.pyc) or the generated local ProtectedData key.
            [ -d "$run/client-home" ] && find "$run/client-home" -type f -size -20M \
                ! -path '*/cache/*' ! -name '*.cfx' ! -name '*.pyc' ! -name '*.key' -print0
            # Files the game wrote inside its content clones (logs, saved settings).
            for base in server client; do
                [ -f "$run/$base.command.txt" ] && [ -d "$run/$base-root" ] && \
                    find "$run/$base-root" -type f -newer "$run/$base.command.txt" -size -20M \
                        ! -path '*/cache/*' ! -name '*.cfx' ! -name '*.pyc' ! -name '*.key' -print0
            done
        done
    }) > "$list"
    (cd "$bundle" && COPYFILE_DISABLE=1 /usr/bin/tar --null -T "$list" -czf results.tgz)
    rm -f "$list"
    echo "Wrote $bundle/results.tgz ($(du -h "$bundle/results.tgz" | cut -f1)): AirDrop this one file back."
}

# ---- main -------------------------------------------------------------------------------------
say "==== run.sh $mode  $(date -u)  [TEMP] [CHOSEN, not recovered] OpenBF2 arm64 M2 bundle"
preflight
case "$mode" in
    preflight) cat "$out/preflight.txt" ;;
    checks) checks ;;
    gate) gate ;;
    smoke) smoke ;;
    smoke-ai) smoke_ai ;;
    play) play ;;
    all) checks; smoke; [ "$interrupted" = 1 ] || smoke_ai ;;
esac
[ "$dry" = 1 ] || collect
say "==== done: check failures $failures, loopback verdict $( [ "$loop_status" = 0 ] && echo PASS || echo CHECK )"
[ "$failures" = 0 ] && [ "$loop_status" = 0 ]
