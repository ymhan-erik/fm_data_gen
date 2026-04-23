#ifndef FM_RECORD_H
#define FM_RECORD_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* FM 레코드 크기 상수 */
#define FM_ADDR_SIZE    6       /* 48bit DRAM 주소 */
#define FM_IO_SIZE      12      /* 96bit XOR fail map */
#define FM_RECORD_SIZE  18      /* 1 레코드 = ADDR + IO */

/* ADDR 필드 비트 레이아웃 */
#define FM_ADDR_BITS    48
#define FM_IO_BITS      96
#define FM_BL_BITS      3       /* 하위 3bit = BL 번호 (0~7) */
#define FM_BL_MASK      0x07
#define FM_BL_COUNT     8       /* 8BL burst */

/*
 * FM 레코드 구조체
 * 1 record = 18 bytes
 *   addr[6] : 48bit DRAM 주소 (big-endian 저장)
 *   io[12]  : 96bit XOR fail map (big-endian 저장)
 */
typedef struct {
    uint8_t addr[FM_ADDR_SIZE];
    uint8_t io[FM_IO_SIZE];
} fm_record_t;

/*
 * ADDR 필드에서 row/BL 분리 매크로
 *
 * addr_val : uint64_t로 변환된 48bit 주소값
 *
 * FM_ADDR_GET_BL(addr_val)  : 하위 3bit (BL 번호, 0~7)
 * FM_ADDR_GET_ROW(addr_val) : 상위 45bit (row 주소)
 */
#define FM_ADDR_GET_BL(addr_val)   ((uint8_t)((addr_val) & FM_BL_MASK))
#define FM_ADDR_GET_ROW(addr_val)  ((uint64_t)((addr_val) >> FM_BL_BITS))

/* addr[6] 배열을 uint64_t로 변환 (big-endian) */
static inline uint64_t fm_addr_to_u64(const uint8_t addr[FM_ADDR_SIZE])
{
    uint64_t v = 0;
    for (int i = 0; i < FM_ADDR_SIZE; i++)
        v = (v << 8) | addr[i];
    return v;
}

/* uint64_t를 addr[6] 배열로 변환 (big-endian, 하위 48bit 사용) */
static inline void fm_u64_to_addr(uint64_t v, uint8_t addr[FM_ADDR_SIZE])
{
    for (int i = FM_ADDR_SIZE - 1; i >= 0; i--) {
        addr[i] = (uint8_t)(v & 0xFF);
        v >>= 8;
    }
}

/*
 * 레코드 읽기/쓰기 유틸 함수
 *
 * fm_record_read  : 파일에서 레코드 n개 읽기, 읽은 개수 반환
 * fm_record_write : 파일에 레코드 n개 쓰기, 쓴 개수 반환
 * fm_record_read_buf  : 바이트 버퍼에서 레코드 1개 파싱
 * fm_record_write_buf : 레코드 1개를 바이트 버퍼에 직렬화
 */
size_t fm_record_read(FILE *fp, fm_record_t *records, size_t n);
size_t fm_record_write(FILE *fp, const fm_record_t *records, size_t n);
void   fm_record_read_buf(const uint8_t buf[FM_RECORD_SIZE], fm_record_t *rec);
void   fm_record_write_buf(const fm_record_t *rec, uint8_t buf[FM_RECORD_SIZE]);

#endif /* FM_RECORD_H */
