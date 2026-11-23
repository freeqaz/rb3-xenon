#!/usr/bin/env python3
# walk60 (2026-11-23): this generator emits LEGACY 30 Hz frames from legacy inputs (the walk .txt files here are
# now 60 Hz, converted by ../hz60.py). Regenerate from main 82ff40429's inputs, then run ../hz60.py on the output.
# mk.py (lane optwalk): writes walks/w_*.txt. Run from ~/tmp/optwalk.
def boot(audio=('right',), video=()):
    s = "wait_screen splash_screen\n+30 start\nwait_screen first_time_calibration\n+30 a\n"
    s += "wait_screen cal_welcome_screen\n+30 right\n+45 a\nwait_screen cal_manual_audio_screen\n"
    t = 30
    for b in audio: s += '+%d %s\n' % (t, b); t += 15
    s += '+%d a\nwait_screen cal_manual_video_screen\n' % (t + 15); t = 30
    for b in video: s += '+%d %s\n' % (t, b); t += 15
    s += '+%d a\nwait_screen cal_manual_complete_screen\n+30 a\nwait_screen hint_goalcomplete_screen\n' % (t + 15)
    s += "+30 a\nwait_screen hint_rb3_welcome_screen\n+30 down\n+45 a\nwait_screen main_hub_screen\n"
    return s
REBOOT = "wait_screen splash_screen\n+30 start\nwait_screen main_hub_screen\n"
SONG = ("+60 a\n+90 a\n+120 a\n+150 a\nwait_screen song_select_screen\n+120 a\n+240 a\n+330 a\n+420 a\n+510 a\n"
        "wait_screen game_screen\n")
class W:
    def __init__(s, t=60): s.t, s.lines = t, []
    def p(s, b, gap=30): s.lines.append('+%d %s' % (s.t, b)); s.t += gap
    def text(s): return '\n'.join(s.lines) + '\n'
def options(w, guitar=True):
    # overshell: start -> options list (return, register_online, change_character, game_options, leave)
    w.p('start', 90)
    for _ in range(3): w.p('down', 20)
    w.p('a', 60)                        # game_options (guitar: auto_vocals no_fail lefty av extras mods)
    for _ in range(3): w.p('down', 20)
    w.p('a', 60)                        # av_settings: dolby bass overscan calibration autosave audio
    w.p('a', 40)                        # dolby
    w.p('down', 20); w.p('a', 40)       # bass boost
    w.p('down', 20); w.p('a', 40)       # overscan
    for _ in range(3): w.p('down', 20)
    w.p('a', 60)                        # audio_options: instruments bg_music crowd sfx ...
    for k, n in enumerate([3, 2, 5, 1]):   # instruments, bg_music, crowd, sfx: A focuses the slider
        if k: w.p('down', 30)
        w.p('a', 40)
        for _ in range(n): w.p('left', 20)
        w.p('a', 40)                    # kAction_Confirm: slider confirm, focus back on the list
    w.p('b', 60); w.p('b', 60)          # back to game_options
    if guitar:
        w.p('up', 20); w.p('a', 60)     # lefty flip (ShellInputInterceptor then swaps up/down)
    for _ in range(4): w.p('b', 40)
def save(name, s): open('walks/' + name, 'w').write(s)
save('w_base.txt', boot() + SONG)
save('w_cal.txt', boot(audio=['right'] * 6, video=['left'] * 4) + SONG)
w = W(); options(w)
save('w_opt.txt', boot(audio=['right'] * 6, video=['left'] * 4) + w.text() + "+%d a\n" % (w.t + 60) + SONG.replace('+60 a\n+90 a\n+120 a\n+150 a\n', '+%d a\n+%d a\n+%d a\n+%d a\n' % (w.t+120, w.t+150, w.t+180, w.t+210)))
w = W(); w.p('start', 90)
for _ in range(3): w.p('down', 20)
w.p('a', 60); w.p('down', 20); w.p('down', 20); w.p('down', 20); w.p('a', 120); w.p('down', 20); w.p('down', 20); w.p('down', 20)
w.p('down', 20); w.p('down', 20); w.p('a', 120)
for _ in range(5): w.p('b', 40)
save('w_reload.txt', REBOOT + w.text() + SONG.replace('+60 a\n+90 a\n+120 a\n+150 a\n', '+%d a\n+%d a\n+%d a\n+%d a\n' % (w.t+120, w.t+150, w.t+180, w.t+210)))
# w_optsave: the options walk, then song select and back to main_hub (its TRANSITION_COMPLETE_MSG
# autosaves, main_hub.dta:721), so the reboot can read the lefty flip back.
w = W(); options(w)
s = boot(audio=['right'] * 6, video=['left'] * 4) + w.text()
s += '+%d a\n+%d a\n+%d a\n+%d a\nwait_screen song_select_screen\n+120 b\nwait_screen main_hub_screen\n+300 a\n' % (w.t+120, w.t+150, w.t+180, w.t+210)
save('w_optsave.txt', s)
# timing legs: video lag +200 ms (40 x right on cal_manual_video), then quickplay a song
save('w_v200.txt', boot(video=['right'] * 40) + SONG)
# w_voc (gamepad = vocals): game_options -> vocal_options (static, synapse, vocal_track_vol, mic_1..3
# gain): toggle static, track volume -3, mic 1 gain -2; then song select and back (autosave)
w = W(); w.p('start', 90)
for _ in range(3): w.p('down', 20)
w.p('a', 60)
for _ in range(2): w.p('down', 20)
w.p('a', 60)                            # vocal_options
w.p('a', 40)                            # static vocals
w.p('down', 20); w.p('down', 20)        # (synapse) -> vocal_track_vol
w.p('a', 40)
for _ in range(3): w.p('left', 20)
w.p('a', 40)
w.p('down', 20); w.p('a', 40)           # mic_1_gain
for _ in range(2): w.p('left', 20)
w.p('a', 40)
for _ in range(4): w.p('b', 40)
s = boot() + w.text() + '+%d a\n+%d a\n+%d a\n+%d a\nwait_screen song_select_screen\n+120 b\nwait_screen main_hub_screen\n+300 a\n' % (w.t+120, w.t+150, w.t+180, w.t+210)
save('w_voc.txt', s)
# w_noas: av_settings -> autosave_enabled (sel 4) -> confirm (yes = disable) -> bass boost on;
# then song select and back. With autosave off, the reboot must not see bass boost.
w = W(); w.p('start', 90)
for _ in range(3): w.p('down', 20)
w.p('a', 60)
for _ in range(3): w.p('down', 20)
w.p('a', 60)                            # av_settings
for _ in range(4): w.p('down', 20)
w.p('a', 90)                            # autosave_enabled -> confirm
w.p('a', 90)                            # first option
for _ in range(3): w.p('up', 20)        # bass boost (sel 1)
w.p('a', 40)
for _ in range(4): w.p('b', 40)
s = boot() + w.text() + '+%d a\n+%d a\n+%d a\n+%d a\nwait_screen song_select_screen\n+120 b\nwait_screen main_hub_screen\n+300 a\n' % (w.t+120, w.t+150, w.t+180, w.t+210)
save('w_noas.txt', s)
