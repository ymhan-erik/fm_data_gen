#include "fm_record.h"
#include <string.h>

size_t fm_record_read(FILE *fp, fm_record_t *records, size_t n)
{
    return fread(records, FM_RECORD_SIZE, n, fp);
}

size_t fm_record_write(FILE *fp, const fm_record_t *records, size_t n)
{
    return fwrite(records, FM_RECORD_SIZE, n, fp);
}

void fm_record_read_buf(const uint8_t buf[FM_RECORD_SIZE], fm_record_t *rec)
{
    memcpy(rec->addr, buf,                FM_ADDR_SIZE);
    memcpy(rec->io,   buf + FM_ADDR_SIZE, FM_IO_SIZE);
}

void fm_record_write_buf(const fm_record_t *rec, uint8_t buf[FM_RECORD_SIZE])
{
    memcpy(buf,                rec->addr, FM_ADDR_SIZE);
    memcpy(buf + FM_ADDR_SIZE, rec->io,   FM_IO_SIZE);
}
