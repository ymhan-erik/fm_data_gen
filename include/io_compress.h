#ifndef IO_COMPRESS_H
#define IO_COMPRESS_H

#include <stdint.h>
#include <stddef.h>
#include "fm_record.h"

/*
 * IO 레코드 단위 RLE 압축
 *
 * 배경:
 *   IO 필드(96bit)는 XOR fail map으로,
 *   yield가 좋을수록 all-zero 레코드가 대부분 → 높은 압축률.
 *   yield가 나쁠수록 non-zero 레코드 증가 → 낮은 압축률 (그 자체가 정보).
 *
 * 토큰 형식 (레코드 단위):
 *
 *   Token A (all-zero 연속):
 *     [0x00][count: uint32_t LE]   (5 bytes)
 *     → IO가 모두 0인 레코드가 count개 연속
 *
 *   Token B (fail 존재):
 *     [0xFF][io_data: 12 bytes]    (13 bytes)
 *     → fail 비트가 있는 레코드 1개의 IO를 그대로 저장
 *
 *   Token C (이전 non-zero IO 반복, v2 신규):
 *     [0xAA][count: uint32_t LE]   (5 bytes)
 *     → 직전 Token B IO 값을 count개 반복 (type4/type7 컬럼 결함 최적화)
 */

/* 토큰 마커 */
#define IO_TOKEN_ZERO   0x00u   /* all-zero 런 토큰 (Token A) */
#define IO_TOKEN_FAIL   0xFFu   /* non-zero IO 토큰 (Token B) */
#define IO_TOKEN_REPEAT 0xAAu   /* 이전 non-zero IO 반복 토큰 (Token C, v2 신규) */

/*
 * io_compress
 *   - records[]의 IO 필드를 레코드 단위 RLE로 압축
 *   - out_buf : 압축 결과 버퍼 (호출자 할당, io_compress_bound() 크기)
 *   - out_size: 실제 압축된 바이트 수
 *   - 성공 시 0, 실패 시 -1 반환
 *
 * io_decompress
 *   - 압축된 buf에서 IO 필드를 복원하여 records[]의 io에 기록
 *   - count : 복원할 레코드 수
 *   - 성공 시 0, 실패 시 -1 반환
 *
 * io_compress_bound
 *   - count개 레코드 IO 압축 시 최대 출력 크기 반환 (버퍼 할당용)
 *   - 최악의 경우: 모든 레코드가 non-zero → count * 13 + 5 bytes
 */
int    io_compress(const fm_record_t *records, size_t count,
                   uint8_t *out_buf, size_t *out_size);

int    io_decompress(const uint8_t *buf, size_t buf_size,
                     fm_record_t *records, size_t count);

size_t io_compress_bound(size_t count);

#endif /* IO_COMPRESS_H */
