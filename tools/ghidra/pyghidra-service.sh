#!/bin/bash
# pyghidra-mcp HTTP service manager
#
# Usage:
#   ./tools/ghidra/pyghidra-service.sh start   # Start the service
#   ./tools/ghidra/pyghidra-service.sh stop    # Stop the service
#   ./tools/ghidra/pyghidra-service.sh status  # Check status
#   ./tools/ghidra/pyghidra-service.sh restart # Restart
#   ./tools/ghidra/pyghidra-service.sh logs    # View logs

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

PORT=8002
HOST=127.0.0.1
PROJECT_PATH="$PROJECT_DIR/ghidra_projects/RB3Xenon/RB3Xenon"
XEX_PATH="$PROJECT_DIR/orig/45410914/default.xex"
MILOHAX_DIR="$(cd "$PROJECT_DIR/.." && pwd)"
PYGHIDRA_MCP="$MILOHAX_DIR/pyghidra-mcp"
PIDFILE="/tmp/claude/pyghidra-mcp-rb3xenon.pid"
LOGFILE="/tmp/claude/pyghidra-mcp-rb3xenon.log"

export JAVA_HOME="/usr/lib/jvm/java-17-openjdk"
# Use VMX128-enabled Ghidra fork (not stock /opt/ghidra)
# Build: cd ../ghidra && gradle buildGhidra
# The setup script extracts build/dist/*.zip → build/ghidra/
export GHIDRA_INSTALL_DIR="$MILOHAX_DIR/ghidra/build/ghidra"
# Use writable temp directory for Ghidra user home (avoids read-only filesystem issues)
export GHIDRA_USER_HOME="/tmp/claude/ghidra_user_rb3xenon"

_kill_port_users() {
    # Kill any process listening on our port (catches orphaned Java/Python processes)
    local pids
    pids=$(lsof -ti ":$PORT" 2>/dev/null || true)
    if [[ -n "$pids" ]]; then
        echo "Killing orphaned processes on port $PORT: $pids"
        echo "$pids" | xargs kill 2>/dev/null || true
        sleep 1
        # Force-kill any survivors
        pids=$(lsof -ti ":$PORT" 2>/dev/null || true)
        if [[ -n "$pids" ]]; then
            echo "$pids" | xargs kill -9 2>/dev/null || true
            sleep 0.5
        fi
    fi
}

cmd_start() {
    # Check if already running via PID file
    if [[ -f "$PIDFILE" ]] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        echo "Service already running (PID: $(cat "$PIDFILE"))"
        return 0
    fi

    # Check if port is already in use (catches orphaned processes from previous runs)
    if lsof -ti ":$PORT" > /dev/null 2>&1; then
        echo "Warning: port $PORT already in use by orphaned process"
        _kill_port_users
    fi

    # Clear any stale locks (safe now that orphaned processes are gone)
    rm -f "$PROJECT_PATH"/*.lock* 2>/dev/null || true

    # Ensure log directory exists
    mkdir -p "$(dirname "$LOGFILE")" 2>/dev/null || true

    echo "Starting pyghidra-mcp service..."
    echo "  Project: $PROJECT_PATH"
    echo "  Binary: $XEX_PATH"
    echo "  Log: $LOGFILE"

    # Start service in a new process group (setsid) so we can kill the whole tree
    # --wait-for-analysis ensures analysis is complete before serving requests
    setsid nohup uv run --python 3.10 --project "$PYGHIDRA_MCP" pyghidra-mcp \
        --transport streamable-http \
        --project-path "$PROJECT_PATH" \
        --wait-for-analysis \
        --cache-dir "$PROJECT_DIR" \
        --log-file "$LOGFILE" \
        "$XEX_PATH" \
        > "$LOGFILE" 2>&1 &

    PID=$!
    echo $PID > "$PIDFILE"
    echo "Started with PID: $PID"
    echo ""
    echo "Server is starting in the background..."
    echo "Check logs with: $0 logs"

    sleep 5
    if ps -p $PID > /dev/null 2>&1; then
        echo "Service process is running (PID: $PID)"
        return 0
    else
        echo "Error: Service process exited unexpectedly"
        tail -20 "$LOGFILE"
        return 1
    fi
}

cmd_stop() {
    if [[ -f "$PIDFILE" ]]; then
        PID=$(cat "$PIDFILE")
        if kill -0 "$PID" 2>/dev/null; then
            echo "Stopping service (PID: $PID)..."
            # Kill the entire process group (uv → python → java)
            kill -- -"$PID" 2>/dev/null || kill "$PID" 2>/dev/null || true
            rm -f "$PIDFILE"
            sleep 1
            # Clean up anything still on the port
            _kill_port_users
            echo "Stopped."
        else
            echo "PID file exists but process not running. Cleaning up."
            rm -f "$PIDFILE"
            # Kill any orphaned processes still holding the port
            _kill_port_users
        fi
    else
        # Try to find and kill any running instance
        PIDS=$(pgrep -f "pyghidra-mcp.*$PORT" || true)
        if [[ -n "$PIDS" ]]; then
            echo "Killing pyghidra-mcp processes: $PIDS"
            kill $PIDS 2>/dev/null || true
        fi
        _kill_port_users
        if [[ -z "$PIDS" ]] && ! lsof -ti ":$PORT" > /dev/null 2>&1; then
            echo "Service not running."
        fi
    fi
}

cmd_status() {
    if [[ -f "$PIDFILE" ]] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        echo "Service running (PID: $(cat "$PIDFILE"))"
        echo "URL: http://$HOST:$PORT/mcp"

        # Check if responsive
        if curl -s "http://$HOST:$PORT/mcp" > /dev/null 2>&1; then
            echo "Status: Ready"
        else
            echo "Status: Starting/Not responding"
        fi
    else
        # Check for orphaned processes on the port
        if lsof -ti ":$PORT" > /dev/null 2>&1; then
            echo "Service not running (stale PID), but port $PORT is in use by orphaned process"
            echo "Run '$0 stop' to clean up"
            return 1
        fi
        echo "Service not running."
        return 1
    fi
}

cmd_logs() {
    if [[ -f "$LOGFILE" ]]; then
        tail -f "$LOGFILE"
    else
        echo "No log file found at $LOGFILE"
    fi
}

cmd_restart() {
    cmd_stop
    sleep 2
    cmd_start
}

cmd_diagnose() {
    echo "Running Ghidra service diagnostics..."
    echo ""
    uv run --python 3.10 --project "$PYGHIDRA_MCP" pyghidra-mcp --diagnose
}

case "${1:-}" in
    start)      cmd_start ;;
    stop)       cmd_stop ;;
    status)     cmd_status ;;
    restart)    cmd_restart ;;
    logs)       cmd_logs ;;
    diagnose)   cmd_diagnose ;;
    *)
        echo "Usage: $0 {start|stop|status|restart|logs|diagnose}"
        exit 1
        ;;
esac
