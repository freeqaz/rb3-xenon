#!/usr/bin/env python3
"""hz60.py: convert an rb3-app walk script from legacy 30 Hz frames to retail's 60 Hz frames.

Lane walk60 (2026-11-23). rb3-app's harness runs default to legacy 30 Hz until both walk suites
migrate; a script converted here is meant for RB3_APP_HZ=60 (the runb.sh in each migrated
directory passes it). Grammar (engine MILO_INPUT_SCRIPT and RB3_APP_EXTRA_PADS pad scripts):

  N button / +N button   frame N doubles (the same time after the wait at twice the frame rate)
  [+N] highlight name    its start frame doubles; the closed-loop selection itself is unchanged
  wait_screen name       written twice: a wait gives up after 1800 frames (the engine's
                         kWaitTimeoutFrames, rb3_app_joypad.cpp's 1800 polls), which is 30 s at
                         60 Hz, so the pair covers the 60 s one wait covered at 30 Hz. A satisfied
                         wait lets the next line run in the same frame, so the copy is inert
                         whenever the first one is satisfied.
  a run of one button on consecutive frames (a hold) stays a hold: t..t+n-1 -> 2t..2(t+n)-1.

Comments and blank lines pass through (numbers inside comments are NOT rewritten). A converted
file starts with MARK; converting it again is refused.

  hz60.py FILE...        convert in place
  hz60.py -c < in > out  filter
"""
import re
import sys

MARK = "# hz60: 60 Hz frames (RB3_APP_HZ=60); converted from the legacy 30 Hz script by tools/rb3_app_walks/hz60.py"
STEP = re.compile(r"^(\s*)(\+?)(\d+)(\s+)(\S+)(.*)$")


def convert(text):
    if MARK in text:
        raise ValueError("already converted (hz60 mark present)")
    lines = text.splitlines()
    out = [MARK]
    i = 0
    while i < len(lines):
        line = lines[i]
        body = line.split("#", 1)[0]
        if re.match(r"^\s*wait_screen\s+\S+", body):
            out += [line, line.split("#", 1)[0].rstrip()]
            i += 1
            continue
        m = STEP.match(body)
        if not m:
            out.append(line)
            i += 1
            continue
        ind, rel, n, sp, btn, rest = m.groups()
        t = int(n)
        # gather a hold: same kind, same button, frames t, t+1, ...
        j = i + 1
        while j < len(lines):
            m2 = STEP.match(lines[j].split("#", 1)[0])
            if not (m2 and m2.group(2) == rel and m2.group(5) == btn and m2.group(6).strip() == rest.strip()
                    and int(m2.group(3)) == t + (j - i)):
                break
            j += 1
        if j - i == 1:
            out.append(f"{ind}{rel}{2 * t}{sp}{btn}{rest}" + (("#" + line.split("#", 1)[1]) if "#" in line else ""))
        else:
            for f in range(2 * t, 2 * (t + (j - i))):
                out.append(f"{ind}{rel}{f}{sp}{btn}{rest}")
        i = j
    return "\n".join(out) + "\n"


def main(argv):
    if argv[1:] == ["-c"]:
        sys.stdout.write(convert(sys.stdin.read()))
        return 0
    rc = 0
    for p in argv[1:]:
        try:
            s = open(p).read()
            open(p, "w").write(convert(s))
        except ValueError as e:
            print(f"{p}: {e}", file=sys.stderr)
            rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
