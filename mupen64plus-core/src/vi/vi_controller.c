/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - vi_controller.c                                         *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2014 Bobby Smiles                                       *
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

#include "vi_controller.h"

#include "main/main.h"
#include "memory/memory.h"
#include "plugin/plugin.h"
#include "r4300/r4300_core.h"
#include "r4300/interupt.h"

#include <string.h>

extern unsigned alternate_vi_timing;

void connect_vi(struct vi_controller* vi,
                struct r4300_core* r4300)
{
    vi->r4300 = r4300;
}

/* Initializes the VI. */
void init_vi(struct vi_controller* vi)
{
    memset(vi->regs, 0, VI_REGS_COUNT*sizeof(uint32_t));

    vi->field = 0;
    vi->delay = vi->next_vi = 5000;
}

/* Reads a word from the VI MMIO register space. */
int read_vi_regs(void* opaque, uint32_t address, uint32_t *word)
{
    struct vi_controller* vi = (struct vi_controller*)opaque;
    uint32_t             reg = VI_REG(address);
    const uint32_t* cp0_regs = r4300_cp0_regs();

    if (reg == VI_CURRENT_REG)
    {
        cp0_update_count();
        if (alternate_vi_timing)
           vi->regs[VI_CURRENT_REG] = (vi->delay - (vi->next_vi - cp0_regs[CP0_COUNT_REG])) % 0x20E;
        else
           vi->regs[VI_CURRENT_REG] = (vi->delay - (vi->next_vi - cp0_regs[CP0_COUNT_REG])) / 1500;
        vi->regs[VI_CURRENT_REG] = (vi->regs[VI_CURRENT_REG] & (~1)) | vi->field;
    }

    *word = vi->regs[reg];

    return 0;
}

/* Writes a word to the VI MMIO register space. */
int write_vi_regs(void* opaque, uint32_t address,
      uint32_t word, uint32_t mask)
{
    struct vi_controller* vi = (struct vi_controller*)opaque;
    uint32_t reg             = VI_REG(address);

    switch (reg)
    {
       case VI_STATUS_REG:
          if ((vi->regs[VI_STATUS_REG] & mask) != (word & mask))
          {
             vi->regs[VI_STATUS_REG] = MASKED_WRITE(&vi->regs[VI_STATUS_REG], word, mask);
             gfx.viStatusChanged();
          }
          return 0;

       case VI_WIDTH_REG:
          if ((vi->regs[VI_WIDTH_REG] & mask) != (word & mask))
          {
             vi->regs[VI_WIDTH_REG] = MASKED_WRITE(&vi->regs[VI_WIDTH_REG], word, mask);
             gfx.viWidthChanged();
          }
          return 0;

       case VI_CURRENT_REG:
          clear_rcp_interrupt(vi->r4300, MI_INTR_VI);
          return 0;
    }

    vi->regs[reg] = MASKED_WRITE(&vi->regs[reg], word, mask);

    return 0;
}

void vi_vertical_interrupt_event(struct vi_controller* vi)
{
   gfx.updateScreen();

   /* allow main module to do things on VI event */
   new_vi();

   /* toggle vi field if in interlaced mode */
   vi->field ^= (vi->regs[VI_STATUS_REG] >> 6) & 0x1;

   /* schedule next vertical interrupt */
   if (vi->regs[VI_V_SYNC_REG] == 0)
      vi->delay = 500000;
   else
      vi->delay = (vi->regs[VI_V_SYNC_REG] + 1) * VI_REFRESH;

   vi->next_vi += vi->delay;

   add_interupt_event_count(VI_INT, vi->next_vi);

   /* trigger interrupt */
   raise_rcp_interrupt(vi->r4300, MI_INTR_VI);
}
