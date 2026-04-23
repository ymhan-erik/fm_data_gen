/*
 * gen_type7.c
 *
 * type_7 테스트 데이터 생성기 — 혼합 결함.
 * 주소: 순차 (type0과 동일).
 * IO: 컬럼(DQ) + 블록 + 단일셀 결함을 합성한 복합 웨이퍼 불량 패턴.
 *
 * 출력: test/data/type7/mixed_light.bin ~ mixed_extreme.bin
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
 * 혼합 결함 IO 생성
 * 1) col_dq개 DQ 비트 마스크 -> 모든 레코드에 OR
 * 2) 블록 범위 내 레코드 -> IO = 0xFF x 12
 * 3) 단일셀: 확률적 1-bit 추가
 * -------------------------------------------------------------------------- */
static void gen_mixed_defect(fm_record_t *records,
                             int col_dq,
                             const block_range_t *blocks, size_t n_blocks,
                             int single_cell_per_1000)
{
    /* 1) 컬럼 마스크 생성 */
    uint8_t col_mask[FM_IO_SIZE];
    memset(col_mask, 0, FM_IO_SIZE);
    if (col_dq > 0) {
        int spacing = 96 / col_dq;
        for (int d = 0; d < col_dq; d++) {
            int bit_pos = d * spacing;
            int byte_idx = bit_pos / 8;
            int bit_idx  = 7 - (bit_pos % 8);
            col_mask[byte_idx] |= (uint8_t)(1 << bit_idx);
        }
    }

    int n_fail = 0;
    for (size_t r = 0; r < N_ROWS; r++) {
        /* 블록 결함 여부 확인 */
        int in_block = 0;
        for (size_t b = 0; b < n_blocks; b++) {
            if (r >= blocks[b].start_row && r <= blocks[b].end_row) {
                in_block = 1;
                break;
            }
        }

        for (int bl = 0; bl < 8; bl++) {
            size_t idx = r * 8 + bl;

            if (in_block) {
                /* 2) 블록 결함: 전체 fail */
                memset(records[idx].io, 0xFF, FM_IO_SIZE);
                n_fail++;
            } else {
                /* 시작: 컬럼 마스크 적용 */
                memcpy(records[idx].io, col_mask, FM_IO_SIZE);

                /* 3) 단일셀: 확률적 1-bit 추가 */
                if (single_cell_per_1000 > 0 &&
                    rand() % 1000 < single_cell_per_1000) {
                    int bit_pos = rand() % 96;
                    records[idx].io[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
                }

                /* non-zero인 경우 fail 카운트 */
                int nonzero = 0;
                for (int b = 0; b < FM_IO_SIZE; b++)
                    if (records[idx].io[b]) { nonzero = 1; break; }
                if (nonzero) n_fail++;
            }
        }
    }

    printf("  col_dq=%d, blocks=%zu, single_cell=%d‰\n",
           col_dq, n_blocks, single_cell_per_1000);
    printf("  fail: %5d / %d  (%.1f%%)\n",
           n_fail, N_RECORDS, 100.0 * n_fail / N_RECORDS);
}

/* --------------------------------------------------------------------------
 * generate_type7 — 통합 바이너리(gen_testdata)에서도 호출 가능
 * -------------------------------------------------------------------------- */
int generate_type7(int n_records)
{
    N_RECORDS = n_records;
    N_ROWS = N_RECORDS / 8;

    srand(0x54595037);   /* "TYP7" 고정 시드 */

    fm_record_t *records = malloc(N_RECORDS * sizeof(fm_record_t));
    if (!records) { fprintf(stderr, "out of memory\n"); return 1; }

    printf("Generating FM type7 test data  (%d records/scenario, %d rows/scenario)\n",
           N_RECORDS, N_ROWS);
    printf("  Mixed defect (column + block + single-cell)\n\n");

    fill_addr_seq(records, N_RECORDS);

    /* -- mixed_light: col 1 DQ, no block, single_cell 1% -- */
    {
        char path[256];
        gen_path(path, sizeof(path), "type7", "mixed_light.bin");
        printf("[1/4] mixed_light\n");
        gen_mixed_defect(records, 1, NULL, 0, 10);

        FILE *fp = fopen(path, "wb");
        if (!fp) { perror(path); free(records); return 1; }
        fwrite(records, FM_RECORD_SIZE, N_RECORDS, fp);
        fclose(fp);
        printf("  -> %-40s  %d bytes\n\n", path, N_RECORDS * FM_RECORD_SIZE);
    }

    /* -- mixed_moderate: col 2 DQ, block row 200~209, single_cell 5% -- */
    {
        char path[256];
        gen_path(path, sizeof(path), "type7", "mixed_moderate.bin");
        static const block_range_t blocks[] = { {200, 209} };
        printf("[2/4] mixed_moderate\n");
        gen_mixed_defect(records, 2, blocks, 1, 50);

        FILE *fp = fopen(path, "wb");
        if (!fp) { perror(path); free(records); return 1; }
        fwrite(records, FM_RECORD_SIZE, N_RECORDS, fp);
        fclose(fp);
        printf("  -> %-40s  %d bytes\n\n", path, N_RECORDS * FM_RECORD_SIZE);
    }

    /* -- mixed_heavy: col 4 DQ, block row 100~299, single_cell 10% -- */
    {
        char path[256];
        gen_path(path, sizeof(path), "type7", "mixed_heavy.bin");
        static const block_range_t blocks[] = { {100, 299} };
        printf("[3/4] mixed_heavy\n");
        gen_mixed_defect(records, 4, blocks, 1, 100);

        FILE *fp = fopen(path, "wb");
        if (!fp) { perror(path); free(records); return 1; }
        fwrite(records, FM_RECORD_SIZE, N_RECORDS, fp);
        fclose(fp);
        printf("  -> %-40s  %d bytes\n\n", path, N_RECORDS * FM_RECORD_SIZE);
    }

    /* -- mixed_extreme: col 8 DQ, block row 0~249 + 500~749, single_cell 20% -- */
    {
        char path[256];
        gen_path(path, sizeof(path), "type7", "mixed_extreme.bin");
        static const block_range_t blocks[] = { {0, 249}, {500, 749} };
        printf("[4/4] mixed_extreme\n");
        gen_mixed_defect(records, 8, blocks, 2, 200);

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
    return generate_type7(n);
}
#endif
