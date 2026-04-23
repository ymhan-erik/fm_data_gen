/*
 * gen_type8.c
 *
 * type8 테스트 데이터 생성기 — 클러스터 결함.
 * 순차 주소 + 공간적으로 뭉치는 결함 패턴 (실제 웨이퍼 파티클/공정 오염 모사).
 * 대부분의 영역은 clean, 특정 클러스터 영역에 결함 집중.
 *
 * 출력: test/data/type8/cluster_*.bin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fm_record.h"
#include "gen_common.h"

static int N_RECORDS = 10000;
static int N_BURSTS;
#define BASE_ADDR  0x000100000000ULL

#define MAX_CLUSTERS 16

typedef struct {
    int center;    /* cluster 중심 burst 인덱스 */
    int radius;    /* 반경 (bursts). center ± radius 범위 */
    int fail_ppm;  /* fail 확률 per-mille (0~1000) */
} cluster_t;

/* --------------------------------------------------------------------------
 * BASE_ADDR 부터 순차 주소 (BL 0~7 × N_BURSTS)
 * -------------------------------------------------------------------------- */
static void fill_addr_seq(fm_record_t *records, size_t n_records)
{
    for (size_t i = 0; i < n_records; i++)
        fm_u64_to_addr(BASE_ADDR + (uint64_t)i, records[i].addr);
}

/* --------------------------------------------------------------------------
 * burst가 클러스터 영역 내인지 판별. 해당 클러스터 인덱스 반환, 아니면 -1.
 * 복수 클러스터 겹침 시 첫 번째 매칭 반환.
 * -------------------------------------------------------------------------- */
static int in_any_cluster(int burst_idx, const cluster_t *clusters, int n_clusters)
{
    for (int c = 0; c < n_clusters; c++) {
        int lo = clusters[c].center - clusters[c].radius;
        int hi = clusters[c].center + clusters[c].radius;
        if (lo < 0) lo = 0;
        if (hi >= N_BURSTS) hi = N_BURSTS - 1;
        if (burst_idx >= lo && burst_idx <= hi)
            return c;
    }
    return -1;
}

/* --------------------------------------------------------------------------
 * 클러스터 결함 IO 생성
 * -------------------------------------------------------------------------- */
static void gen_clustered(fm_record_t *records,
                          const cluster_t *clusters, int n_clusters)
{
    int n_fail = 0;
    int n_in_cluster = 0;

    for (int b = 0; b < N_BURSTS; b++) {
        int cidx = in_any_cluster(b, clusters, n_clusters);
        for (int bl = 0; bl < 8; bl++) {
            int idx = b * 8 + bl;
            memset(records[idx].io, 0, FM_IO_SIZE);
            if (cidx >= 0) {
                n_in_cluster++;
                if (rand() % 1000 < clusters[cidx].fail_ppm) {
                    /* sparse single-bit IO (리텐션 결함 모사) */
                    int bit_pos = rand() % 96;
                    records[idx].io[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
                    n_fail++;
                }
            }
        }
    }
    printf("  fail: %5d / %d  (%.1f%%)  [%d records in cluster zones]\n",
           n_fail, N_RECORDS, 100.0 * n_fail / N_RECORDS, n_in_cluster);
}

/* --------------------------------------------------------------------------
 * generate_type8 — 통합 바이너리(gen_testdata)에서도 호출 가능
 * -------------------------------------------------------------------------- */
int generate_type8(int n_records)
{
    N_RECORDS = n_records;
    N_BURSTS = N_RECORDS / 8;

    srand(0x54595038);   /* type8 고정 시드 */

    fm_record_t *records = malloc(N_RECORDS * sizeof(fm_record_t));
    if (!records) { fprintf(stderr, "out of memory\n"); return 1; }

    printf("Generating FM type8 test data  (%d records/scenario, %d bursts/scenario)\n\n",
           N_RECORDS, N_BURSTS);

    fill_addr_seq(records, N_RECORDS);

    /* 시나리오 정의 */
    typedef struct {
        const char *filename;
        const char *label;
        cluster_t   clusters[MAX_CLUSTERS];
        int         n_clusters;
    } scenario_t;

    static const scenario_t S[] = {
        {
            "cluster_sparse.bin",
            "cluster sparse (2 clusters, r=15, 30%)",
            { {200, 15, 300}, {900, 15, 300} }, 2
        },
        {
            "cluster_medium.bin",
            "cluster medium (4 clusters, r=20, 40%)",
            { {100, 20, 400}, {400, 20, 400}, {700, 20, 400}, {1100, 20, 400} }, 4
        },
        {
            "cluster_dense.bin",
            "cluster dense (6 clusters, r=25, 50%)",
            { {50, 25, 500}, {250, 25, 500}, {500, 25, 500},
              {750, 25, 500}, {950, 25, 500}, {1150, 25, 500} }, 6
        },
        {
            "cluster_overlap.bin",
            "cluster overlap (3 clusters, r=50, 40%)",
            { {300, 50, 400}, {380, 50, 400}, {900, 50, 400} }, 3
        },
    };
    int n = (int)(sizeof(S) / sizeof(S[0]));

    for (int i = 0; i < n; i++) {
        char path[256];
        gen_path(path, sizeof(path), "type8", S[i].filename);

        printf("[%d/%d] %s\n", i + 1, n, S[i].label);
        gen_clustered(records, S[i].clusters, S[i].n_clusters);

        FILE *fp = fopen(path, "wb");
        if (!fp) { perror(path); free(records); return 1; }
        fwrite(records, FM_RECORD_SIZE, N_RECORDS, fp);
        fclose(fp);
        printf("  -> %-40s  %d bytes\n\n",
               path, N_RECORDS * FM_RECORD_SIZE);
    }

    printf("Done. %d scenarios x %d records (%d bursts x 8 BL)\n",
           n, N_RECORDS, N_BURSTS);
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
    return generate_type8(n);
}
#endif
