/*
 * gen_type0.c
 *
 * type_0 테스트 데이터 생성기 — 순차 주소 + 리텐션 결함.
 * 주소가 BASE_ADDR 부터 순차 증가하며,
 * ROW당 BL 0~7의 레코드 8개를 연속으로 생성한다.
 * IO: sparse single-bit (96-bit 중 1-bit만 set, 리텐션 결함 모사)
 *
 * 출력: test/data/type0/fail_01.bin ~ full_burst_fail.bin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fm_record.h"
#include "gen_common.h"

static int N_RECORDS = 10000;
static int N_ROWS;
#define BASE_ADDR  0x000100000000ULL

/* full_burst_fail 시나리오: fail할 burst 인덱스 */
static const size_t DEFECT_BURST_IDX[] = { 100, 200, 500, 1000, 1200 };
#define N_DEFECT_BURSTS  (sizeof(DEFECT_BURST_IDX) / sizeof(DEFECT_BURST_IDX[0]))

/* --------------------------------------------------------------------------
 * BASE_ADDR 부터 순차 주소 (BL 0~7 × N_ROWS)
 * -------------------------------------------------------------------------- */
static void fill_addr_seq(fm_record_t *records, size_t n_records)
{
    for (size_t i = 0; i < n_records; i++)
        fm_u64_to_addr(BASE_ADDR + (uint64_t)i, records[i].addr);
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

static void gen_full_burst_fail(fm_record_t *records)
{
    int n_fail = 0;
    for (size_t r = 0; r < N_ROWS; r++) {
        int is_defect = 0;
        for (size_t d = 0; d < N_DEFECT_BURSTS; d++)
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
           n_fail, N_RECORDS, 100.0 * n_fail / N_RECORDS, N_DEFECT_BURSTS);
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
    int n_fail = 0;
    for (size_t r = 0; r < N_ROWS; r++) {
        int is_defect = 0;
        for (size_t d = 0; d < N_DEFECT_BURSTS; d++)
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
           n_fail, N_RECORDS, 100.0 * n_fail / N_RECORDS, N_DEFECT_BURSTS);
}

/* --------------------------------------------------------------------------
 * generate_type0 — 통합 바이너리(gen_testdata)에서도 호출 가능
 * -------------------------------------------------------------------------- */
int generate_type0(int n_records)
{
    N_RECORDS = n_records;
    N_ROWS = N_RECORDS / 8;

    srand(0x464D1234);   /* 고정 시드: 재현 가능 (기존 gen_testdata 호환) */

    fm_record_t *records = malloc(N_RECORDS * sizeof(fm_record_t));
    if (!records) { fprintf(stderr, "out of memory\n"); return 1; }

    printf("Generating FM type0 test data  (%d records/scenario, %d rows/scenario)\n\n",
           N_RECORDS, N_ROWS);

    fill_addr_seq(records, N_RECORDS);

    static const struct {
        const char *filename;
        const char *label;
        int         fail_per_1000;
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
        gen_path(path, sizeof(path), "type0", S[i].filename);

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
#include <sys/stat.h>
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
    g_outdir = ".";
    mkdir("type0", 0755);
    return generate_type0(n);
}
#endif
