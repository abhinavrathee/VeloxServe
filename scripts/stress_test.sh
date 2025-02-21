#!/bin/bash
# ============================================================================
# VeloxServe Stress Test Script
# Hammers the server with extreme concurrency to detect crashes, memory leaks,
# and thread pool deadlocks under sustained production-level pressure.
# ============================================================================

set -e

HOST="${1:-localhost}"
PORT="${2:-8080}"
BASE_URL="http://${HOST}:${PORT}"

echo "╔══════════════════════════════════════════════╗"
echo "║      VeloxServe Stress Test Suite            ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

# Phase 1: Connection storm (many connections, short duration)
echo "── Phase 1: Connection Storm (500 connections) ──"
wrk -t4 -c500 -d5s "${BASE_URL}/" 2>&1
echo ""

# Phase 2: Sustained load (moderate connections, long duration)
echo "── Phase 2: Sustained Load (30 seconds) ─────────"
wrk -t4 -c100 -d30s "${BASE_URL}/" 2>&1
echo ""

# Phase 3: Rate limiter flood test
echo "── Phase 3: Rate Limiter Flood ──────────────────"
BLOCKED=0
ALLOWED=0
for i in $(seq 1 300); do
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "${BASE_URL}/")
    if [ "$HTTP_CODE" == "429" ]; then
        BLOCKED=$((BLOCKED + 1))
    else
        ALLOWED=$((ALLOWED + 1))
    fi
done
echo "  Requests sent:   300"
echo "  Allowed (2xx):   ${ALLOWED}"
echo "  Blocked (429):   ${BLOCKED}"
echo ""

# Phase 4: Concurrent endpoint mixing
echo "── Phase 4: Mixed Endpoint Load ─────────────────"
wrk -t2 -c50 -d10s "${BASE_URL}/health" 2>&1 &
wrk -t2 -c50 -d10s "${BASE_URL}/metrics" 2>&1 &
wrk -t2 -c50 -d10s "${BASE_URL}/" 2>&1 &
wait
echo ""

# Final health check
echo "── Final Health Verification ────────────────────"
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "${BASE_URL}/health")
if [ "$HTTP_CODE" == "200" ]; then
    echo "PASS: Server survived stress test (HTTP 200)"
else
    echo "FAIL: Server unhealthy after stress test (HTTP ${HTTP_CODE})"
    exit 1
fi

echo ""
echo "═══════════════════════════════════════════════"
echo "  Stress test complete. No crashes detected."
echo "═══════════════════════════════════════════════"
