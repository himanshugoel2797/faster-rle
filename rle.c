#include "rle.h"
#include <x86intrin.h>

uint32_t rle_encode(uint64_t *raw_Data_u64, uint32_t len, uint8_t *dst)
{
    uint64_t cur_val = 0;
    uint64_t next_val = raw_Data_u64[0];
    uint32_t c_runLen = 0;
    uint32_t c_val = next_val & 0xff;
    uint64_t *comp_ptr = (uint64_t *)dst;
    uint64_t comp_str = 0;
    uint32_t comp_str_cntr = 0;

    uint64_t *comp_ptr_start = comp_ptr;
    for (int j = 1; j < len / sizeof(uint64_t); j++)
    {
        if (len / sizeof(uint64_t) != (65536 / 8))
            printf("len = %d\r\n", len);
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
                comp_str_cntr++;
                comp_str = (comp_str << 16) | (c_val << 8) | (c_runLen / 8 - 1);
                if ((comp_str_cntr & 3) == 0)
                    *(comp_ptr++) = comp_str;
                //*(comp_ptr++) = c_runLen / 8 - 1;
                //*(comp_ptr++) = c_val;

                c_runLen = 64;
                c_val = proc_byte;
            }
        }
        else
        {
            int32_t avail_bits = 64;
            if ((equality_mask & 0xffffffff) == 0)
            {
                uint8_t proc_byte = cur_val;
                if (c_val == proc_byte && c_runLen + 32 <= 256 * 8)
                    c_runLen += 32;
                else
                {
                    comp_str_cntr++;
                    comp_str = (comp_str << 16) | (c_val << 8) | (c_runLen / 8 - 1);
                    if ((comp_str_cntr & 3) == 0)
                        *(comp_ptr++) = comp_str;
                    //last write, no need to save values further
                    c_runLen = 32;
                    c_val = proc_byte;
                }

                avail_bits -= 32;
                cur_val >>= 32;
                equality_mask >>= 32;
                equality_mask &= ~0xff;
            }
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
                    comp_str_cntr++;
                    comp_str = (comp_str << 16) | (c_val << 8) | (c_runLen / 8 - 1);
                    if ((comp_str_cntr & 3) == 0)
                        *(comp_ptr++) = comp_str;

                    c_runLen = matched_bits;
                    c_val = proc_byte;
                }

                avail_bits -= matched_bits;
                cur_val >>= matched_bits;
                equality_mask >>= matched_bits;
                equality_mask &= ~0xff;
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
            comp_str_cntr++;
            comp_str = (comp_str << 16) | (c_val << 8) | (c_runLen / 8 - 1);
            if ((comp_str_cntr & 3) == 0)
                *(comp_ptr++) = comp_str;
            //last write, no need to save values further
            c_runLen = 64;
            c_val = proc_byte;
        }
    }
    else
    {
        int32_t avail_bits = 64;
        if ((equality_mask & 0xffffffff) == 0)
        {
            uint8_t proc_byte = cur_val;
            if (c_val == proc_byte && c_runLen + 32 <= 256 * 8)
                c_runLen += 32;
            else
            {
                comp_str_cntr++;
                comp_str = (comp_str << 16) | (c_val << 8) | (c_runLen / 8 - 1);
                if ((comp_str_cntr & 3) == 0)
                    *(comp_ptr++) = comp_str;
                //last write, no need to save values further
                c_runLen = 32;
                c_val = proc_byte;
            }

            avail_bits -= 32;
            cur_val >>= 32;
            equality_mask >>= 32;
            equality_mask &= ~0xff;
        }
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
                comp_str_cntr++;
                comp_str = (comp_str << 16) | (c_val << 8) | (c_runLen / 8 - 1);
                if ((comp_str_cntr & 3) == 0)
                    *(comp_ptr++) = comp_str;

                c_runLen = matched_bits;
                c_val = proc_byte;
            }

            avail_bits -= matched_bits;
            cur_val >>= matched_bits;
            equality_mask >>= matched_bits;
            equality_mask &= ~0xff;
        }
    }
    if (c_runLen != 0)
    {
        comp_str_cntr++;
        comp_str = (comp_str << 16) | (c_val << 8) | (c_runLen / 8 - 1);
        if ((comp_str_cntr & 3) == 0)
            *(comp_ptr++) = comp_str;
    }
    *(comp_ptr++) = comp_str;

    return comp_str_cntr * 2;
}

uint32_t rle_decode(uint64_t *comp_data_space, uint32_t rle_len, uint8_t *decomp_pool)
{
    uint8_t *tmp_ptr = (uint8_t *)decomp_pool;
    uint64_t *comp_ptr = (uint64_t *)comp_data_space;
    uint32_t rle_len2 = (rle_len + 7) / 8;
    uint32_t iter = 0;
    uint64_t last_write = 0;

    const uint64_t mask_lut[] = {
        0xffffffffffffffff,
        0xffffffffffffff00,
        0xffffffffffff0000,
        0xffffffffff000000,
        0xffffffff00000000,
        0xffffff0000000000,
        0xffff000000000000,
        0xff00000000000000,
        0x0000000000000000,
    };

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
#define INNER_LOOP(off)                                 \
    len = ((v_net >> (off * 8)) & 0xff) + 1;            \
    v = (v_net >> ((off + 1) * 8)) & 0xff;              \
    v_64 = v * 0x0101010101010101;                      \
    tmp_ptr_64 = (uint64_t *)&tmp_ptr[iter & ~7];       \
    mask = mask_lut[(iter & 0x7)];                      \
    v_64_s = last_write ^ ((last_write ^ v_64) & mask); \
    last_write = v_64_s;                                \
    *(tmp_ptr_64++) = v_64_s;                           \
    iter += len;                                        \
    len = (len + 7) / 8;                                \
    if (len > 0)                                        \
        last_write = v_64;                              \
    while (len-- > 0)                                   \
        *(tmp_ptr_64++) = v_64;

        INNER_LOOP(6)
        INNER_LOOP(4)
        INNER_LOOP(2)
        INNER_LOOP(0)
    }

    //for (int j = 0; j < 64 * 1024; j++)
    //    if (decomp_pool[j] != raw_Data[j])
    //    {
    //        std::cout << "Not matched at " << j << " Value found decomp[j]: " << (int)decomp_pool[j] << "\r\n";
    //        break;
    //    }
    return iter;
}