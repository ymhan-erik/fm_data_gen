/*
 * gen_type4.c
 *
 * type_4 테스트 데이터 생성기 — 컬럼(DQ) 결함.
 * 주소는 type0과 동일 (순차), IO는 컬럼(DQ) 결함 패턴.
 * 모든 레코드에 동일한 IO 비트 패턴을 적용하여
 * BL(Bit Line) open/short 결함을 시뮬레이션한다.
 *
 * 출력: test/data/type4/col_1dq.bin ~ col_all.bin
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
 * 컬럼 결함 IO 생성: n_dq개의 DQ 비트를 96-bit IO에 분산 배치
 * 모든 레코드에 동일 패턴 적용
 * -------------------------------------------------------------------------- */
static void gen_col_defect(fm_record_t *records, int n_dq)
{
    uint8_t pattern[FM_IO_SIZE];
    memset(pattern, 0, FM_IO_SIZE);

    if (n_dq >= 96) {
        /* 전체 DQ 결함 */
        memset(pattern, 0xFF, FM_IO_SIZE);
    } else {
        /* n_dq개의 비트를 96-bit(12 bytes)에 균등 분산 */
        int spacing = 96 / n_dq;
        for (int d = 0; d < n_dq; d++) {
            int bit_pos = d * spacing;
            int byte_idx = bit_pos / 8;
            int bit_idx  = 7 - (bit_pos % 8);  /* MSB-first */
            pattern[byte_idx] |= (uint8_t)(1 << bit_idx);
        }
    }

    printf("  IO pattern (%d DQ): ", n_dq);
    for (int b = 0; b < FM_IO_SIZE; b++)
        printf("%02X", pattern[b]);
    printf("\n");

    for (size_t i = 0; i < N_RECORDS; i++)
        memcpy(records[i].io, pattern, FM_IO_SIZE);

    printf("  fail: %5d / %d  (100.0%% — all records same pattern)\n",
           N_RECORDS, N_RECORDS);
}

/* --------------------------------------------------------------------------
 * generate_type4 — 통합 바이너리(gen_testdata)에서도 호출 가능
 * -------------------------------------------------------------------------- */
int generate_type4(int n_records)
{
    N_RECORDS = n_records;
    N_ROWS = N_RECORDS / 8;

    srand(0x54595034);   /* "TYP4" 고정 시드 */

    fm_record_t *records = malloc(N_RECORDS * sizeof(fm_record_t));
    if (!records) { fprintf(stderr, "out of memory\n"); return 1; }

    printf("Generating FM type4 test data  (%d records/scenario, %d rows/scenario)\n",
           N_RECORDS, N_ROWS);
    printf("  Column (DQ) defect patterns\n\n");

    fill_addr_seq(records, N_RECORDS);

    static const struct {
        const char *filename;
        const char *label;
        int         n_dq;
    } S[] = {
        { "col_1dq.bin",   "1 DQ defect",    1  },
        { "col_4dq.bin",   "4 DQ defect",    4  },
        { "col_16dq.bin",  "16 DQ defect",  16  },
        { "col_all.bin",   "96 DQ (all)",   96  },
    };
    int n = (int)(sizeof(S) / sizeof(S[0]));

    for (int i = 0; i < n; i++) {
        char path[256];
        gen_path(path, sizeof(path), "type4", S[i].filename);

        printf("[%d/%d] %s\n", i + 1, n, S[i].label);
        gen_col_defect(records, S[i].n_dq);

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
    mkdir("type4", 0755);
    return generate_type4(n);
}
#endif
