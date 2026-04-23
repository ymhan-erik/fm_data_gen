#include "decoder.h"
#include "addr_compress.h"
#include "io_compress.h"
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * 헤더 직렬화 유틸 (little-endian, encoder.c와 동일 포맷)
 * decoder.c는 encoder.c에 의존하지 않는 독립 바이너리를 위해
 * 헤더 파싱을 자체 구현한다.
 * -------------------------------------------------------------------------- */

#define HDR_BYTES  48u

static uint32_t get_u32le(const uint8_t *p)
{
    return  (uint32_t)p[0]
          | ((uint32_t)p[1] <<  8)
          | ((uint32_t)p[2] << 16)
          | ((uint32_t)p[3] << 24);
}

static uint64_t get_u64le(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--)
        v = (v << 8) | p[i];
    return v;
}

static int read_header(FILE *fp, fm_file_header_t *h)
{
    uint8_t buf[HDR_BYTES];
    if (fread(buf, 1, HDR_BYTES, fp) != HDR_BYTES)
        return -1;

    h->magic        = get_u32le(buf +  0);
    h->version      = get_u32le(buf +  4);
    h->record_count = get_u64le(buf +  8);
    h->addr_offset  = get_u64le(buf + 16);
    h->addr_size    = get_u64le(buf + 24);
    h->io_offset    = get_u64le(buf + 32);
    h->io_size      = get_u64le(buf + 40);

    if (h->magic != FM_MAGIC) {
        fprintf(stderr, "decoder: bad magic 0x%08X (expected 0x%08X)\n",
                h->magic, FM_MAGIC);
        return -1;
    }
    if (h->version != 1 && h->version != 2) {
        fprintf(stderr, "decoder: unsupported version %u\n", h->version);
        return -1;
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * fm_decode_from_file
 *
 * 1. 헤더 읽기 & 검증
 * 2. ADDR 블록 읽기 → addr_decompress (delta bitstream)
 * 3. IO 블록 읽기   → io_decompress   (bitmap RLE token)
 * 4. ADDR + IO 결합 → fm_record_t 배열 반환
 * -------------------------------------------------------------------------- */
int fm_decode_from_file(FILE            *fp,
                        fm_record_t    **out_records,
                        size_t          *out_count,
                        fm_file_header_t *out_hdr)
{
    int          ret      = -1;
    fm_record_t *records  = NULL;
    uint8_t     *addr_buf = NULL;
    uint8_t     *io_buf   = NULL;

    fm_file_header_t hdr;
    if (read_header(fp, &hdr) < 0)
        goto done;

    size_t count   = (size_t)hdr.record_count;
    size_t addr_sz = (size_t)hdr.addr_size;
    size_t io_sz   = (size_t)hdr.io_size;

    records  = calloc(count, sizeof(fm_record_t));
    addr_buf = malloc(addr_sz);
    io_buf   = malloc(io_sz);
    if (!records || !addr_buf || !io_buf)
        goto done;

    /* ADDR 압축 블록 읽기 */
    if (fseek(fp, (long)hdr.addr_offset, SEEK_SET) < 0)            goto done;
    if (fread(addr_buf, 1, addr_sz, fp) != addr_sz)                 goto done;

    /* IO 압축 블록 읽기 */
    if (fseek(fp, (long)hdr.io_offset, SEEK_SET) < 0)              goto done;
    if (fread(io_buf, 1, io_sz, fp) != io_sz)                       goto done;

    /* ADDR delta decoding: version에 따라 다른 디코더 사용 */
    int addr_ret;
    if (hdr.version == 1)
        addr_ret = addr_decompress(addr_buf, addr_sz, records, count);
    else
        addr_ret = addr_decompress_v2(addr_buf, addr_sz, records, count);
    if (addr_ret < 0) {
        fprintf(stderr, "decoder: addr_decompress failed\n");
        goto done;
    }

    /* IO bitmap RLE decoding */
    if (io_decompress(io_buf, io_sz, records, count) < 0) {
        fprintf(stderr, "decoder: io_decompress failed\n");
        goto done;
    }

    if (out_hdr)
        *out_hdr = hdr;

    *out_records = records;
    *out_count   = count;
    records = NULL;   /* 소유권 이전 */
    ret = 0;

done:
    free(addr_buf);
    free(io_buf);
    free(records);
    return ret;
}
