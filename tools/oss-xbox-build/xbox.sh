#!/usr/bin/env bash
# xbox.sh — one-stop wrapper for talking to the RGH 360 through the .54 relay.
#
# Topology: this box -> ssh free@192.168.8.54 (relay) -> Xbox 192.168.9.180
#   - XBDM  : TCP 730 (xbdm.xex, DashLaunch plugin2) — survives title changes
#   - FTP   : xboxftp:xboxftp — ONLY while Aurora dashboard is running
#   - RB3E  : UDP broadcast 21070 ([alive] = DLL loaded proof)
# The Xbox ignores ICMP; "is it up" = does XBDM answer.
#
# usage:
#   xbox.sh cmd 'modules' ['getexecstate' ...]   raw XBDM command(s)
#   xbox.sh state                                getexecstate shortcut
#   xbox.sh up                                   poll until XBDM answers
#   xbox.sh launch-rb3                           magicboot RB3 (Usb0 path)
#   xbox.sh reboot                               magicboot cold (full reboot -> Aurora)
#   xbox.sh notify [secs] [logfile]              arm notify capture on relay (background)
#   xbox.sh notify-tail [logfile]                tail the capture log
#   xbox.sh alive [secs]                         arm RB3E UDP 21070 listener (background)
#   xbox.sh alive-tail                           tail the alive log
#   xbox.sh ftp-ls <path>                        FTP dir listing (e.g. /Usb0/Games/rb3)
#   xbox.sh ftp-get <remote> <local-on-relay>    FTP download to relay /tmp/xboxdbg
#   xbox.sh ftp-put <local-file> <remote-path>   FTP upload (scp's to relay first)
#   xbox.sh ftp-sha <path>                       download + sha256 of a remote file
#   xbox.sh push-tools                           redeploy the python helpers to the relay
#   xbox.sh ssh [cmd...]                         raw shell on the relay
#
# NOTE for Claude: Bash calls touching the LAN need dangerouslyDisableSandbox:true.
# WARNING: never magicboot the Hdd Aurora path (\Device\Harddisk0\...Aurora.xex)
#          — it crash-loops the console (black screen). Use `reboot` instead;
#          the Default boot lands in Aurora on its own.
set -euo pipefail

RELAY=free@192.168.8.54
XBOX=192.168.9.180
DBG=/tmp/xboxdbg
TOOLS_DIR="$(cd "$(dirname "$0")" && pwd)"
SSH="ssh -o BatchMode=yes -o ConnectTimeout=5 $RELAY"

rssh() { $SSH "$@"; }

ftp_script() {  # run an lftp script body on the relay
    rssh "lftp -u xboxftp,xboxftp -e 'set ftp:passive-mode true; set net:timeout 8; set net:max-retries 1; $1; bye' $XBOX"
}

case "${1:-help}" in
  cmd)   shift; rssh "python3 $DBG/xbdm_cmd.py $XBOX $(printf '%q ' "$@")" ;;
  state) rssh "python3 $DBG/xbdm_cmd.py $XBOX getexecstate" ;;
  up)    for i in $(seq 1 30); do
             if rssh "timeout 4 python3 $DBG/xbdm_cmd.py $XBOX getexecstate" 2>/dev/null; then exit 0; fi
             echo "waiting for XBDM... ($i)"; sleep 4
         done; echo "XBDM never answered"; exit 1 ;;
  launch-rb3)
         rssh "python3 $DBG/xbdm_cmd.py $XBOX 'magicboot title=\\Device\\Mass0\\Games\\rb3\\default.xex directory=\\Device\\Mass0\\Games\\rb3'" ;;
  reboot)
         rssh "python3 $DBG/xbdm_cmd.py $XBOX 'magicboot cold'" || true
         echo "cold reboot issued; run: xbox.sh up" ;;
  notify)
         secs=${2:-600}; log=${3:-$DBG/notify.log}
         rssh "pkill -f '^python3 $DBG/xbdm_notify' 2>/dev/null; sleep 1; setsid python3 $DBG/xbdm_notify.py $XBOX $secs </dev/null >>$log 2>&1 & sleep 2; tail -5 $log"
         echo "notify capture armed -> $log (relay)" ;;
  notify-tail)
         rssh "tail -40 ${2:-$DBG/notify.log}" ;;
  alive)
         secs=${2:-3600}
         rssh "pkill -f '^python3 $DBG/rb3e_alive' 2>/dev/null; sleep 1; setsid python3 $DBG/rb3e_alive_listen.py $secs </dev/null >>$DBG/rb3e_alive.log 2>&1 & sleep 1; tail -3 $DBG/rb3e_alive.log"
         echo "alive listener armed -> $DBG/rb3e_alive.log (relay)" ;;
  alive-tail)
         rssh "tail -20 $DBG/rb3e_alive.log" ;;
  ftp-ls)
         ftp_script "cls -l ${2:?path}" ;;
  ftp-get)
         ftp_script "get ${2:?remote} -o ${3:-$DBG/$(basename "${2:?}")}" ;;
  ftp-put)
         f=${2:?local file}; r=${3:?remote path}
         scp -o BatchMode=yes "$f" "$RELAY:$DBG/_upload.$$" >/dev/null
         ftp_script "put $DBG/_upload.$$ -o $r"
         rssh "rm -f $DBG/_upload.$$" ;;
  ftp-sha)
         f=$DBG/_sha.$$; ftp_script "get ${2:?path} -o $f" && rssh "sha256sum $f | cut -c1-16; stat -c%s $f; rm -f $f" ;;
  push-tools)
         rssh "mkdir -p $DBG"
         scp -o BatchMode=yes "$TOOLS_DIR"/{xbdm_cmd.py,xbdm_notify.py,rb3e_alive_listen.py} "$RELAY:$DBG/" ;;
  ssh)   shift; rssh "$@" ;;
  *)     sed -n '2,30p' "$0" ;;
esac
