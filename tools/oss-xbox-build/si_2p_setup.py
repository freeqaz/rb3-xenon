#!/usr/bin/env python3
"""si_2p_setup.py — headlessly drive RB3 (RB3DX TU5 + RB3Enhanced) on the devkit
into 2-player SAME-INSTRUMENT gameplay: P1 guitar EXPERT, P2 guitar EASY.

Hardware-validated 2026-07-15 (console 192.168.8.180, RB3E HTTP :21070).
No controllers/instruments need to be connected; both players run on autoplay.

Usage:
    python3 si_2p_setup.py                 # full run (idempotent-ish; see notes)
    python3 si_2p_setup.py --dry-run       # print every DTA call, send nothing
    python3 si_2p_setup.py --song <short>  # pick a song (default blitzkriegbop)
    python3 si_2p_setup.py --host <ip>

Validated sequence (each step readback-verified; see
docs/tools/DTA-NAVIGATION-NOTES.md for the full working/not-working ledger):

 1. wait for RB3E HTTP ({+ 1 1} -> 2)
 2. splash_screen -> {ui goto_screen main_hub_screen}
 3. {gamemode set_mode qp_coop}
 4. {setup_game <song> '' ((guitar expert 1) (guitar easy 1))}
      - HMX's own debug entry point (ui/global.dta): joins pads 0+1 into the
        session, sets track/controller/difficulty/autoplay/prefab chars,
        sets the song, auto-assigns slots. The prefab chars are REQUIRED -
        users without chars crash seldiff/gameplay load (0xC0000005 @ +0x70c).
 5. {ui goto_screen preloading_screen}; poll {preload_panel is_loaded} == 1
 6. {meta_performer select_random_venue}  - REQUIRED, and required HERE:
      without a venue BandDirector never becomes ReadyForMidiParsers and the
      game_screen transition hangs forever (black screen); selecting the venue
      BEFORE entering preloading crash-loops the main thread instead
      (0xC0000005 @0x82577f00). Preload first, venue second.
 7. {net_sync disable} + {ui goto_screen {gamemode get game_screen}}
      - entering preloading_screen out-of-flow means on_preload_ok never
        auto-fires; we run its body manually. NEVER goto elsewhere while the
        preload panel is active (crashes the main thread + wedges RB3E HTTP).
 8. poll {{ui current_screen} name} == game_screen, {game is_loaded} == 1
 9. clear the "reconnect controller" overshell pause (no real pads present):
      {set {var fake_controllers} 1}
      per user: {<user> set_controller_type <its connected_controller_type>}
      {overshell update_all}   -> slots revert from kState_ReconnectController
      (Track type stays guitar; controller_type is only the hardware match.)
10. verify: game is_playing 1, get_paused 0, num_active_players 2,
      song_ms advancing, P1 diff 3 / P2 diff 0, both track guitar.

SI hardware evidence on the XBDM notify stream at game load:
    [RB3E:MSG] same-instrument: watcher constructed for track 2   (x2)
    [RB3E:MSG] same-instrument: cloned gem DB for track 2 (claim 2)
"""
import argparse
import sys
import time
import urllib.parse
import urllib.request

DEF_HOST = "192.168.8.180"
DEF_PORT = 21070
DEF_SONG = "blitzkriegbop"
DEF_CONFIG = "guitar:expert,guitar:easy"

DIFF_NUM = {"easy": "0", "medium": "1", "hard": "2", "expert": "3"}


def parse_config(spec):
    """'drum:expert,drum:easy' -> [('drum','expert'), ('drum','easy')].
    Pad N is the Nth entry. Vocals are refused (SI safety rule)."""
    out = []
    for i, part in enumerate(spec.split(",")):
        part = part.strip()
        if not part:
            continue
        inst, _, diff = part.partition(":")
        inst, diff = inst.strip(), diff.strip().lower()
        if diff not in DIFF_NUM:
            raise ValueError(f"bad difficulty {diff!r} in {part!r}")
        if inst in ("vocals", "vocal", "mic"):
            raise ValueError("vocals are not allowed in SI test configs")
        out.append((inst, diff))
    if not (1 <= len(out) <= 4):
        raise ValueError("config must have 1..4 players")
    return out


def config_dta(players):
    return "(" + " ".join(f"({inst} {diff} 1)" for inst, diff in players) + ")"


class Console:
    def __init__(self, host, port, dry_run=False, timeout=20):
        self.base = f"http://{host}:{port}"
        self.dry = dry_run
        self.timeout = timeout

    @staticmethod
    def check_braces(script):
        """Refuse to send brace-unbalanced DTA: an unbalanced script wedges
        ALL RB3E networking (recovery = title relaunch)."""
        depth = 0
        for ch in script:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth < 0:
                    raise ValueError(f"unbalanced braces (early close): {script}")
        if depth != 0:
            raise ValueError(f"unbalanced braces (depth {depth}): {script}")

    def execute(self, script):
        self.check_braces(script)
        if self.dry:
            print(f"[dry-run] /execute {script}")
            return ""
        url = f"{self.base}/execute?script={urllib.parse.quote(script)}"
        with urllib.request.urlopen(url, timeout=self.timeout) as r:
            out = r.read().decode("utf-8", "replace")
        print(f">>> {script}\n    -> {out!r}")
        time.sleep(1)  # pace the main thread; rapid-fire probes are untested
        return out

    def probe(self, script):
        """execute() that treats a transient HTTP timeout as 'not yet' —
        the main thread can be slow to service scripts mid-load."""
        try:
            return self.execute(script)
        except (TimeoutError, OSError):
            print(f"[probe] timeout on {script} (loading?), will retry")
            return None

    def http_alive(self):
        if self.dry:
            return True
        try:
            return self.execute("{+ 1 1}") == "2"
        except Exception:
            return False

    def screen(self):
        return self.execute("{{ui current_screen} name}")


def wait_for(desc, fn, timeout_s, interval=5):
    print(f"[wait] {desc} (timeout {timeout_s}s)")
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if fn():
            print(f"[wait] {desc}: OK")
            return True
        time.sleep(interval)
    print(f"[wait] {desc}: TIMED OUT")
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", default=DEF_HOST)
    ap.add_argument("--port", type=int, default=DEF_PORT)
    ap.add_argument("--song", default=DEF_SONG)
    ap.add_argument("--config", default=DEF_CONFIG,
                    help="per-pad instrument:difficulty list, e.g. "
                         "'drum:expert,drum:easy' or "
                         "'guitar:expert,guitar:easy,guitar:medium' (max 4, "
                         "no vocals)")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    players = parse_config(args.config)
    npl = len(players)
    print(f"[config] {npl} player(s): {players} -> {config_dta(players)}")

    c = Console(args.host, args.port, dry_run=args.dry_run)

    # 0. reachability
    if not c.http_alive():
        print("FATAL: RB3E HTTP not answering. If wedged, recover with:\n"
              "  python3 xbdm_cmd.py %s 'magicboot title=\\Device\\Mass0\\Games"
              "\\rb3\\default.xex directory=\\Device\\Mass0\\Games\\rb3'\n"
              "(then wait ~90s; if the console lands in Aurora, re-issue the "
              "same magicboot — an interrupted one falls back to the dash)"
              % args.host)
        return 1

    # idempotence: already in gameplay with the target state? then no-op.
    if not args.dry_run:
        scr = c.screen()
        if scr == "game_screen":
            playing = c.execute("{game is_playing}") == "1"
            n = c.execute("{beatmatch num_active_players}")
            if playing and n == str(npl):
                print(f"[ok] already in {npl}-player gameplay; nothing to do")
                return 0
            print("WARNING: console already mid-game but not in target state; "
                  "relaunch the title first (magicboot) for a clean run.")
            return 1
        if scr not in ("splash_screen", "intro_movie_screen",
                       "main_hub_screen"):
            print(f"FATAL: unexpected screen {scr!r} — this recipe is only "
                  "safe from a fresh title boot. Relaunch RB3 (magicboot) "
                  "and re-run.")
            return 1
        # setup_game after a completed song crash-loops the main thread
        # (hardware-verified) — refuse if users are already participating.
        if c.execute("{{user_mgr get_user_from_pad_num 0} is_participating}") == "1":
            print("FATAL: users already participating (post-game state). "
                  "setup_game would crash the title. Relaunch RB3 first.")
            return 1

    # 1. get to main hub (from splash/intro this is safe; goto is ignored
    #    until the splash is ready, so retry)
    if not args.dry_run:
        def at_hub():
            s = c.screen()
            if s == "main_hub_screen":
                return True
            if s == "splash_screen":
                c.execute("{ui goto_screen main_hub_screen}")
            return False
        if not wait_for("main_hub_screen", at_hub, 180):
            return 1
    else:
        c.execute("{ui goto_screen main_hub_screen}")

    # 2. game mode + full player/song setup via HMX's debug entry point
    c.execute("{gamemode set_mode qp_coop}")
    c.execute("{setup_game %s '' %s}" % (args.song, config_dta(players)))

    if not args.dry_run:
        ok = True
        for pad, (inst, diff) in enumerate(players):
            got_d = c.execute(
                "{{user_mgr get_user_from_pad_num %d} get_difficulty}" % pad)
            got_t = c.execute(
                "{{user_mgr get_user_from_pad_num %d} get_track_sym}" % pad)
            if got_d != DIFF_NUM[diff] or got_t != inst:
                print(f"MISMATCH pad {pad}: want ({inst},{DIFF_NUM[diff]}) "
                      f"got ({got_t},{got_d})")
                ok = False
        if not ok:
            print("FATAL: setup_game readback mismatch")
            return 1

    # 2b. verify the song actually loaded. setup_game INTERMITTENTLY drops its
    #     song arg right after a fresh boot (song-cache readiness race) and lands
    #     on a random song; a silently-wrong song poisons every gem-count
    #     comparison downstream. setup_game's own final step is
    #     {meta_performer set_song <song>}, so re-issuing it + reading back is the
    #     fix. Slots in here (after setup_game, before preload); no goto_screen.
    if not args.dry_run:
        got = c.execute("{meta_performer song}")
        tries = 0
        while got != args.song and tries < 5:
            tries += 1
            print(f"WARNING: song readback {got!r} != requested "
                  f"{args.song!r} (retry {tries}/5); re-issuing set_song")
            c.execute("{meta_performer set_song %s}" % args.song)
            time.sleep(2)
            got = c.execute("{meta_performer song}")
        if got != args.song:
            print(f"FATAL: song still {got!r} after {tries} retries; refusing "
                  f"to continue — gem counts would be compared across the wrong "
                  f"song. Relaunch the title and re-run.")
            return 1
        print(f"[ok] song verified: {got!r}")

    # 3. preload the song. ORDER MATTERS (hardware-verified): entering
    #    preloading with the venue ALREADY selected crash-looped the main
    #    thread (0xC0000005 null read @0x82577f00); the validated order is
    #    preload first, venue after.
    time.sleep(2)
    c.execute("{ui goto_screen preloading_screen}")
    if not args.dry_run:
        time.sleep(5)  # don't hammer /execute during the initial load burst
        if not wait_for("preload_panel is_loaded",
                        lambda: c.probe("{preload_panel is_loaded}") == "1", 180):
            print("FATAL: preload never finished. Do NOT navigate away — "
                  "that crashes the main thread. Relaunch the title instead.")
            return 1

    # 4. venue is mandatory: without one, BandDirector never becomes
    #    ReadyForMidiParsers and the game_screen transition hangs forever.
    c.execute("{meta_performer select_random_venue}")
    if not args.dry_run and not c.execute("{meta_performer get_venue}"):
        print("FATAL: no venue selected — game_screen would hang forever")
        return 1

    # 5. manual on_preload_ok body (auto-fire is missed when the screen is
    #    entered out-of-flow)
    c.execute("{net_sync disable}")
    c.execute("{ui goto_screen {gamemode get game_screen}}")
    if not args.dry_run:
        if not wait_for("game_screen loaded",
                        lambda: c.probe("{{ui current_screen} name}") == "game_screen"
                        and c.probe("{game is_loaded}") == "1", 240, interval=5):
            return 1

    # 6. dismiss the "reconnect controller" overshell pause: fake the pads.
    c.execute("{set {var fake_controllers} 1}")
    if not args.dry_run:
        for pad in range(npl):
            conn = c.execute("{{user_mgr get_user_from_pad_num %d} "
                             "connected_controller_type}" % pad)
            # match the user's controller_type to whatever the platform
            # reports as "connected" for that pad; track type is unaffected
            if conn in ("0", "1", "2", "3", "4"):
                c.execute("{{user_mgr get_user_from_pad_num %d} "
                          "set_controller_type %s}" % (pad, conn))
            else:
                print(f"WARNING: pad {pad} connected type read {conn!r}; "
                      "skipping match (slot may stay in reconnect state)")
        c.execute("{overshell update_all}")
        time.sleep(2)
        if not wait_for("overshell unpaused",
                        lambda: c.execute("{overshell should_pause}") == "0"
                        and c.execute("{game get_paused}") == "0", 60, interval=3):
            print("WARNING: game still paused — check slot views:")
            for i in range(4):
                c.execute("{{overshell get_panel_from_slot_num %d} "
                          "get_current_view}" % i)
            return 1
    else:
        for pad in range(npl):
            c.execute("{{user_mgr get_user_from_pad_num %d} "
                      "set_controller_type 1}" % pad)
        c.execute("{overshell update_all}")

    # 7. final verification
    if not args.dry_run:
        ms1 = c.execute("{game get_song_ms}")
        time.sleep(5)
        ms2 = c.execute("{game get_song_ms}")
        print("\n=== FINAL STATE ===")
        print("screen:        ", c.screen())
        print("is_playing:    ", c.execute("{game is_playing}"))
        print("paused:        ", c.execute("{game get_paused}"))
        print("players:       ", c.execute("{beatmatch num_active_players}"))
        for pad in range(npl):
            print(f"P{pad + 1} track/diff: ",
                  c.execute("{{user_mgr get_user_from_pad_num %d} "
                            "get_track_sym}" % pad),
                  c.execute("{{user_mgr get_user_from_pad_num %d} "
                            "get_difficulty}" % pad))
        for pl in range(npl):
            print(f"player {pl} gem_count: ",
                  c.execute("{{beatmatch active_player %d} get_gem_count}"
                            % pl))
        print("song_ms moved: ", ms1, "->", ms2)
    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
