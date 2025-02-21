#!/bin/bash
# ============================================================================
# VeloxServe Benchmark Script
# Runs wrk stress tests against a running VeloxServe instance.
# Requirements: wrk (https://github.com/wg/wrk)
# ============================================================================

set -e

HOST="${1:-localhost}"
PORT="${2:-8080}"
DURATION="${3:-10s}"
THREADS="${4:-4}"
CONNECTIONS="${5:-200}"

BASE_URL="http://${HOST}:${PORT}"

echo "╔══════════════════════════════════════════════╗"
echo "║        VeloxServe Benchmark Suite            ║"
echo "╠══════════════════════════════════════════════╣"
echo "║  Target:       ${BASE_URL}"
echo "║  Duration:     ${DURATION}"
echo "║  Threads:      ${THREADS}"
echo "║  Connections:  ${CONNECTIONS}"
echo "╚══════════════════════════════════════════════╝"
echo ""

# 1. Health check
echo "── [1/4] Health Check ──────────────────────────"
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "${BASE_URL}/health")
if [ "$HTTP_CODE" != "200" ]; then
    echo "FAIL: Server returned HTTP ${HTTP_CODE} on /health"
    exit 1
fi
echo "PASS: Server is healthy (HTTP 200)"
echo ""

# 2. Static file benchmark
echo "── [2/4] Static File Serving ────────────────────"
wrk -t${THREADS} -c${CONNECTIONS} -d${DURATION} "${BASE_URL}/" 2>&1
echo ""

# 3. Metrics endpoint benchmark
echo "── [3/4] Metrics Endpoint ───────────────────────"
wrk -t2 -c50 -d${DURATION} "${BASE_URL}/metrics" 2>&1
echo ""

# 4. Summary metrics after load
echo "── [4/4] Post-Benchmark Telemetry ───────────────"
curl -s "${BASE_URL}/metrics"
echo ""
echo ""
echo "═══════════════════════════════════════════════"
echo "  Benchmark complete."
echo "═══════════════════════════════════════════════"
