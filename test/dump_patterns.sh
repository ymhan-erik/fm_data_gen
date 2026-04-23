#!/usr/bin/env bash
# dump_patterns.sh — 타입별 대표 시나리오의 특징적 구간을 dump_records로 출력
# Usage: bash dump_patterns.sh [--md]
#   --md : Markdown 형식으로 출력 (docs/test_data_patterns.md 생성용)
set -euo pipefail

DUMP=${DUMP:-./build/dump_records}
DATA_DIR=${DATA_DIR:-test/data}
MD_MODE=0

if [[ "${1:-}" == "--md" ]]; then
    MD_MODE=1
fi

if [[ ! -x "$DUMP" ]]; then
    echo "Error: $DUMP not found. Run 'make dump_records' first." >&2
    exit 1
fi

show_section() {
    local title="$1" desc="$2" file="$3" offset="$4" count="$5"
    if [[ ! -f "$file" ]]; then
        echo "WARNING: $file not found, skipping." >&2
        echo ""
        return
    fi
    if [[ $MD_MODE -eq 1 ]]; then
        echo "### $title"
        echo ""
        echo "> **주목:** $desc"
        echo ""
        echo '```'
        "$DUMP" "$file" "$offset" "$count"
        echo '```'
        echo ""
    else
        echo "================================================================"
        printf " %s\n" "$title"
        printf " 주목: %s\n" "$desc"
        echo "================================================================"
        "$DUMP" "$file" "$offset" "$count"
        echo ""
    fi
}

if [[ $MD_MODE -eq 1 ]]; then
    cat <<'HEADER'
# FM 테스트 데이터 패턴 특징 요약

각 타입별 대표 시나리오에서 특징적인 레코드 구간을 추출한 것입니다.
레코드 1개 = 18 bytes: ADDR(6B, 48-bit) + IO(12B, 96-bit XOR fail map).

| 필드 | 설명 |
|------|------|
| ADDR | 48-bit DRAM 주소. 하위 3bit = BL(0~7), 상위 45bit = burst 주소 |
| IO | 96-bit XOR 페일 맵 (all-zero = pass, non-zero = fail) |
| BL | Burst Length index (0~7), ADDR 하위 3bit |
| BURST | burst 주소 (ADDR >> 3) |

---

## Fail Rate(%) 생성 방식

`fail_01.bin` ~ `fail_50.bin` 같은 시나리오 파일명의 숫자는 **레코드 단위 fail 확률**을 의미합니다.

### 핵심 로직 (type0/1/6 기준)

```c
for each record (10,000개):
    if rand() % 1000 < fail_per_1000:    // 레코드 단위 확률 판정
        IO = 0                            // 96-bit 클리어
        bit_pos = rand() % 96             // 96-bit 중 랜덤 1-bit 위치 선택
        IO[bit_pos] = 1                   // sparse single-bit set (리텐션 결함)
    else:
        IO = 0                            // pass (IO 전부 0)
```

### 시나리오별 파라미터

| 파일 | fail_per_1000 | 의미 | IO 내용 |
|------|--------------|------|---------|
| fail_01.bin | 10 | ~1% 레코드가 fail | 96-bit 중 1-bit만 set |
| fail_05.bin | 50 | ~5% 레코드가 fail | 96-bit 중 1-bit만 set |
| fail_10.bin | 100 | ~10% 레코드가 fail | 96-bit 중 1-bit만 set |
| fail_20.bin | 200 | ~20% 레코드가 fail | 96-bit 중 1-bit만 set |
| fail_50.bin | 500 | ~50% 레코드가 fail | 96-bit 중 1-bit만 set |
| fail_100.bin | (특수) | 100% 전체 fail | IO = 0xFF...FF (96-bit 전부 1, dense) |
| full_burst_fail.bin | (특수) | 특정 5개 burst만 fail | 해당 burst IO=0xFF, 나머지 0x00 (dense) |
| fail_100_sparse.bin | (특수) | 100% 전체 fail | 타입 고유 IO 패턴 유지 (type0/1/6: sparse 1-bit) |
| full_burst_fail_sparse.bin | (특수) | 특정 5개 burst만 fail | 타입 고유 IO 패턴 유지 |
| fail_100_native.bin | (특수) | 100% 전체 fail | 타입 고유 IO 패턴 유지 (type2/3: 랜덤 12B) |
| full_burst_fail_native.bin | (특수) | 특정 5개 burst만 fail | 타입 고유 IO 패턴 유지 |

**참고**: `_sparse`/`_native` 시나리오는 fail rate만 변경하고 IO 패턴은 해당 타입의 fail_01~50과 동일하게 유지합니다. 기존 `fail_100`/`full_burst_fail`은 all-0xFF(dense) 패턴으로, 다른 결함 모드(전체 DQ 오픈)를 모사합니다.

**핵심**: fail 레코드의 IO에는 96-bit 중 **딱 1-bit만** set됩니다. 이는 DRAM 리텐션 결함(retention fault)을 모사한 것으로, 셀 1개만 데이터를 유지하지 못하는 상황입니다.

---

## 타입별 IO 생성 방식 비교

| 타입 | IO 패턴 | fail 판정 | 설명 |
|------|---------|----------|------|
| type0, 1, 6 | sparse 1-bit | 확률적 (레코드별 rand) | 리텐션 결함 — 96-bit 중 1-bit만 |
| type2, 3 | 랜덤 12B | 확률적 (레코드별 rand) | 고엔트로피 — IO 12바이트 전체 랜덤 (압축 worst-case) |
| type4 | 고정 DQ 마스크 | 결정적 (전 레코드 동일) | 컬럼 결함 — 모든 레코드가 동일 IO 패턴 |
| type5 | 0xFF / 0x00 | 결정적 (burst 범위) | 블록 결함 — 특정 burst 범위만 96-bit 전부 fail |
| type7 | col + block + 1-bit | 복합 (세 모드 OR) | 혼합 — DQ 마스크 + 블록 범위 + 랜덤 1-bit 합성 |
| type8 | sparse 1-bit (지역) | 확률적 (공간 제한) | 클러스터 — 특정 영역 내에서만 sparse fail 발생 |

**압축 관점에서의 의미**:
- type0/1/6: IO 대부분 0x00 → Token A(zero-run)가 길게 나옴 → **높은 압축률**
- type4: 모든 IO 동일 → Token C(repeat)로 한번에 처리 → **매우 높은 압축률**
- type2/3: IO가 매번 다름 → Token B(raw 13B) 연속 → **최저 압축률** (스트레스 테스트)
- fail_50% 근방: Token A/C 모두 짧게 단편화 → **실질적 최악 압축률**

---

HEADER
fi

# TYPE0: 순차+리텐션 — ADDR +1 순차 증가, sparse 1-bit IO
show_section \
    "TYPE0: 순차+리텐션 (fail_10.bin, records 0~15)" \
    "ADDR +1 순차 증가, IO는 96-bit 중 1-bit만 set (sparse)" \
    "$DATA_DIR/type0/fail_10.bin" 0 16

# TYPE1: 랜덤+리텐션 — burst 간 ADDR 불연속
show_section \
    "TYPE1: 랜덤+리텐션 (fail_10.bin, records 0~15)" \
    "랜덤 ADDR (burst 간 불연속 점프), IO는 sparse 1-bit" \
    "$DATA_DIR/type1/fail_10.bin" 0 16

# TYPE2: 고엔트로피 IO (fail_10.bin, records 0~15)
show_section \
    "TYPE2: 고엔트로피 IO (fail_10.bin, records 0~15)" \
    "순차 ADDR, IO가 랜덤 12바이트 (고엔트로피, Token B 연속)" \
    "$DATA_DIR/type2/fail_10.bin" 0 16

# TYPE3: 뱅크 인터리브 — 4뱅크 교차 (0x4000 간격 점프)
show_section \
    "TYPE3: 뱅크 인터리브 (fail_10.bin, records 0~31)" \
    "ADDR가 Bank0->1->2->3 교차 (0x4000 간격), BL 0~7 반복" \
    "$DATA_DIR/type3/fail_10.bin" 0 32

# TYPE4: 컬럼 결함 — 모든 IO 동일 DQ 마스크
show_section \
    "TYPE4: 컬럼 결함 (col_4dq.bin, records 0~15)" \
    "모든 IO가 동일 패턴 (DQ 마스크 반복, Token C 최적)" \
    "$DATA_DIR/type4/col_4dq.bin" 0 16

# TYPE5: 블록 결함 — clean→defect 전환 경계
show_section \
    "TYPE5: 블록 결함 (block_multi.bin, records 392~415)" \
    "burst 49~51 경계: clean(IO=0) -> defect(IO!=0) 전환" \
    "$DATA_DIR/type5/block_multi.bin" 392 24

# TYPE6: 역방향 스캔 — ADDR 감소
show_section \
    "TYPE6: 역방향 스캔 (fail_10.bin, records 0~15)" \
    "ADDR 감소 (역방향), BL 0~7 순서 유지, delta=-1 최적화 대상" \
    "$DATA_DIR/type6/fail_10.bin" 0 16

# TYPE7: 혼합 결함 — col_mask + block 전환 경계
show_section \
    "TYPE7: 혼합 결함 (mixed_moderate.bin, records 1592~1615)" \
    "burst 199~201 경계: col_mask + block defect 전환" \
    "$DATA_DIR/type7/mixed_moderate.bin" 1592 24

# TYPE8: 클러스터 결함 — clean→cluster 진입 구간
show_section \
    "TYPE8: 클러스터 결함 (cluster_medium.bin, records 632~663)" \
    "clean -> cluster 진입 구간 (sparse 1-bit 집중)" \
    "$DATA_DIR/type8/cluster_medium.bin" 632 32

if [[ $MD_MODE -eq 0 ]]; then
    echo "Done. 9 type sections dumped."
fi
