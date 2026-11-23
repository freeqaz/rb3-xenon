#!/usr/bin/env python3
# mk.py: writes the songselwalk walks (relative to wait_screen song_select_screen).
# Run from ~/tmp/songselwalk (needs walks/hdr.txt there); writes walks/w_*.txt.
import os
HDR = open('/home/free/tmp/songselwalk/walks/hdr.txt').read()
QP = "+60 a\n+90 a\n+120 a\n+150 a\nwait_screen song_select_screen\n"
class W:
    def __init__(s, t=150): s.t, s.lines = t, []
    def p(s, b, gap=30): s.lines.append('+%d %s' % (s.t, b)); s.t += gap
    def hold(s, b, n=60, gap=30):
        for i in range(n): s.lines.append('+%d %s' % (s.t + i, b))
        s.t += n + gap
    def wait(s, n): s.t += n
    def text(s): return '\n'.join(s.lines) + '\n'
def save(name, body, pre=HDR + QP):
    open('walks/' + name, 'w').write(pre + body)
# sorts: settings row 1 = sort; options 0..7 (4 = by_rank, offline-inactive)
w = W()
for i in [1, 2, 3, 5, 6, 7, 0]:
    w.p('select', 60); w.p('a', 60)
    for _ in range(9): w.p('up', 10)
    for _ in range(i): w.p('down', 15)
    w.p('a', 60); w.p('select', 30)
    w.wait(240)
    for _ in range(6): w.p('down', 20)   # scroll a little: preview + highlight
    w.wait(120)
save('w_sorts.txt', w.text())
# filters: settings rows after the filters header; for each filter k (0..8) pick its first option
for k in range(9):
    w = W()
    w.p('select', 60)
    for _ in range(4 + k): w.p('down', 15)  # row1 sort -> row 5 header skipped? log tells
    w.p('a', 60); w.p('a', 60)              # options.lst: toggle first option
    w.p('down', 20); w.p('a', 60)           # and the second
    w.p('b', 40); w.p('select', 30)         # back to settings, exit (report_sort_and_filters)
    w.wait(300)
    for _ in range(4): w.p('down', 20)
    # reset all: open, Option (y) on settings.lst -> filter_reset_confirm_screen, confirm
    w.p('select', 60); w.p('y', 90); w.p('a', 90); w.p('select', 30)
    w.wait(300)
    save('w_filt%d.txt' % k, w.text())
# setlist: make (hold A on a song), add two, finish (hold A), save locally, edit screen, ok
def setlist_body(w, finish_extra=()):
    w.p('down', 20); w.p('down', 40)          # hl 4: first song
    w.hold('a', 60, 60)                       # make a setlist with it
    w.p('down', 30); w.p('a', 60)             # + next song
    w.p('down', 30); w.p('down', 30); w.p('a', 60)
    w.hold('a', 60, 120)                      # finish -> setlist_save_screen
    w.p('down', 40); w.p('a', 150)            # opt2: save locally -> edit_setlist_screen
    w.p('a', 90)                              # edit_name.btn -> virtual keyboard
    for b in finish_extra: w.p(b, 40)
    for _ in range(4): w.p('down', 30)        # to ok.btn
    w.p('a', 300)
w = W(); setlist_body(w)
w.wait(600)
w.p('b', 200); w.p('b', 200); w.p('b', 200); w.p('b', 200)
save('w_setlist.txt', w.text())
RL = "wait_screen splash_screen\n+30 start\nwait_screen main_hub_screen\n+300 a\n+330 a\n+360 a\n+390 a\nwait_screen song_select_screen\n"
w = W(); w.p('up', 30); w.p('up', 60); w.p('a', 300)       # view_setlists
for _ in range(4): w.p('down', 40)
w.wait(200); w.p('y', 200)                                  # details on a setlist
for _ in range(3): w.p('down', 40)
w.p('b', 100); w.p('a', 600)                                # play the highlighted setlist
w.p('a', 120); w.p('a', 120); w.p('a', 120); w.p('a', 600)
save('w_reload.txt', w.text(), RL)
# create + name (VK) + save, then rename it from the setlist browser's details
w = W(); setlist_body(w)
w.p('a', 300)                     # message_ok
for _ in range(3): w.p('b', 200)  # back to song select, unwind the setlist
w.p('b', 200); w.p('b', 200); w.p('b', 300)
save('w_name.txt', w.text())
w = W(); setlist_body(w)
w.p('a', 300)                     # message_ok -> setlist browser on the new setlist
w.p('y', 200)                     # details
w.p('down', 60); w.p('a', 300)    # details_2 = edit_setlist
w.p('a', 120)                     # edit_name.btn -> VK entry 2
for _ in range(4): w.p('down', 30)
w.p('a', 300); w.p('a', 300)      # ok, message_ok
w.wait(200)
save('w_rename.txt', w.text())
w = W()
w.hold('a', 60, 60)               # on random_song: make a setlist with one random song
for _ in range(4): w.p('a', 60)   # four more random songs
w.hold('a', 60, 150)              # finish -> setlist_save_screen
w.p('a', 300)                     # opt1: play without saving
w.p('a', 200); w.p('a', 200)
save('w_random.txt', w.text())
w = W()
w.p('select', 60); w.p('a', 60)
for _ in range(9): w.p('up', 10)
for _ in range(4): w.p('down', 15)        # 0,1,2,3,(4 inactive),5 = by_recent
w.p('a', 60); w.p('select', 300)
for _ in range(3): w.p('down', 20)
w.p('y', 120)                             # details on a song
for _ in range(5): w.p('down', 40)
w.p('b', 120)
w.p('select', 60); w.p('a', 60)           # back to by_song
for _ in range(9): w.p('up', 10)
w.p('a', 60); w.p('select', 300)
w.hold('rb', 20, 90)                      # held PageDown: shortcut panel (groups)
w.p('down', 40); w.p('down', 40); w.p('a', 120)
w.p('rb', 60); w.p('rb', 60); w.p('lb', 120)
save('w_misc.txt', w.text())
