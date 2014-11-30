/*
 * Copying Sound Buffer to Mix_Chunk.
 */

#include "xm7_types.h"
#include "xm7.h"
#include "sdl_cpuid.h"
#include "cache_wrapper.h"

extern struct XM7_CPUID *pCpuID;

#ifdef USE_SSE2
extern void CopySoundBuffer_SSE2(DWORD *from, WORD *to, int size);
extern int AddSoundBuffer_SSE2(Sint16 *dst, int rpos, Sint32 *opnsrc, Sint16 *beepsrc, Sint16 *cmtsrc, Sint16 *wavsrc[], int wavchannels, int samples);
#endif
#ifdef USE_MMX
extern void CopySoundBuffer_MMX(DWORD *from, WORD *to, int size);
extern int AddSoundBuffer_MMX(Sint16 *dst, int rpos, Sint32 *opnsrc, Sint16 *beepsrc, Sint16 *cmtsrc, Sint16 *wavsrc[], int wavchannels, int samples);
#endif



static inline Sint16 _clamp(Sint32 b)
{
    if(b < -0x7fff) return -0x7fff;
    if(b > 0x7fff) return 0x7fff;
    return (Sint16) b;
}

int AddSoundBuffer(Sint16 *dst, int rpos, Sint32 *opnsrc, Sint16 *beepsrc, Sint16 *cmtsrc, Sint16 *wavsrc[], int wavchannels, int samples)
{
   int len1, len2;
   int i;
   Sint16 *wavp[16];

   if(samples <= 0) return 0;
   if(dst == NULL) return 0;
   if((opnsrc == NULL) || (beepsrc == NULL) || (cmtsrc == NULL)) return 0;
   if(wavchannels >= 16) wavchannels = 16;
   for(i = 0; i < wavchannels; i++) {
      wavp[i] = wavsrc[i];
      wavp[i] = &(wavp[i][rpos]);
   }

   
 if(pCpuID != NULL) {
 #if defined(USE_SSE2)
    if(pCpuID->use_sse2) {
	   int ii;
	   BOOL flag = TRUE;
	   //if(((uint64_t)opnsrc  % 16) == 0) flag = FALSE;
	   //if(((uint64_t)beepsrc % 16) == 0) flag = FALSE;
	   //if(((uint64_t)cmtsrc % 16) == 0) flag = FALSE;
	   //if(((uint64_t)dst % 16) == 0) flag = FALSE;
	   //for(ii = 0; ii < wavchannels; ii++) {
	   //	if((uint64_t)wavsrc[ii] % 16 != 0) flag = FALSE;
	   //}
	   if(flag == TRUE) return AddSoundBuffer_SSE2(dst, rpos, opnsrc, beepsrc, cmtsrc, wavsrc, wavchannels, samples);
     }
 #endif
 #if defined(USE_MMX)
   if(pCpuID->use_mmx) {
	   int ii;
	   BOOL flag = TRUE;
	   if(((uint64_t)opnsrc  % 8) == 0) flag = FALSE;
	   if(((uint64_t)beepsrc % 8) == 0) flag = FALSE;
	   if(((uint64_t)cmtsrc % 8) == 0) flag = FALSE;
	   if(((uint64_t)dst % 8) == 0) flag = FALSE;
	   for(ii = 0; ii < wavchannels; ii++) {
		if((uint64_t)wavsrc[ii] % 8 != 0) flag = FALSE;
	   }
	   if(flag == TRUE) return AddSoundBuffer_MMX(dst, rpos, opnsrc, beepsrc, cmtsrc, wavsrc, wavchannels, samples);
   }
 #endif
 }
   
   len1 = 0;
   len2 = samples;
   {
      Sint32 tmp4;
      Sint16 tmp5;
      Sint32 *opn2 = (Sint32 *)opnsrc;
      Sint16 *beep2 = (Sint16 *)beepsrc;
      Sint16 *cmt2 = (Sint16 *)cmtsrc;
      Sint16 *dst2 = (Sint16 *)dst;
      Sint16 *wav2;
      int j;
      _prefetch_data_write_l1(dst2, sizeof(Sint16) * samples);
      _prefetch_data_read_l2(opn2, sizeof(Sint32) * samples);
      _prefetch_data_read_l2(beep2, sizeof(Sint16) * samples);
      _prefetch_data_read_l2(cmt2, sizeof(Sint16) * samples);
      for(j = 0; j < wavchannels; j++) {
	 if(wavsrc[j] != NULL ) {
	    wav2 = wavp[j];
	    _prefetch_data_read_l2(wav2, sizeof(Sint16) * samples);
	 }
      }
      
      for (i = 0; i < len2; i++) {
	 tmp4 = *opn2++;
	 tmp5 = _clamp(tmp4);
	 tmp5 = tmp5 + *beep2++;
	 tmp5 = tmp5 + *cmt2++;
	 for(j = 0; j < wavchannels; j++) {
	    wav2 = wavp[j];
	    if(wav2 != NULL) tmp5 = tmp5 + wav2[i];
	 }
	 *dst2++ = tmp5;
      }
   }
   return len2;
}


void CopySoundBufferGeneric(DWORD * from, WORD * to, int size)
{
    int         i,j;
    Sint32 tmp1;
    Sint32 *p = (Sint32 *)from;
    Sint16 *t = (Sint16 *)to;
    struct XM7_CPUID cpuid;

#if (__GNUC__ >= 4)
    v8hi_t tmp2;
    v4hi tmp3;
    v4hi *l;
    v8hi_t *h;
    if (p == NULL) {
        return;
    }
    if (t == NULL) {
        return;
    }
 if(pCpuID != NULL) {
  #if defined(USE_SSE2)
    if(pCpuID->use_sse2) {
	CopySoundBuffer_SSE2(from, to, size);
        return;
    }
 #endif
 #if defined(USE_MMX)
    if(pCpuID->use_mmx) {
	CopySoundBuffer_MMX(from, to, size);
        return;
    }
 #endif
 }
   
 // Not using MMX or SSE2
    h = (v8hi_t *)p;
    l = (v4hi *)t;
    i = (size >> 3) << 3;
    _prefetch_data_write_l1(l, sizeof(Uint16) * size);
    _prefetch_data_read_l2(h, sizeof(Uint32) * size);
    for (j = 0; j < i; j += 8) {
        tmp2 = *h++;
        tmp3.ss[0] =_clamp(tmp2.si[0]);
        tmp3.ss[1] =_clamp(tmp2.si[1]);
        tmp3.ss[2] =_clamp(tmp2.si[2]);
        tmp3.ss[3] =_clamp(tmp2.si[3]);
        tmp3.ss[4] =_clamp(tmp2.si[4]);
        tmp3.ss[5] =_clamp(tmp2.si[5]);
        tmp3.ss[6] =_clamp(tmp2.si[6]);
        tmp3.ss[7] =_clamp(tmp2.si[7]);
        *l++ = tmp3;
   }
   p = (Sint32 *)h;
   t = (Sint16 *)l;
   if(i >= size) return;
   for (j = 0; j < (size - i); j++) {
      tmp1 = *p++;
      *t++ = _clamp(tmp1);
   }
#else // GCC <= 3.x
//   p = (Sint32 *)h;
//   t = (Sint16 *)l;
   i = size >> 3;
   for (j = 0; j < i; j++) {
      tmp1 = *p++;
      *t++ = _clamp(tmp1);
      tmp1 = *p++;
      *t++ = _clamp(tmp1);
      tmp1 = *p++;
      *t++ = _clamp(tmp1);
      tmp1 = *p++;
      *t++ = _clamp(tmp1);
      tmp1 = *p++;
      *t++ = _clamp(tmp1);
      tmp1 = *p++;
      *t++ = _clamp(tmp1);
      tmp1 = *p++;
      *t++ = _clamp(tmp1);
      tmp1 = *p++;
      *t++ = _clamp(tmp1);
   }
   i = j << 3;
   if(i >= size) return;
   for (j = 0; j < (size - i); j++) {
      tmp1 = *p++;
      *t++ = _clamp(tmp1);
   }
#endif   
}

