/*
 * api_vram4096.cpp
 * Convert VRAM -> VirtualVram
 * (C) 2011 K.Ohta <whatisthis.sowhat@gmail.com>
 */

#if 0
#include <SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include "api_draw.h"
#include "api_scaler.h"
#include "agar_xm7.h"
#include "agar_draw.h"
#include "agar_gldraw.h"
#include "xm7.h"
#include "display.h"
#include "subctrl.h"
#include "device.h"
#endif

#include <SDL.h>
//#include <GL/gl.h>
//#include <GL/glext.h>
#include "api_draw.h"
#include "api_scaler.h"
#include "api_vram.h"

void CalcPalette_4096Colors(Uint32 index, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
 {
    Uint32 ds;
//     if((index > 4095) || (index < 0)) return;
#ifdef SDL_LIL_ENDIAN
	ds =r + (g << 8)+ (b << 16) + (a<<24);
#else
	ds = r<<24 + g<<16 + b<<8 + 255<<0;
#endif
    rgbAnalogGDI[index] = ds;
}



static inline void putword(Uint32 *disp, Uint32 *cbuf)
{
		disp[0] = cbuf[0] & 0xfff;
		disp[1] = cbuf[1] & 0xfff;
		disp[2] = cbuf[2] & 0xfff;
		disp[3] = cbuf[3] & 0xfff;
		disp[4] = cbuf[4] & 0xfff;
		disp[5] = cbuf[5] & 0xfff;
		disp[6] = cbuf[6] & 0xfff;
		disp[7] = cbuf[7] & 0xfff;
}

static inline void putword2(Uint32 *disp, Uint32 *cbuf)
{
		disp[0] = rgbAnalogGDI[cbuf[0]];
		disp[1] = rgbAnalogGDI[cbuf[1]];
		disp[2] = rgbAnalogGDI[cbuf[2]];
		disp[3] = rgbAnalogGDI[cbuf[3]];
		disp[4] = rgbAnalogGDI[cbuf[4]];
		disp[5] = rgbAnalogGDI[cbuf[5]];
		disp[6] = rgbAnalogGDI[cbuf[6]];
		disp[7] = rgbAnalogGDI[cbuf[7]];
}


static void getvram_4096(Uint32 addr, Uint32 *cbuf)
{
        Uint8            b[4],
                        r[4],
                        g[4];
//        Uint32 			dat[8];

        /*
         * R,G,Bについて8bit単位で描画する。
         * 高速化…キャッシュヒット率の向上を考慮して、
         * インライン展開と細かいループの廃止を同時に行う
         */

        g[3] = vram_pg[addr + 0x00000];
        g[2] = vram_pg[addr + 0x02000];
        g[1] = vram_pg[addr + 0x04000];
        g[0] = vram_pg[addr + 0x06000];

        r[3] = vram_pr[addr + 0x00000];
        r[2] = vram_pr[addr + 0x02000];
        r[1] = vram_pr[addr + 0x04000];
        r[0] = vram_pr[addr + 0x06000];

        b[3] = vram_pb[addr + 0x00000];
        b[2] = vram_pb[addr + 0x02000];
        b[1] = vram_pb[addr + 0x04000];
        b[0] = vram_pb[addr + 0x06000];


        /*
         * bit7
         */
        cbuf[7] =
    	((b[0] & 0x01)) + ((b[1] & 0x01) << 1) + ((b[2] & 0x01) << 2) +
    	((b[3] & 0x01) << 3)
    	+ ((r[0] & 0x01) << 4) + ((r[1] & 0x01) << 5) +
    	((r[2] & 0x01) << 6) + ((r[3] & 0x01) << 7)
    	+ ((g[0] & 0x01) << 8) + ((g[1] & 0x01) << 9) +
    	((g[2] & 0x01) << 10) + ((g[3] & 0x01) << 11);

        /*
         * bit6
         */
        cbuf[6] =
    	((b[0] & 0x02) >> 1) + ((b[1] & 0x02)) + ((b[2] & 0x02) << 1) +
    	((b[3] & 0x02) << 2)
    	+ ((r[0] & 0x02) << 3) + ((r[1] & 0x02) << 4) +
    	((r[2] & 0x02) << 5) + ((r[3] & 0x02) << 6)
    	+ ((g[0] & 0x02) << 7) + ((g[1] & 0x02) << 8) +
    	((g[2] & 0x02) << 9) + ((g[3] & 0x02) << 10);

        /*
         * bit5
         */
        cbuf[5] =
    	((b[0] & 0x04) >> 2) + ((b[1] & 0x04) >> 1) + ((b[2] & 0x04)) +
    	((b[3] & 0x04) << 1)
    	+ ((r[0] & 0x04) << 2) + ((r[1] & 0x04) << 3) +
    	((r[2] & 0x04) << 4) + ((r[3] & 0x04) << 5)
    	+ ((g[0] & 0x04) << 6) + ((g[1] & 0x04) << 7) +
    	((g[2] & 0x04) << 8) + ((g[3] & 0x04) << 9);

        /*
         * bit4
         */
        cbuf[4] =
    	((b[0] & 0x08) >> 3) + ((b[1] & 0x08) >> 2) +
    	((b[2] & 0x08) >> 1) + ((b[3] & 0x08))
    	+ ((r[0] & 0x08) << 1) + ((r[1] & 0x08) << 2) +
    	((r[2] & 0x08) << 3) + ((r[3] & 0x08) << 4)
    	+ ((g[0] & 0x08) << 5) + ((g[1] & 0x08) << 6) +
    	((g[2] & 0x08) << 7) + ((g[3] & 0x08) << 8);

        /*
         * bit3
         */
        cbuf[3] =
    	((b[0] & 0x10) >> 4) + ((b[1] & 0x10) >> 3) +
    	((b[2] & 0x10) >> 2) + ((b[3] & 0x10) >> 1)
    	+ ((r[0] & 0x10)) + ((r[1] & 0x10) << 1) + ((r[2] & 0x10) << 2) +
    	((r[3] & 0x10) << 3)
    	+ ((g[0] & 0x10) << 4) + ((g[1] & 0x10) << 5) +
    	((g[2] & 0x10) << 6) + ((g[3] & 0x10) << 7);

        /*
         * bit2
         */
        cbuf[2] =
    	((b[0] & 0x20) >> 5) + ((b[1] & 0x20) >> 4) +
    	((b[2] & 0x20) >> 3) + ((b[3] & 0x20) >> 2)
    	+ ((r[0] & 0x20) >> 1) + ((r[1] & 0x20)) + ((r[2] & 0x20) << 1) +
    	((r[3] & 0x20) << 2)
    	+ ((g[0] & 0x20) << 3) + ((g[1] & 0x20) << 4) +
    	((g[2] & 0x20) << 5) + ((g[3] & 0x20) << 6);

        /*
         * bit1
         */
        cbuf[1] =
    	((b[0] & 0x40) >> 6) + ((b[1] & 0x40) >> 5) +
    	((b[2] & 0x40) >> 4) + ((b[3] & 0x40) >> 3)
    	+ ((r[0] & 0x40) >> 2) + ((r[1] & 0x40) >> 1) + ((r[2] & 0x40)) +
    	((r[3] & 0x40) << 1)
    	+ ((g[0] & 0x40) << 2) + ((g[1] & 0x40) << 3) +
    	((g[2] & 0x40) << 4) + ((g[3] & 0x40) << 5);

        /*
         * bit0
         */
        cbuf[0] =
    	((b[0] & 0x80) >> 7) + ((b[1] & 0x80) >> 6) +
    	((b[2] & 0x80) >> 5) + ((b[3] & 0x80) >> 4)
    	+ ((r[0] & 0x80) >> 3) + ((r[1] & 0x80) >> 2) +
    	((r[2] & 0x80) >> 1) + ((r[3] & 0x80))
    	+ ((g[0] & 0x80) << 1) + ((g[1] & 0x80) << 2) +
    	((g[2] & 0x80) << 3) + ((g[3] & 0x80) << 4);
}



void CreateVirtualVram4096(Uint32 *p, int x, int y, int w, int h, int mode, Uint32 mpage)
{
	int ww, hh;
	int xx, yy;
	Uint32 addr;
	Uint32 *disp;
	Uint32 c[8];

    if(p == NULL) return;
	LockVram();
	ww = (w>>3) + (x>>3);
	hh = h + y;
	for(yy = y; yy < hh; yy++) {
		for(xx = x>>3 ; xx < ww; xx++) {
			addr = yy  * 40 + xx ;
			getvram_4096(addr, c);
			disp = &p[xx * 8 + 320 * yy];
			putword2(disp,  c);
			addr++;
			}
	}
   UnlockVram();
   return;
}

/*
 * 8x8のピースをVRAMから作成する：VramLockしない事に注意
 */
void CreateVirtualVram4096_1Pcs(Uint32 *p, int x, int y, int pitch, int mode)
{
    Uint32 c[8];
    Uint8 *disp = (Uint8 *)p;
    Uint32 addr;

    addr = y * 40 + x;
    // Loop廃止(高速化)

    getvram_4096(addr, c);
    putword2((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword2((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword2((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword2((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword2((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword2((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword2((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword2((Uint32 *)disp,  c);

}

/*
 * 8x8のピースをVRAMから作成する(パレットなし)：VramLockしない事に注意
 */
void CreateVirtualVram4096_1Pcs_Nopal(Uint32 *p, int x, int y, int pitch, int mode)
{
    Uint32 c[8];
    Uint8 *disp = (Uint8 *)p;
    Uint32 addr;

    addr = y * 40 + x;
    // Loop廃止(高速化)

    getvram_4096(addr, c);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_4096(addr, c);
    putword((Uint32 *)disp,  c);

}
