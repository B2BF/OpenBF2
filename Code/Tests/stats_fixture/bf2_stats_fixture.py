#!/usr/bin/env python3
"""SCAFFOLDING [TEMP] (Code/Tests, 2026-09-28) - a LOCAL stand-in for BF2's stats web service.

It exists so the rebuilt client's stats LOADING (BFHQ, the end-of-round rank, ...) can be tested with
chosen numbers while no ranked server feeds the real service. It listens on 127.0.0.1 only, keeps no
state, writes nothing, and talks to no other host.

Point a locally built client (or dedicated server) at it with +gamespy <test domain> and the
test-only name redirect (Code/Tests/name_redirect): the stats host BF2Web.<test domain> resolves to
127.0.0.1, and OPENBF2_TEST_REDIRECT_HTTP_PORT=<port> moves its port 80 to this fixture.

What it answers - the three getplayerinfo.aspx queries SwiffHost::fetchPlayerStats (BF2.exe 0x49D450)
makes, in the layout KeyParser::parse (0x61DAF0) reads:

    O
    H   asof            <- the parser skips these four tokens
    D   <unix time>
    H   key key ...     <- ONE header row: the key map the client reads as map 0
    D   val val ...
    $   <n>   $         <- n is not checked by the client; the non-whitespace character count

  1. the main query (info=per*,cmb*,...): every key the client reads, by name (case-insensitive);
  2. the favourites query (info=ktm-,vtm-,wtm-,mtm-&kit=&vehicle=&weapon=&map=): READ BY POSITION -
     getKey(0, 2..5) - so the order is exactly pid, nick, ktm-K, vtm-V, wtm-W, mtm-M;
  3. the maps query (info=mtm-,mwn-,mls-): the 31 map ids the client walks.

Since 2026-09-29 also getawardsinfo.aspx (a veteran's medals, badges and ribbons, ids from the stock
awards.con; --no-awards for none) and getleaderboard.aspx (every board, --lb-size players, the fixture's
own player at --lb-position), for the BFHQ awards and leaderboard pages.

Every other endpoint gets what BF2Web.openspy.net answers today ("E 404"), so the client behaves as it
does against the real service for them.

⚠ The client's tokenizer skips RUNS of tabs, so an EMPTY value would shift every later value onto the
wrong key. A key whose value would be empty is left out instead (the client reads a missing key as "").

Example - 20 score, and 5 kills / 6 deaths with the assault rifle (weapon 0):
    python bf2_stats_fixture.py --score 20 --weapon 0 --kills 5 --deaths 6
"""
import argparse
import http.server
import math
import time
import urllib.parse

WEAPONS, VEHICLES, ARMIES, KITS = 19, 13, 10, 7
# The shipped order of the map ids the third query is read for (SwiffHost.cpp kStatsMapIds).
MAP_IDS = [0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 100, 101, 102, 103, 105, 110, 200, 201, 202,
           300, 301, 302, 303, 304, 305, 306, 307, 601, 602, 603, 604]


def ratio(kills, deaths):
    """A kill:death ratio as the stats service writes it - reduced, e.g. 10/12 -> "5:6"."""
    g = math.gcd(kills, deaths) or 1
    return "%d:%d" % (kills // g, deaths // g)


def body(keys, values):
    """The KeyParser layout for one header row and one data row. Empty values are dropped with
    their key (see the module note)."""
    pairs = [(k, str(v)) for k, v in zip(keys, values) if str(v) != ""]
    for k, v in pairs:
        if "\t" in v or "\n" in v:
            raise ValueError("a value may not contain a tab or newline: %r" % v)
    lines = ["O",
             "H\tasof",
             "D\t%d" % int(time.time()),
             "H\t" + "\t".join(k for k, _ in pairs),
             "D\t" + "\t".join(v for _, v in pairs)]
    text = "\n".join(lines) + "\n"
    count = len(text.replace("\t", "").replace("\n", ""))
    return text + "$\t%d\t$\n" % count


NOT_FOUND = "E\t404\n$\t4\t$\n"          # what BF2Web.openspy.net answers for an unknown player


def table(head_keys, head_values, keys, rows):
    """The KeyParser layout for one leading header row and a table of data rows (the awards and
    leaderboard answers). No value may be empty (see the module note)."""
    lines = ["O",
             "H\t" + "\t".join(head_keys),
             "D\t" + "\t".join(str(v) for v in head_values),
             "H\t" + "\t".join(keys)]
    for row in rows:
        values = [str(v) for v in row]
        if any(v == "" or "\t" in v or "\n" in v for v in values):
            raise ValueError("a table value may not be empty or contain a tab/newline: %r" % (values,))
        lines.append("D\t" + "\t".join(values))
    text = "\n".join(lines) + "\n"
    count = len(text.replace("\t", "").replace("\n", ""))
    return text + "$\t%d\t$\n" % count


# ---- getawardsinfo.aspx (added 2026-09-29) --------------------------------------------------------
# PlayerAwards::getAwards asks "getawardsinfo.aspx?pid=<pid>" and parses with KeyParser type Stats:
# the four tokens after "O" are skipped (this fixture's "H asof / D <time>"), then ONE header row
# "award level when first" and one data row per award, read by name. The ids are awards.con's (the
# stock Menu_client.zip catalogue the menu registers through swiffHost.addAward): a badge is <= 2000000
# and carries its level 1..3, a medal is 2000001..3000000, a ribbon is above 3000000.
VETERAN_AWARDS = [
    # award, level, days ago (when), days ago (first)
    (2021403, 1, 3, 3),     # medal
    (2020719, 2, 10, 40),   # medal, earned twice
    (2191319, 1, 20, 20),   # medal
    (1031406, 3, 1, 30),    # badge at level 3
    (1031619, 2, 5, 25),    # badge at level 2
    (1031119, 1, 12, 12),   # badge at level 1
    (3191305, 1, 2, 2),     # ribbon
    (3190605, 1, 8, 8),     # ribbon
    (3241213, 1, 15, 15),   # ribbon
]


def awards_answer(a, pid):
    now = int(time.time())
    rows = [] if a.no_awards else [(award, level, now - when * 86400, now - first * 86400)
                                   for award, level, when, first in VETERAN_AWARDS]
    return table(["asof"], [now], ["award", "level", "when", "first"], rows)


# ---- getleaderboard.aspx (added 2026-09-29) --------------------------------------------------------
# LeaderBoard asks "getleaderboard.aspx?<type query>&pos=<p>&before=0&after=19" (the global boards) or
# "getleaderboard.aspx?pid=<pid>&<type query>" (one player's row) and parses with KeyParser type Type2:
# the fifth and sixth tokens after "O" are the board's size and its update time ("H size asof /
# D <size> <asof>"), then one header row and the data rows, read by name. SwiffHost::fetchLeaderboard
# (BF2.exe 0x4A0330) reads these keys per board:
LEADERBOARD_KEYS = {
    ("score", "overall"):   ["score", "totaltime"],
    ("risingstar", None):   ["weeklyscore", "totaltime", "date"],
    ("score", "commander"): ["coscore", "cotime"],
    ("score", "team"):      ["teamscore", "totaltime"],
    ("score", "combat"):    ["score", "totalkills", "totaltime"],
    ("kit", "*"):           ["killswith", "deathsby", "timeplayed"],
    # The client reads "detahsby" [sic] for the next two, so their "deaths by" column shows 0 even
    # though the service sends deathsby - as it did against the real service.
    ("vehicle", "*"):       ["killswith", "deathsby", "timeused"],
    ("weapon", "*"):        ["killswith", "deathsby", "timeused", "accuracy"],
}
COUNTRIES = ["US", "GB", "DE", "SE", "FR", "NL", "CA", "AU", "BE", "NO"]


def leaderboard_row(a, n, key_names, me_pid):
    """Row n (1-based) of any board: deterministic numbers, the fixture's own player at --lb-position."""
    mine = n == a.lb_position
    pid = me_pid if mine else 40000000 + n
    nick = a.nick if mine else "Player%03d" % n
    values = []
    for k in key_names:
        if k == "weeklyscore":
            values.append("%.4f" % (99.5 - n * 0.37))
        elif k == "date":
            values.append(time.strftime("%Y-%m-%d", time.gmtime(time.time() - n * 3600)))
        elif k in ("totaltime", "cotime", "timeplayed", "timeused"):
            values.append(360000 - n * 1234)
        elif k == "accuracy":
            values.append(max(1, 60 - n % 50))
        else:
            values.append(max(0, 100000 - n * 997))
    return [n, pid, nick] + values + [n % 22, COUNTRIES[n % len(COUNTRIES)]]


def leaderboard_answer(a, q):
    kind = q.get("type", "")
    ident = q.get("id")
    keys = LEADERBOARD_KEYS.get((kind, ident)) or LEADERBOARD_KEYS.get((kind, "*" if ident is not None else None))
    if keys is None:
        return NOT_FOUND
    now = int(time.time())
    names = ["n", "pid", "nick"] + keys + ["playerrank", "countrycode"]
    me_pid = int(q.get("pid", 0) or 0) or a.lb_pid
    if "pid" in q:
        rows = [leaderboard_row(a, a.lb_position, keys, me_pid)]
    else:
        pos = max(1, int(q.get("pos", 1) or 1))
        after = int(q.get("after", 19) or 19)
        rows = [leaderboard_row(a, n, keys, me_pid) for n in range(pos, min(a.lb_size, pos + after) + 1)]
    return table(["size", "asof"], [a.lb_size, now - 1800], names, rows)


class Profile:
    def __init__(self, a):
        self.a = a

    def main_query(self, pid):
        a = self.a
        now = int(time.time())
        minutes = max(a.weapon_time / 60.0, 1.0)
        v = {}
        v["pid"] = pid
        v["nick"] = a.nick
        v["scor"] = a.score
        v["jond"] = now - 7 * 86400
        v["lbtl"] = now
        v["wins"] = a.wins
        v["loss"] = a.losses
        v["rank"] = a.rank
        v["kill"] = a.kills
        v["deth"] = a.deaths
        v["suic"] = 0
        v["ospm"] = "%.2f" % (a.score / minutes)
        v["klpm"] = "%.2f" % (a.kills / minutes)
        v["dtpm"] = "%.2f" % (a.deaths / minutes)
        v["klpr"] = "%.2f" % a.kills
        v["dtpr"] = "%.2f" % a.deaths
        v["bksk"] = a.kills
        v["wdsk"] = a.deaths
        v["bbrs"] = a.score
        v["osaa"] = a.accuracy
        for key in ("twsc", "cpcp", "cacp", "dfcp", "kila", "heal", "rviv", "rsup", "rpar", "tgte",
                    "dkas", "dsab", "cdsc", "cmsc", "kick", "ban", "vrk", "mvks", "vmks", "mvrs",
                    "vmrs", "de-6", "de-7", "de-8"):
            v[key] = 0
        v["tsql"] = 0
        v["tsqm"] = 0
        v["tlwf"] = a.weapon_time
        v["tcdr"] = 0
        v["fkit"] = a.kit
        v["fmap"] = a.map
        v["fveh"] = a.vehicle
        v["fwea"] = a.weapon
        for w in range(WEAPONS):
            mine = w == a.weapon
            v["wtm-%d" % w] = a.weapon_time if mine else 0
            v["wkl-%d" % w] = a.kills if mine else 0
            v["wdt-%d" % w] = a.deaths if mine else 0
            v["wac-%d" % w] = a.accuracy if mine else 0
            v["wkd-%d" % w] = ratio(a.kills, a.deaths) if mine else "0:0"
        for i in range(VEHICLES):
            v["vtm-%d" % i] = 0
            v["vkl-%d" % i] = 0
            v["vdt-%d" % i] = 0
            v["vkd-%d" % i] = "0:0"
            v["vkr-%d" % i] = 0
        for i in range(ARMIES):
            v["atm-%d" % i] = a.weapon_time if i == a.army else 0
            v["awn-%d" % i] = a.wins if i == a.army else 0
            v["alo-%d" % i] = a.losses if i == a.army else 0
            v["abr-%d" % i] = a.score if i == a.army else 0
        for i in range(KITS):
            mine = i == a.kit
            v["ktm-%d" % i] = a.weapon_time if mine else 0
            v["kkl-%d" % i] = a.kills if mine else 0
            v["kdt-%d" % i] = a.deaths if mine else 0
            v["kkd-%d" % i] = ratio(a.kills, a.deaths) if mine else "0:0"
        return body(list(v.keys()), list(v.values()))

    def favourites_query(self, pid, kit, vehicle, weapon, gamemap):
        a = self.a
        keys = ["pid", "nick", "ktm-%d" % kit, "vtm-%d" % vehicle, "wtm-%d" % weapon, "mtm-%d" % gamemap]
        values = [pid, a.nick,
                  a.weapon_time if kit == a.kit else 0,
                  0,
                  a.weapon_time if weapon == a.weapon else 0,
                  a.weapon_time if gamemap == a.map else 0]
        return body(keys, values)

    def maps_query(self, pid):
        a = self.a
        keys, values = ["pid", "nick"], [pid, a.nick]
        for m in MAP_IDS:
            keys += ["mtm-%d" % m, "mwn-%d" % m, "mls-%d" % m]
            values += [a.weapon_time if m == a.map else 0,
                       a.wins if m == a.map else 0,
                       a.losses if m == a.map else 0]
        return body(keys, values)


def make_handler(profile):
    class Handler(http.server.BaseHTTPRequestHandler):
        server_version = "BF2StatsFixture/1"

        def do_GET(self):
            url = urllib.parse.urlsplit(self.path)
            name = url.path.rsplit("/", 1)[-1].lower()
            q = {k: v[0] for k, v in urllib.parse.parse_qs(url.query, keep_blank_values=True).items()}
            answer = NOT_FOUND
            if name == "getplayerinfo.aspx":
                pid = q.get("pid", "0")
                info = q.get("info", "")
                if info.startswith("ktm-,vtm-,wtm-,mtm-"):
                    answer = profile.favourites_query(pid, int(q.get("kit", 0)), int(q.get("vehicle", 0)),
                                                      int(q.get("weapon", 0)), int(q.get("map", 0)))
                elif info.startswith("mtm-,mwn-,mls-"):
                    answer = profile.maps_query(pid)
                else:
                    answer = profile.main_query(pid)
            elif name == "getawardsinfo.aspx":
                answer = awards_answer(profile.a, q.get("pid", "0"))
            elif name == "getleaderboard.aspx":
                answer = leaderboard_answer(profile.a, q)
            data = answer.encode("ascii")
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            print("[fixture] %s -> %s" % (self.path[:160], "404" if answer is NOT_FOUND else "%d bytes" % len(data)),
                  flush=True)

        def log_message(self, fmt, *args):    # the one line above is enough
            pass

    return Handler


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--port", type=int, default=18123)
    p.add_argument("--nick", default="StatsFixture")
    p.add_argument("--score", type=int, default=20, help="global score (scor)")
    p.add_argument("--rank", type=int, default=0, help="rank id (0 = Private)")
    p.add_argument("--weapon", type=int, default=0, help="weapon index 0..18 (0 = assault rifle)")
    p.add_argument("--kills", type=int, default=5)
    p.add_argument("--deaths", type=int, default=6)
    p.add_argument("--accuracy", type=int, default=20, help="percent")
    p.add_argument("--weapon-time", type=int, default=300, help="seconds with the weapon")
    p.add_argument("--kit", type=int, default=1, help="favourite kit 0..6 (1 = assault)")
    p.add_argument("--vehicle", type=int, default=0, help="favourite vehicle category 0..12")
    p.add_argument("--map", type=int, default=0, help="favourite map id (0 = Kubra Dam)")
    p.add_argument("--army", type=int, default=0, help="army 0..9 (0 = USMC)")
    p.add_argument("--wins", type=int, default=0)
    p.add_argument("--losses", type=int, default=0)
    p.add_argument("--no-awards", action="store_true", help="answer getawardsinfo with no awards")
    p.add_argument("--lb-size", type=int, default=57, help="players on every leaderboard")
    p.add_argument("--lb-position", type=int, default=42, help="the fixture player's position on every board")
    p.add_argument("--lb-pid", type=int, default=1, help="the fixture player's id on the global boards")
    p.add_argument("--selftest", action="store_true", help="print the answers and exit")
    a = p.parse_args()
    profile = Profile(a)
    if a.selftest:
        print(profile.main_query("1"))
        print(profile.favourites_query("1", a.kit, a.vehicle, a.weapon, a.map))
        print(profile.maps_query("1"))
        print(awards_answer(a, "1"))
        print(leaderboard_answer(a, {"type": "kit", "id": "2", "pos": "41", "before": "0", "after": "19"}))
        print(leaderboard_answer(a, {"pid": "1", "type": "risingstar"}))
        return
    server = http.server.ThreadingHTTPServer(("127.0.0.1", a.port), make_handler(profile))
    print("[fixture] BF2 stats fixture on http://127.0.0.1:%d/ASP/  (score %d, weapon %d: %d kills / %d deaths)"
          % (a.port, a.score, a.weapon, a.kills, a.deaths), flush=True)
    print("[fixture] start the client with +gamespy test.openbf2 and the test name redirect "
          "(OPENBF2_TEST_REDIRECT_HTTP_PORT=%d)  - Ctrl+C stops" % a.port, flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
