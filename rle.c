#include "rle.h"
#include <string.h>
#include <x86intrin.h>

uint32_t naive_rle(uint64_t *raw_Data_u64, uint32_t len, uint8_t *dst)
{
    uint8_t *raw_Data = (uint8_t *)raw_Data_u64;

    uint8_t *init_dst = dst;
    uint8_t cur_val = *(raw_Data++);
    uint8_t cur_run = 0;
    for (int i = 1; i < len; i++)
    {
        if ((*raw_Data != cur_val) | (cur_run == 255))
        {
            dst[0] = cur_run;
            dst[1] = cur_val;
            dst += 2;

            cur_val = *raw_Data;
            cur_run = -1;
        }
        cur_run++;
        raw_Data++;
    }

    return (dst - init_dst);
}

uint32_t entropy_rle(uint8_t *raw_Data_u64, uint32_t len, uint8_t *dst)
{
    uint32_t counts[256];
    memset(counts, 0, 256 * sizeof(uint32_t));
    for (int i = 0; i < len; i++)
        counts[raw_Data_u64[i]]++;
    uint32_t max_idx = 0;
    for (int i = 1; i < 256; i++)
        if (counts[i] > counts[max_idx])
            max_idx = i;
    return -1;
}

uint32_t rle_encode(uint64_t *raw_Data_u64, uint32_t len, uint8_t *dst)
{
    uint64_t cur_val = 0;
    uint64_t next_val = raw_Data_u64[0];
    uint32_t c_runLen = 0;
    uint32_t c_val = next_val & 0xff;
    uint8_t *comp_ptr = (uint8_t *)dst;
    uint64_t comp_str = 0;
    uint32_t comp_str_cntr = 0;

    uint64_t *comp_ptr_start = comp_ptr;
    for (int j = 1; j < len / sizeof(uint64_t); j++)
    {
        //load these into vector registers
        //shift vector by 1 byte
        //blendv
        //xor
        //cmpeq
        //movemask
        //not
        //tzcnt trick
        cur_val = next_val;
        next_val = raw_Data_u64[j];
        uint64_t proc_muld = (uint8_t)cur_val * 0x0101010101010101;
        if (proc_muld == cur_val)
        {
            uint8_t proc_byte = cur_val;
            if (c_val == proc_byte && c_runLen + 64 <= 256 * 8)
                c_runLen += 64;
            else
            {
                comp_ptr[0] = c_runLen / 8 - 1;
                comp_ptr[1] = c_val;
                comp_ptr += 2;

                c_runLen = 64;
                c_val = proc_byte;
            }
        }
        else
        {
            uint64_t cur_val_shifted = (cur_val >> 8) | (next_val << 56);
            uint64_t equality_mask = cur_val_shifted ^ cur_val;
            //count matched parts

            int32_t avail_bits = 64;
            for (; avail_bits > 0;)
            {
                uint8_t proc_byte = cur_val;
                int32_t matched_bits = __tzcnt_u64(equality_mask) & ~7;
                matched_bits += 8;
                if (matched_bits > avail_bits)
                    matched_bits = avail_bits;

                if (c_val == proc_byte && c_runLen + matched_bits <= 256 * 8)
                    c_runLen += matched_bits;
                else
                {
                    comp_ptr[0] = c_runLen / 8 - 1;
                    comp_ptr[1] = c_val;
                    comp_ptr += 2;
                    c_runLen = matched_bits;
                    c_val = proc_byte;
                }

                avail_bits -= matched_bits;
                cur_val >>= matched_bits;
                equality_mask >>= matched_bits;
            }
        }
    }

    cur_val = next_val;
    uint64_t cur_val_shifted = (cur_val >> 8);
    uint64_t equality_mask = cur_val_shifted ^ cur_val;
    //count matched parts
    if (equality_mask == 0)
    {
        uint8_t proc_byte = cur_val;
        if (c_val == proc_byte && c_runLen + 64 <= 256 * 8)
            c_runLen += 64;
        else
        {
            comp_ptr[0] = c_runLen / 8 - 1;
            comp_ptr[1] = c_val;
            comp_ptr += 2;
            //last write, no need to save values further
            c_runLen = 64;
            c_val = proc_byte;
        }
    }
    else
    {
        int32_t avail_bits = 64;
        for (; avail_bits > 0;)
        {
            uint8_t proc_byte = cur_val;
            int32_t matched_bits = __tzcnt_u64(equality_mask) & ~7;
            matched_bits += 8;
            if (matched_bits > avail_bits)
                matched_bits = avail_bits;

            if (c_val == proc_byte && c_runLen + matched_bits <= 256 * 8)
                c_runLen += matched_bits;
            else
            {
                comp_ptr[0] = c_runLen / 8 - 1;
                comp_ptr[1] = c_val;
                comp_ptr += 2;

                c_runLen = matched_bits;
                c_val = proc_byte;
            }

            avail_bits -= matched_bits;
            cur_val >>= matched_bits;
            equality_mask >>= matched_bits;
        }
    }
    if (c_runLen != 0)
    {
        comp_ptr[0] = c_runLen / 8 - 1;
        comp_ptr[1] = c_val;
        comp_ptr += 2;
    }

    return (comp_ptr - dst);
}

uint32_t rle_decode(uint64_t *comp_data_space, uint32_t rle_len, uint8_t *decomp_pool)
{
    uint8_t *tmp_ptr = (uint8_t *)decomp_pool;
    uint64_t *comp_ptr = (uint64_t *)comp_data_space;
    uint8_t *comp_ptr_8 = (uint8_t *)comp_data_space;
    uint64_t *decomp_64 = (uint64_t *)decomp_pool;
    uint32_t iter = 0;
    uint64_t last_write = 0;

    //build 64 bit values and write them in one go
    //(iter & 7) << 3 | (len & 7)
    const uint64_t mask_lut[] = {
        0xffffffffffffffff,
        0xffffffffffffff00,
        0xffffffffffff0000,
        0xffffffffff000000,
        0xffffffff00000000,
        0xffffff0000000000,
        0xffff000000000000,
        0xff00000000000000,
    };

    for (int j = 0; j < rle_len / 8; j++)
    {
        uint64_t *tmp_ptr_64;
        int32_t len;
        uint8_t v;
        uint64_t v_64;
        uint64_t mask;
        uint64_t iter_tmp;
        uint64_t v_64_s;

        //Align the value being written so the pointer can be aligned
        //std::cout << "len: " << std::dec << len << " v: " << v << " iter: " << iter << std::hex << " v_64_s: " << v_64_s << std::endl;
#define INNER_LOOP(off)                                   \
    len = comp_ptr_8[off];                                \
    v = comp_ptr_8[(off + 1)];                            \
    if ((iter & 0xf) == 0)                                \
    {                                                     \
        tmp_ptr_64 = (uint64_t *)&tmp_ptr[iter];          \
        iter += len + 1;                                  \
        len /= 16;                                        \
        __m128i c16 = _mm_set1_epi8(v);                   \
        while (len-- >= 0)                                \
        {                                                 \
            _mm_store_si128((__m128i *)tmp_ptr_64, c16);  \
            tmp_ptr_64 += 2;                              \
        }                                                 \
    }                                                     \
    else                                                  \
    {                                                     \
        v_64 = v * 0x0101010101010101;                    \
        tmp_ptr_64 = (uint64_t *)&tmp_ptr[iter & ~7];     \
        iter_tmp = iter & 0x7;                            \
        mask = mask_lut[iter_tmp];                        \
        v_64_s = (*(tmp_ptr_64) & ~mask) | (v_64 & mask); \
        *(tmp_ptr_64++) = v_64_s;                         \
        iter += len + 1;                                  \
        len = (len - (7 - iter_tmp) + 7) / 8;             \
        while (len-- > 0)                                 \
            *(tmp_ptr_64++) = v_64;                       \
    }

        INNER_LOOP(0)
        INNER_LOOP(2)
        INNER_LOOP(4)
        INNER_LOOP(6)
        comp_ptr_8 += 8;
        //INNER_LOOP(2)
        //INNER_LOOP(4)
        //INNER_LOOP(6)
    }

    rle_len = rle_len & 0xf;
    if (rle_len > 0)
    {
        uint64_t *tmp_ptr_64;
        int32_t len;
        uint8_t v;
        uint64_t v_64;
        uint64_t mask;
        uint64_t iter_tmp;
        uint64_t v_64_s;

        INNER_LOOP(0);

        if (rle_len > 2)
        {
            INNER_LOOP(2);
        }
        if (rle_len > 4)
        {
            INNER_LOOP(4);
        }
    }
    //for (int j = 0; j < 64 * 1024; j++)
    //    if (decomp_pool[j] != raw_Data[j])
    //    {
    //        std::cout << "Not matched at " << j << " Value found decomp[j]: " << (int)decomp_pool[j] << "\r\n";
    //        break;
    //    }
    return iter;
}