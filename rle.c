#include "rle.h"
#include <x86intrin.h>

uint32_t rle_encode(uint64_t *raw_Data_u64, uint32_t len, uint8_t *dst)
{
    uint64_t cur_val = 0;
    uint64_t next_val = raw_Data_u64[0];
    uint32_t c_runLen = 0;
    uint32_t c_val = next_val & 0xff;
    uint8_t *comp_ptr = (uint8_t *)dst;
    for (int j = 1; j < len / sizeof(uint64_t); j++)
    {
        cur_val = next_val;
        next_val = raw_Data_u64[j];
        uint64_t cur_val_shifted = (cur_val >> 8) | (next_val << 56);
        uint64_t equality_mask = cur_val_shifted ^ cur_val;
        //count matched parts
        if (equality_mask == 0)
        {
            uint8_t proc_byte = cur_val;
            if (c_val == proc_byte && c_runLen + 64 <= 256 * 8)
                c_runLen += 64;
            else
            {
                *(comp_ptr++) = c_runLen / 8 - 1;
                *(comp_ptr++) = c_val;

                c_runLen = 64;
                c_val = proc_byte;
            }
        }
        else
            for (int32_t avail_bits = 64; avail_bits != 0;)
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
                    *(comp_ptr++) = c_runLen / 8 - 1;
                    *(comp_ptr++) = c_val;

                    c_runLen = matched_bits;
                    c_val = proc_byte;
                }

                avail_bits -= matched_bits;
                cur_val >>= matched_bits;
                equality_mask >>= matched_bits;
                equality_mask &= ~0xff;
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
            *(comp_ptr++) = c_runLen / 8 - 1;
            *(comp_ptr++) = c_val;

            //last write, no need to save values further
            c_runLen = 64;
            c_val = proc_byte;
        }
    }
    else
        for (int32_t avail_bits = 64; avail_bits != 0;)
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
                *(comp_ptr++) = c_runLen / 8 - 1;
                *(comp_ptr++) = c_val;

                c_runLen = matched_bits;
                c_val = proc_byte;
            }

            avail_bits -= matched_bits;
            cur_val >>= matched_bits;
            equality_mask >>= matched_bits;
            equality_mask &= ~0xff;
        }
    if (c_runLen != 0)
    {
        *(comp_ptr++) = c_runLen / 8 - 1;
        *(comp_ptr++) = c_val;
    }

    return comp_ptr - dst;
}

uint32_t rle_decode(uint64_t *comp_data_space, uint32_t rle_len, uint8_t *decomp_pool)
{
    uint8_t *tmp_ptr = (uint8_t *)decomp_pool;
    uint64_t *comp_ptr = (uint64_t *)comp_data_space;
    uint32_t rle_len2 = (rle_len + 7) / 8;
    uint32_t iter = 0;

    for (int j = 0; j < rle_len2; j++)
    {
        uint64_t v_net = *(comp_ptr++);

        int32_t len;
        uint8_t v;
        uint64_t v_64;
        uint64_t *tmp_ptr_64;
        uint64_t mask;
        uint64_t v_64_s;

        //Align the value being written so the pointer can be aligned
        //std::cout << "len: " << std::dec << len << " v: " << v << " iter: " << iter << std::hex << " v_64_s: " << v_64_s << std::endl;
#define INNER_LOOP(off)                              \
    len = ((v_net >> (off * 8)) & 0xff) + 1;         \
    v = (v_net >> ((off + 1) * 8)) & 0xff;           \
    v_64 = v * 0x0101010101010101;                   \
    tmp_ptr_64 = (uint64_t *)&tmp_ptr[iter & ~7];    \
    mask = 0xffffffffffffffff << ((iter & 0x7) * 8); \
    v_64_s = v_64 & mask;                            \
    v_64_s |= (*tmp_ptr_64) & ~mask;                 \
    *(tmp_ptr_64++) = v_64_s;                        \
    iter += len;                                     \
    len = (len + 7) / 8;                             \
    while (len-- > 0)                                \
        *(tmp_ptr_64++) = v_64;

        INNER_LOOP(0)
        INNER_LOOP(2)
        INNER_LOOP(4)
        INNER_LOOP(6)
    }

    //for (int j = 0; j < 64 * 1024; j++)
    //    if (decomp_pool[j] != raw_Data[j])
    //    {
    //        std::cout << "Not matched at " << j << " Value found decomp[j]: " << (int)decomp_pool[j] << "\r\n";
    //        break;
    //    }
    return iter;
}