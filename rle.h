#ifndef _FASTER_RLE_H_
#define _FASTER_RLE_H_

#include <stdint.h>

uint32_t rle_encode(uint64_t *raw_Data_u64, uint32_t len, uint8_t *dst);
uint32_t rle_decode(uint64_t *comp_data_space, uint32_t rle_len, uint8_t *decomp_pool);

#endif