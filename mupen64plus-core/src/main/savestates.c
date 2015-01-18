/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - savestates.c                                            *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2012 CasualJames                                        *
 *   Copyright (C) 2009 Olejl Tillin9                                      *
 *   Copyright (C) 2008 Richard42 Tillin9                                  *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdlib.h>
#include <string.h>

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "api/m64p_config.h"
#include "api/config.h"

#include "savestates.h"
#include "main.h"
#include "rom.h"
#include "util.h"

#include "memory/memory.h"
#include "memory/flashram.h"
#include "plugin/plugin.h"
#include "r4300/tlb.h"
#include "r4300/cp0.h"
#include "r4300/cp1.h"
#include "r4300/r4300.h"
#include "r4300/cached_interp.h"
#include "r4300/interupt.h"
#include "r4300/new_dynarec/new_dynarec.h"
#include "osal/preproc.h"

static const char* savestate_magic = "M64+SAVE";
static const int savestate_latest_version = 0x00010000;  /* 1.0 */

#define GETARRAY(buff, type, count) \
    (to_little_endian_buffer(buff, sizeof(type),count), \
     buff += count*sizeof(type), \
     (type *)(buff-count*sizeof(type)))
#define COPYARRAY(dst, buff, type, count) \
    memcpy(dst, GETARRAY(buff, type, count), sizeof(type)*count)
#define GETDATA(buff, type) *GETARRAY(buff, type, 1)

#define PUTARRAY(src, buff, type, count) \
    memcpy(buff, src, sizeof(type)*count); \
    to_little_endian_buffer(buff, sizeof(type), count); \
    buff += count*sizeof(type);

#define PUTDATA(buff, type, value) \
    do { type x = value; PUTARRAY(&x, buff, type, 1); } while(0)

int savestates_load_m64p(const unsigned char *data, size_t size)
{
    unsigned char header[44], *curr;
    char queue[1024];
    int version;
    int i;

	(void)header;

    curr = (unsigned char*)data; // < HACK

    /* Read and check Mupen64Plus magic number. */
    if(strncmp((char *)curr, savestate_magic, 8)!=0)
    {
        return 0;
    }
    curr += 8;

    version = *curr++;
    version = (version << 8) | *curr++;
    version = (version << 8) | *curr++;
    version = (version << 8) | *curr++;
    if(version != 0x00010000)
    {
        return 0;
    }

    if(memcmp((char *)curr, ROM_SETTINGS.MD5, 32))
    {
        return 0;
    }
    curr += 32;

    // Parse savestate
    g_rdram_regs[RDRAM_CONFIG_REG] = GETDATA(curr, uint32_t);
    g_rdram_regs[RDRAM_DEVICE_ID_REG] = GETDATA(curr, uint32_t);
    g_rdram_regs[RDRAM_DELAY_REG] = GETDATA(curr, uint32_t);
    g_rdram_regs[RDRAM_MODE_REG] = GETDATA(curr, uint32_t);
    g_rdram_regs[RDRAM_REF_INTERVAL_REG] = GETDATA(curr, uint32_t);
    g_rdram_regs[RDRAM_REF_ROW_REG] = GETDATA(curr, uint32_t);
    g_rdram_regs[RDRAM_RAS_INTERVAL_REG] = GETDATA(curr, uint32_t);
    g_rdram_regs[RDRAM_MIN_INTERVAL_REG] = GETDATA(curr, uint32_t);
    g_rdram_regs[RDRAM_ADDR_SELECT_REG] = GETDATA(curr, uint32_t);
    g_rdram_regs[RDRAM_DEVICE_MANUF_REG] = GETDATA(curr, uint32_t);

    curr += 4; /* Padding from old implementation */
    g_mi_regs[MI_INIT_MODE_REG] = GETDATA(curr, uint32_t);
    curr += 4; // Duplicate MI init mode flags from old implementation
    g_mi_regs[MI_VERSION_REG] = GETDATA(curr, uint32_t);
    g_mi_regs[MI_INTR_REG] = GETDATA(curr, uint32_t);
    g_mi_regs[MI_INTR_MASK_REG] = GETDATA(curr, uint32_t);
    curr += 4; /* Padding from old implementation. */
    curr += 8; // Duplicated MI intr flags and padding from old implementation

    pi_register.pi_dram_addr_reg = GETDATA(curr, unsigned int);
    pi_register.pi_cart_addr_reg = GETDATA(curr, unsigned int);
    pi_register.pi_rd_len_reg = GETDATA(curr, unsigned int);
    pi_register.pi_wr_len_reg = GETDATA(curr, unsigned int);
    pi_register.read_pi_status_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_lat_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_pwd_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_pgs_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_rls_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_lat_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_pwd_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_pgs_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_rls_reg = GETDATA(curr, unsigned int);

    g_sp_regs[SP_MEM_ADDR_REG] = GETDATA(curr, uint32_t);
    g_sp_regs[SP_DRAM_ADDR_REG] = GETDATA(curr, uint32_t);
    g_sp_regs[SP_RD_LEN_REG] = GETDATA(curr, uint32_t);
    g_sp_regs[SP_WR_LEN_REG] = GETDATA(curr, uint32_t);
    curr += 4; /* Padding from old implementation. */
    g_sp_regs[SP_STATUS_REG] = GETDATA(curr, uint32_t);
    curr += 16; // Duplicated SP flags and padding from old implementation
    g_sp_regs[SP_DMA_FULL_REG] = GETDATA(curr, uint32_t);
    g_sp_regs[SP_DMA_BUSY_REG] = GETDATA(curr, uint32_t);
    g_sp_regs[SP_SEMAPHORE_REG] = GETDATA(curr, uint32_t);

    g_sp_regs2[SP_PC_REG] = GETDATA(curr, uint32_t);
    g_sp_regs2[SP_IBIST_REG] = GETDATA(curr, uint32_t);

    si_register.si_dram_addr = GETDATA(curr, unsigned int);
    si_register.si_pif_addr_rd64b = GETDATA(curr, unsigned int);
    si_register.si_pif_addr_wr64b = GETDATA(curr, unsigned int);
    si_register.si_stat = GETDATA(curr, unsigned int);

    g_vi_regs[VI_STATUS_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_ORIGIN_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_WIDTH_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_V_INTR_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_CURRENT_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_BURST_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_V_SYNC_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_H_SYNC_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_LEAP_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_H_START_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_V_START_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_V_BURST_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_X_SCALE_REG] = GETDATA(curr, uint32_t);
    g_vi_regs[VI_Y_SCALE_REG] = GETDATA(curr, uint32_t);
    g_vi_delay = GETDATA(curr, unsigned int);

    gfx.viStatusChanged();
    gfx.viWidthChanged();

    ri_register.ri_mode = GETDATA(curr, unsigned int);
    ri_register.ri_config = GETDATA(curr, unsigned int);
    ri_register.ri_current_load = GETDATA(curr, unsigned int);
    ri_register.ri_select = GETDATA(curr, unsigned int);
    ri_register.ri_refresh = GETDATA(curr, unsigned int);
    ri_register.ri_latency = GETDATA(curr, unsigned int);
    ri_register.ri_error = GETDATA(curr, unsigned int);
    ri_register.ri_werror = GETDATA(curr, unsigned int);

    g_ai_regs[AI_DRAM_ADDR_REG] = GETDATA(curr, uint32_t);
    g_ai_regs[AI_LEN_REG] = GETDATA(curr, uint32_t);
    g_ai_regs[AI_CONTROL_REG] = GETDATA(curr, uint32_t);
    g_ai_regs[AI_STATUS_REG] = GETDATA(curr, uint32_t);
    g_ai_regs[AI_DACRATE_REG] = GETDATA(curr, uint32_t);
    g_ai_regs[AI_BITRATE_REG] = GETDATA(curr, uint32_t);
    g_ai_fifo[1].delay        = GETDATA(curr, unsigned int);
    g_ai_fifo[1].length       = GETDATA(curr, uint32_t);
    g_ai_fifo[0].delay        = GETDATA(curr, unsigned int);
    g_ai_fifo[0].length       = GETDATA(curr, uint32_t);
    audio.aiDacrateChanged(ROM_PARAMS.systemtype);

    g_dpc_regs[DPC_START_REG] = GETDATA(curr, uint32_t);
    g_dpc_regs[DPC_END_REG]   = GETDATA(curr, uint32_t);
    g_dpc_regs[DPC_CURRENT_REG] = GETDATA(curr, uint32_t);
    curr += 4; /* Padding from old implementation. */
    g_dpc_regs[DPC_STATUS_REG] = GETDATA(curr, uint32_t);
    curr += 12; // Duplicated DPC flags and padding from old implementation
    g_dpc_regs[DPC_CLOCK_REG] = GETDATA(curr, uint32_t);
    g_dpc_regs[DPC_BUFBUSY_REG] = GETDATA(curr, uint32_t);
    g_dpc_regs[DPC_PIPEBUSY_REG] = GETDATA(curr, uint32_t);
    g_dpc_regs[DPC_TMEM_REG] = GETDATA(curr, uint32_t);

    g_dps_regs[DPS_TBIST_REG] = GETDATA(curr, uint32_t);
    g_dps_regs[DPS_TEST_MODE_REG] = GETDATA(curr, uint32_t);
    g_dps_regs[DPS_BUFTEST_ADDR_REG] = GETDATA(curr, uint32_t);
    g_dps_regs[DPS_BUFTEST_DATA_REG] = GETDATA(curr, uint32_t);

    COPYARRAY(g_rdram, curr, uint32_t, RDRAM_MAX_SIZE/4);
    COPYARRAY(g_sp_mem, curr, uint32_t, SP_MEM_SIZE/4);
    COPYARRAY(PIF_RAM, curr, unsigned char, 0x40);

    flashram_info.use_flashram = GETDATA(curr, int);
    flashram_info.mode = GETDATA(curr, int);
    flashram_info.status = GETDATA(curr, unsigned long long);
    flashram_info.erase_offset = GETDATA(curr, unsigned int);
    flashram_info.write_pointer = GETDATA(curr, unsigned int);

    COPYARRAY(tlb_LUT_r, curr, unsigned int, 0x100000);
    COPYARRAY(tlb_LUT_w, curr, unsigned int, 0x100000);

    llbit = GETDATA(curr, unsigned int);
    COPYARRAY(reg, curr, long long int, 32);
    COPYARRAY(g_cp0_regs, curr, unsigned int, 32);
    set_fpr_pointers(g_cp0_regs[CP0_STATUS_REG]);
    lo = GETDATA(curr, long long int);
    hi = GETDATA(curr, long long int);
    COPYARRAY(reg_cop1_fgr_64, curr, long long int, 32);
    if ((g_cp0_regs[CP0_STATUS_REG] & 0x04000000) == 0)  // 32-bit FPR mode requires data shuffling because 64-bit layout is always stored in savestate file
        shuffle_fpr_data(0x04000000, 0);
    FCR0 = GETDATA(curr, int);
    FCR31 = GETDATA(curr, int);

    for (i = 0; i < 32; i++)
    {
        tlb_e[i].mask = GETDATA(curr, short);
        curr += 2;
        tlb_e[i].vpn2 = GETDATA(curr, int);
        tlb_e[i].g = GETDATA(curr, char);
        tlb_e[i].asid = GETDATA(curr, unsigned char);
        curr += 2;
        tlb_e[i].pfn_even = GETDATA(curr, int);
        tlb_e[i].c_even = GETDATA(curr, char);
        tlb_e[i].d_even = GETDATA(curr, char);
        tlb_e[i].v_even = GETDATA(curr, char);
        curr++;
        tlb_e[i].pfn_odd = GETDATA(curr, int);
        tlb_e[i].c_odd = GETDATA(curr, char);
        tlb_e[i].d_odd = GETDATA(curr, char);
        tlb_e[i].v_odd = GETDATA(curr, char);
        tlb_e[i].r = GETDATA(curr, char);
   
        tlb_e[i].start_even = GETDATA(curr, unsigned int);
        tlb_e[i].end_even = GETDATA(curr, unsigned int);
        tlb_e[i].phys_even = GETDATA(curr, unsigned int);
        tlb_e[i].start_odd = GETDATA(curr, unsigned int);
        tlb_e[i].end_odd = GETDATA(curr, unsigned int);
        tlb_e[i].phys_odd = GETDATA(curr, unsigned int);
    }

#ifdef NEW_DYNAREC
    if (r4300emu == CORE_DYNAREC) {
        pcaddr = GETDATA(curr, unsigned int);
        pending_exception = 1;
        invalidate_all_pages();
    } else {
        if(r4300emu != CORE_PURE_INTERPRETER)
        {
            for (i = 0; i < 0x100000; i++)
                invalid_code[i] = 1;
        }
        generic_jump_to(GETDATA(curr, unsigned int)); // PC
    }
#else
    if(r4300emu != CORE_PURE_INTERPRETER)
    {
        for (i = 0; i < 0x100000; i++)
            invalid_code[i] = 1;
    }
    generic_jump_to(GETDATA(curr, unsigned int)); // PC
#endif

    next_interupt = GETDATA(curr, unsigned int);
    next_vi = GETDATA(curr, unsigned int);
    vi_field = GETDATA(curr, unsigned int);

    memcpy(queue, curr, sizeof(queue));
    to_little_endian_buffer(queue, 4, 256);
    load_eventqueue_infos(queue);

#ifdef NEW_DYNAREC
    if (r4300emu == CORE_DYNAREC)
        last_addr = pcaddr;
    else
#endif
       last_addr = PC->addr;

    // deliver callback to indicate completion of state loading operation
    StateChanged(M64CORE_STATE_LOADCOMPLETE, 1);

    return 1;
}

int savestates_save_m64p(unsigned char *data, size_t size)
{
    unsigned char outbuf[4], *curr;
    int i, queuelength;
    char queue[1024];

    curr = (unsigned char*)data;

    if (!curr)
       return 0;

    queuelength = save_eventqueue_infos(queue);

    // Write the save state data to memory
    PUTARRAY(savestate_magic, curr, unsigned char, 8);

    outbuf[0] = (savestate_latest_version >> 24) & 0xff;
    outbuf[1] = (savestate_latest_version >> 16) & 0xff;
    outbuf[2] = (savestate_latest_version >>  8) & 0xff;
    outbuf[3] = (savestate_latest_version >>  0) & 0xff;
    PUTARRAY(outbuf, curr, unsigned char, 4);

    PUTARRAY(ROM_SETTINGS.MD5, curr, char, 32);

    PUTDATA(curr, uint32_t, g_rdram_regs[RDRAM_CONFIG_REG]);
    PUTDATA(curr, uint32_t, g_rdram_regs[RDRAM_DEVICE_ID_REG]);
    PUTDATA(curr, uint32_t, g_rdram_regs[RDRAM_DELAY_REG]);
    PUTDATA(curr, uint32_t, g_rdram_regs[RDRAM_MODE_REG]);
    PUTDATA(curr, uint32_t, g_rdram_regs[RDRAM_REF_INTERVAL_REG]);
    PUTDATA(curr, uint32_t, g_rdram_regs[RDRAM_REF_ROW_REG]);
    PUTDATA(curr, uint32_t, g_rdram_regs[RDRAM_RAS_INTERVAL_REG]);
    PUTDATA(curr, uint32_t, g_rdram_regs[RDRAM_MIN_INTERVAL_REG]);
    PUTDATA(curr, uint32_t, g_rdram_regs[RDRAM_ADDR_SELECT_REG]);
    PUTDATA(curr, uint32_t, g_rdram_regs[RDRAM_DEVICE_MANUF_REG]);

    PUTDATA(curr, uint32_t, 0);
    PUTDATA(curr, uint32_t, g_mi_regs[MI_INIT_MODE_REG]);
    PUTDATA(curr, uint8_t, g_mi_regs[MI_INIT_MODE_REG] & 0x7F);
    PUTDATA(curr, uint8_t, (g_mi_regs[MI_INIT_MODE_REG] & 0x80) != 0);
    PUTDATA(curr, uint8_t, (g_mi_regs[MI_INIT_MODE_REG] & 0x100) != 0);
    PUTDATA(curr, uint8_t, (g_mi_regs[MI_INIT_MODE_REG] & 0x200) != 0);
    PUTDATA(curr, uint32_t, g_mi_regs[MI_VERSION_REG]);
    PUTDATA(curr, uint32_t, g_mi_regs[MI_INTR_REG]);
    PUTDATA(curr, uint32_t, g_mi_regs[MI_INTR_MASK_REG]);
    PUTDATA(curr, uint32_t, 0); /* Padding from old implementation */
    PUTDATA(curr, uint8_t, (g_mi_regs[MI_INTR_MASK_REG] & 0x1) != 0);
    PUTDATA(curr, uint8_t, (g_mi_regs[MI_INTR_MASK_REG] & 0x2) != 0);
    PUTDATA(curr, uint8_t, (g_mi_regs[MI_INTR_MASK_REG] & 0x4) != 0);
    PUTDATA(curr, uint8_t, (g_mi_regs[MI_INTR_MASK_REG] & 0x8) != 0);
    PUTDATA(curr, uint8_t, (g_mi_regs[MI_INTR_MASK_REG] & 0x10) != 0);
    PUTDATA(curr, uint8_t, (g_mi_regs[MI_INTR_MASK_REG] & 0x20) != 0);
    PUTDATA(curr, uint16_t, 0); // Padding from old implementation

    PUTDATA(curr, unsigned int, pi_register.pi_dram_addr_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_cart_addr_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_rd_len_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_wr_len_reg);
    PUTDATA(curr, unsigned int, pi_register.read_pi_status_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_lat_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_pwd_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_pgs_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_rls_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_lat_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_pwd_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_pgs_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_rls_reg);

    PUTDATA(curr, uint32_t, g_sp_regs[SP_MEM_ADDR_REG]);
    PUTDATA(curr, uint32_t, g_sp_regs[SP_DRAM_ADDR_REG]);
    PUTDATA(curr, uint32_t, g_sp_regs[SP_RD_LEN_REG]);
    PUTDATA(curr, uint32_t, g_sp_regs[SP_WR_LEN_REG]);
    PUTDATA(curr, uint32_t, 0); /* Padding from old implementation */
    PUTDATA(curr, uint32_t, g_sp_regs[SP_STATUS_REG]);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x1) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x2) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x4) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x8) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x10) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x20) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x40) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x80) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x100) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x200) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x400) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x800) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x1000) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x2000) != 0);
    PUTDATA(curr, uint8_t, (g_sp_regs[SP_STATUS_REG] & 0x4000) != 0);
    PUTDATA(curr, uint8_t, 0);
    PUTDATA(curr, uint32_t, g_sp_regs[SP_DMA_FULL_REG]);
    PUTDATA(curr, uint32_t, g_sp_regs[SP_DMA_BUSY_REG]);
    PUTDATA(curr, uint32_t, g_sp_regs[SP_SEMAPHORE_REG]);

    PUTDATA(curr, uint32_t, g_sp_regs2[SP_PC_REG]);
    PUTDATA(curr, uint32_t, g_sp_regs2[SP_IBIST_REG]);

    PUTDATA(curr, unsigned int, si_register.si_dram_addr);
    PUTDATA(curr, unsigned int, si_register.si_pif_addr_rd64b);
    PUTDATA(curr, unsigned int, si_register.si_pif_addr_wr64b);
    PUTDATA(curr, unsigned int, si_register.si_stat);

    PUTDATA(curr, uint32_t, g_vi_regs[VI_STATUS_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_ORIGIN_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_WIDTH_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_V_INTR_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_CURRENT_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_BURST_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_V_SYNC_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_H_SYNC_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_LEAP_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_H_START_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_V_START_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_V_BURST_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_X_SCALE_REG]);
    PUTDATA(curr, uint32_t, g_vi_regs[VI_Y_SCALE_REG]);
    PUTDATA(curr, unsigned int, g_vi_delay);

    PUTDATA(curr, unsigned int, ri_register.ri_mode);
    PUTDATA(curr, unsigned int, ri_register.ri_config);
    PUTDATA(curr, unsigned int, ri_register.ri_current_load);
    PUTDATA(curr, unsigned int, ri_register.ri_select);
    PUTDATA(curr, unsigned int, ri_register.ri_refresh);
    PUTDATA(curr, unsigned int, ri_register.ri_latency);
    PUTDATA(curr, unsigned int, ri_register.ri_error);
    PUTDATA(curr, unsigned int, ri_register.ri_werror);

    PUTDATA(curr, uint32_t, g_ai_regs[AI_DRAM_ADDR_REG]);
    PUTDATA(curr, uint32_t, g_ai_regs[AI_LEN_REG]);
    PUTDATA(curr, uint32_t, g_ai_regs[AI_CONTROL_REG]);
    PUTDATA(curr, uint32_t, g_ai_regs[AI_STATUS_REG]);
    PUTDATA(curr, uint32_t, g_ai_regs[AI_DACRATE_REG]);
    PUTDATA(curr, uint32_t, g_ai_regs[AI_BITRATE_REG]);
    PUTDATA(curr, unsigned int, g_ai_fifo[1].delay);
    PUTDATA(curr, uint32_t, g_ai_fifo[1].length);
    PUTDATA(curr, unsigned int, g_ai_fifo[0].delay);
    PUTDATA(curr, uint32_t, g_ai_fifo[0].length);

    PUTDATA(curr, uint32_t, g_dpc_regs[DPC_START_REG]);
    PUTDATA(curr, uint32_t, g_dpc_regs[DPC_END_REG]);
    PUTDATA(curr, uint32_t, g_dpc_regs[DPC_CURRENT_REG]);
    PUTDATA(curr, uint32_t, 0); /* Padding from oold implementation */
    PUTDATA(curr, uint32_t, g_dpc_regs[DPC_STATUS_REG]);
    PUTDATA(curr, uint8_t, (g_dpc_regs[DPC_STATUS_REG] & 0x1) != 0);
    PUTDATA(curr, uint8_t, (g_dpc_regs[DPC_STATUS_REG] & 0x2) != 0);
    PUTDATA(curr, uint8_t, (g_dpc_regs[DPC_STATUS_REG] & 0x4) != 0);
    PUTDATA(curr, uint8_t, (g_dpc_regs[DPC_STATUS_REG] & 0x8) != 0);
    PUTDATA(curr, uint8_t, (g_dpc_regs[DPC_STATUS_REG] & 0x10) != 0);
    PUTDATA(curr, uint8_t, (g_dpc_regs[DPC_STATUS_REG] & 0x20) != 0);
    PUTDATA(curr, uint8_t, (g_dpc_regs[DPC_STATUS_REG] & 0x40) != 0);
    PUTDATA(curr, uint8_t, (g_dpc_regs[DPC_STATUS_REG] & 0x80) != 0);
    PUTDATA(curr, uint8_t, (g_dpc_regs[DPC_STATUS_REG] & 0x100) != 0);
    PUTDATA(curr, uint8_t, (g_dpc_regs[DPC_STATUS_REG] & 0x200) != 0);
    PUTDATA(curr, uint8_t, (g_dpc_regs[DPC_STATUS_REG] & 0x400) != 0);
    PUTDATA(curr, uint8_t, 0);
    PUTDATA(curr, uint32_t, g_dpc_regs[DPC_CLOCK_REG]);
    PUTDATA(curr, uint32_t, g_dpc_regs[DPC_BUFBUSY_REG]);
    PUTDATA(curr, uint32_t, g_dpc_regs[DPC_PIPEBUSY_REG]);
    PUTDATA(curr, uint32_t, g_dpc_regs[DPC_TMEM_REG]);

    PUTDATA(curr, uint32_t, g_dps_regs[DPS_TBIST_REG]);
    PUTDATA(curr, uint32_t, g_dps_regs[DPS_TEST_MODE_REG]);
    PUTDATA(curr, uint32_t, g_dps_regs[DPS_BUFTEST_ADDR_REG]);
    PUTDATA(curr, uint32_t, g_dps_regs[DPS_BUFTEST_DATA_REG]);

    PUTARRAY(g_rdram, curr, uint32_t, RDRAM_MAX_SIZE/4);
    PUTARRAY(g_sp_mem, curr, uint32_t, SP_MEM_SIZE/4);
    PUTARRAY(PIF_RAM, curr, unsigned char, 0x40);

    PUTDATA(curr, int, flashram_info.use_flashram);
    PUTDATA(curr, int, flashram_info.mode);
    PUTDATA(curr, unsigned long long, flashram_info.status);
    PUTDATA(curr, unsigned int, flashram_info.erase_offset);
    PUTDATA(curr, unsigned int, flashram_info.write_pointer);

    PUTARRAY(tlb_LUT_r, curr, unsigned int, 0x100000);
    PUTARRAY(tlb_LUT_w, curr, unsigned int, 0x100000);

    PUTDATA(curr, unsigned int, llbit);
    PUTARRAY(reg, curr, long long int, 32);
    PUTARRAY(g_cp0_regs, curr, unsigned int, 32);
    PUTDATA(curr, long long int, lo);
    PUTDATA(curr, long long int, hi);

    if ((g_cp0_regs[CP0_STATUS_REG] & 0x04000000) == 0) // FR bit == 0 means 32-bit (MIPS I) FGR mode
        shuffle_fpr_data(0, 0x04000000);  // shuffle data into 64-bit register format for storage
    PUTARRAY(reg_cop1_fgr_64, curr, long long int, 32);
    if ((g_cp0_regs[CP0_STATUS_REG] & 0x04000000) == 0)
        shuffle_fpr_data(0x04000000, 0);  // put it back in 32-bit mode

    PUTDATA(curr, int, FCR0);
    PUTDATA(curr, int, FCR31);
    for (i = 0; i < 32; i++)
    {
        PUTDATA(curr, short, tlb_e[i].mask);
        PUTDATA(curr, short, 0);
        PUTDATA(curr, int, tlb_e[i].vpn2);
        PUTDATA(curr, char, tlb_e[i].g);
        PUTDATA(curr, unsigned char, tlb_e[i].asid);
        PUTDATA(curr, short, 0);
        PUTDATA(curr, int, tlb_e[i].pfn_even);
        PUTDATA(curr, char, tlb_e[i].c_even);
        PUTDATA(curr, char, tlb_e[i].d_even);
        PUTDATA(curr, char, tlb_e[i].v_even);
        PUTDATA(curr, char, 0);
        PUTDATA(curr, int, tlb_e[i].pfn_odd);
        PUTDATA(curr, char, tlb_e[i].c_odd);
        PUTDATA(curr, char, tlb_e[i].d_odd);
        PUTDATA(curr, char, tlb_e[i].v_odd);
        PUTDATA(curr, char, tlb_e[i].r);
   
        PUTDATA(curr, unsigned int, tlb_e[i].start_even);
        PUTDATA(curr, unsigned int, tlb_e[i].end_even);
        PUTDATA(curr, unsigned int, tlb_e[i].phys_even);
        PUTDATA(curr, unsigned int, tlb_e[i].start_odd);
        PUTDATA(curr, unsigned int, tlb_e[i].end_odd);
        PUTDATA(curr, unsigned int, tlb_e[i].phys_odd);
    }
#ifdef NEW_DYNAREC
    if (r4300emu == CORE_DYNAREC)
        PUTDATA(curr, unsigned int, pcaddr);
    else
#endif
    if (PC)
        PUTDATA(curr, unsigned int, PC->addr);
    else
       return 0;

    PUTDATA(curr, unsigned int, next_interupt);
    PUTDATA(curr, unsigned int, next_vi);
    PUTDATA(curr, unsigned int, vi_field);

    to_little_endian_buffer(queue, 4, queuelength/4);
    PUTARRAY(queue, curr, char, queuelength);

    // deliver callback to indicate completion of state saving operation
    StateChanged(M64CORE_STATE_SAVECOMPLETE, 1);

    return 1;
}
