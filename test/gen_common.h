#ifndef GEN_COMMON_H
#define GEN_COMMON_H

#include <stdio.h>

/* 테스트 데이터 세트 버전 — test_data_spec.md 버전 이력과 동기화 */
#define GEN_TESTDATA_VERSION "1.0"

/*
 * 출력 디렉토리 접두사.
 * 기본값: "test/data"  (make testdata — sw/ 디렉토리 기준)
 * gen_testdata.c에서 "."으로 변경 (CWD 기준 출력)
 */
extern const char *g_outdir;

/* g_outdir/type_subdir/filename → buf 조합 */
static inline char *gen_path(char *buf, size_t bufsz,
                             const char *type_subdir, const char *filename)
{
    snprintf(buf, bufsz, "%s/%s/%s", g_outdir, type_subdir, filename);
    return buf;
}

#endif
