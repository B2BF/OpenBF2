#!/usr/bin/expect -f
# [TEMP] [CHOSEN, not recovered] c-loopback: drive the sandboxed dedicated server in a PTY.
# Derived from Code/Tests/s2_10_threads/smoke.tcl. Arguments: <run dir> <server binary> <extra
# args...>. Writes <run>/server.playing once the status monitor shows "playing", keeps draining
# the PTY while <run>/server.stop is absent, then types quit and waits for a clean exit.
set run [lindex $argv 0]
set binary [lindex $argv 1]
set extra [lrange $argv 2 end]
# loopback.py --mod: LB_MOD names the mod (default bf2).
set mod [expr {[info exists env(LB_MOD)] ? $env(LB_MOD) : "bf2"}]
set env(TERM) xterm
set env(LINES) 40
set env(COLUMNS) 120
cd "$run/server-root"
log_file -noappend "$run/server.pty.log"
log_user 1
# loopback.py --name-redirect: LB_WRAP holds the words placed before the binary, one per line.
set wrap {}
if {[info exists env(LB_WRAP)]} { set wrap [split $env(LB_WRAP) "\n"] }
spawn /usr/bin/sandbox-exec -D OWNER_HOME=$env(LB_OWNER_HOME) -D RUN_OUT=$env(LB_RUN_OUT) \
    -D GAME_ROOT=$env(LB_GAME_ROOT) -f "$run/server.sb" {*}$wrap $binary +modPath mods/$mod \
    +config smoke_cfg/ServerSettings.con +mapList smoke_cfg/maplist.con +port 16890 {*}$extra
set child [exp_pid]
proc stopFailed {message} {
    global child run
    set f [open "$run/server.result" w]; puts $f "FAIL $message"; close $f
    catch {exec /bin/kill -TERM $child}
    exit 1
}
set timeout [expr {[info exists env(LB_SERVER_LOAD_SECONDS)] ? $env(LB_SERVER_LOAD_SECONDS) : 240}]
expect {
    -re {Status:[^\r\n]*playing} {}
    eof { stopFailed "server exited before playing" }
    timeout { stopFailed "map load timed out" }
}
set f [open "$run/server.playing" w]; puts $f [clock milliseconds]; close $f
set timeout 1
while {![file exists "$run/server.stop"]} {
    # [TEMP] loopback.py --server-send: type each line of server.send at the console, then drop it.
    if {[file exists "$run/server.send"]} {
        set sf [open "$run/server.send" r]; set lines [split [read $sf] "\n"]; close $sf
        file delete "$run/server.send"
        foreach line $lines { if {$line ne ""} { send -- "$line\r" } }
    }
    expect {
        eof { stopFailed "server exited while the client ran" }
        timeout {}
        -re {.+} {}
    }
}
send -- "quit\r"
set timeout 30
expect {
    eof {}
    timeout { stopFailed "server did not quit within 30 seconds" }
    -re {.+} { exp_continue }
}
set result [wait]
set f [open "$run/server.result" w]; puts $f "EXIT $result"; close $f
