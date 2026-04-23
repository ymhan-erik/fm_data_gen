#include "encoder.h"
#include "addr_compress.h"
#include "io_compress.h"
#include <stdlib.h>
#include <string.h>

/* 직렬화 헤더 크기: 4+4+8+8+8+8+8 = 48 bytes (플랫폼 독립) */
#define HDR_BYTES  48u

/* --------------------------------------------------------------------------
 * 내부 직렬화 유틸 (little-endian)
 * -------------------------------------------------------------------------- */

static void put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >>  8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_u64le(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; i++) { p[i] = (uint8_t)(v & 0xFF); v >>= 8; }
}

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

/* --------------------------------------------------------------------------
 * fm_header_write
 * -------------------------------------------------------------------------- */
int fm_header_write(FILE *fp, const fm_file_header_t *h)
{
    uint8_t buf[HDR_BYTES];
    put_u32le(buf +  0, h->magic);
    put_u32le(buf +  4, h->version);
    put_u64le(buf +  8, h->record_count);
    put_u64le(buf + 16, h->addr_offset);
    put_u64le(buf + 24, h->addr_size);
    put_u64le(buf + 32, h->io_offset);
    put_u64le(buf + 40, h->io_size);
    return (fwrite(buf, 1, HDR_BYTES, fp) == HDR_BYTES) ? 0 : -1;
}

/* --------------------------------------------------------------------------
 * fm_header_read
 * magic, version 검증 포함
 * -------------------------------------------------------------------------- */
int fm_header_read(FILE *fp, fm_file_header_t *h)
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
        fprintf(stderr, "fm_header_read: bad magic 0x%08X\n", h->magic);
        return -1;
    }
    if (h->version != 1 && h->version != FM_VERSION) {
        fprintf(stderr, "fm_header_read: unsupported version %u\n", h->version);
        return -1;
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * fm_encode
 *
 * records[] → ADDR/IO 분리 압축 → fp에 기록
 *   파일 레이아웃: [헤더 48B][ADDR 압축 블록][IO 압축 블록]
 * -------------------------------------------------------------------------- */
int fm_encode(FILE *fp, const fm_record_t *records, size_t count)
{
    int      ret       = -1;
    uint8_t *addr_buf  = NULL;
    uint8_t *io_buf    = NULL;

    size_t addr_bound = addr_compress_bound(count);
    size_t io_bound   = io_compress_bound(count);

    addr_buf = malloc(addr_bound);
    io_buf   = malloc(io_bound);
    if (!addr_buf || !io_buf)
        goto done;

    /* ADDR 압축 */
    size_t addr_size;
    if (addr_compress(records, count, addr_buf, &addr_size) < 0)
        goto done;

    /* IO 압축 */
    size_t io_size;
    if (io_compress(records, count, io_buf, &io_size) < 0)
        goto done;

    /* 헤더 구성 (오프셋은 헤더 바로 뒤부터) */
    fm_file_header_t hdr;
    hdr.magic        = FM_MAGIC;
    hdr.version      = FM_VERSION;
    hdr.record_count = (uint64_t)count;
    hdr.addr_offset  = HDR_BYTES;
    hdr.addr_size    = (uint64_t)addr_size;
    hdr.io_offset    = HDR_BYTES + (uint64_t)addr_size;
    hdr.io_size      = (uint64_t)io_size;

    /* 기록: 헤더 → ADDR 블록 → IO 블록 */
    if (fm_header_write(fp, &hdr)                       < 0) goto done;
    if (fwrite(addr_buf, 1, addr_size, fp) != addr_size)     goto done;
    if (fwrite(io_buf,   1, io_size,   fp) != io_size)       goto done;

    ret = 0;
done:
    free(addr_buf);
    free(io_buf);
    return ret;
}

/* --------------------------------------------------------------------------
 * fm_decode
 *
 * fp(압축 파일) → ADDR/IO 압축 해제 → *out_records 복원
 * 호출자가 *out_records를 free() 해야 함
 * -------------------------------------------------------------------------- */
int fm_decode(FILE *fp, fm_record_t **out_records, size_t *out_count)
{
    int          ret      = -1;
    fm_record_t *records  = NULL;
    uint8_t     *addr_buf = NULL;
    uint8_t     *io_buf   = NULL;

    fm_file_header_t hdr;
    if (fm_header_read(fp, &hdr) < 0)
        goto done;

    size_t count    = (size_t)hdr.record_count;
    size_t addr_sz  = (size_t)hdr.addr_size;
    size_t io_sz    = (size_t)hdr.io_size;

    records  = calloc(count, sizeof(fm_record_t));
    addr_buf = malloc(addr_sz);
    io_buf   = malloc(io_sz);
    if (!records || !addr_buf || !io_buf)
        goto done;

    /* ADDR 블록 읽기 */
    if (fseek(fp, (long)hdr.addr_offset, SEEK_SET) < 0)            goto done;
    if (fread(addr_buf, 1, addr_sz, fp) != addr_sz)                 goto done;

    /* IO 블록 읽기 */
    if (fseek(fp, (long)hdr.io_offset, SEEK_SET) < 0)              goto done;
    if (fread(io_buf, 1, io_sz, fp) != io_sz)                       goto done;

    /* 압축 해제: ADDR은 version에 따라 다른 디코더 사용 */
    int addr_ret;
    if (hdr.version == 1)
        addr_ret = addr_decompress(addr_buf, addr_sz, records, count);
    else
        addr_ret = addr_decompress_v2(addr_buf, addr_sz, records, count);
    if (addr_ret < 0)                                               goto done;
    if (io_decompress(io_buf, io_sz, records, count) < 0)           goto done;

    *out_records = records;
    *out_count   = count;
    records = NULL;   /* 소유권 이전, goto done에서 free 방지 */
    ret = 0;
done:
    free(addr_buf);
    free(io_buf);
    free(records);
    return ret;
}
