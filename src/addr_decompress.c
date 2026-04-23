#include "addr_compress.h"
#include <stdint.h>

#define ADDR_MASK  0xFFFFFFFFFFFFULL   /* 48bit 마스크 */

/* --------------------------------------------------------------------------
 * 비트스트림 reader (MSB-first: 스트림 비트 0 = byte[0]의 bit7)
 * -------------------------------------------------------------------------- */

typedef struct {
    const uint8_t *buf;
    size_t         size;
    size_t         pos;
} bitreader_t;

static int br_read(bitreader_t *br, int n, uint64_t *out)
{
    uint64_t v = 0;
    for (int i = n - 1; i >= 0; i--) {
        size_t byte_i = br->pos / 8;
        int    bit_i  = 7 - (int)(br->pos % 8);
        if (byte_i >= br->size)
            return -1;
        if ((br->buf[byte_i] >> bit_i) & 1u)
            v |= (1ULL << i);
        br->pos++;
    }
    *out = v;
    return 0;
}

/* --------------------------------------------------------------------------
 * addr_decompress  (v1)
 *
 * 비트스트림 디코딩 (v1 포맷, 하위 호환용):
 *   첫 레코드  : 48bit 절대 주소
 *   이후 레코드:
 *     [0]           → curr = prev + 1            (delta +1)
 *     [1][0][s16]   → curr = prev + sign_ext(16) (작은 delta)
 *     [1][1][addr48]→ curr = addr48              (절대 주소)
 * -------------------------------------------------------------------------- */
int addr_decompress(const uint8_t *buf, size_t buf_size,
                    fm_record_t *records, size_t count)
{
    if (count == 0)
        return 0;

    bitreader_t br  = { buf, buf_size, 0 };
    uint64_t    raw;

    /* 첫 레코드: 절대 주소 */
    if (br_read(&br, 48, &raw) < 0)
        return -1;
    uint64_t prev = raw & ADDR_MASK;
    fm_u64_to_addr(prev, records[0].addr);

    for (size_t i = 1; i < count; i++) {
        uint64_t flag0;
        if (br_read(&br, 1, &flag0) < 0)
            return -1;

        uint64_t curr;
        if (flag0 == 0) {
            curr = prev + 1;

        } else {
            uint64_t flag1;
            if (br_read(&br, 1, &flag1) < 0)
                return -1;

            if (flag1 == 0) {
                uint64_t d16;
                if (br_read(&br, 16, &d16) < 0)
                    return -1;
                int64_t delta = (int64_t)(int16_t)(uint16_t)d16;
                curr = (uint64_t)((int64_t)prev + delta);
            } else {
                if (br_read(&br, 48, &raw) < 0)
                    return -1;
                curr = raw;
            }
        }

        curr &= ADDR_MASK;
        fm_u64_to_addr(curr, records[i].addr);
        prev = curr;
    }

    return 0;
}

/* --------------------------------------------------------------------------
 * addr_decompress_v2  (v2)
 *
 * 비트스트림 디코딩 (v2 포맷, delta=-1 특수 케이스 포함):
 *   첫 레코드  : 48bit 절대 주소
 *   이후 레코드:
 *     [0]              → curr = prev + 1             (delta +1)
 *     [1][0]           → curr = prev - 1             (delta -1, 역방향 스캔)
 *     [1][1][0][s16]   → curr = prev + sign_ext(16)  (작은 delta)
 *     [1][1][1][addr48]→ curr = addr48               (절대 주소)
 * -------------------------------------------------------------------------- */
int addr_decompress_v2(const uint8_t *buf, size_t buf_size,
                       fm_record_t *records, size_t count)
{
    if (count == 0)
        return 0;

    bitreader_t br  = { buf, buf_size, 0 };
    uint64_t    raw;

    /* 첫 레코드: 절대 주소 */
    if (br_read(&br, 48, &raw) < 0)
        return -1;
    uint64_t prev = raw & ADDR_MASK;
    fm_u64_to_addr(prev, records[0].addr);

    for (size_t i = 1; i < count; i++) {
        uint64_t flag0;
        if (br_read(&br, 1, &flag0) < 0)
            return -1;

        uint64_t curr;
        if (flag0 == 0) {
            /* [0]: delta +1 */
            curr = prev + 1;

        } else {
            uint64_t flag1;
            if (br_read(&br, 1, &flag1) < 0)
                return -1;

            if (flag1 == 0) {
                /* [1][0]: delta -1 (역방향 스캔) */
                curr = prev - 1;

            } else {
                uint64_t flag2;
                if (br_read(&br, 1, &flag2) < 0)
                    return -1;

                if (flag2 == 0) {
                    /* [1][1][0][s16]: 작은 delta */
                    uint64_t d16;
                    if (br_read(&br, 16, &d16) < 0)
                        return -1;
                    int64_t delta = (int64_t)(int16_t)(uint16_t)d16;
                    curr = (uint64_t)((int64_t)prev + delta);
                } else {
                    /* [1][1][1][addr48]: 절대 주소 */
                    if (br_read(&br, 48, &raw) < 0)
                        return -1;
                    curr = raw;
                }
            }
        }

        curr &= ADDR_MASK;
        fm_u64_to_addr(curr, records[i].addr);
        prev = curr;
    }

    return 0;
}
