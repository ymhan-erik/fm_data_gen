/*
 * gen_type3.c
 *
 * type_3 테스트 데이터 생성기 — 뱅크 인터리브.
 * 4뱅크 인터리브 주소 패턴: Bank0~3을 교차 스캔.
 * ROW당 BL 0~7의 레코드 8개를 연속으로 생성한다.
 * IO: fail_pct 확률로 랜덤 바이트 (멀티뱅크 병렬 테스트)
 *
 * 스캔 순서:
 *   Bank0 row0(8BL) -> Bank1 row0(8BL) -> Bank2 row0(8BL) -> Bank3 row0(8BL)
 *   -> Bank0 row1(8BL) -> ...
 *
 * 출력: test/data/type3/fail_01.bin ~ full_burst_fail.bin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fm_record.h"
#include "gen_common.h"

static int N_RECORDS = 10000;
static int N_ROWS;
#define N_BANKS       4
#define BANK_SPACING  0x4000ULL   /* 16384 */

static const uint64_t BANK_BASES[N_BANKS] = {
    0x000100000000ULL,
    0x000100000000ULL + BANK_SPACING,
    0x000100000000ULL + BANK_SPACING * 2,
    0x000100000000ULL + BANK_SPACING * 3,
};

/* full_burst_fail 시나리오: fail할 burst 인덱스 (N_ROWS 내) */
static const size_t DEFECT_BURST_IDX[] = { 100, 200, 500, 1000, 1200 };
#define N_DEFECT_BURSTS  (sizeof(DEFECT_BURST_IDX) / sizeof(DEFECT_BURST_IDX[0]))

/* --------------------------------------------------------------------------
 * 4뱅크 인터리브 주소: Bank0~3을 라운드로빈하며 row 증가
 * -------------------------------------------------------------------------- */
static void fill_addr_bank_interleave(fm_record_t *records, size_t n_records)
{
    size_t n_rows = n_records / 8;
    size_t idx = 0;
    size_t bank_row[N_BANKS];
    memset(bank_row, 0, sizeof(bank_row));

    /* 라운드로빈: bank 0,1,2,3,0,1,2,3,... */
    for (size_t r = 0; r < n_rows; r++) {
        size_t bank = r % N_BANKS;
        uint64_t row_addr = BANK_BASES[bank] + bank_row[bank] * 8;

        for (int bl = 0; bl < 8; bl++) {
            fm_u64_to_addr(row_addr | (uint64_t)bl, records[idx].addr);
            idx++;
        }
        bank_row[bank]++;
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
            for (int b = 0; b < FM_IO_SIZE; b++)
                records[i].io[b] = (uint8_t)(rand() & 0xFF);
            records[i].io[0] |= 0x01;
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

static void gen_fail_100_native(fm_record_t *records)
{
    for (size_t i = 0; i < N_RECORDS; i++) {
        for (int b = 0; b < FM_IO_SIZE; b++)
            records[i].io[b] = (uint8_t)(rand() & 0xFF);
        records[i].io[0] |= 0x01;   /* non-zero 보장 */
    }
    printf("  fail: %5d / %d  (100.0%%, random 12B)\n", N_RECORDS, N_RECORDS);
}

static void gen_full_burst_fail_native(fm_record_t *records)
{
    int n_fail = 0;
    for (size_t r = 0; r < N_ROWS; r++) {
        int is_defect = 0;
        for (size_t d = 0; d < N_DEFECT_BURSTS; d++)
            if (r == DEFECT_BURST_IDX[d]) { is_defect = 1; break; }

        for (int bl = 0; bl < 8; bl++) {
            if (is_defect) {
                for (int b = 0; b < FM_IO_SIZE; b++)
                    records[r * 8 + bl].io[b] = (uint8_t)(rand() & 0xFF);
                records[r * 8 + bl].io[0] |= 0x01;
                n_fail++;
            } else {
                memset(records[r * 8 + bl].io, 0x00, FM_IO_SIZE);
            }
        }
    }
    printf("  fail: %5d / %d  (%.1f%%, random 12B)  [%zu defect bursts x 8 BL]\n",
           n_fail, N_RECORDS, 100.0 * n_fail / N_RECORDS, N_DEFECT_BURSTS);
}

/* --------------------------------------------------------------------------
 * generate_type3 — 통합 바이너리(gen_testdata)에서도 호출 가능
 * -------------------------------------------------------------------------- */
int generate_type3(int n_records)
{
    N_RECORDS = n_records;
    N_ROWS = N_RECORDS / 8;

    srand(0x54595033);   /* "TYP3" 고정 시드 */

    fm_record_t *records = malloc(N_RECORDS * sizeof(fm_record_t));
    if (!records) { fprintf(stderr, "out of memory\n"); return 1; }

    printf("Generating FM type3 test data  (%d records/scenario, %d rows/scenario)\n",
           N_RECORDS, N_ROWS);
    printf("  4-bank interleave, bank spacing = 0x%llX\n\n",
           (unsigned long long)BANK_SPACING);

    fill_addr_bank_interleave(records, N_RECORDS);

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
        { "fail_100_native.bin",        "fail 100% random 12B",  -3 },
        { "full_burst_fail_native.bin", "full burst random 12B", -4 },
    };
    int n = (int)(sizeof(S) / sizeof(S[0]));

    for (int i = 0; i < n; i++) {
        char path[256];
        gen_path(path, sizeof(path), "type3", S[i].filename);

        printf("[%d/%d] %s\n", i + 1, n, S[i].label);
        if      (S[i].fail_per_1000 == -1) gen_fail_100(records);
        else if (S[i].fail_per_1000 == -2) gen_full_burst_fail(records);
        else if (S[i].fail_per_1000 == -3) gen_fail_100_native(records);
        else if (S[i].fail_per_1000 == -4) gen_full_burst_fail_native(records);
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
    return generate_type3(n);
}
#endif
