#ifndef DECODER_H
#define DECODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "fm_record.h"
#include "encoder.h"    /* fm_file_header_t, FM_MAGIC, FM_VERSION */

/*
 * fm_decode_from_file
 *
 *   .fmio 파일(fp)을 읽어 FM 레코드 배열로 복원한다.
 *
 *   동작:
 *     1. 파일 헤더 읽기 & 검증 (magic, version)
 *     2. ADDR 압축 블록 읽기 → addr_decompress (delta bitstream)
 *     3. IO  압축 블록 읽기 → io_decompress   (bitmap RLE token)
 *     4. ADDR + IO 결합 → fm_record_t 복원
 *
 *   출력:
 *     *out_records : 복원된 레코드 배열 (호출자가 free() 해야 함)
 *     *out_count   : 복원된 레코드 수
 *     *out_hdr     : 파일 헤더 (NULL 전달 가능)
 *
 *   반환: 성공 0, 실패 -1
 */
int fm_decode_from_file(FILE            *fp,
                        fm_record_t    **out_records,
                        size_t          *out_count,
                        fm_file_header_t *out_hdr);

#endif /* DECODER_H */
