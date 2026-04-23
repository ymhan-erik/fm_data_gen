#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "encoder.h"
#include "fm_record.h"

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s [-v] input.bin  output.fmio   (encode)\n", prog);
    fprintf(stderr, "  %s  -d  input.fmio output.bin    (decode)\n", prog);
    fprintf(stderr, "  -v  verbose: print compression statistics\n");
}

/* --------------------------------------------------------------------------
 * 인코드 경로
 * -------------------------------------------------------------------------- */

static void print_encode_stats(size_t count, long orig_size, long out_size,
                                const fm_file_header_t *hdr)
{
    long orig_addr = (long)(count * FM_ADDR_SIZE);
    long orig_io   = (long)(count * FM_IO_SIZE);
    long comp_addr = (long)hdr->addr_size;
    long comp_io   = (long)hdr->io_size;

    printf("=== FM Encoder Stats ===\n");
    printf("Records        : %zu\n",       count);
    printf("Original size  : %ld bytes\n", orig_size);
    printf("  ADDR block   : %ld -> %ld bytes  (%.1f%%)\n",
           orig_addr, comp_addr, orig_addr > 0 ? 100.0 * comp_addr / orig_addr : 0.0);
    printf("  IO   block   : %ld -> %ld bytes  (%.1f%%)\n",
           orig_io,   comp_io,   orig_io   > 0 ? 100.0 * comp_io   / orig_io   : 0.0);
    printf("Output size    : %ld bytes  (%.1f%% of original, %.2fx)\n",
           out_size,
           orig_size > 0 ? 100.0 * out_size / orig_size : 0.0,
           orig_size > 0 ? (double)orig_size / out_size  : 0.0);
}

static int do_encode(const char *in_path, const char *out_path, int verbose)
{
    FILE *in_fp = fopen(in_path, "rb");
    if (!in_fp) { perror(in_path); return 1; }

    if (fseek(in_fp, 0, SEEK_END) < 0) { perror("fseek"); fclose(in_fp); return 1; }
    long file_size = ftell(in_fp);
    rewind(in_fp);

    if (file_size <= 0) {
        fprintf(stderr, "Error: %s is empty or unreadable\n", in_path);
        fclose(in_fp); return 1;
    }
    if (file_size % FM_RECORD_SIZE != 0) {
        fprintf(stderr, "Error: %s size (%ld) is not a multiple of %d\n",
                in_path, file_size, FM_RECORD_SIZE);
        fclose(in_fp); return 1;
    }

    size_t count = (size_t)(file_size / FM_RECORD_SIZE);
    fm_record_t *records = malloc(count * sizeof(fm_record_t));
    if (!records) {
        fprintf(stderr, "Error: out of memory (%zu records)\n", count);
        fclose(in_fp); return 1;
    }

    if (fm_record_read(in_fp, records, count) != count) {
        fprintf(stderr, "Error: read failed\n");
        fclose(in_fp); free(records); return 1;
    }
    fclose(in_fp);

    FILE *out_fp = fopen(out_path, "w+b");
    if (!out_fp) { perror(out_path); free(records); return 1; }

    if (fm_encode(out_fp, records, count) < 0) {
        fprintf(stderr, "Error: encoding failed\n");
        fclose(out_fp); free(records); remove(out_path); return 1;
    }

    if (verbose) {
        long out_size = ftell(out_fp);
        rewind(out_fp);
        fm_file_header_t hdr;
        if (fm_header_read(out_fp, &hdr) == 0)
            print_encode_stats(count, file_size, out_size, &hdr);
        else
            fprintf(stderr, "Warning: could not read back header for stats\n");
    }

    fclose(out_fp);
    free(records);
    return 0;
}

/* --------------------------------------------------------------------------
 * 디코드 경로
 * -------------------------------------------------------------------------- */

static int do_decode(const char *in_path, const char *out_path, int verbose)
{
    FILE *in_fp = fopen(in_path, "rb");
    if (!in_fp) { perror(in_path); return 1; }

    fm_record_t *records = NULL;
    size_t count = 0;
    if (fm_decode(in_fp, &records, &count) < 0) {
        fprintf(stderr, "Error: decoding failed\n");
        fclose(in_fp); return 1;
    }
    fclose(in_fp);

    FILE *out_fp = fopen(out_path, "wb");
    if (!out_fp) { perror(out_path); free(records); return 1; }

    size_t n = fm_record_write(out_fp, records, count);
    fclose(out_fp);

    if (n != count) {
        fprintf(stderr, "Error: wrote %zu / %zu records\n", n, count);
        free(records); return 1;
    }

    if (verbose)
        printf("Decoded: %zu records -> %zu bytes\n",
               count, count * FM_RECORD_SIZE);

    free(records);
    return 0;
}

/* --------------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    int         decode      = 0;
    int         verbose     = 0;
    const char *input_path  = NULL;
    const char *output_path = NULL;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-d") == 0) decode  = 1;
        else if (strcmp(argv[i], "-v") == 0) verbose = 1;
        else if (!input_path)                input_path  = argv[i];
        else if (!output_path)               output_path = argv[i];
        else { print_usage(argv[0]); return 1; }
    }

    if (!input_path || !output_path) { print_usage(argv[0]); return 1; }

    return decode
        ? do_decode(input_path, output_path, verbose)
        : do_encode(input_path, output_path, verbose);
}
