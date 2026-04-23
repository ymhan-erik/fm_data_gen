/*
 * gen_testdata.c
 *
 * 통합 테스트 데이터 생성기.
 * type0~type8 전체 51개 시나리오를 단일 바이너리로 생성.
 *
 * 빌드: make gen_testdata  (sw/ 디렉토리에서)
 * 실행: ./gen_testdata [N]   ← 임의 디렉토리에서 실행 가능
 *       N = 레코드 수 (기본 10000, 8의 배수)
 *
 * CWD에 type0/~type8/ 디렉토리를 자동 생성하고 .bin 파일을 출력한다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gen_common.h"

/* gen_type*.c에서 export하는 함수 */
extern int generate_type0(int n_records);
extern int generate_type1(int n_records);
extern int generate_type2(int n_records);
extern int generate_type3(int n_records);
extern int generate_type4(int n_records);
extern int generate_type5(int n_records);
extern int generate_type6(int n_records);
extern int generate_type7(int n_records);
extern int generate_type8(int n_records);

static void ensure_dirs(void)
{
    char buf[256];
    for (int i = 0; i <= 8; i++) {
        snprintf(buf, sizeof(buf), "%s/type%d", g_outdir, i);
        mkdir(buf, 0755);  /* 이미 존재하면 무시 */
    }
}

int main(int argc, char *argv[])
{
    int n = 10000;
    if (argc >= 2) {
        n = atoi(argv[1]);
        if (n < 8 || n % 8 != 0) {
            fprintf(stderr, "Error: N must be >= 8 and multiple of 8 (got %d)\n", n);
            return 1;
        }
    }

    /* CWD 기준으로 출력 */
    g_outdir = ".";

    printf("=== gen_testdata v%s: %d records/scenario ===\n\n",
           GEN_TESTDATA_VERSION, n);
    ensure_dirs();

    int rc;
    rc = generate_type0(n); if (rc) return rc; putchar('\n');
    rc = generate_type1(n); if (rc) return rc; putchar('\n');
    rc = generate_type2(n); if (rc) return rc; putchar('\n');
    rc = generate_type3(n); if (rc) return rc; putchar('\n');
    rc = generate_type4(n); if (rc) return rc; putchar('\n');
    rc = generate_type5(n); if (rc) return rc; putchar('\n');
    rc = generate_type6(n); if (rc) return rc; putchar('\n');
    rc = generate_type7(n); if (rc) return rc; putchar('\n');
    rc = generate_type8(n); if (rc) return rc;

    printf("\nAll done. 51 scenarios generated.\n");
    return 0;
}
