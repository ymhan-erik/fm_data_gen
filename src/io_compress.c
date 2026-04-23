#include "io_compress.h"
#include <string.h>

/* 토큰 크기 */
#define ZERO_TOKEN_SIZE    5    /* 0x00 + 4-byte count */
#define FAIL_TOKEN_SIZE   13   /* 0xFF + 12-byte io */
#define REPEAT_TOKEN_SIZE  5    /* 0xAA + 4-byte count */

/* --------------------------------------------------------------------------
 * 내부 유틸
 * -------------------------------------------------------------------------- */

static int is_io_zero(const uint8_t io[FM_IO_SIZE])
{
    for (int i = 0; i < FM_IO_SIZE; i++)
        if (io[i]) return 0;
    return 1;
}

static int is_io_equal(const uint8_t a[FM_IO_SIZE], const uint8_t b[FM_IO_SIZE])
{
    for (int i = 0; i < FM_IO_SIZE; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

static void write_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >>  8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* --------------------------------------------------------------------------
 * io_compress_bound
 *
 * 최악의 경우: 모든 레코드가 서로 다른 non-zero (Token B만 emit)
 *   → count * 13 bytes
 * 끝에 zero/repeat 토큰이 추가될 여지: +5 bytes (여유분)
 * -------------------------------------------------------------------------- */
size_t io_compress_bound(size_t count)
{
    return count * FAIL_TOKEN_SIZE + ZERO_TOKEN_SIZE;
}

/* --------------------------------------------------------------------------
 * flush 헬퍼
 * -------------------------------------------------------------------------- */
static void flush_zero_run(uint8_t **p, uint32_t *zero_run)
{
    if (*zero_run > 0) {
        **p = IO_TOKEN_ZERO;
        (*p)++;
        write_u32le(*p, *zero_run);
        (*p) += 4;
        *zero_run = 0;
    }
}

static void flush_repeat_run(uint8_t **p, uint32_t *repeat_run)
{
    if (*repeat_run > 0) {
        **p = IO_TOKEN_REPEAT;
        (*p)++;
        write_u32le(*p, *repeat_run);
        (*p) += 4;
        *repeat_run = 0;
    }
}

/* --------------------------------------------------------------------------
 * io_compress  (v2)
 *
 * 알고리즘:
 *   records[]를 순회하며:
 *   - all-zero IO        : zero_run 카운터 증가 (Token A)
 *   - non-zero, prev와 동일: repeat_run 카운터 증가 (Token C)
 *   - non-zero, prev와 다름: 누적 런 flush 후 Token B 기록, prev_io 갱신
 *   순회 종료 후 남은 런 flush
 * -------------------------------------------------------------------------- */
int io_compress(const fm_record_t *records, size_t count,
                uint8_t *out_buf, size_t *out_size)
{
    uint8_t  *p          = out_buf;
    uint32_t  zero_run   = 0;
    uint32_t  repeat_run = 0;
    uint8_t   prev_io[FM_IO_SIZE];
    int       have_prev  = 0;

    for (size_t i = 0; i < count; i++) {
        if (is_io_zero(records[i].io)) {
            /* all-zero: repeat 런 먼저 flush 후 zero_run 누적 */
            flush_repeat_run(&p, &repeat_run);
            /* overflow 방지 */
            if (zero_run == UINT32_MAX) {
                flush_zero_run(&p, &zero_run);
            }
            zero_run++;

        } else if (have_prev && is_io_equal(records[i].io, prev_io)) {
            /* non-zero이고 이전과 동일: zero 런 먼저 flush 후 repeat_run 누적 */
            flush_zero_run(&p, &zero_run);
            /* overflow 방지 */
            if (repeat_run == UINT32_MAX) {
                flush_repeat_run(&p, &repeat_run);
            }
            repeat_run++;

        } else {
            /* non-zero이고 이전과 다름: 두 런 모두 flush 후 Token B */
            flush_zero_run(&p, &zero_run);
            flush_repeat_run(&p, &repeat_run);
            *p++ = IO_TOKEN_FAIL;
            memcpy(p, records[i].io, FM_IO_SIZE);
            p += FM_IO_SIZE;
            memcpy(prev_io, records[i].io, FM_IO_SIZE);
            have_prev = 1;
        }
    }

    /* 끝에 남은 런 flush */
    flush_zero_run(&p, &zero_run);
    flush_repeat_run(&p, &repeat_run);

    *out_size = (size_t)(p - out_buf);
    return 0;
}

/* io_decompress 는 src/io_decompress.c 에 구현됨 */
