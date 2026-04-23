#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decoder.h"
#include "fm_record.h"

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-v] input.fmio output.bin\n", prog);
    fprintf(stderr, "  -v  verbose: print decoding statistics\n");
}

static void print_stats(const fm_file_header_t *hdr, size_t count, long in_size)
{
    long out_size  = (long)(count * FM_RECORD_SIZE);
    long addr_orig = (long)(count * FM_ADDR_SIZE);
    long io_orig   = (long)(count * FM_IO_SIZE);
    long addr_comp = (long)hdr->addr_size;
    long io_comp   = (long)hdr->io_size;

    printf("=== FM Decoder Stats ===\n");
    printf("Input  (.fmio)  : %ld bytes\n", in_size);
    printf("  Header        : %u bytes\n",  48u);
    printf("  ADDR block    : %ld bytes (compressed)  →  %ld bytes  (%.1fx)\n",
           addr_comp, addr_orig,
           addr_comp > 0 ? (double)addr_orig / addr_comp : 0.0);
    printf("  IO   block    : %ld bytes (compressed)  →  %ld bytes  (%.1fx)\n",
           io_comp, io_orig,
           io_comp   > 0 ? (double)io_orig   / io_comp   : 0.0);
    printf("Records         : %zu\n",       count);
    printf("Output (.bin)   : %ld bytes  (%.1fx expansion)\n",
           out_size,
           in_size > 0 ? (double)out_size / in_size : 0.0);
}

int main(int argc, char *argv[])
{
    int         verbose    = 0;
    const char *in_path    = NULL;
    const char *out_path   = NULL;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-v") == 0) verbose  = 1;
        else if (!in_path)                   in_path  = argv[i];
        else if (!out_path)                  out_path = argv[i];
        else { print_usage(argv[0]); return 1; }
    }
    if (!in_path || !out_path) { print_usage(argv[0]); return 1; }

    /* 입력 파일 */
    FILE *in_fp = fopen(in_path, "rb");
    if (!in_fp) { perror(in_path); return 1; }

    /* 파일 크기 (verbose용) */
    long in_size = 0;
    if (verbose) {
        fseek(in_fp, 0, SEEK_END);
        in_size = ftell(in_fp);
        rewind(in_fp);
    }

    /* 디코딩 */
    fm_record_t     *records = NULL;
    size_t           count   = 0;
    fm_file_header_t hdr;

    if (fm_decode_from_file(in_fp, &records, &count, &hdr) < 0) {
        fprintf(stderr, "Error: decode failed\n");
        fclose(in_fp);
        return 1;
    }
    fclose(in_fp);

    /* 출력 파일 */
    FILE *out_fp = fopen(out_path, "wb");
    if (!out_fp) { perror(out_path); free(records); return 1; }

    size_t n = fm_record_write(out_fp, records, count);
    fclose(out_fp);

    if (n != count) {
        fprintf(stderr, "Error: wrote %zu / %zu records\n", n, count);
        free(records);
        return 1;
    }

    if (verbose)
        print_stats(&hdr, count, in_size);

    free(records);
    return 0;
}
