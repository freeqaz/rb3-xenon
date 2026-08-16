#!/bin/bash
# Test script for Ghidra service hardening features
# Verifies: port cleanup, health checks, logging, diagnostics
#
# ⚠ Why this file has an accumulator (lane task93, 2026-08-16)
# ────────────────────────────────────────────────────────────
# It used to have none. Every check sat inside an `if` condition, which makes
# `set -e` inert (a command whose status is being TESTED never triggers ERR), so
# a failing check printed a red ✗ and execution carried straight on. The summary
# then printed eight UNCONDITIONAL green ticks, and the script ended on an
# `echo`, so `$?` was 0. Measured with every dependency stubbed to fail: six
# red ✗ on screen, eight ✓ in the summary, exit 0.
#
# So: every check now records a verdict, the summary is PRINTED FROM those
# verdicts rather than hardcoded, and the script exits 1 if anything failed.
# Do not re-add a tick that is not derived from `record`.
#
# Test-log directory is overridable so the suite can be exercised against stubs
# without touching a live service:
#   GHIDRA_TEST_LOGDIR=<dir> GHIDRA_SERVICE_LOG=<file> ./test-hardening.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
SERVICE_SCRIPT="$SCRIPT_DIR/pyghidra-service.sh"

TESTLOGDIR="${GHIDRA_TEST_LOGDIR:-/tmp/claude}"
LOGFILE="${GHIDRA_SERVICE_LOG:-/tmp/claude/pyghidra-mcp-rb3xenon.log}"
mkdir -p "$TESTLOGDIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# ── verdict accumulator ──────────────────────────────────────────────────────
# Three outcomes. FAIL is the only one that sets the exit status; WARN exists so
# a thing this suite genuinely CANNOT verify is visible as ⚠ rather than
# laundered into a ✓.
FAILED=0
WARNED=0
RESULT_LABELS=()
RESULT_STATES=()

record() {  # record <PASS|FAIL|WARN> <label> [detail]
    local state="$1" label="$2" detail="${3:-}"
    RESULT_LABELS+=("$label")
    RESULT_STATES+=("$state")
    case "$state" in
        PASS) echo -e "${GREEN}✓ ${label}${detail:+: $detail}${NC}" ;;
        FAIL) echo -e "${RED}✗ ${label}${detail:+: $detail}${NC}"; FAILED=$((FAILED + 1)) ;;
        WARN) echo -e "${YELLOW}⚠ ${label}${detail:+: $detail}${NC}"; WARNED=$((WARNED + 1)) ;;
    esac
}

echo "=============================================================="
echo "Ghidra Service Hardening Tests (rb3-xenon, port 8002)"
echo "=============================================================="
echo ""

# Test 1: Diagnostic Mode
echo -e "${YELLOW}TEST 1: Diagnostic Mode${NC}"
echo "Running: pyghidra-mcp --diagnose"
if python3 -m pyghidra_mcp --diagnose > "$TESTLOGDIR/test-diagnose.log" 2>&1; then
    record PASS "Diagnostic mode (--diagnose flag)"
    tail -10 "$TESTLOGDIR/test-diagnose.log" || true
else
    record FAIL "Diagnostic mode (--diagnose flag)" "pyghidra-mcp --diagnose returned nonzero"
    tail -10 "$TESTLOGDIR/test-diagnose.log" || true
fi
echo ""

# Test 2: Port Cleanup
echo -e "${YELLOW}TEST 2: Port Cleanup${NC}"
echo "Starting service (will clean up stale port if needed)..."
$SERVICE_SCRIPT stop > /dev/null 2>&1 || true
sleep 1

if $SERVICE_SCRIPT start > "$TESTLOGDIR/test-start.log" 2>&1; then
    record PASS "Port cleanup / service start"
else
    record FAIL "Port cleanup / service start" "see $TESTLOGDIR/test-start.log"
    tail -20 "$TESTLOGDIR/test-start.log" || true
fi
sleep 3
echo ""

# Test 3: Service Health Check
# NB: `curl -s` succeeds on a 4xx, so the endpoint must be checked by STATUS
# CODE, not by exit status -- this endpoint has been observed answering 404
# while the old check read it as healthy.
echo -e "${YELLOW}TEST 3: Service Health Check${NC}"
HTTP_CODE="$(curl -s -o /dev/null -w '%{http_code}' \
             --max-time 10 http://127.0.0.1:8002/mcp/v1 2>/dev/null || echo "000")"
if [[ "$HTTP_CODE" =~ ^[23] ]]; then
    record PASS "Service responsiveness (HTTP endpoint)" "HTTP $HTTP_CODE"
else
    record FAIL "Service responsiveness (HTTP endpoint)" \
           "HTTP $HTTP_CODE from http://127.0.0.1:8002/mcp/v1 (000 = no connection)"
fi
echo ""

# Test 4: Logging to File
echo -e "${YELLOW}TEST 4: Logging to File${NC}"
if [[ -f "$LOGFILE" ]]; then
    SIZE=$(stat -f%z "$LOGFILE" 2>/dev/null || stat -c%s "$LOGFILE" 2>/dev/null || echo "0")
    if [[ "$SIZE" =~ ^[0-9]+$ ]] && [[ "$SIZE" -gt 0 ]]; then
        record PASS "File logging" "$LOGFILE ($SIZE bytes)"
        echo "Last 5 log entries:"
        tail -5 "$LOGFILE" | sed 's/^/  /' || true
    else
        record FAIL "File logging" "$LOGFILE exists but is empty"
    fi
else
    record FAIL "File logging" "$LOGFILE not found (service never wrote a log)"
fi
echo ""

# Test 5: Service Status Check
echo -e "${YELLOW}TEST 5: Service Status${NC}"
if $SERVICE_SCRIPT status > "$TESTLOGDIR/test-status.log" 2>&1; then
    record PASS "Service status reporting"
    sed 's/^/  /' "$TESTLOGDIR/test-status.log" || true
else
    record FAIL "Service status reporting" "\`$SERVICE_SCRIPT status\` returned nonzero"
fi
echo ""

# Test 6: Restart Test
echo -e "${YELLOW}TEST 6: Restart Test${NC}"
echo "Restarting service..."
if $SERVICE_SCRIPT restart > "$TESTLOGDIR/test-restart.log" 2>&1; then
    record PASS "Service restart capability"
else
    record FAIL "Service restart capability" "see $TESTLOGDIR/test-restart.log"
fi
sleep 3
echo ""

# Test 7: Log Rotation Check
# This check used to print "Log rotation configured: Keep last 10 files of 10 MB
# each" followed by an unconditional "✓ Log rotation settings verified" -- a
# claim with NOTHING behind it. Nothing in this repo configures rotation:
# `pyghidra-service.sh` passes `--log-file` and additionally redirects stdout to
# the same path with `>`, which a rotating handler could not survive anyway.
# What IS checkable is the `--log-file` wiring; rotation itself is upstream and
# this suite cannot see it, so it reports WARN rather than a green tick.
echo -e "${YELLOW}TEST 7: Log Rotation Capability${NC}"
if grep -q -- '--log-file' "$SERVICE_SCRIPT"; then
    record PASS "Log file wiring (--log-file passed by the service script)"
else
    record FAIL "Log file wiring (--log-file passed by the service script)" \
           "no --log-file in $SERVICE_SCRIPT"
fi
if ls "${LOGFILE}".* >/dev/null 2>&1; then
    record PASS "Log rotation" "rotated siblings present: $(ls "${LOGFILE}".* | wc -l)"
else
    record WARN "Log rotation" \
           "UNVERIFIED -- nothing in this repo configures rotation and no rotated \
sibling of $LOGFILE exists; do not report rotation as tested"
fi
echo ""

# Test 8: Service Diagnostics Command
echo -e "${YELLOW}TEST 8: Diagnose Command in Service Script${NC}"
if $SERVICE_SCRIPT diagnose > "$TESTLOGDIR/test-service-diagnose.log" 2>&1; then
    record PASS "Diagnose command in service script"
    head -20 "$TESTLOGDIR/test-service-diagnose.log" | sed 's/^/  /' || true
else
    record FAIL "Diagnose command in service script" \
           "see $TESTLOGDIR/test-service-diagnose.log"
fi
echo ""

# ── Summary ──────────────────────────────────────────────────────────────────
# Printed FROM the accumulator. Never hardcode a tick here.
echo "=============================================================="
echo "Test Summary"
echo "=============================================================="
for i in "${!RESULT_LABELS[@]}"; do
    case "${RESULT_STATES[$i]}" in
        PASS) echo -e "  ${GREEN}✓${NC} ${RESULT_LABELS[$i]}" ;;
        FAIL) echo -e "  ${RED}✗${NC} ${RESULT_LABELS[$i]}" ;;
        WARN) echo -e "  ${YELLOW}⚠${NC} ${RESULT_LABELS[$i]} (unverified)" ;;
    esac
done
echo ""
echo "checks: ${#RESULT_LABELS[@]}  failed: $FAILED  unverified: $WARNED"
echo ""
echo "Test logs saved to:"
echo "  - $TESTLOGDIR/test-*.log"
echo "  - $LOGFILE"
echo ""
echo "For detailed service logs: tail -f $LOGFILE"
echo "For diagnostics: ./tools/ghidra/pyghidra-service.sh diagnose"
echo ""

if [[ "$FAILED" -gt 0 ]]; then
    echo -e "${RED}RESULT: FAIL ($FAILED of ${#RESULT_LABELS[@]} checks failed)${NC}"
    exit 1
fi
echo -e "${GREEN}RESULT: PASS (${#RESULT_LABELS[@]} checks, $WARNED unverified)${NC}"
exit 0
