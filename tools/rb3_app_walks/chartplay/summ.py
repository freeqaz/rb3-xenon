#!/usr/bin/env python3
# summ.py run.log...: per song (a run of game_screen frames), p0's last hit/miss/streak/long/score/stars/crowd,
# the band's score, min crowd, and the chartplay summary lines.
import re,sys
pat=re.compile(r'p0 track (\d+) en \d+ auto (\d) energy ([\d.]+) deploy (\d) long (\d+) score (\d+) hit (\d+) miss (\d+) streak (\d+) stars ([\d.]+) crowd ([\d.]+)')
for fn in sys.argv[1:]:
    songs=[]; cur=None; last=0; cps=[]
    for line in open(fn,errors='replace'):
        if line.startswith('rb3-app-chartplay: song'): cps.append(line.strip()[19:])
        if not line.startswith('rb3-app-det: frame'): continue
        fr=int(line.split()[2]); last=fr
        m=pat.search(line)
        if m and ' screen game_screen' in line:
            v=m.groups()
            if cur is None or int(v[6])+int(v[7]) < cur['hm']:
                cur={'start':fr,'mincrowd':9,'hm':0}; songs.append(cur)
            cur.update(end=fr,track=v[0],auto=v[1],energy=v[2],long=v[4],score=v[5],hit=v[6],miss=v[7],streak=v[8],stars=v[9],crowd=v[10],hm=int(v[6])+int(v[7]))
            cur['mincrowd']=min(cur['mincrowd'],float(v[10]))
    print('==',fn,'frames',last)
    for s in songs:
        print('  song frames %(start)d-%(end)d track %(track)s auto %(auto)s hit %(hit)s miss %(miss)s streak %(streak)s long %(long)s score %(score)s stars %(stars)s crowd %(crowd)s mincrowd %(mincrowd).4f energy %(energy)s'%s)
    for c in cps: print('  chartplay',c)
