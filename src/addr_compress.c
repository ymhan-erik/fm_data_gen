#include "addr_compress.h"
#include <string.h>
#include <stdint.h>

#define ADDR_MASK  0xFFFFFFFFFFFFULL   /* 48bit 마스크 */

/* --------------------------------------------------------------------------
 * 비트스트림 writer / reader (MSB-first: 스트림 비트 0 = byte[0]의 bit7)
 * -------------------------------------------------------------------------- */

typedef struct {
    uint8_t *buf;
    size_t   cap;   /* 버퍼 용량 (bytes) */
    size_t   pos;   /* 다음 쓸 비트 위치 */
} bitwriter_t;

/*
 * bw_write: val의 비트 [n-1 .. 0]을 MSB-first로 스트림에 기록
 * 성공 시 0, 버퍼 초과 시 -1
 */
static int bw_write(bitwriter_t *bw, uint64_t val, int n)
{
    for (int i = n - 1; i >= 0; i--) {
        size_t byte_i = bw->pos / 8;
        int    bit_i  = 7 - (int)(bw->pos % 8);
        if (byte_i >= bw->cap)
            return -1;
        if ((val >> i) & 1u)
            bw->buf[byte_i] |=  (uint8_t)(1u << bit_i);
        else
            bw->buf[byte_i] &= (uint8_t)~(1u << bit_i);
        bw->pos++;
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * addr_compress_bound
 *
 * 최악의 경우 (v2): 첫 레코드 48bit + 이후 레코드마다 51bit ([1][1][1][48bit])
 * -------------------------------------------------------------------------- */
size_t addr_compress_bound(size_t count)
{
    if (count == 0)
        return 0;
    size_t bits = 48 + (count - 1) * 51;
    return (bits + 7) / 8 + 1;   /* +1: 바이트 정렬 여유 */
}

/* --------------------------------------------------------------------------
 * addr_compress  (v2)
 *
 * 비트스트림 인코딩:
 *   첫 레코드  : addr 48bit 그대로
 *   이후 레코드: delta = curr - prev (int64_t)
 *     delta == +1                : [0]              (1 bit)
 *     delta == -1                : [1][0]            (2 bits, 역방향 스캔 최적화)
 *     s16 범위 (!=±1)            : [1][1][0][s16]   (19 bits)
 *     그 외                     : [1][1][1][addr48] (51 bits, 절대 주소)
 * -------------------------------------------------------------------------- */
int addr_compress(const fm_record_t *records, size_t count,
                  uint8_t *out_buf, size_t *out_size)
{
    if (count == 0) {
        *out_size = 0;
        return 0;
    }

    size_t bound = addr_compress_bound(count);
    memset(out_buf, 0, bound);

    bitwriter_t bw   = { out_buf, bound, 0 };
    uint64_t    prev = fm_addr_to_u64(records[0].addr);

    /* 첫 레코드: 절대 주소 48bit */
    if (bw_write(&bw, prev, 48) < 0)
        return -1;

    for (size_t i = 1; i < count; i++) {
        uint64_t curr  = fm_addr_to_u64(records[i].addr);
        int64_t  delta = (int64_t)curr - (int64_t)prev;

        if (delta == 1) {
            /* [0]: BL 내 연속 (+1), 1bit */
            if (bw_write(&bw, 0, 1) < 0)
                return -1;

        } else if (delta == -1) {
            /* [1][0]: 역방향 스캔 (-1), 2bits */
            if (bw_write(&bw, 1, 1) < 0) return -1;
            if (bw_write(&bw, 0, 1) < 0) return -1;

        } else if (delta >= INT16_MIN && delta <= INT16_MAX) {
            /* [1][1][0][s16]: 작은 delta (row 변경 등), 19bits */
            if (bw_write(&bw, 1, 1) < 0) return -1;
            if (bw_write(&bw, 1, 1) < 0) return -1;
            if (bw_write(&bw, 0, 1) < 0) return -1;
            if (bw_write(&bw, (uint64_t)(uint16_t)(int16_t)delta, 16) < 0)
                return -1;

        } else {
            /* [1][1][1][addr48]: 큰 점프, 절대 주소 저장, 51bits */
            if (bw_write(&bw, 1, 1) < 0) return -1;
            if (bw_write(&bw, 1, 1) < 0) return -1;
            if (bw_write(&bw, 1, 1) < 0) return -1;
            if (bw_write(&bw, curr & ADDR_MASK, 48) < 0)
                return -1;
        }

        prev = curr;
    }

    *out_size = (bw.pos + 7) / 8;
    return 0;
}

/* addr_decompress 는 src/addr_decompress.c 에 구현됨 */
