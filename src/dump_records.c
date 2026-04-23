/*
 * dump_records.c
 *
 * FM 레코드 바이너리 파일을 텍스트로 출력.
 *
 * Usage:
 *   dump_records <input.bin> [offset] [count]
 *
 *   offset : 시작 레코드 인덱스 (0-based, 기본값 0)
 *   count  : 출력할 레코드 수 (기본값: 파일 끝까지)
 *
 * 출력 형식:
 *   #idx  ADDR(48bit hex)  IO(96bit hex)  BL  BURST
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define FM_RECORD_SIZE  18
#define FM_ADDR_SIZE     6
#define FM_IO_SIZE      12

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s <input.bin> [offset] [count]\n", argv[0]);
        return 1;
    }

    const char *in_path = argv[1];
    long offset = (argc >= 3) ? strtol(argv[2], NULL, 0) : 0;
    long count  = (argc >= 4) ? strtol(argv[3], NULL, 0) : -1; /* -1 = 끝까지 */

    if (offset < 0) { fprintf(stderr, "Error: offset >= 0\n"); return 1; }

    FILE *fin = fopen(in_path, "rb");
    if (!fin) {
        fprintf(stderr, "Error: '%s': %s\n", in_path, strerror(errno));
        return 1;
    }

    fseek(fin, 0, SEEK_END);
    long file_size     = ftell(fin);
    long total_records = file_size / FM_RECORD_SIZE;

    if (offset >= total_records) {
        fprintf(stderr, "Error: offset(%ld) >= 총 레코드 수(%ld)\n", offset, total_records);
        fclose(fin);
        return 1;
    }

    if (count < 0 || offset + count > total_records)
        count = total_records - offset;

    fseek(fin, offset * FM_RECORD_SIZE, SEEK_SET);

    /* 헤더 */
    printf("# input       : %s\n", in_path);
    printf("# total       : %ld records\n", total_records);
    printf("# offset      : %ld\n", offset);
    printf("# count       : %ld\n", count);
    printf("#\n");
    printf("%-8s  %-12s  %-24s  %-2s  %s\n",
           "#idx", "ADDR", "IO", "BL", "BURST");
    printf("%-8s  %-12s  %-24s  %-2s  %s\n",
           "--------", "------------", "------------------------", "--", "--------------------");

    uint8_t buf[FM_RECORD_SIZE];

    for (long i = 0; i < count; i++) {
        if (fread(buf, 1, FM_RECORD_SIZE, fin) != (size_t)FM_RECORD_SIZE) {
            fprintf(stderr, "Error: 레코드 %ld 읽기 실패\n", offset + i);
            break;
        }

        /* ADDR → uint64_t */
        uint64_t addr = 0;
        for (int j = 0; j < FM_ADDR_SIZE; j++)
            addr = (addr << 8) | buf[j];

        uint8_t bl    = (uint8_t)(addr & 0x07);
        uint64_t burst = addr >> 3;

        /* 출력: idx, ADDR hex, IO hex, BL, BURST */
        printf("%-8ld  ", offset + i);

        for (int j = 0; j < FM_ADDR_SIZE; j++)
            printf("%02X", buf[j]);
        printf("  ");

        for (int j = FM_ADDR_SIZE; j < FM_RECORD_SIZE; j++)
            printf("%02X", buf[j]);
        printf("  ");

        printf("%-2u  %llu\n", bl, (unsigned long long)burst);
    }

    fclose(fin);
    return 0;
}
