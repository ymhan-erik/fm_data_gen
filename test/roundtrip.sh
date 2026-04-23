#!/usr/bin/env bash
# FM Roundtrip Test
# 원본 → encoder → decoder → diff
# 전부 PASS → 알고리즘 확정, HLS 포팅 Go!
# 하나라도 FAIL → 버그 수정 필요

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

ENCODER="./build/enc_hello"
DECODER="./build/decoder"
DATA_DIR="${DATA_DIR:-test/data/type0}"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# ── 의존성 확인 ──────────────────────────────────────────────────────────────
ok=1
for tool in "$ENCODER" "$DECODER"; do
    [ -x "$tool" ] || { echo "ERROR: $tool not found. Run 'make all' first."; ok=0; }
done
[ "$ok" -eq 1 ] || exit 1

# ── 시나리오 자동 탐지 (DATA_DIR 내 *.bin) ────────────────────────────────────
SCENARIOS=()
LABELS=()
for f in "$DATA_DIR"/*.bin; do
    [ -f "$f" ] || { echo "ERROR: No .bin files in $DATA_DIR. Run 'make testdata' first."; exit 1; }
    name=$(basename "$f" .bin)
    SCENARIOS+=("$name")
    LABELS+=("$name")
done

# ── 테이블 ────────────────────────────────────────────────────────────────────
SEP="+-------------+--------+--------+-----------+"

echo ""
printf "| %-11s | %-6s | %-6s | %-9s |\n" "Scenario" "Encode" "Decode" "Roundtrip"
echo "$SEP"

pass_count=0
fail_count=0

for i in "${!SCENARIOS[@]}"; do
    name="${SCENARIOS[$i]}"
    label="${LABELS[$i]}"
    orig="$DATA_DIR/${name}.bin"
    fmio="$TMP/${name}.fmio"
    decoded="$TMP/${name}_dec.bin"

    # Encode
    enc_status="OK"
    if ! "$ENCODER" "$orig" "$fmio" 2>/dev/null; then
        enc_status="ERR"
    fi

    # Decode
    dec_status="OK"
    if [ "$enc_status" = "OK" ]; then
        if ! "$DECODER" "$fmio" "$decoded" 2>/dev/null; then
            dec_status="ERR"
        fi
    else
        dec_status="-"
    fi

    # Diff
    rt_status="FAIL"
    if [ "$enc_status" = "OK" ] && [ "$dec_status" = "OK" ]; then
        if cmp -s "$orig" "$decoded"; then
            rt_status="PASS"
            (( pass_count++ )) || true
        else
            (( fail_count++ )) || true
        fi
    else
        (( fail_count++ )) || true
    fi

    printf "| %-11s | %-6s | %-6s | %-9s |\n" \
        "$label" "$enc_status" "$dec_status" "$rt_status"
done

echo "$SEP"
echo ""

total=${#SCENARIOS[@]}

if [ "$fail_count" -eq 0 ]; then
    echo "Result: ALL PASS  ($pass_count / $total)"
    echo ""
    echo ">>> 알고리즘 확정, HLS 포팅 Go!"
    exit 0
else
    echo "Result: FAIL  ($pass_count passed, $fail_count failed / $total)"
    echo ""
    echo ">>> 버그 수정 필요"
    exit 1
fi
