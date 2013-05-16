/***************************************************************************
 *   Copyright (C) 2013 by James Holodnak                                  *
 *   jamesholodnak@gmail.com                                               *
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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "misc/log.h"
#include "nes/nes.h"
#include "system/video.h"

readfunc_t ppu_memread;
writefunc_t ppu_memwrite;

static u8 read_ppu_memory(u32 addr)
{
	if(nes.ppu.readpages[addr >> 10])
		return(nes.ppu.readpages[addr >> 10][addr & 0x3FF]);
	else if(nes.ppu.readfuncs[addr >> 10])
		return(nes.ppu.readfuncs[addr >> 10](addr));
	else if(log_unhandled_io)
		log_printf("ppu_memread: read from unmapped memory at $%04X\n",addr);
	return(0);
}

static void write_ppu_memory(u32 addr,u8 data)
{
	u8 page = (addr >> 10) & 0xF;

	//check if mapped to memory pointer
	if(nes.ppu.writepages[page]) {
		nes.ppu.writepages[page][addr & 0x3FF] = data;

		//if address is in chr ram, update the cache
		if(addr < 0x2000) {
		/*	if((addr & 0xF) == 0xF)*/ {
				cache_t *cache = nes.ppu.cachepages[page];
				cache_t *cache_hflip = nes.ppu.cachepages_hflip[page];
				u8 *chr = nes.ppu.readpages[page];
				u32 a = addr & 0x3F0;

				cache_tile(chr + a,cache + (a / 8));
				cache_tile_hflip(chr + a,cache_hflip + (a / 8));
			}
		}
	}

	//see if write function is mapped
	else if(nes.ppu.writefuncs[page])
		nes.ppu.writefuncs[page](addr,data);

	//not mapped, report error
	else if(log_unhandled_io)
		log_printf("ppu_memwrite: write to unmapped memory at $%04X = $%02X\n",addr,data);
}

readfunc_t ppu_getreadfunc()
{
	return(ppu_memread);
}

writefunc_t ppu_getwritefunc()
{
	return(ppu_memwrite);
}

void ppu_setreadfunc(readfunc_t readfunc)
{
	ppu_memread = (readfunc == 0) ? read_ppu_memory : readfunc;
}

void ppu_setwritefunc(writefunc_t writefunc)
{
	ppu_memwrite = (writefunc == 0) ? write_ppu_memory : writefunc;
}

u8 ppu_pal_read(u32 addr)
{
	return(nes.ppu.palette[addr]);
}

void ppu_pal_write(u32 addr,u8 data)
{
	nes.ppu.palette[addr] = data;
	video_updatepalette(addr,data);
}

u8 ppu_read(u32 addr)
{
	u8 ret = 0;

	switch(addr & 7) {
		case 2:
			//bottom 5 bits come from the $2007 buffer
			ret = (nes.ppu.status & 0xE0) | (nes.ppu.buf & 0x1F);

			//clear vblank flag
			if(ret & 0x80) {
//				log_printf("ppu_read:  frame %d, scanline %d, cycle %d:  clearing VBLANK flag\n",FRAMES,SCANLINE,LINECYCLES);
				nes.ppu.status &= 0x60;
			}

			//nmi suppression
			if(SCANLINE == 241) {
				if(LINECYCLES == 1) {
					ret &= 0x7F;
					cpu_clear_nmi();
				}
				if(LINECYCLES < 4 && LINECYCLES > 1) {
					cpu_clear_nmi();
				}
			}

//			log_printf("ppu_read:  read $2002 @ cycle %d (line %d, frame %d)\n",LINECYCLES,SCANLINE,FRAMES);
			TOGGLE = 0;
			nes.ppu.buf = ret;
//			if(SCANLINE >= 260)
//				log_printf("ppu_read:  read $2002 @ cycle %d (line %d, frame %d)\n",LINECYCLES,SCANLINE,FRAMES);
			break;
		case 4:
			nes.ppu.buf = nes.ppu.oam[nes.ppu.oamaddr];
			break;
		case 7:
//			log_message("2007 read: ppuscroll = $%04X, scanline = %d\n",nes->ppu.scroll,nes->scanline);
			nes.ppu.buf = nes.ppu.latch;
			SCROLL &= 0x7FFF;
			nes.ppu.latch = ppu_memread(SCROLL);
			if((SCROLL & 0x3F00) == 0x3F00)
				nes.ppu.buf = ppu_pal_read(SCROLL & 0x1F);
			if(CONTROL0 & 4)
				SCROLL += 32;
			else
				SCROLL += 1;
			break;
	}
	return(nes.ppu.buf);
}

void ppu_write(u32 addr,u8 data)
{
	int i;

	nes.ppu.buf = data;
	switch(addr & 7) {
		case 0:
			if((STATUS & 0x80) && (data & 0x80) && ((CONTROL0 & 0x80) == 0))
				cpu_set_nmi();
			if(((data & 0x80) == 0) && (SCANLINE == 241) && (LINECYCLES < 4))
				cpu_clear_nmi();
			CONTROL0 = data;
			TMPSCROLL = (TMPSCROLL & 0x73FF) | ((data & 3) << 10);
//			log_printf("control0:  $%02X (nmi %s) (pixel %d, line %d, frame %d)\n",CONTROL0,(CONTROL0&0x80)?"on":"off",LINECYCLES,SCANLINE,FRAMES);
			return;
		case 1:
			CONTROL1 = data;
			return;
		case 3:
			nes.ppu.oamaddr = data;
			return;
		case 4:
			nes.ppu.oam[nes.ppu.oamaddr++] = data;
			return;
		case 5:				//scroll
//			log_printf("ppu_write:  write to $%04X (tog=%d) @ cycle %d, line %d, frame %d\n",addr,TOGGLE,LINECYCLES,SCANLINE,FRAMES);
			if(TOGGLE == 0) { //first write
				TMPSCROLL = (TMPSCROLL & ~0x001F) | (data >> 3);
				SCROLLX = data & 7;
				TOGGLE = 1;
			}
			else { //second write
				TMPSCROLL &= ~0x73E0;
				TMPSCROLL |= ((data & 0xF8) << 2) | ((data & 7) << 12);
				TOGGLE = 0;
			}
			return;
		case 6:				//vram addr
//			log_printf("ppu_write:  write to $%04X (tog=%d) @ cycle %d, line %d, frame %d\n",addr,TOGGLE,LINECYCLES,SCANLINE,FRAMES);
			if(TOGGLE == 0) { //first write
				TMPSCROLL = (TMPSCROLL & ~0xFF00) | ((data & 0x7F) << 8);
				TOGGLE = 1;
			}
			else { //second write
				SCROLL = TMPSCROLL = (TMPSCROLL & ~0x00FF) | data;
				TOGGLE = 0;
				//kludge
				nes.ppu.busaddr = SCROLL;
			}
			return;
		case 7:				//vram data
			if(SCROLL < 0x3F00) {
				ppu_memwrite(SCROLL,data);
			}
			else {
				if((SCROLL & 0x0F) == 0) {
					for(i=0;i<8;i++)
						ppu_pal_write(i * 4,data);
				}
				else if(SCROLL & 3)
					ppu_pal_write(SCROLL & 0x1F,data);
			}
			if(CONTROL0 & 4)
				SCROLL += 32;
			else
				SCROLL += 1;
			SCROLL &= 0x7FFF;
			return;
	}
}
