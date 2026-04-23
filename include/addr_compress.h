#ifndef ADDR_COMPRESS_H
#define ADDR_COMPRESS_H

#include <stdint.h>
#include <stddef.h>
#include "fm_record.h"

/*
 * ADDR Delta 비트스트림 압축
 *
 * 배경:
 *   FM 레코드 ADDR은 8BL burst 단위로 연속 접근됨.
 *   → BL 내 이동: delta = +1 이 대부분 (가장 빈번)
 *   → row 변경  : delta가 작은 값 (드물게)
 *   → 큰 점프   : delta가 큰 값 (매우 드물게)
 *
 * 비트스트림 인코딩 (MSB-first):
 *
 *   첫 레코드   : [addr: 48bit]           절대 주소
 *
 *   이후 레코드 : delta = curr - prev (int64_t)
 *
 *   v1:
 *   delta == +1           →  [0]                1 bit   (BL 내 연속)
 *   -32768 ≤ delta ≤ 32767 →  [1][0][delta: s16] 18 bits (row 변경 등)
 *   그 외                 →  [1][1][addr: 48bit] 50 bits (큰 점프, 절대 주소)
 *
 *   v2 (delta=-1 특수 케이스 추가, type6 역방향 스캔 최적화):
 *   delta == +1           →  [0]                 1 bit   (BL 내 연속)
 *   delta == -1           →  [1][0]               2 bits  (역방향 스캔, 신규)
 *   s16 범위 (!=±1)       →  [1][1][0][delta:s16] 19 bits (row 변경 등)
 *   그 외                 →  [1][1][1][addr:48bit] 51 bits (큰 점프, 절대 주소)
 *
 * 압축률:
 *   8BL burst 연속이면 7/8 레코드가 1bit → 원본 대비 ~98% 압축
 */

/*
 * addr_compress
 *   - records[]의 ADDR 필드를 비트스트림 delta 방식으로 압축
 *   - out_buf : 압축 결과 버퍼 (호출자 할당, addr_compress_bound() 크기)
 *   - out_size: 실제 압축된 바이트 수
 *   - 성공 시 0, 실패 시 -1 반환
 *
 * addr_decompress
 *   - 압축된 buf에서 ADDR 필드를 복원하여 records[]의 addr에 기록
 *   - count : 복원할 레코드 수
 *   - 성공 시 0, 실패 시 -1 반환
 *
 * addr_compress_bound
 *   - count개 레코드 압축 시 최대 출력 크기 반환 (버퍼 할당용)
 *   - 최악의 경우 (v2): 첫 레코드 48bit + 이후 레코드당 51bit (111+addr48)
 *
 * addr_decompress      : v1 비트스트림 디코딩 (구 포맷 하위 호환용)
 * addr_decompress_v2   : v2 비트스트림 디코딩 (delta=-1 특수 케이스 포함)
 */
int    addr_compress(const fm_record_t *records, size_t count,
                     uint8_t *out_buf, size_t *out_size);

int    addr_decompress(const uint8_t *buf, size_t buf_size,
                       fm_record_t *records, size_t count);

int    addr_decompress_v2(const uint8_t *buf, size_t buf_size,
                          fm_record_t *records, size_t count);

size_t addr_compress_bound(size_t count);

#endif /* ADDR_COMPRESS_H */
