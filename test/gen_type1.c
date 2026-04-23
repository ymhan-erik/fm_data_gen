/*
 * gen_type1.c
 *
 * type_1 테스트 데이터 생성기 — 랜덤 주소 + 리텐션 결함.
 * type_0과 달리 주소가 랜덤이며,
 * ROW당 BL 0~7의 레코드 8개를 연속으로 생성한다.
 * IO: sparse single-bit (96-bit 중 1-bit만 set, 리텐션 결함 모사)
 *
 *   N_RECORDS / 8 개의 ROW를 랜덤 생성
 *   각 ROW마다 BL=0~7 레코드 8개 → 총 N_RECORDS개
 *
 * 출력: test/data/type1/fail_01.bin ~ full_burst_fail.bin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fm_record.h"
#include "gen_common.h"

static int N_RECORDS = 10000;   /* 8의 배수여야 함 */
static int N_ROWS;

/* --------------------------------------------------------------------------
 * 랜덤 ROW 주소 생성 후 BL 0~7 전개
 * -------------------------------------------------------------------------- */
static void fill_addr_random_8bl(fm_record_t *records, size_t n_records)
{
    size_t n_rows = n_records / 8;
    for (size_t r = 0; r < n_rows; r++) {
        /* 48-bit 랜덤 주소, 하위 3비트 = 0 (BL align) */
        uint64_t base = (  ((uint64_t)(rand() & 0xFFFF) << 32)
                         | ((uint64_t)(rand() & 0xFFFF) << 16)
                         | ((uint64_t)(rand() & 0xFFFF))       )
                        & 0xFFFFFFFFFFFFULL;
        base &= ~7ULL;

        /* BL=0~7 레코드 8개 */
        for (int bl = 0; bl < 8; bl++)
            fm_u64_to_addr(base | (uint64_t)bl, records[r * 8 + bl].addr);
    }
}

/* --------------------------------------------------------------------------
 * IO 채우기 함수들
 * -------------------------------------------------------------------------- */
static void gen_fail_pct(fm_record_t *records, int fail_per_1000)
{
    int n_fail = 0;
    for (size_t i = 0; i < N_RECORDS; i++) {
        if (rand() % 1000 < fail_per_1000) {
            /* sparse single-bit IO: 96-bit 중 1-bit만 set (리텐션 결함 모사) */
            memset(records[i].io, 0, FM_IO_SIZE);
            int bit_pos = rand() % 96;
            records[i].io[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
            n_fail++;
        } else {
            memset(records[i].io, 0, FM_IO_SIZE);
        }
    }
    printf("  fail: %5d / %d  (%.1f%%)\n",
           n_fail, N_RECORDS, 100.0 * n_fail / N_RECORDS);
}

static void gen_fail_100(fm_record_t *records)
{
    for (size_t i = 0; i < N_RECORDS; i++)
        memset(records[i].io, 0xFF, FM_IO_SIZE);
    printf("  fail: %5d / %d  (100.0%%)\n", N_RECORDS, N_RECORDS);
}

/* full_burst_fail: N_ROWS 중 5개 burst 전체 BL fail */
static void gen_full_burst_fail(fm_record_t *records)
{
    /* defect burst 인덱스 (N_ROWS 내) */
    static const size_t DEFECT_BURST_IDX[] = { 10, 50, 200, 500, 1000 };
    size_t n_defect = sizeof(DEFECT_BURST_IDX) / sizeof(DEFECT_BURST_IDX[0]);

    int n_fail = 0;
    for (size_t r = 0; r < N_ROWS; r++) {
        int is_defect = 0;
        for (size_t d = 0; d < n_defect; d++)
            if (r == DEFECT_BURST_IDX[d]) { is_defect = 1; break; }

        for (int bl = 0; bl < 8; bl++) {
            if (is_defect) {
                memset(records[r * 8 + bl].io, 0xFF, FM_IO_SIZE);
                n_fail++;
            } else {
                memset(records[r * 8 + bl].io, 0x00, FM_IO_SIZE);
            }
        }
    }
    printf("  fail: %5d / %d  (%.1f%%)  [%zu defect bursts x 8 BL]\n",
           n_fail, N_RECORDS, 100.0 * n_fail / N_RECORDS, n_defect);
}

static void gen_fail_100_sparse(fm_record_t *records)
{
    for (size_t i = 0; i < N_RECORDS; i++) {
        memset(records[i].io, 0, FM_IO_SIZE);
        int bit_pos = rand() % 96;
        records[i].io[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
    }
    printf("  fail: %5d / %d  (100.0%%, sparse)\n", N_RECORDS, N_RECORDS);
}

static void gen_full_burst_fail_sparse(fm_record_t *records)
{
    static const size_t DEFECT_BURST_IDX[] = { 10, 50, 200, 500, 1000 };
    size_t n_defect = sizeof(DEFECT_BURST_IDX) / sizeof(DEFECT_BURST_IDX[0]);

    int n_fail = 0;
    for (size_t r = 0; r < N_ROWS; r++) {
        int is_defect = 0;
        for (size_t d = 0; d < n_defect; d++)
            if (r == DEFECT_BURST_IDX[d]) { is_defect = 1; break; }

        for (int bl = 0; bl < 8; bl++) {
            if (is_defect) {
                memset(records[r * 8 + bl].io, 0, FM_IO_SIZE);
                int bit_pos = rand() % 96;
                records[r * 8 + bl].io[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
                n_fail++;
            } else {
                memset(records[r * 8 + bl].io, 0x00, FM_IO_SIZE);
            }
        }
    }
    printf("  fail: %5d / %d  (%.1f%%, sparse)  [%zu defect bursts x 8 BL]\n",
           n_fail, N_RECORDS, 100.0 * n_fail / N_RECORDS, n_defect);
}

/* --------------------------------------------------------------------------
 * generate_type1 — 통합 바이너리(gen_testdata)에서도 호출 가능
 * -------------------------------------------------------------------------- */
int generate_type1(int n_records)
{
    N_RECORDS = n_records;
    N_ROWS = N_RECORDS / 8;

    srand(0x54595031);   /* "TYP1" 고정 시드: 재현 가능 */

    fm_record_t *records = malloc(N_RECORDS * sizeof(fm_record_t));
    if (!records) { fprintf(stderr, "out of memory\n"); return 1; }

    printf("Generating FM type1 test data  (%d records/scenario, %d rows/scenario)\n\n",
           N_RECORDS, N_ROWS);

    /* 주소는 모든 시나리오 공통 (한 번만 생성) */
    fill_addr_random_8bl(records, N_RECORDS);

    static const struct {
        const char *filename;
        const char *label;
        int         fail_per_1000;  /* -1=100%fail  -2=full_burst_fail */
    } S[] = {
        { "fail_01.bin",          "fail  1%",    10  },
        { "fail_05.bin",          "fail  5%",    50  },
        { "fail_10.bin",          "fail 10%",   100  },
        { "fail_20.bin",          "fail 20%",   200  },
        { "fail_50.bin",          "fail 50%",   500  },
        { "fail_100.bin",         "fail 100%",   -1  },
        { "full_burst_fail.bin",  "full burst fail",  -2  },
        { "fail_100_sparse.bin",        "fail 100% sparse",   -3 },
        { "full_burst_fail_sparse.bin", "full burst sparse",  -4 },
    };
    int n = (int)(sizeof(S) / sizeof(S[0]));

    for (int i = 0; i < n; i++) {
        char path[256];
        gen_path(path, sizeof(path), "type1", S[i].filename);

        printf("[%d/%d] %s\n", i + 1, n, S[i].label);
        if      (S[i].fail_per_1000 == -1) gen_fail_100(records);
        else if (S[i].fail_per_1000 == -2) gen_full_burst_fail(records);
        else if (S[i].fail_per_1000 == -3) gen_fail_100_sparse(records);
        else if (S[i].fail_per_1000 == -4) gen_full_burst_fail_sparse(records);
        else                               gen_fail_pct(records, S[i].fail_per_1000);

        FILE *fp = fopen(path, "wb");
        if (!fp) { perror(path); free(records); return 1; }
        fwrite(records, FM_RECORD_SIZE, N_RECORDS, fp);
        fclose(fp);
        printf("  -> %-40s  %d bytes\n",
               path, N_RECORDS * FM_RECORD_SIZE);
        putchar('\n');
    }

    printf("Done. %d scenarios x %d records (%d rows x 8 BL)\n",
           n, N_RECORDS, N_ROWS);
    free(records);
    return 0;
}

#ifndef GEN_TESTDATA_COMBINED
int main(int argc, char *argv[])
{
    int n = 10000;
    if (argc >= 2) {
        n = atoi(argv[1]);
        if (n < 8 || n % 8 != 0) {
            fprintf(stderr, "Error: N must be >= 8 and multiple of 8\n");
            return 1;
        }
    }
    return generate_type1(n);
}
#endif
