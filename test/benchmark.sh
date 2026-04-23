#!/usr/bin/env bash
# FM Compression Benchmark
# 5가지 압축 방식 비교: FMIO / FMIO+zstd / zstd / FMIO+lz4 / lz4

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

ENCODER="./build/enc_hello"
DATA_DIR="${DATA_DIR:-test/data/type0}"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

NREPS=10   # 타이밍 반복 횟수

# ── 의존성 확인 ──────────────────────────────────────────────────────────────
OK=1

if [ ! -x "$ENCODER" ]; then
    echo "ERROR: $ENCODER not found. Run 'make all' first."
    OK=0
fi

if ! command -v zstd &>/dev/null; then
    echo "INFO:  zstd not found.  Install: sudo apt install zstd"
    HAVE_ZSTD=0
else
    HAVE_ZSTD=1
fi

if ! command -v lz4 &>/dev/null; then
    echo "INFO:  lz4 not found.   Install: sudo apt install lz4"
    HAVE_LZ4=0
else
    HAVE_LZ4=1
fi

[ "$OK" -eq 1 ] || exit 1

# ── 테스트 데이터 확인 ────────────────────────────────────────────────────────
has_bin=0
for f in "$DATA_DIR"/*.bin; do
    [ -f "$f" ] && has_bin=1 && break
done
if [ "$has_bin" -eq 0 ]; then
    echo "ERROR: No .bin files in $DATA_DIR. Run 'make testdata' first."
    exit 1
fi

# ── 타이밍 헬퍼 ──────────────────────────────────────────────────────────────
# NREPS 번 실행 후 평균 ms 출력 (warmup 2회 포함)
time_avg_ms() {
    local cmd="$1"
    # warmup
    eval "$cmd" >/dev/null 2>&1 || true
    eval "$cmd" >/dev/null 2>&1 || true
    # timed
    local s e
    s=$(date +%s%N)
    for _ in $(seq "$NREPS"); do
        eval "$cmd" >/dev/null 2>&1
    done
    e=$(date +%s%N)
    printf '%d' $(( (e - s) / 1000000 / NREPS ))
}

# 크기를 % 문자열로 변환
pct() { awk "BEGIN{printf \"%.1f%%\", $1/$2*100}"; }

# ms 포맷 (0이면 "<1")
fmt_ms() { [ "$1" -eq 0 ] && echo "<1" || echo "${1}"; }

# ── 시나리오 자동 탐지 (DATA_DIR 내 *.bin) ────────────────────────────────────
NAMES=()
LABELS=()
for f in "$DATA_DIR"/*.bin; do
    name=$(basename "$f" .bin)
    NAMES+=("$name")
    LABELS+=("$name")
done

# ── 결과 저장 배열 ─────────────────────────────────────────────────────────────
declare -A SZ_RAW SZ_FMIO SZ_FMIO_ZSTD SZ_ZSTD SZ_FMIO_LZ4 SZ_LZ4
declare -A TC_FMIO TD_FMIO
declare -A TC_ZSTD_ON_FMIO TD_ZSTD_ON_FMIO  # zstd applied to fmio output
declare -A TC_ZSTD TD_ZSTD
declare -A TC_LZ4_ON_FMIO  TD_LZ4_ON_FMIO
declare -A TC_LZ4  TD_LZ4

echo ""
echo "Benchmarking ... (${NREPS} reps each)"
echo ""

for i in "${!NAMES[@]}"; do
    name="${NAMES[$i]}"
    label="${LABELS[$i]}"
    BIN="$DATA_DIR/${name}.bin"
    FMIO="$TMP/${name}.fmio"
    DEC="$TMP/${name}_dec.bin"

    printf "  %-12s ... " "$label"

    # ── 파일 생성 (1회) ────────────────────────────────────────────────────
    "$ENCODER" "$BIN" "$FMIO"

    SZ_RAW[$name]=$(stat -c%s "$BIN")
    SZ_FMIO[$name]=$(stat -c%s "$FMIO")

    # ── FMIO encode / decode 시간 ──────────────────────────────────────────
    TC_FMIO[$name]=$(time_avg_ms "$ENCODER $BIN $FMIO")
    TD_FMIO[$name]=$(time_avg_ms "$ENCODER -d $FMIO $DEC")

    # ── zstd ───────────────────────────────────────────────────────────────
    if [ "$HAVE_ZSTD" -eq 1 ]; then
        FMIO_ZST="$TMP/${name}.fmio.zst"
        BIN_ZST="$TMP/${name}.bin.zst"

        zstd -q -f "$FMIO" -o "$FMIO_ZST"
        zstd -q -f "$BIN"  -o "$BIN_ZST"

        SZ_FMIO_ZSTD[$name]=$(stat -c%s "$FMIO_ZST")
        SZ_ZSTD[$name]=$(stat -c%s "$BIN_ZST")

        TC_ZSTD_ON_FMIO[$name]=$(time_avg_ms "zstd -q -f $FMIO -o $TMP/t.zst")
        TD_ZSTD_ON_FMIO[$name]=$(time_avg_ms "zstd -q -d -f $FMIO_ZST -o $TMP/t.fmio")
        TC_ZSTD[$name]=$(time_avg_ms "zstd -q -f $BIN -o $TMP/t.zst")
        TD_ZSTD[$name]=$(time_avg_ms "zstd -q -d -f $BIN_ZST -o $TMP/t.bin")
    fi

    # ── lz4 ────────────────────────────────────────────────────────────────
    if [ "$HAVE_LZ4" -eq 1 ]; then
        FMIO_LZ4="$TMP/${name}.fmio.lz4"
        BIN_LZ4="$TMP/${name}.bin.lz4"

        lz4 -q -f "$FMIO" "$FMIO_LZ4"
        lz4 -q -f "$BIN"  "$BIN_LZ4"

        SZ_FMIO_LZ4[$name]=$(stat -c%s "$FMIO_LZ4")
        SZ_LZ4[$name]=$(stat -c%s "$BIN_LZ4")

        TC_LZ4_ON_FMIO[$name]=$(time_avg_ms "lz4 -q -f $FMIO $TMP/t.lz4")
        TD_LZ4_ON_FMIO[$name]=$(time_avg_ms "lz4 -q -f -d $FMIO_LZ4 $TMP/t.fmio")
        TC_LZ4[$name]=$(time_avg_ms "lz4 -q -f $BIN $TMP/t.lz4")
        TD_LZ4[$name]=$(time_avg_ms "lz4 -q -f -d $BIN_LZ4 $TMP/t.bin")
    fi

    echo "done"
done

# ── 테이블 출력 헬퍼 ──────────────────────────────────────────────────────────
SEP1="+-------------+---------+----------+-----------+-----------+----------+-----------+"

print_size_table() {
    echo ""
    echo "┌─────────────────────────────────────────────────────────────────────────────────┐"
    printf "│  %-73s│\n" "Compression Ratio  (compressed / raw × 100,  낮을수록 좋음)"
    echo "└─────────────────────────────────────────────────────────────────────────────────┘"
    echo "$SEP1"
    printf "| %-11s | %7s | %8s | %9s | %9s | %8s | %9s |\n" \
        "Scenario" "Raw(KB)" "FMIO" "FMIO+zstd" "zstd" "FMIO+lz4" "lz4"
    echo "$SEP1"

    for i in "${!NAMES[@]}"; do
        name="${NAMES[$i]}"
        label="${LABELS[$i]}"
        raw=${SZ_RAW[$name]}
        raw_kb=$(awk "BEGIN{printf \"%.1f\", $raw/1024}")

        col_fmio=$(pct "${SZ_FMIO[$name]}" "$raw")

        if [ "$HAVE_ZSTD" -eq 1 ]; then
            col_fmio_zstd=$(pct "${SZ_FMIO_ZSTD[$name]}" "$raw")
            col_zstd=$(pct "${SZ_ZSTD[$name]}" "$raw")
        else
            col_fmio_zstd="N/A"
            col_zstd="N/A"
        fi

        if [ "$HAVE_LZ4" -eq 1 ]; then
            col_fmio_lz4=$(pct "${SZ_FMIO_LZ4[$name]}" "$raw")
            col_lz4=$(pct "${SZ_LZ4[$name]}" "$raw")
        else
            col_fmio_lz4="N/A"
            col_lz4="N/A"
        fi

        printf "| %-11s | %7s | %8s | %9s | %9s | %8s | %9s |\n" \
            "$label" "${raw_kb}K" \
            "$col_fmio" "$col_fmio_zstd" "$col_zstd" \
            "$col_fmio_lz4" "$col_lz4"
    done
    echo "$SEP1"
}

SEP2="+-------------+--------+------------+--------+------------+--------+"

print_time_table() {
    local mode="$1"   # "enc" or "dec"
    local title="$2"

    echo ""
    printf "  %s  (ms, avg of %d runs)\n" "$title" "$NREPS"
    echo "$SEP2"
    printf "| %-11s | %6s | %10s | %6s | %10s | %6s |\n" \
        "Scenario" "FMIO" "FMIO+zstd" "zstd" "FMIO+lz4" "lz4"
    echo "$SEP2"

    for i in "${!NAMES[@]}"; do
        name="${NAMES[$i]}"
        label="${LABELS[$i]}"

        if [ "$mode" = "enc" ]; then
            t_fmio="${TC_FMIO[$name]}"
            t_fmio_zstd=$(( t_fmio + ${TC_ZSTD_ON_FMIO[$name]:-0} ))
            t_zstd="${TC_ZSTD[$name]:-0}"
            t_fmio_lz4=$(( t_fmio + ${TC_LZ4_ON_FMIO[$name]:-0} ))
            t_lz4="${TC_LZ4[$name]:-0}"
        else
            t_fmio="${TD_FMIO[$name]}"
            t_fmio_zstd=$(( ${TD_ZSTD_ON_FMIO[$name]:-0} + t_fmio ))
            t_zstd="${TD_ZSTD[$name]:-0}"
            t_fmio_lz4=$(( ${TD_LZ4_ON_FMIO[$name]:-0} + t_fmio ))
            t_lz4="${TD_LZ4[$name]:-0}"
        fi

        col_zstd=$(     [ "$HAVE_ZSTD" -eq 1 ] && fmt_ms "$t_zstd"      || echo "N/A")
        col_fmio_zstd=$([ "$HAVE_ZSTD" -eq 1 ] && fmt_ms "$t_fmio_zstd" || echo "N/A")
        col_lz4=$(      [ "$HAVE_LZ4"  -eq 1 ] && fmt_ms "$t_lz4"       || echo "N/A")
        col_fmio_lz4=$( [ "$HAVE_LZ4"  -eq 1 ] && fmt_ms "$t_fmio_lz4"  || echo "N/A")

        printf "| %-11s | %6s | %10s | %6s | %10s | %6s |\n" \
            "$label" \
            "$(fmt_ms "$t_fmio")" "$col_fmio_zstd" "$col_zstd" \
            "$col_fmio_lz4" "$col_lz4"
    done
    echo "$SEP2"
}

# ── 결과 출력 ─────────────────────────────────────────────────────────────────
print_size_table
print_time_table "enc" "Encode time"
print_time_table "dec" "Decode time"

echo ""
echo "Legend:"
echo "  FMIO      = FM domain encoder (our encoder)"
echo "  FMIO+zstd = FMIO encode, then zstd compress"
echo "  zstd      = zstd compress on raw .bin"
echo "  FMIO+lz4  = FMIO encode, then lz4 compress"
echo "  lz4       = lz4 compress on raw .bin"
echo "  Ratio: compressed_size / raw_size * 100  (lower = better)"
echo "  Time : FMIO+zstd = FMIO encode time + zstd time (each measured separately)"
echo ""
