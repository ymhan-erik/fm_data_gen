/*
 * extract_records.c
 *
 * FM 레코드 바이너리 파일에서 일부 레코드를 추출하여 새 파일로 저장.
 *
 * Usage:
 *   extract_records <input.bin> <offset> <count> <output.bin>
 *
 *   offset : 시작 레코드 인덱스 (0-based)
 *   count  : 추출할 레코드 수
 *
 * 레코드 1개 = 18 bytes (ADDR 6B + IO 12B)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define FM_RECORD_SIZE  18

int main(int argc, char *argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <input.bin> <offset> <count> <output.bin>\n",
                argv[0]);
        fprintf(stderr, "  offset : 시작 레코드 인덱스 (0-based)\n");
        fprintf(stderr, "  count  : 추출할 레코드 수\n");
        return 1;
    }

    const char *in_path  = argv[1];
    long        offset   = strtol(argv[2], NULL, 0);
    long        count    = strtol(argv[3], NULL, 0);
    const char *out_path = argv[4];

    if (offset < 0 || count <= 0) {
        fprintf(stderr, "Error: offset >= 0, count > 0 이어야 합니다.\n");
        return 1;
    }

    /* 입력 파일 열기 */
    FILE *fin = fopen(in_path, "rb");
    if (!fin) {
        fprintf(stderr, "Error: 입력 파일 열기 실패 '%s': %s\n",
                in_path, strerror(errno));
        return 1;
    }

    /* 파일 크기로 총 레코드 수 계산 */
    fseek(fin, 0, SEEK_END);
    long file_size    = ftell(fin);
    long total_records = file_size / FM_RECORD_SIZE;

    if (offset >= total_records) {
        fprintf(stderr, "Error: offset(%ld) >= 총 레코드 수(%ld)\n",
                offset, total_records);
        fclose(fin);
        return 1;
    }

    /* count 초과 방지 */
    if (offset + count > total_records) {
        long adjusted = total_records - offset;
        fprintf(stderr, "Warning: count(%ld) 조정 → %ld (파일 끝 초과)\n",
                count, adjusted);
        count = adjusted;
    }

    /* offset 위치로 이동 */
    fseek(fin, (long)(offset * FM_RECORD_SIZE), SEEK_SET);

    /* 출력 파일 열기 */
    FILE *fout = fopen(out_path, "wb");
    if (!fout) {
        fprintf(stderr, "Error: 출력 파일 열기 실패 '%s': %s\n",
                out_path, strerror(errno));
        fclose(fin);
        return 1;
    }

    /* 레코드 단위로 복사 */
    uint8_t buf[FM_RECORD_SIZE];
    long written = 0;

    for (long i = 0; i < count; i++) {
        if (fread(buf, 1, FM_RECORD_SIZE, fin) != FM_RECORD_SIZE) {
            fprintf(stderr, "Error: 레코드 %ld 읽기 실패\n", offset + i);
            break;
        }
        if (fwrite(buf, 1, FM_RECORD_SIZE, fout) != FM_RECORD_SIZE) {
            fprintf(stderr, "Error: 레코드 %ld 쓰기 실패\n", offset + i);
            break;
        }
        written++;
    }

    fclose(fin);
    fclose(fout);

    printf("추출 완료: %s\n", out_path);
    printf("  입력 총 레코드 수 : %ld\n", total_records);
    printf("  offset            : %ld\n", offset);
    printf("  추출 레코드 수    : %ld\n", written);
    printf("  출력 크기         : %ld bytes\n", written * FM_RECORD_SIZE);

    return (written == count) ? 0 : 1;
}
