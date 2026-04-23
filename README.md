# fm_data_gen

ATE DRAM 테스트용 FM(Fail Map) 데이터 생성·압축·검증 도구.

## FM Record 포맷

레코드 1개 = **18 bytes**

| 필드 | 크기 | 설명 |
|------|------|------|
| ADDR | 6 bytes (48-bit) | DRAM 주소. 하위 3 bit = BL(0-7), 상위 45 bit = 행 주소 |
| IO   | 12 bytes (96-bit) | XOR 페일 맵 비트필드 |

## Quick Start

```bash
make all          # enc_hello, decoder, gen_testdata 빌드
make testdata     # test/data/type0~type8/ 에 61개 .bin 생성 (기본 10,000 레코드)
make roundtrip    # 61/61 인코드→디코드→diff bit-exact 검증
```

레코드 수 변경:

```bash
make testdata N=1000000   # 100만 레코드로 생성
```

## 빌드 타겟

| 타겟 | 설명 |
|------|------|
| `make all` | enc_hello, decoder, gen_testdata 빌드 |
| `make testdata` | type0~type8 전체 테스트 데이터 생성 |
| `make testdata_type0` ~ `make testdata_type8` | 개별 타입 생성 |
| `make roundtrip` | 전체 인코드→디코드→diff 검증 |
| `make benchmark` | 타입별 압축 성능 비교 |
| `make dump_records` | 바이너리 → 텍스트 덤프 도구 빌드 |
| `make dump_patterns` | 타입별 IO 패턴 터미널 출력 |
| `make dump_patterns_md` | docs/test_data_patterns.md 재생성 |
| `make extract_records` | 바이너리 레코드 부분 추출 도구 빌드 |
| `make clean` | 빌드 산출물 삭제 |

## 테스트 데이터 유형

모든 타입 공통: 버스트당 BL 0~7 연속 8개.

| 타입 | 이름 | ADDR 패턴 | IO 패턴 |
|------|------|----------|---------|
| type0 | 순차+리텐션 | `BASE_ADDR + i` (연속 증가) | sparse 1-bit (리텐션 결함) |
| type1 | 랜덤+리텐션 | 버스트 랜덤 생성 | sparse 1-bit (리텐션 결함) |
| type2 | 고엔트로피 IO | 순차 (type0 동일) | 랜덤 12바이트 (스트레스) |
| type3 | 뱅크 인터리브 | 4뱅크 교차 스캔 (간격 0x4000) | 확률적 랜덤 fail |
| type4 | 컬럼 결함 | 순차 (type0 동일) | 모든 레코드 동일 IO 비트 |
| type5 | 블록 결함 | 순차 (type0 동일) | 특정 burst 범위만 집중 실패 |
| type6 | 역방향 스캔 | 버스트 역순, BL 내 0→7 | sparse 1-bit (리텐션 결함) |
| type7 | 혼합 결함 | 순차 (type0 동일) | 컬럼+블록+단일셀 합성 |
| type8 | 클러스터 결함 | 순차 (type0 동일) | 클러스터 내 sparse 1-bit |

## 시나리오별 파일 목록

### type0, type1, type2, type3, type6 (각 9개)

| 파일 | 설명 |
|------|------|
| `fail_01.bin` | 페일률 1% |
| `fail_05.bin` | 페일률 5% |
| `fail_10.bin` | 페일률 10% |
| `fail_20.bin` | 페일률 20% |
| `fail_50.bin` | 페일률 50% |
| `fail_100.bin` | 페일률 100% (전체 페일, IO=all-0xFF dense) |
| `full_burst_fail.bin` | 특정 5개 버스트 × 8 BL = 40개 페일 (IO=all-0xFF dense) |
| `fail_100_sparse.bin` / `fail_100_native.bin` | 페일률 100% (타입 고유 IO 패턴 유지) |
| `full_burst_fail_sparse.bin` / `full_burst_fail_native.bin` | 5개 버스트 페일 (타입 고유 IO 패턴 유지) |

### type4 (4개)

| 파일 | 설명 |
|------|------|
| `col_1dq.bin` | 1개 DQ 결함 |
| `col_4dq.bin` | 4개 DQ 결함 |
| `col_16dq.bin` | 16개 DQ 결함 |
| `col_all.bin` | 96개 DQ 전체 결함 |

### type5 (4개)

| 파일 | 설명 |
|------|------|
| `block_small.bin` | burst 500~509 (10 bursts, 80 fail) |
| `block_medium.bin` | burst 100~299 (200 bursts, 1600 fail) |
| `block_large.bin` | burst 0~499 (500 bursts, 4000 fail) |
| `block_multi.bin` | 3블록 (bursts 50~69 + 400~419 + 900~949, 720 fail) |

### type7 (4개)

| 파일 | 설명 |
|------|------|
| `mixed_light.bin` | 컬럼 1 DQ + 단일셀 1% |
| `mixed_moderate.bin` | 컬럼 2 DQ + 블록 burst 200~209 + 단일셀 5% |
| `mixed_heavy.bin` | 컬럼 4 DQ + 블록 burst 100~299 + 단일셀 10% |
| `mixed_extreme.bin` | 컬럼 8 DQ + 블록 burst 0~249+500~749 + 단일셀 20% |

### type8 (4개)

| 파일 | 설명 |
|------|------|
| `cluster_sparse.bin` | 2 클러스터, r=15, 내부 30% |
| `cluster_medium.bin` | 4 클러스터, r=20, 내부 40% |
| `cluster_dense.bin` | 6 클러스터, r=25, 내부 50% |
| `cluster_overlap.bin` | 3 클러스터, r=50, 내부 40% (겹침 허용) |

## .fmio 압축 파일 포맷

```
[Header 48 bytes]
  magic        4B  0x464D494F ("FMIO"), little-endian
  version      4B  2
  record_count 8B  레코드 수
  addr_offset  8B  ADDR 압축 블록 파일 오프셋
  addr_size    8B  ADDR 압축 블록 크기 (bytes)
  io_offset    8B  IO 압축 블록 파일 오프셋
  io_size      8B  IO 압축 블록 크기 (bytes)
[ADDR 압축 블록]
[IO 압축 블록]
```

### ADDR 압축 (Delta Encoding + Bit-stream, MSB-first)

첫 레코드는 prefix 없이 addr48만 기록.

| 케이스 | 비트 패턴 | 총 비트 |
|--------|-----------|---------|
| delta == +1 | `0` | 1 |
| delta == -1 | `1 0` | 2 |
| -32768 ≤ delta ≤ +32767 (delta != +/-1) | `1 1 0` + s16 MSB-first | 19 |
| 그 외 | `1 1 1` + addr48 MSB-first | 51 |

### IO 압축 (Bitmap RLE)

| 토큰 | 바이트 | 설명 |
|------|--------|------|
| Token A | `0x00` + `u32le count` | 연속 all-zero IO 레코드 count개 |
| Token B | `0xFF` + 12 bytes io | IO가 0이 아닌 레코드 1개 |
| Token C | `0xAA` + `u32le count` | 직전 non-zero IO를 count회 반복 |

## IO 패턴 분석

`docs/test_data_patterns.md` 참조. 또는:

```bash
make dump_patterns     # 터미널에 타입별 IO 패턴 출력
make dump_patterns_md  # docs/test_data_patterns.md 재생성
```
