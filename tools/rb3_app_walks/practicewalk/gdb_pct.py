# gdb_pct.py: log practice hit-percent queries (args and return value), Game::Jump targets,
# speed changes and MarkGemsAsProcessed. No debug info needed (reads SysV registers).
# Run as WRAP="gdb -batch -x gdb_pct.py --args".
import gdb
gdb.execute("set pagination off"); gdb.execute("set confirm off")
gdb.execute("handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint pass")
def x(n): return float(gdb.parse_and_eval("$xmm%d.v4_float[0]" % n))
def r(n): return int(gdb.parse_and_eval("(int)$%s" % n))
n = {"pct": 0}
class FB(gdb.FinishBreakpoint):
    def __init__(self, fr, a): super().__init__(fr, internal=True); self.a = a
    def stop(self):
        n["pct"] += 1
        print("GDB PCT #%d slot %d start %.1f end %.1f -> %.2f" % ((n["pct"],) + self.a + (x(0),)), flush=True)
        return False
    def out_of_scope(self): pass
class B(gdb.Breakpoint):
    def stop(self):
        FB(gdb.newest_frame(), (r("esi"), x(0), x(1))); return False
class P(gdb.Breakpoint):
    def __init__(self, spec, tag, fn): super().__init__(spec); self.tag = tag; self.fn = fn
    def stop(self):
        print("GDB %s %s" % (self.tag, self.fn()), flush=True); return False
B("GemPlayer::OnGetPercentHitGemsPractice")
P("Game::Jump", "JUMP", lambda: "ms %.1f b %d" % (x(0), r("esi") & 0xff))
P("Game::SetMusicSpeed", "SPEED", lambda: "%.3f" % x(0))
P("PracticePanel::MarkGemsAsProcessed", "MARK", lambda: "")
gdb.execute("run")
