#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "fm_record.h"

/*
 * 압축 파일 매직 넘버 & 버전
 *
 * MAGIC: "FMIO" = 0x464D494F
 */
#define FM_MAGIC        0x464D494Fu
#define FM_VERSION      2

/*
 * 압축 파일 헤더 구조체 (파일 선두에 기록)
 *
 *  magic           : 0x464D494F ("FMIO")
 *  version         : 1
 *  record_count    : 원본 FM 레코드 총 개수
 *  addr_offset     : 파일 내 ADDR 압축 블록 시작 오프셋 (bytes)
 *  addr_size       : ADDR 압축 블록 크기 (bytes)
 *  io_offset       : 파일 내 IO 압축 블록 시작 오프셋 (bytes)
 *  io_size         : IO 압축 블록 크기 (bytes)
 *
 * 파일 레이아웃:
 *   [fm_file_header_t] [ADDR 압축 블록] [IO 압축 블록]
 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t record_count;
    uint64_t addr_offset;
    uint64_t addr_size;
    uint64_t io_offset;
    uint64_t io_size;
} fm_file_header_t;

#define FM_HEADER_SIZE  sizeof(fm_file_header_t)

/*
 * 엔코더 함수
 *
 * fm_encode
 *   - records[]를 읽어 ADDR/IO를 각각 압축 후 fp에 기록
 *   - 성공 시 0, 실패 시 -1 반환
 *
 * fm_decode
 *   - fp에서 압축 파일을 읽어 records[]를 복원
 *   - out_count에 복원된 레코드 수 저장
 *   - 성공 시 0, 실패 시 -1 반환
 *   - 호출자가 records 메모리 해제 책임
 *
 * fm_header_read  : 파일에서 헤더를 읽고 검증 (magic, version 확인)
 * fm_header_write : 헤더를 파일에 기록
 */
int fm_encode(FILE *fp, const fm_record_t *records, size_t count);
int fm_decode(FILE *fp, fm_record_t **records, size_t *out_count);

int fm_header_read(FILE *fp, fm_file_header_t *header);
int fm_header_write(FILE *fp, const fm_file_header_t *header);

#endif /* ENCODER_H */
