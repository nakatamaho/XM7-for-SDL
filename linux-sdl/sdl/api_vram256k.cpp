/*
 * api_vram256k.cpp
 * Convert VRAM -> VirtualVram
 * (C) 2011 K.Ohta <whatisthis.sowhat@gmail.com>
 */


#include <SDL.h>
#include "api_draw.h"
//#include "api_scaler.h"
#include "api_vram.h"


static inline void putword(Uint32 *disp, Uint32 *cbuf)
{
   
		disp[0] = cbuf[7];
		disp[1] = cbuf[6];
		disp[2] = cbuf[5];
		disp[3] = cbuf[4];
		disp[4] = cbuf[3];
		disp[5] = cbuf[2];
		disp[6] = cbuf[1];
		disp[7] = cbuf[0];
}



extern "C" 
{
extern Uint32 lshift_5bit1v(v4hi *v);
extern v8hi lshift_6bit8v(v4hi *v);
}

static v8hi gpixel2cbuf(Uint32 addr, Uint32 mpage)
{
   Uint8 ret = 0;
   v4hi v;
   v8hi v1;
   v8hi *p;
   Uint8 *vram_p = vram_pb;
   
    v.i[0] = v.i[1] = v.i[2] = v.i[3] = 0;
    if(!(mpage & 0x40)){
        v.b[5] = vram_p[addr + 0x10000]; 
        v.b[4] = vram_p[addr + 0x12000]; 
        v.b[3] = vram_p[addr + 0x14000]; 
        v.b[2] = vram_p[addr + 0x16000]; 
        v.b[1] = vram_p[addr + 0x28000]; 
        v.b[0] = vram_p[addr + 0x2a000]; 
        v1 = lshift_6bit8v(&v);
        v1.i[0] <<= 8;
        v1.i[1] <<= 8;
        v1.i[2] <<= 8;
        v1.i[3] <<= 8;
        v1.i[4] <<= 8;
        v1.i[5] <<= 8;
        v1.i[6] <<= 8;
        v1.i[7] <<= 8;
        return v1;
    
    } else {
       v8hi r;
       r.v = (v8si){0, 0, 0, 0, 0, 0, 0, 0};
       return r;
   }
   

}

static v8hi rpixel2cbuf(Uint32 addr, Uint32 mpage)
{
   Uint8 ret = 0;
   v4hi v;
   v8hi *p;
   v8hi v1;
   Uint8 *vram_p = vram_pb;
   
    v.i[0] = v.i[1] = v.i[2] = v.i[3] = 0;
    if(!(mpage & 0x20)){
        v.b[5] = vram_p[addr + 0x08000]; 
        v.b[4] = vram_p[addr + 0x0a000]; 
        v.b[3] = vram_p[addr + 0x0c000]; 
        v.b[2] = vram_p[addr + 0x0e000]; 
        v.b[1] = vram_p[addr + 0x20000]; 
        v.b[0] = vram_p[addr + 0x22000]; 
        v1 = lshift_6bit8v(&v);
        return v1;
   } else {
       v8hi r;
       r.v = (v8si){0, 0, 0, 0, 0, 0, 0, 0};
       return r;
   }
}

static v8hi bpixel2cbuf(Uint32 addr, Uint32 mpage)
{
   Uint8 ret = 0;
   v4hi v;
   v8hi *p;
   v8hi v1;
   Uint8 *vram_p = vram_pb;
   
    v.i[0] = v.i[1] = v.i[2] = v.i[3] = 0;
    if(!(mpage & 0x10)){
        v.b[5] = vram_p[addr + 0x00000]; 
        v.b[4] = vram_p[addr + 0x02000]; 
        v.b[3] = vram_p[addr + 0x04000]; 
        v.b[2] = vram_p[addr + 0x06000]; 
        v.b[1] = vram_p[addr + 0x18000]; 
        v.b[0] = vram_p[addr + 0x1a000]; 
        

        v1 = lshift_6bit8v(&v);
        v1.i[0] <<= 16;
        v1.i[1] <<= 16;
        v1.i[2] <<= 16;
        v1.i[3] <<= 16;
        v1.i[4] <<= 16;
        v1.i[5] <<= 16;
        v1.i[6] <<= 16;
        v1.i[7] <<= 16;
        return v1;
   } else {
       v8hi r;
       r.v = (v8si){0, 0, 0, 0, 0, 0, 0, 0};
       return r;
   }
}




static void getvram_256k(Uint32 addr, Uint32 *cbuf, Uint32 mpage)
{
   v8hi *p;
   v8hi r, g, b;
   /*
     * R,G,Bについて8bit単位で描画する。
     * 高速化…キャッシュヒット率の向上を考慮して、
     * インライン展開と細かいループの廃止を同時に行う
     */
   p = (v8hi *)cbuf;

   
   b = bpixel2cbuf(addr, mpage);
   r = rpixel2cbuf(addr, mpage);
   g = gpixel2cbuf(addr, mpage);
   p->v = r.v + g.v + b.v;
}


void CreateVirtualVram256k(Uint32 *p, int x, int y, int w, int h, int mode, Uint32 mpage)
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
			getvram_256k(addr, c, mpage);
			disp = &p[xx * 8 + 320 * yy];
			putword(disp,  c);
			addr++;
			}
	}
   UnlockVram();
   return;
}

/*
 * 8x8のピースをVRAMから作成する：VramLockしない事に注意
 */
void CreateVirtualVram256k_1Pcs(Uint32 *p, int x, int y, int pitch, int mpage)
{
    Uint32 c[8];
    Uint8 *disp = (Uint8 *)p;
    Uint32 addr;

    addr = y * 40 + x;
    // Loop廃止(高速化)

    getvram_256k(addr, c, mpage);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_256k(addr, c, mpage);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_256k(addr, c, mpage);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_256k(addr, c, mpage);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_256k(addr, c, mpage);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_256k(addr, c, mpage);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_256k(addr, c, mpage);
    putword((Uint32 *)disp,  c);
    addr += 40;
    disp += pitch;

    getvram_256k(addr, c, mpage);
    putword((Uint32 *)disp,  c);

}

