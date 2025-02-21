#!/bin/bash
# ============================================================================
# VeloxServe Health Check Script
# Periodically probes the server /health and /metrics endpoints and reports
# status. Suitable for cron jobs and monitoring integrations.
# ============================================================================

HOST="${1:-localhost}"
PORT="${2:-8080}"
INTERVAL="${3:-5}"
BASE_URL="http://${HOST}:${PORT}"

echo "╔══════════════════════════════════════════════╗"
echo "║      VeloxServe Health Monitor               ║"
echo "║  Probing: ${BASE_URL}"
echo "║  Interval: ${INTERVAL}s (Ctrl+C to stop)"
echo "╚══════════════════════════════════════════════╝"
echo ""

check_count=0
fail_count=0

while true; do
    check_count=$((check_count + 1))
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

    # Probe /health
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 3 "${BASE_URL}/health" 2>/dev/null)

    if [ "$HTTP_CODE" == "200" ]; then
        # Fetch key metrics
        METRICS=$(curl -s --connect-timeout 3 "${BASE_URL}/metrics" 2>/dev/null)
        CACHE_HITS=$(echo "$METRICS" | grep "cache_hits" | awk '{print $2}')
        CACHE_MISSES=$(echo "$METRICS" | grep "cache_misses" | awk '{print $2}')
        RATE_BLOCKED=$(echo "$METRICS" | grep "rate_limit_blocked" | awk '{print $2}')
        ACTIVE_IPS=$(echo "$METRICS" | grep "rate_limit_active_ips" | awk '{print $2}')

        echo "[${TIMESTAMP}] ✅ HEALTHY  | cache: ${CACHE_HITS:-0}H/${CACHE_MISSES:-0}M | blocked: ${RATE_BLOCKED:-0} | ips: ${ACTIVE_IPS:-0}"
    else
        fail_count=$((fail_count + 1))
        echo "[${TIMESTAMP}] ❌ DOWN     | HTTP: ${HTTP_CODE} | consecutive_fails: ${fail_count}"

        if [ $fail_count -ge 3 ]; then
            echo "[${TIMESTAMP}] ⚠️  ALERT: Server has failed ${fail_count} consecutive health checks!"
        fi
    fi

    sleep "${INTERVAL}"
done
