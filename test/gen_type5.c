/*
 * gen_type5.c
 *
 * type_5 테스트 데이터 생성기 — 블록 결함.
 * 주소는 type0과 동일 (순차), IO는 블록 결함 패턴.
 * 특정 row 범위만 집중적으로 실패하는 MAT/Sub-array 결함을 시뮬레이션한다.
 *
 * 출력: test/data/type5/block_small.bin ~ block_multi.bin
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

/* --------------------------------------------------------------------------
 * 순차 주소 (type0과 동일)
 * -------------------------------------------------------------------------- */
static void fill_addr_seq(fm_record_t *records, size_t n_records)
{
    for (size_t i = 0; i < n_records; i++)
        fm_u64_to_addr(BASE_ADDR + (uint64_t)i, records[i].addr);
}

/* --------------------------------------------------------------------------
 * 블록 결함 구조체
 * -------------------------------------------------------------------------- */
typedef struct {
    size_t start_row;
    size_t end_row;    /* inclusive */
} block_range_t;

/* --------------------------------------------------------------------------
 * 블록 결함 IO 생성: 지정된 row 범위의 레코드만 0xFF, 나머지 0x00
 * -------------------------------------------------------------------------- */
static void gen_block_defect(fm_record_t *records,
                             const block_range_t *blocks, size_t n_blocks)
{
    int n_fail = 0;
    for (size_t r = 0; r < N_ROWS; r++) {
        int in_block = 0;
        for (size_t b = 0; b < n_blocks; b++) {
            if (r >= blocks[b].start_row && r <= blocks[b].end_row) {
                in_block = 1;
                break;
            }
        }

        for (int bl = 0; bl < 8; bl++) {
            if (in_block) {
                memset(records[r * 8 + bl].io, 0xFF, FM_IO_SIZE);
                n_fail++;
            } else {
                memset(records[r * 8 + bl].io, 0x00, FM_IO_SIZE);
            }
        }
    }
    printf("  fail: %5d / %d  (%.1f%%)\n",
           n_fail, N_RECORDS, 100.0 * n_fail / N_RECORDS);
}

/* --------------------------------------------------------------------------
 * generate_type5 — 통합 바이너리(gen_testdata)에서도 호출 가능
 * -------------------------------------------------------------------------- */
int generate_type5(int n_records)
{
    N_RECORDS = n_records;
    N_ROWS = N_RECORDS / 8;

    srand(0x54595035);   /* "TYP5" 고정 시드 */

    fm_record_t *records = malloc(N_RECORDS * sizeof(fm_record_t));
    if (!records) { fprintf(stderr, "out of memory\n"); return 1; }

    printf("Generating FM type5 test data  (%d records/scenario, %d rows/scenario)\n",
           N_RECORDS, N_ROWS);
    printf("  Block defect patterns\n\n");

    fill_addr_seq(records, N_RECORDS);

    /* -- block_small: row 500~509 (10 rows, 80 fail) -- */
    {
        char path[256];
        gen_path(path, sizeof(path), "type5", "block_small.bin");
        static const block_range_t blocks[] = { {500, 509} };
        printf("[1/4] block_small  (rows 500-509)\n");
        gen_block_defect(records, blocks, 1);

        FILE *fp = fopen(path, "wb");
        if (!fp) { perror(path); free(records); return 1; }
        fwrite(records, FM_RECORD_SIZE, N_RECORDS, fp);
        fclose(fp);
        printf("  -> %-40s  %d bytes\n\n", path, N_RECORDS * FM_RECORD_SIZE);
    }

    /* -- block_medium: row 100~299 (200 rows, 1600 fail) -- */
    {
        char path[256];
        gen_path(path, sizeof(path), "type5", "block_medium.bin");
        static const block_range_t blocks[] = { {100, 299} };
        printf("[2/4] block_medium  (rows 100-299)\n");
        gen_block_defect(records, blocks, 1);

        FILE *fp = fopen(path, "wb");
        if (!fp) { perror(path); free(records); return 1; }
        fwrite(records, FM_RECORD_SIZE, N_RECORDS, fp);
        fclose(fp);
        printf("  -> %-40s  %d bytes\n\n", path, N_RECORDS * FM_RECORD_SIZE);
    }

    /* -- block_large: row 0~499 (500 rows, 4000 fail) -- */
    {
        char path[256];
        gen_path(path, sizeof(path), "type5", "block_large.bin");
        static const block_range_t blocks[] = { {0, 499} };
        printf("[3/4] block_large  (rows 0-499)\n");
        gen_block_defect(records, blocks, 1);

        FILE *fp = fopen(path, "wb");
        if (!fp) { perror(path); free(records); return 1; }
        fwrite(records, FM_RECORD_SIZE, N_RECORDS, fp);
        fclose(fp);
        printf("  -> %-40s  %d bytes\n\n", path, N_RECORDS * FM_RECORD_SIZE);
    }

    /* -- block_multi: 3 blocks (rows 50~69, 400~419, 900~949 -> 90 rows, 720 fail) */
    {
        char path[256];
        gen_path(path, sizeof(path), "type5", "block_multi.bin");
        static const block_range_t blocks[] = { {50, 69}, {400, 419}, {900, 949} };
        printf("[4/4] block_multi  (rows 50-69 + 400-419 + 900-949)\n");
        gen_block_defect(records, blocks, 3);

        FILE *fp = fopen(path, "wb");
        if (!fp) { perror(path); free(records); return 1; }
        fwrite(records, FM_RECORD_SIZE, N_RECORDS, fp);
        fclose(fp);
        printf("  -> %-40s  %d bytes\n\n", path, N_RECORDS * FM_RECORD_SIZE);
    }

    printf("Done. 4 scenarios x %d records (%d rows x 8 BL)\n",
           N_RECORDS, N_ROWS);
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
    return generate_type5(n);
}
#endif
