#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "rle.c"

void main(int argc, char *argv[])
{
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    uint32_t pos = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *src_pool = calloc((pos + 7) / 8, 8);
    uint8_t *dst_pool = calloc((pos + 7) / 8, 8);

    fread(src_pool, 1, pos, f);
    fclose(f);

    uint32_t encoded_len = rle_encode((uint64_t *)src_pool, pos, dst_pool);

    printf("Source Len: %d \r\n", pos);
    printf("Encoded Len: %d (%f%%) \r\n", encoded_len, (encoded_len / (float)pos) * 100);

    struct timespec tstart;
    struct timespec tstop;
    uint32_t iter_cnt = 10;

    clock_gettime(CLOCK_REALTIME, &tstart);
    for (int i = 0; i < iter_cnt; i++)
        rle_encode((uint64_t *)src_pool, pos, dst_pool);
    clock_gettime(CLOCK_REALTIME, &tstop);

    uint64_t start = (uint64_t)tstart.tv_sec * 1000000000L + (uint64_t)tstart.tv_nsec;
    uint64_t stop = (uint64_t)tstop.tv_sec * 1000000000L + (uint64_t)tstop.tv_nsec;

    double us = (stop - start) / (1000.0 * iter_cnt);
    double mbps = pos / us;
    printf("Encode time: %f MB/s\r\n", mbps);

    clock_gettime(CLOCK_REALTIME, &tstart);
    for (int i = 0; i < iter_cnt; i++)
        rle_decode((uint64_t *)dst_pool, encoded_len, src_pool);
    clock_gettime(CLOCK_REALTIME, &tstop);

    start = (uint64_t)tstart.tv_sec * 1000000000L + (uint64_t)tstart.tv_nsec;
    stop = (uint64_t)tstop.tv_sec * 1000000000L + (uint64_t)tstop.tv_nsec;
    us = (stop - start) / (1000.0 * iter_cnt);
    mbps = pos / us;
    printf("Decode time: %f MB/s\r\n", mbps);
}