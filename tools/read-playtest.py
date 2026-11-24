#!/usr/bin/env python3
"""Summarize the most recent desktop playtest, or a specified session directory."""
import argparse
import csv
import re
from pathlib import Path


def report_guitars(log):
    if not log.is_file():
        return
    edges = re.compile(r"rb3-playtest: poll \d+ crkd pad (\d+) tilt=([01]) select=([01])")
    assigned = re.compile(r"CRKD Guitar LM: joystick \d+ assigned to pad (\d+)")
    states, counts = {}, {}
    with log.open(errors="replace") as stream:
        for line in stream:
            if "crkd pad" in line and (event := edges.search(line)):
                pad, tilt, select = map(int, event.groups())
                prev_tilt, prev_select = states.get(pad, (0, 0))
                totals = counts.setdefault(pad, [0, 0])
                totals[0] += int(tilt > prev_tilt)
                totals[1] += int(select > prev_select)
                states[pad] = tilt, select
            elif "CRKD Guitar LM:" in line and (device := assigned.search(line)):
                counts.setdefault(int(device.group(1)), [0, 0])
    for pad, (tilts, selects) in sorted(counts.items()):
        print(f"CRKD pad {pad}: tilt presses {tilts}, Select presses {selects}.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, nargs="?")
    args = parser.parse_args()
    if args.directory:
        timeline = args.directory / "timeline.tsv"
    else:
        logs = Path(__file__).resolve().parents[2] / "logs"
        timeline = max(logs.glob("rb3-xenon-*/timeline.tsv"),
                       key=lambda p: p.parent.name, default=None)
    if timeline is None or not timeline.is_file():
        parser.exit(1, "No playtest timeline found. Open rb3-xenon-play.command first.\n")

    print(timeline.parent)
    active = {}
    previous_song = None
    last = worst = None
    marks = set()

    def report_song():
        for (song, player), row in active.items():
            print(f"  {song}, player {player}: last sample at {row['matcher_ms']} ms, "
                  f"score {row['score']}, hits {row['hits']}, misses {row['misses']}")
        active.clear()

    with timeline.open(newline="") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            if None in row.values():  # a live or interrupted final write
                continue
            last = row
            if worst is None or float(row["worst_frame_ms"]) > float(worst["worst_frame_ms"]):
                worst = row
            if row["event"] == "mark" and row["frame"] not in marks:
                marks.add(row["frame"])
                shot = timeline.parent / f"frame-{row['frame']}.png"
                camera = row.get("camera_shot", "-")
                context = row["screen"]
                if row["song"] != "-":
                    context += f", {row['song']} at {row['matcher_ms']} ms"
                if camera != "-":
                    context += f", {camera}"
                print(f"  Bookmark at {row['wall_s']} s, {context}: "
                      f"{shot.name if shot.is_file() else 'image unavailable'}")
            song = row["song"]
            if active and (row["screen"] != "game_screen" or
                           (song != "-" and previous_song != song)):
                report_song()
            if row["player"] != "-":
                active[song, row["player"]] = row
                previous_song = song
    report_song()
    report_guitars(timeline.parent / "game.log")
    if last:
        print(f"Last screen: {last['screen']} at {last['wall_s']} s. "
              f"Longest frame interval: {worst['worst_frame_ms']} ms in the sample "
              f"ending at {worst['wall_s']} s.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
