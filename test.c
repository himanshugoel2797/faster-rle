#include "rle.c"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void main(int argc, char *argv[])
{
    FILE *f = fopen(/*argv[1]*/ "chnk_raw.bin", "rb");
    fseek(f, 0, SEEK_END);
    uint32_t pos = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *src_pool = calloc((pos + 16), 1);
    uint8_t *dst_pool = calloc((pos + 16) * 4, 1);
    uint8_t *dec_pool = calloc((pos + 16), 1);

    fread(src_pool, 1, pos, f);
    fclose(f);

    pos = (pos + 7) & ~7;
    printf("Source Len: %d \r\n", pos);

    uint32_t expected_len = naive_rle((uint64_t *)src_pool, pos, dst_pool);
    printf("Expected Len: %d (%f%%) \r\n", expected_len,
           (expected_len / (float)pos) * 100);

    uint32_t encoded_len = rle_encode((uint64_t *)src_pool, pos, dst_pool);
    printf("Encoded Len: %d (%f%%) \r\n", encoded_len,
           (encoded_len / (float)pos) * 100);
    fflush(stdout);

    f = fopen("out.bin", "wb");
    fwrite(dst_pool, 1, encoded_len, f);
    fclose(f);

    rle_decode((uint64_t *)dst_pool, encoded_len, dec_pool);
    for (uint32_t i = 0; i < pos; i++)
        if (src_pool[i] != dec_pool[i])
        {
            printf("Mismatch at [%d] = %d should be %d\r\n", i, dec_pool[i],
                   src_pool[i]);

            break;
        }

    struct timespec tstart;
    struct timespec tstop;
    uint32_t iter_cnt = 100000;

    clock_gettime(CLOCK_REALTIME, &tstart);
    for (int i = 0; i < iter_cnt; i++)
        rle_encode((uint64_t *)src_pool, pos, dst_pool);
    clock_gettime(CLOCK_REALTIME, &tstop);

    uint64_t start =
        (uint64_t)tstart.tv_sec * 1000000000L + (uint64_t)tstart.tv_nsec;
    uint64_t stop =
        (uint64_t)tstop.tv_sec * 1000000000L + (uint64_t)tstop.tv_nsec;

    double us = (stop - start) / (1000.0 * iter_cnt);
    double mbps = pos / us;
    printf("Encode time: %f MB/s (%f us)\r\n", mbps, us);

    clock_gettime(CLOCK_REALTIME, &tstart);
    for (int i = 0; i < iter_cnt; i++)
        rle_decode((uint64_t *)dst_pool, encoded_len, src_pool);
    clock_gettime(CLOCK_REALTIME, &tstop);

    start = (uint64_t)tstart.tv_sec * 1000000000L + (uint64_t)tstart.tv_nsec;
    stop = (uint64_t)tstop.tv_sec * 1000000000L + (uint64_t)tstop.tv_nsec;
    us = (stop - start) / (1000.0 * iter_cnt);
    mbps = pos / us;
    printf("Decode time: %f MB/s (%f us)\r\n", mbps, us);
}