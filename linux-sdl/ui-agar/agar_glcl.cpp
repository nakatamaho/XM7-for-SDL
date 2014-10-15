/*
 * Renderer using OPENCL/GL
 * (C) 2012 K.Ohta <whatisthis.sowhat@gmail.com>
 * History:
 * Nov 01,2012: Initial
 */


#include <agar/core.h>
#include <agar/core/types.h>
#include <agar/gui.h>


#include "api_draw.h"
#include "api_kbd.h"

#include "agar_xm7.h"
#include "agar_draw.h"
#include "agar_gldraw.h"
#include "agar_glutil.h"
#include "agar_logger.h"
#include "xm7.h"
#include "display.h"
#include "subctrl.h"
#include "device.h"

#include "agar_glcl.h"

#define LOGSIZE 1024*1024

extern "C"{
extern Uint8 *vram_pb;
extern Uint8 *vram_pr;
extern Uint8 *vram_pg;
}

extern PFNGLBINDBUFFERPROC glBindBuffer;



GLCLDraw::GLCLDraw()
{
   cl_int ret;
   pixelBuffer = NULL;
   bModeOld = -1;
   kernel = NULL;
}

GLCLDraw::~GLCLDraw()
{
   cl_int ret;
    if(kernel != NULL) ret = clReleaseKernel(kernel);
    if(program != NULL) ret |= clReleaseProgram(program);
    if(command_queue != NULL) ret |= clReleaseCommandQueue(command_queue);
    if(context != NULL) ret |= clReleaseContext(context);
    if(properties != NULL) free(properties);
    if(inbuf != NULL) ret |= clReleaseMemObject(inbuf);
    if(outbuf != NULL) ret |= clReleaseMemObject(outbuf);
    if(palette != NULL) ret |= clReleaseMemObject(palette);
    if(table != NULL) ret |= clReleaseMemObject(table);
    if(pixelBuffer != NULL) free(pixelBuffer);
}

static void cl_notify_log(const char *errinfo, const void *private_info, size_t cb, void *user_data)
{
   Uint8 dump[128];
   char dumpStr[1024];
   int i;
   
   dumpStr[0] = '\0';
   XM7_DebugLog(XM7_LOG_WARN, "CL Notify: %s", errinfo);
}

int GLCLDraw::GetGLEnabled(void)
{
  if(bCLEnableKhrGLShare != 0) return -1;
  return 0;
}

Uint32 *GLCLDraw::GetPixelBuffer(void)
{
  return pixelBuffer;
}

cl_int GLCLDraw::InitContext(void)
{
   cl_int ret;
   size_t len;
   char extension_data[256];
   size_t extension_len;
   int i;
   
   properties = malloc(16 * sizeof(intptr_t));
   ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
   if(ret != CL_SUCCESS) return ret;
   
   ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 8, device_id,
                            &ret_num_devices);
   
   if(ret != CL_SUCCESS) return ret;

   for(i = 0; i < ret_num_devices; i++ ){
     clGetDeviceInfo(device_id[i], CL_DEVICE_EXTENSIONS,
		   255, extension_data, &extension_len);
     XM7_DebugLog(XM7_LOG_DEBUG, "CL Extension features(%d):%s", i, extension_data);
     if(strcasestr(extension_data, "cl_khr_gl_sharing") != NULL) {
       bCLEnableKhrGLShare = -1;
     } else {
       bCLEnableKhrGLShare = 0;
     }
   }

   if(bCLEnableKhrGLShare != 0) { // This is only under X11. Must fix.
     XM7_DebugLog(XM7_LOG_DEBUG, "GL Interoperability enabled.");
     properties[0] = CL_GL_CONTEXT_KHR;
     properties[1] = (cl_context_properties)glXGetCurrentContext();
     XM7_DebugLog(XM7_LOG_DEBUG, "GL Context = %08x", glXGetCurrentContext());
     properties[2] = CL_GLX_DISPLAY_KHR;
     properties[3] = (cl_context_properties)glXGetCurrentDisplay();
     XM7_DebugLog(XM7_LOG_DEBUG, "GL Display = %08x", glXGetCurrentDisplay());
     properties[4] = CL_CONTEXT_PLATFORM;
     properties[5] = (cl_context_properties)platform_id;
     properties[6] = 0;
   } else {
     XM7_DebugLog(XM7_LOG_DEBUG, "GL Interoperability disabled.");
     properties[0] = CL_CONTEXT_PLATFORM;
     properties[1] = (cl_context_properties)platform_id;
     properties[2] = 0;
   }
//   if(device_id == NULL) return -1;
   
   context = clCreateContext(properties, 1, device_id, cl_notify_log, NULL, &ret);
   if(ret != CL_SUCCESS) return ret;
       
   command_queue = clCreateCommandQueue(context, device_id[0],
                                         CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &ret);
//   command_queue = clCreateCommandQueue(context, device_id[0],
//                                         0, &ret);
   XM7_DebugLog(XM7_LOG_DEBUG, "CL: Command queue created.");
   return ret;
}


cl_int GLCLDraw::BuildFromSource(const char *p)
{
    cl_int ret;
    size_t codeSize;
    char *logBuf;
   
    codeSize = strlen(p);
    program = clCreateProgramWithSource(context, 1, (const char **)&p,
                                        (const size_t *)&codeSize, &ret);
    XM7_DebugLog(XM7_LOG_INFO, "CL: Build Result=%d", ret);

    // Compile from source
    ret = clBuildProgram(program, 1, device_id, NULL, NULL, NULL);
    XM7_DebugLog(XM7_LOG_INFO, "Compile Result=%d", ret);
    if(ret != CL_SUCCESS) {  // Printout error log.
       logBuf = (char *)malloc(LOGSIZE * sizeof(char));
       memset(logBuf, 0x00, LOGSIZE * sizeof(char));
       
       ret = clGetProgramBuildInfo(program, device_id[0], CL_PROGRAM_BUILD_LOG, 
				   LOGSIZE - 1, (void *)logBuf, NULL);
       if(logBuf != NULL) printf("Build Log:\n%s", logBuf);
       free(logBuf);
       return ret;
    }
   return ret;
}

cl_int GLCLDraw::copysub(int xbegin, int ybegin, int drawwidth, int drawheight, int w, int h, int mpage)
{
   cl_int ret = 0;
   Uint8 *pr, *pg, *pb;
   Uint8 *p;
   
   int xb = xbegin / 8;
   int yb = ybegin;
   int ww = w / 8;
   int x;
   int y;
   int offset = (drawwidth / 8) * ybegin;
   int voffset = 0x4000;
      
   if(drawheight > 200) voffset = 0x8000;
   
   
   pg = (Uint8 *)vram_pg;
   pr = (Uint8 *)vram_pr;
   pb = (Uint8 *)vram_pb;
   pb = &pb[offset + xb];
   pr = &pr[offset + xb];
   pg = &pg[offset + xb];
   
   if((pb == NULL) || (pg == NULL) || (pr == NULL)) return -1;
   if(drawwidth ==  w) {
      int band = (drawwidth  / 8) * h;
      p = clEnqueueMapBuffer(command_queue, inbuf, CL_TRUE, CL_MAP_WRITE,
			     offset, voffset * 3,
			     0, NULL, &event_uploadvram[0], &ret);
      if(p != NULL) {
	   memcpy(p, pb, band);  
	   memcpy(&p[voffset], pr, band);  
	   memcpy(&p[voffset * 2], pg, band);
	   ret |= clEnqueueUnmapMemObject(command_queue, inbuf,
					  p, 0, NULL, &event_uploadvram[1]);
      }
      
   } else {
      int dwb = drawwidth / 8;
      int yy;
      int yoffset = 0;

      p = clEnqueueMapBuffer(command_queue, inbuf, CL_TRUE, CL_MAP_WRITE,
			     offset + xb, voffset * 3,
			     0, NULL, &event_uploadvram[0], &ret);
    if(p != NULL) {
	   for(yy = 0; yy < h; yy++) {
	      memcpy(&p[yoffset], &pb[yoffset], ww);  
	      memcpy(&p[voffset + yoffset], &pr[yoffset], ww);  
	      memcpy(&p[voffset * 2 + yoffset], &pg[yoffset], ww);
	      yoffset += dwb;
	   }
	 
	   ret |= clEnqueueUnmapMemObject(command_queue, inbuf,
					  p, 0, NULL, &event_uploadvram[1]);
      }
//      printf("Window: %d x %d -> %d,%d inbuf=%08x STS=%d\n",ww, h, xb, ybegin, inbuf, ret);
   }
  
   return ret;
}

cl_int GLCLDraw::copy4096sub(int xbegin, int ybegin, int drawwidth, int drawheight, int w, int h, int mpage)
{
   cl_int ret = 0;
   Uint8 *pr, *pg, *pb;
   Uint8 *p;
   int xb = xbegin / 8;
   int yb = ybegin;
   int ww = w / 8;
   int x;
   int y;
   int offset = (drawwidth / 8) * ybegin + xb;
   int vramoffset = 0x2000;   
   
   pg = (Uint8 *)vram_pg;
   pr = (Uint8 *)vram_pr;
   pb = (Uint8 *)vram_pb;
   pb = &pb[offset];
   pr = &pr[offset];
   pg = &pg[offset];
   
   if((pb == NULL) || (pg == NULL) || (pr == NULL)) return -1;
   if(drawwidth ==  w) {
      int band = (drawwidth  / 8) * h;
      p = clEnqueueMapBuffer(command_queue, inbuf, CL_TRUE, CL_MAP_WRITE,
			     offset, vramoffset * 12,
			     0, NULL, &event_uploadvram[0], &ret);
      if(p != NULL) {
	 // B
	   memcpy(&p[0], &pb[0], band);  
	   memcpy(&p[vramoffset], &pb[vramoffset], band);  
	   memcpy(&p[vramoffset * 2], &pb[vramoffset * 2], band);  
	   memcpy(&p[vramoffset * 3], &pb[vramoffset * 3], band);
	 // R
	   memcpy(&p[vramoffset * 4], &pr[0], band);  
	   memcpy(&p[vramoffset * 5], &pr[vramoffset], band);  
	   memcpy(&p[vramoffset * 6], &pr[vramoffset * 2], band);  
	   memcpy(&p[vramoffset * 7], &pr[vramoffset * 3], band);  
	 
	 // G
	   memcpy(&p[vramoffset * 8], &pg[0], band);  
	   memcpy(&p[vramoffset * 9], &pg[vramoffset], band);  
	   memcpy(&p[vramoffset * 10], &pg[vramoffset * 2], band);  
	   memcpy(&p[vramoffset * 11], &pg[vramoffset * 3], band);  
	   ret |= clEnqueueUnmapMemObject(command_queue, inbuf,
					  p, 0, NULL, &event_uploadvram[1]);
      }

   } else {
      int dwb = drawwidth / 8;
      int yy;
      int yoffset = 0;
      int band = ww;
      p = clEnqueueMapBuffer(command_queue, inbuf, CL_TRUE, CL_MAP_WRITE,
			     offset, vramoffset * 12,
			     0, NULL, &event_uploadvram[0], &ret);
      if(p != NULL) {
	 // B
	 for(yy = 0; yy < h; yy++) {
	   memcpy(&p[yoffset], &pb[yoffset], band);  
	   memcpy(&p[vramoffset     + yoffset], &pb[vramoffset + yoffset], band);  
	   memcpy(&p[vramoffset * 2 + yoffset], &pb[vramoffset * 2 + yoffset], band);  
	   memcpy(&p[vramoffset * 3 + yoffset], &pb[vramoffset * 3 + yoffset], band);
	 // R
	   memcpy(&p[vramoffset * 4 + yoffset], &pr[yoffset], band);  
	   memcpy(&p[vramoffset * 5 + yoffset], &pr[vramoffset + yoffset], band);  
	   memcpy(&p[vramoffset * 6 + yoffset], &pr[vramoffset * 2 + yoffset], band);  
	   memcpy(&p[vramoffset * 7 + yoffset], &pr[vramoffset * 3 + yoffset], band);  
	 
	 // G
	   memcpy(&p[vramoffset * 8 + yoffset], &pg[yoffset], band);  
	   memcpy(&p[vramoffset * 9 + yoffset], &pg[vramoffset + yoffset], band);  
	   memcpy(&p[vramoffset * 10 + yoffset], &pg[vramoffset * 2 + yoffset], band);  
	   memcpy(&p[vramoffset * 11 + yoffset], &pg[vramoffset * 3 + yoffset], band);
	   yoffset += dwb;
	 }
	 
	 ret |= clEnqueueUnmapMemObject(command_queue, inbuf,
					  p, 0, NULL, &event_uploadvram[1]);
      }


//      printf("Window: %d x %d -> %d,%d inbuf=%08x STS=%d\n",ww, h, xb, ybegin, inbuf, ret);
   }
  
   return ret;
}

cl_int GLCLDraw::copy256ksub(int xbegin, int ybegin, int drawwidth, int drawheight, int w, int h, int mpage)
{
   cl_int ret = 0;
   Uint8 *pr, *pg, *pb;
   Uint8 *p;
   int xb = xbegin / 8;
   int yb = ybegin;
   int ww = w / 8;
   int x;
   int y;
   int offset = (drawwidth / 8) * ybegin;
   int band = (drawwidth  / 8) * h;
   int vramoffset = 0x2000;   
   
   pg = (Uint8 *)vram_pg;
   pr = (Uint8 *)vram_pr;
   pb = (Uint8 *)vram_pb;
   
   if((pb == NULL) || (pg == NULL) || (pr == NULL)) return -1;
   pb = &pb[offset];
   pr = &pr[offset];
   pg = &pg[offset];
      
   p = clEnqueueMapBuffer(command_queue, inbuf, CL_TRUE, CL_MAP_WRITE,
			  offset, vramoffset * 18,
			  0, NULL, &event_uploadvram[0], &ret);
  if(p != NULL) {
     // B
     memcpy(&p[0], &pb[0], band);  
     memcpy(&p[vramoffset], &pb[vramoffset], band);  
     memcpy(&p[vramoffset * 2], &pb[vramoffset * 2], band);  
     memcpy(&p[vramoffset * 3], &pb[vramoffset * 3], band);
     memcpy(&p[vramoffset * 4], &pb[vramoffset * 4], band);  
     memcpy(&p[vramoffset * 5], &pb[vramoffset * 5], band);
     // R
     memcpy(&p[vramoffset * 6], &pr[0], band);  
     memcpy(&p[vramoffset * 7], &pr[vramoffset], band);  
     memcpy(&p[vramoffset * 8], &pr[vramoffset * 2], band);  
     memcpy(&p[vramoffset * 9], &pr[vramoffset * 3], band);  
     memcpy(&p[vramoffset * 10], &pr[vramoffset * 4], band);  
     memcpy(&p[vramoffset * 11], &pr[vramoffset * 5], band);  
	 
	 // G
     memcpy(&p[vramoffset * 12], &pg[0], band);  
     memcpy(&p[vramoffset * 13], &pg[vramoffset], band);  
     memcpy(&p[vramoffset * 14], &pg[vramoffset * 2], band);  
     memcpy(&p[vramoffset * 15], &pg[vramoffset * 3], band);  
     memcpy(&p[vramoffset * 16], &pg[vramoffset * 4], band);  
     memcpy(&p[vramoffset * 17], &pg[vramoffset * 5], band);
	 
     ret |= clEnqueueUnmapMemObject(command_queue, inbuf,
				    p, 0, NULL, &event_uploadvram[1]);
  }
  return ret;
}


cl_int GLCLDraw::window_copy8(void)
{
   WORD wdtop, wdbtm;
   cl_int ret = 0;
   
   if(bPaletFlag) { // 描画モードでVRAM変更
      Palet640();
      bPaletFlag = FALSE;
      nDrawTop = 0;
      nDrawBottom = 400;
      nDrawLeft = 0;
      nDrawRight = 640;
//      SetDrawFlag(TRUE);
   }
   if (bClearFlag) {
//      AllClear();
   }
   
   if((nDrawTop < nDrawBottom) && (nDrawLeft < nDrawRight)) {
      if(window_open) { // ハードウェアウインドウ開いてる
	 if ((nDrawTop >> 1) < window_dy1) {
	    //			vramhdr->SetVram(vram_dptr, 80, 200);
	    SetVram_200l(vram_dptr);
	    ret |= copysub(0, nDrawTop >> 1, 640, 200, 640, window_dy1, multi_page);
	 }
	 /* ウィンドウ内の描画 */
	 if ((nDrawTop >> 1) > window_dy1) {
	    wdtop = nDrawTop >> 1;
	 }
	 else {
	    wdtop = window_dy1;
	 }
	 
	 if ((nDrawBottom >> 1)< window_dy2) {
	    wdbtm = nDrawBottom >> 1;
	 }
	 else {
	    wdbtm = window_dy2;
	 }
	 
	 if (wdbtm > wdtop) {
	    //		vramhdr->SetVram(vram_bdptr, 80, 200);
	    SetVram_200l(vram_bdptr);
	    ret |= copysub(window_dx1, wdtop, 640, 200, window_dx2 - window_dx1 , wdbtm - wdtop , multi_page);
	 }
	 /* ハードウェアウインドウ外下部 */
	 if ((nDrawBottom >> 1) > window_dy2) {
	    //	vramhdr->SetVram(vram_dptr, 80, 200);
	    SetVram_200l(vram_dptr);
			        
	    ret |= copysub(0 , wdbtm, 640, 200, 640, (nDrawBottom >> 1) - wdbtm, multi_page);
	 }
      } else { // ハードウェアウィンドウ開いてない
	 //	vramhdr->SetVram(vram_dptr, 80, 200);
	 SetVram_200l(vram_dptr);
	 ret |= copysub(0, 0, 640, 200, 640, 200, multi_page);
      }
   }
   return ret;
}

cl_int GLCLDraw::window_copy8_400l(void)
{
   WORD wdtop, wdbtm;
   cl_int ret = 0;
   
   if (bClearFlag) {
      AllClear();
   }
   if(bPaletFlag) { // 描画モードでVRAM変更
      Palet640();
      bPaletFlag = FALSE;
      nDrawTop = 0;
      nDrawBottom = 400;
      nDrawLeft = 0;
      nDrawRight = 640;
//      SetDrawFlag(TRUE);
   }
//   nDrawTop = 0;
//   nDrawBottom = 400;
//   nDrawLeft = 0;
//   nDrawRight = 640;
   if((nDrawTop < nDrawBottom) && (nDrawLeft < nDrawRight)) {
      if(window_open) { // ハードウェアウインドウ開いてる
	 if (nDrawTop  < window_dy1) {
	    //			vramhdr->SetVram(vram_dptr, 80, 200);
	    SetVram_200l(vram_dptr);
	    ret |= copysub(0, nDrawTop, 640, 400, 640, window_dy1 - nDrawTop, multi_page);
	 }
	 /* ウィンドウ内の描画 */
	 if (nDrawTop > window_dy1) {
	    wdtop = nDrawTop;
	 }
	 else {
	    wdtop = window_dy1;
	 }
	 
	 if (nDrawBottom < window_dy2) {
	    wdbtm = nDrawBottom;
	 }
	 else {
	    wdbtm = window_dy2;
	 }
	 
	 if (wdbtm > wdtop) {
	    //		vramhdr->SetVram(vram_bdptr, 80, 200);
	    SetVram_200l(vram_bdptr);
	    ret |= copysub(window_dx1, wdtop, 640, 400, window_dx2 - window_dx1 , wdbtm - wdtop , multi_page);
	 }
	 /* ハードウェアウインドウ外下部 */
	 if (nDrawBottom > window_dy2) {
	    //	vramhdr->SetVram(vram_dptr, 80, 200);
	    SetVram_200l(vram_dptr);
	    ret |= copysub(0 , wdbtm, 640, 400, 640, nDrawBottom - wdbtm, multi_page);
	 }
      } else { // ハードウェアウィンドウ開いてない
	 //	vramhdr->SetVram(vram_dptr, 80, 200);
	 SetVram_200l(vram_dptr);
	 ret |= copysub(0, 0, 640, 400, 640, 400, multi_page);
      }
   }
   nDrawTop = 0;
   nDrawBottom = 400;
   nDrawLeft = 0;
   nDrawRight = 640;
   return ret;
}

cl_int GLCLDraw::window_copy4096(void)
{
   WORD wdtop, wdbtm;
   cl_int ret = 0;
   
   if(bPaletFlag) { // 描画モードでVRAM変更
      Palet320();
      bPaletFlag = FALSE;
      nDrawTop = 0;
      nDrawBottom = 200;
      nDrawLeft = 0;
      nDrawRight = 320;
//      SetDrawFlag(TRUE);
   }
//   if (bClearFlag) {
//      AllClear();
//   }
   
   if((nDrawTop < nDrawBottom) && (nDrawLeft < nDrawRight)) {
      if(window_open) { // ハードウェアウインドウ開いてる
	 if (nDrawTop < window_dy1) {
	    //			vramhdr->SetVram(vram_dptr, 80, 200);
	    SetVram_200l(vram_dptr);
	    ret |= copy4096sub(0, nDrawTop , 320, 200, 320, window_dy1, multi_page);
	 }
	 /* ウィンドウ内の描画 */
	 if (nDrawTop > window_dy1) {
	    wdtop = nDrawTop;
	 }
	 else {
	    wdtop = window_dy1;
	 }
	 
	 if (nDrawBottom < window_dy2) {
	    wdbtm = nDrawBottom;
	 }
	 else {
	    wdbtm = window_dy2;
	 }
	 
	 if (wdbtm > wdtop) {
	    //		vramhdr->SetVram(vram_bdptr, 80, 200);
	    SetVram_200l(vram_bdptr);
	    ret |= copy4096sub(window_dx1, wdtop, 320, 200, window_dx2 - window_dx1 , wdbtm - wdtop , multi_page);
	 }
	 /* ハードウェアウインドウ外下部 */
	 if (nDrawBottom  > window_dy2) {
	    //	vramhdr->SetVram(vram_dptr, 80, 200);
	    SetVram_200l(vram_dptr);
			        
	    ret |= copy4096sub(0 , wdbtm, 320, 200, 320, nDrawBottom - wdbtm, multi_page);
	 }
      } else { // ハードウェアウィンドウ開いてない
	 //	vramhdr->SetVram(vram_dptr, 80, 200);
	 SetVram_200l(vram_dptr);
	 ret |= copy4096sub(0, 0, 320, 200, 320, 200, multi_page);
      }
   }
   nDrawTop = 0;
   nDrawBottom = 200;
   nDrawLeft = 0;
   nDrawRight = 320;
   return ret;
}


cl_int GLCLDraw::GetVram(int bmode)
{
   cl_int ret = 0;
   cl_int r;
   int w;
   int h;
   Uint8 *pr,*pg,*pb;
   size_t gws[] = {nCLGlobalWorkThreads}; // Parallel jobs.
   size_t lws[] = {1}; // local jobs.
   size_t *goff = NULL;
   int mpage = multi_page;
   int dummy = 0;

   
   if(inbuf == NULL) return -1;
   if(outbuf == NULL) return -1;

   if((bModeOld != -1) && (bmode != bModeOld)) {
     clReleaseKernel(kernel);
     kernel = NULL;
   }
   
   switch(bmode) {
    case SCR_400LINE:
      w = 640;
      h = 400;
      gws[0] = h;
      if(kernel == NULL) kernel = clCreateKernel(program, "getvram8", &ret);
      ret |= clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&inbuf);
      ret |= clSetKernelArg(kernel, 1, sizeof(int),    (void *)&w);
      ret |= clSetKernelArg(kernel, 2, sizeof(int), (void *)&h);
      ret |= clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&outbuf);
      ret |= clSetKernelArg(kernel, 4, sizeof(cl_mem), (void *)&palette);
      ret |= clSetKernelArg(kernel, 5, sizeof(cl_mem), (void *)&table);
      ret |= clSetKernelArg(kernel, 6, sizeof(int), (void *)&bCLSparse);
      ret |= clSetKernelArg(kernel, 7, sizeof(int), (void *)&dummy);
      ret |= window_copy8_400l();
      ret |= clEnqueueWriteBuffer(command_queue, palette, CL_TRUE, 0,
                              8 * sizeof(Uint32), (void *)&rgbTTLGDI[0]
                              , 0, NULL, &event_uploadvram[3]);
      break;
    case SCR_200LINE:
      w = 640;
      h = 200;
      gws[0] = h;
      if(kernel == NULL) kernel = clCreateKernel(program, "getvram8", &ret);
      ret |= clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&inbuf);
      ret |= clSetKernelArg(kernel, 1, sizeof(int),    (void *)&w);
      ret |= clSetKernelArg(kernel, 2, sizeof(int), (void *)&h);
      ret |= clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&outbuf);
      ret |= clSetKernelArg(kernel, 4, sizeof(cl_mem), (void *)&palette);
      ret |= clSetKernelArg(kernel, 5, sizeof(cl_mem), (void *)&table);
      ret |= clSetKernelArg(kernel, 6, sizeof(int), (void *)&bCLSparse);
      ret |= clSetKernelArg(kernel, 7, sizeof(int), (void *)&dummy);
      ret |= window_copy8();
      ret |= clEnqueueWriteBuffer(command_queue, palette, CL_TRUE, 0,
                              8 * sizeof(Uint32), (void *)&rgbTTLGDI[0]
                              , 0, NULL, &event_uploadvram[2]);
      break;
    case SCR_262144:// Windowはなし
      w = 320;
      h = 200;
      gws[0] = h;
      if(kernel == NULL) kernel = clCreateKernel(program, "getvram256k", &ret);
      ret |= clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&inbuf);
      ret |= clSetKernelArg(kernel, 1, sizeof(int),    (void *)&w);
      ret |= clSetKernelArg(kernel, 2, sizeof(int), (void *)&h);
      ret |= clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&outbuf);
      ret |= clSetKernelArg(kernel, 4, sizeof(cl_mem), (void *)&palette);
      ret |= clSetKernelArg(kernel, 5, sizeof(cl_mem), (void *)&table);
      ret |= clSetKernelArg(kernel, 6, sizeof(int), (void *)&mpage);
      ret |= clSetKernelArg(kernel, 7, sizeof(int), (void *)&bCLSparse);
      ret |= copy256ksub(0, 0, 320, 200, 320, 200, mpage);
      break;
    case SCR_4096:
      w = 320;
      h = 200;
      gws[0] = h;
      if(kernel == NULL) kernel = clCreateKernel(program, "getvram4096", &ret);
      ret |= clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&inbuf);
      ret |= clSetKernelArg(kernel, 1, sizeof(int),    (void *)&w);
      ret |= clSetKernelArg(kernel, 2, sizeof(int), (void *)&h);
      ret |= clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&outbuf);
      ret |= clSetKernelArg(kernel, 4, sizeof(cl_mem), (void *)&palette);
      ret |= clSetKernelArg(kernel, 5, sizeof(cl_mem), (void *)&table);
      ret |= clSetKernelArg(kernel, 6, sizeof(int), (void *)&bCLSparse);
      ret |= clSetKernelArg(kernel, 7, sizeof(int), (void *)&dummy);
      ret |= window_copy4096();
      ret |= clEnqueueWriteBuffer(command_queue, palette, CL_FALSE, 0,
                              4096 * sizeof(Uint32), (void *)&rgbAnalogGDI[0]
                              , 0, NULL, &event_uploadvram[3]);
      break;
   }
   glFinish();
   bModeOld = bMode;

   if(GetGLEnabled() != 0) {
     int need_alpha = 1;
     ret |= clSetKernelArg(kernel, 8, sizeof(int), (void *)&need_alpha);
     ret |= clEnqueueAcquireGLObjects (command_queue,
				  1, (cl_mem *)&outbuf,
				  3, event_uploadvram, &event_copytotexture);
     if(bCLSparse) {
       ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, 
				     goff, gws, lws, 
				     1, &event_copytotexture,  &event_exec);
     } else {
       ret = clEnqueueTask(command_queue,
			    kernel, 1, &event_copytotexture, &event_exec);
     }
   } else {
     int need_alpha = 0;
     ret |= clSetKernelArg(kernel, 8, sizeof(int), (void *)&need_alpha);
     clFinish(command_queue);
     if(bCLSparse) {
       ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, 
				     goff, gws, lws, 
				     0, NULL,  &event_exec);
     } else {
       ret = clEnqueueTask(command_queue,
			    kernel, 0, NULL, &event_exec);
     }
   }
   //XM7_DebugLog(XM7_LOG_DEBUG, "CL: execute kernel: %d", ret);
   
   
   //   clFinish(command_queue);

   if(GetGLEnabled() != 0) {
     ret |= clEnqueueReleaseGLObjects (command_queue,
				  1, (cl_mem *)&outbuf,
				  1, &event_exec, &event_release);
   } else {
//      pixelBuffer = clEnqueueMapBuffer(command_queue, outbuf, CL_TRUE, CL_MAP_READ,
//				       0, (size_t)(640 * 400 * sizeof(Uint32)),
//				       1, &event_exec, &event_release, &ret);

      /* Mapping Buffer occures error :( */
      ret |= clEnqueueReadBuffer(command_queue, outbuf, CL_TRUE, 0,
                              w * h * sizeof(Uint32), (void *)pixelBuffer
                              , 1, &event_exec, &event_release);
      //      XM7_DebugLog(XM7_LOG_DEBUG, "CL: read from result : %d", ret);
   }

   clFinish(command_queue);
//   glFinish();
   return ret;
				   
}


cl_int GLCLDraw::SetupTable(void)
{
   cl_int r = 0;
   unsigned int i;
   unsigned int j;
   cl_uint *tbl;
   cl_uint v[8];
  
   tbl = (cl_uint *)malloc(0x100 * 8 * 20 * sizeof(cl_uint));
    for(j = 0; j < 256; j++) {
       v[0] = (j & 0x80) >> 7;
       v[1] = (j & 0x40) >> 6;
       v[2] = (j & 0x20) >> 5;
       v[3] = (j & 0x10) >> 4;
       v[4] = (j & 0x08) >> 3;
       v[5] = (j & 0x04) >> 2;
       v[6] = (j & 0x02) >> 1;
       v[7] = (j & 0x01);
       for(i = 0; i < 20 ; i++) {
	  tbl[i * 0x800 + j * 8 + 0] = v[0]; 
	  tbl[i * 0x800 + j * 8 + 1] = v[1]; 
	  tbl[i * 0x800 + j * 8 + 2] = v[2]; 
	  tbl[i * 0x800 + j * 8 + 3] = v[3]; 
	  tbl[i * 0x800 + j * 8 + 4] = v[4]; 
	  tbl[i * 0x800 + j * 8 + 5] = v[5]; 
	  tbl[i * 0x800 + j * 8 + 6] = v[6]; 
	  tbl[i * 0x800 + j * 8 + 7] = v[7];
	  v[0] = v[0] << 1;
	  v[1] = v[1] << 1;
	  v[2] = v[2] << 1;
	  v[3] = v[3] << 1;
	  v[4] = v[4] << 1;
	  v[5] = v[5] << 1;
	  v[6] = v[6] << 1;
	  v[7] = v[7] << 1;
       }
    }
    if(table != NULL) {
      cl_event tbl_ev;
      r |= clEnqueueWriteBuffer(command_queue, table, CL_TRUE, 0,
                              0x100 * 8 * 20 * sizeof(cl_uint), (void *)tbl
                              , 0, NULL, &tbl_ev);
     clFinish(command_queue);

    }
   free(tbl);
   return r;
}


cl_int GLCLDraw::SetupBuffer(GLuint *texid)
{
   cl_int ret = 0;
   cl_int r = 0;
   GLuint tid;
   unsigned int size = 640 * 400 * sizeof(cl_uchar4);
   
//   if(bCLDirectMapping) {
//   } else {
   inbuf = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, // Reduce HOST-CPU usage.
			     (size_t)(0x8000 * 6 * sizeof(Uint8)), NULL, &r);
   ret |= r;
//   }
   
   XM7_DebugLog(XM7_LOG_INFO, "CL: Alloc STS: inbuf : %d", r);

   palette = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR,
 		  (size_t)(4096 * sizeof(Uint32)), NULL, &r);
   ret |= r;
   XM7_DebugLog(XM7_LOG_INFO, "CL: Alloc STS: palette : %d", r);

   table = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR,
 		  (size_t)(0x100 * 8 * 20 * sizeof(cl_uint)), NULL, &r);
   ret |= r;
   XM7_DebugLog(XM7_LOG_INFO, "CL: Alloc STS: table : %d", r);

   // Texture直接からPBO使用に変更 20121102
   if(GetGLEnabled() != 0) {
     if(bGL_PIXEL_UNPACK_BUFFER_BINDING && (texid != NULL)) {
       tid = *texid;
       glGenBuffers(1, &pbo);
       glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
       glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
       //    XM7_DebugLog(XM7_LOG_DEBUG, "CL: PBO=%08x Size=%d context=%08x", pbo, size, context);
       outbuf = clCreateFromGLBuffer(context, CL_MEM_WRITE_ONLY, 
				     pbo, &r);
       glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
       ret |= r;
     } else {
       ret = CL_DEVICE_NOT_AVAILABLE;
       r = ret;
     }
     XM7_DebugLog(XM7_LOG_INFO, "CL: Alloc STS: outbuf (GLCL): %d", r);
   } else { // Fallback
     pixelBuffer = (Uint32 *)malloc(640 * 400 * sizeof(Uint32));
     outbuf = clCreateBuffer(context, CL_MEM_WRITE_ONLY | 0,
 		  (size_t)(640 * 400 * sizeof(Uint32)), NULL, &r);
     ret |= r;
     XM7_DebugLog(XM7_LOG_INFO, "CL: Alloc STS: outbuf (CL): %d", r);
   }
   
   return ret;
}

GLuint GLCLDraw::GetPbo(void)
{
   return pbo;
}
