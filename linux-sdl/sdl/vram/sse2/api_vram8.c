﻿/*
 * api_vram8.cpp
 * Convert VRAM -> VirtualVram
 * (C) 2011 K.Ohta <whatisthis.sowhat@gmail.com>
 */


#include <SDL.h>
#include "api_draw.h"
#include "api_vram.h"
#include "sdl_cpuid.h"



#if (__GNUC__ >= 4)
static void getvram_8_vec(Uint32 addr, v8hi_t *cbuf)
{
    uint8_t r, g, b;
//    volatile v4hi cbuf __attribute__((aligned(32)));
        /*
         * R,G,Bについて8bit単位で描画する。
         * 高速化…キャッシュヒット率の向上とVector演算(MMXetc)の速度効果を考慮して、
         * ループの廃止を同時に行う
         */

    g = vram_pg[addr];
    r = vram_pr[addr];
    b = vram_pb[addr];

    cbuf->v = aPlanes[B0 + b] |
              aPlanes[B1 + r] |
              aPlanes[B2 + g];
   return;
}

static inline void  putword8_vec(Uint32 *disp, volatile v8hi_t c, Uint32 *pal)
{

   v8si *dst = (v8si *)disp;
   volatile v8hi_t r1;
   
//   if(disp == NULL) return;
//   c.v &= (v8si){7, 7, 7, 7, 7, 7, 7, 7,};
   r1.i[0] = rgbTTLGDI[c.i[0] & 7]; // ?!
   r1.i[1] = rgbTTLGDI[c.i[1] & 7];
   r1.i[2] = rgbTTLGDI[c.i[2] & 7];
   r1.i[3] = rgbTTLGDI[c.i[3] & 7];
   r1.i[4] = rgbTTLGDI[c.i[4] & 7];
   r1.i[5] = rgbTTLGDI[c.i[5] & 7];
   r1.i[6] = rgbTTLGDI[c.i[6] & 7];
   r1.i[7] = rgbTTLGDI[c.i[7] & 7];
   *dst = r1.v;
}

#else
static inline void planeto8(Uint32 *c, uint8_t r, unit8_t g, uint8_t b)
{
   Uint8 mask;
   
   mask = 0x80;
   c[0] = ((r & mask) >> 6) | ((g & mask) >> 5) || ((b & mask) >> 7);
   mask >>= 1;
   c[1] = ((r & mask) >> 5) | ((g & mask) >> 4) || ((b & mask) >> 6);
   mask >>= 1;
   c[2] = ((r & mask) >> 4) | ((g & mask) >> 3) || ((b & mask) >> 5);
   mask >>= 1;
   c[3] = ((r & mask) >> 3) | ((g & mask) >> 2) || ((b & mask) >> 4);
   mask >>= 1;
   c[4] = ((r & mask) >> 2) | ((g & mask) >> 1) || ((b & mask) >> 3);
   mask >>= 1;
   c[5] = ((r & mask) >> 1) | (g & mask) || ((b & mask) >> 2);
   mask >>= 1;
   c[6] = (r & mask) | ((g & mask) << 1) || ((b & mask) >> 1);
   mask >>= 1;
   c[7] = ((r & mask) << 1) | ((g & mask) << 2) || (b & mask);
   mask >>= 1;
}

static void getvram_8(Uint32 addr, Uint32 *cbuf)
{
    uint8_t r, g, b;
        /*
         * R,G,Bについて8bit単位で描画する。
         * 高速化…キャッシュヒット率の向上とVector演算(MMXetc)の速度効果を考慮して、
         * ループの廃止を同時に行う
         */
   
   g = vram_pg[addr];
   r = vram_pr[addr];
   b = vram_pb[addr];
   planeto8(cbuf, r, g, b);
  
   return;
}

static inline void  putword8(Uint32 *disp, Uint32 *c, Uint32 *pal)
{

   Uint32 *r1 = disp;

   r1[0] = pal[c[0] & 7]; // ?!
   r1[1] = pal[c[1] & 7];
   r1[2] = pal[c[2] & 7];
   r1[3] = pal[c[3] & 7];
   r1[4] = pal[c[4] & 7];
   r1[5] = pal[c[5] & 7];
   r1[6] = pal[c[6] & 7];
   r1[7] = pal[c[7] & 7];
}

#endif // __GNUC__ >= 4



/*
 * 8x8のピースをVRAMから作成する：VramLockしない事に注意
 */
void CreateVirtualVram8_1Pcs_SSE2(Uint32 *p, int x, int y, int pitch, int mode)
{
#if (__GNUC__ >= 4)   
    v8hi_t c;
    Uint32 *pal = (Uint32 *)rgbTTLGDI;
    Uint8 *disp =(Uint8 *) p;
    Uint32 addr;

    if((p == NULL) || (pal == NULL)) return;
    pitch = sizeof(Uint32) * 8;
    addr = y * 80 + x;

    // Loop廃止(高速化)
    if(aPlanes == NULL) {
       c.v = (v8si){0,0,0,0,0,0,0,0};
       putword8_vec((Uint32 *)disp,  c, pal);
       disp += pitch;
       putword8_vec((Uint32 *)disp,  c, pal);
       disp += pitch;
       putword8_vec((Uint32 *)disp,  c, pal);
       disp += pitch;
       putword8_vec((Uint32 *)disp,  c, pal);
       disp += pitch;
       putword8_vec((Uint32 *)disp,  c, pal);
       disp += pitch;
       putword8_vec((Uint32 *)disp,  c, pal);
       disp += pitch;
       putword8_vec((Uint32 *)disp,  c, pal);
       disp += pitch;
       putword8_vec((Uint32 *)disp,  c, pal);
//       disp += pitch;
       return;
     } else {
       getvram_8_vec(addr, &c);
       putword8_vec((Uint32 *)disp, c, pal);
       addr += 80;
       disp += pitch;

       getvram_8_vec(addr , &c);
       putword8_vec((Uint32 *)disp,  c, pal);
       addr += 80;
       disp += pitch;

       getvram_8_vec(addr, &c);
       putword8_vec((Uint32 *)disp,  c, pal);
       addr += 80;
       disp += pitch;

       getvram_8_vec(addr , &c);
       putword8_vec((Uint32 *)disp,  c, pal);
       addr += 80;
       disp += pitch;

       getvram_8_vec(addr, &c);
       putword8_vec((Uint32 *)disp,  c, pal);
       addr += 80;
       disp += pitch;

       getvram_8_vec(addr, &c);
       putword8_vec((Uint32 *)disp,  c, pal);
       addr += 80;
       disp += pitch;

       getvram_8_vec(addr, &c);
       putword8_vec((Uint32 *)disp,  c, pal);
       addr += 80;
       disp += pitch;

       getvram_8_vec(addr, &c);
       putword8_vec((Uint32 *)disp,  c, pal);
//    addr += 80;
//    disp += pitch;
     }
#else 
    Uint32 c[8];
    Uint32 *pal = (Uint32 *)rgbTTLGDI;
    Uint8 *disp =(Uint8 *) p;

    if((p == NULL) || (pal == NULL)) return;
    pitch = sizeof(Uint32) * 8;
    addr = y * 80 + x;

    // Loop廃止(高速化)
   getvram_8(addr, c);
   putword8((Uint32 *)disp, c, pal);
   addr += 80;
   disp += pitch;
   
   getvram_8(addr , c);
   putword8((Uint32 *)disp,  c, pal);
   addr += 80;
   disp += pitch;
   
   getvram_8(addr, c);
   putword8((Uint32 *)disp,  c, pal);
   addr += 80;
   disp += pitch;
   
   getvram_8(addr , c);
   putword8((Uint32 *)disp,  c, pal);
   addr += 80;
   disp += pitch;
   
   getvram_8(addr, c);
   putword8((Uint32 *)disp,  c, pal);
   addr += 80;
   disp += pitch;
   
   getvram_8(addr, c);
   putword8((Uint32 *)disp,  c, pal);
   addr += 80;
   disp += pitch;
   
   getvram_8(addr, c);
   putword8((Uint32 *)disp,  c, pal);
   addr += 80;
   disp += pitch;
   
   getvram_8(addr, c);
   putword8((Uint32 *)disp,  c, pal);
   //    addr += 80;
   //    disp += pitch;
     
#endif   
}

