#include "io_compress.h"
#include <string.h>

static uint32_t read_u32le(const uint8_t *p)
{
    return  (uint32_t)p[0]
          | ((uint32_t)p[1] <<  8)
          | ((uint32_t)p[2] << 16)
          | ((uint32_t)p[3] << 24);
}

/* --------------------------------------------------------------------------
 * io_decompress  (v1/v2 겸용)
 *
 * Token 스트림 디코딩 (io_compress의 역연산):
 *   Token A [0x00][count u32le] → all-zero IO 레코드 count개
 *   Token B [0xFF][io 12bytes]  → fail IO 레코드 1개 (prev_io 갱신)
 *   Token C [0xAA][count u32le] → 직전 Token B IO 반복 (v2 신규; v1 데이터에 없음)
 * -------------------------------------------------------------------------- */
int io_decompress(const uint8_t *buf, size_t buf_size,
                  fm_record_t *records, size_t count)
{
    const uint8_t *p   = buf;
    const uint8_t *end = buf + buf_size;
    size_t filled = 0;
    uint8_t prev_io[FM_IO_SIZE];
    int     have_prev = 0;

    while (filled < count) {
        if (p >= end)
            return -1;

        uint8_t token = *p++;

        if (token == IO_TOKEN_ZERO) {
            if (p + 4 > end)
                return -1;
            uint32_t n = read_u32le(p);
            p += 4;
            if (n == 0 || n > count - filled)
                return -1;
            for (uint32_t j = 0; j < n; j++)
                memset(records[filled++].io, 0, FM_IO_SIZE);

        } else if (token == IO_TOKEN_FAIL) {
            if (p + FM_IO_SIZE > end)
                return -1;
            memcpy(records[filled].io, p, FM_IO_SIZE);
            memcpy(prev_io, p, FM_IO_SIZE);
            have_prev = 1;
            filled++;
            p += FM_IO_SIZE;

        } else if (token == IO_TOKEN_REPEAT) {
            if (!have_prev)
                return -1;
            if (p + 4 > end)
                return -1;
            uint32_t n = read_u32le(p);
            p += 4;
            if (n == 0 || n > count - filled)
                return -1;
            for (uint32_t j = 0; j < n; j++)
                memcpy(records[filled++].io, prev_io, FM_IO_SIZE);

        } else {
            return -1;
        }
    }

    return 0;
}
