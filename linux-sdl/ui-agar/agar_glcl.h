/*
 * Header for CL with GL
 * (C) 2012 K.Ohta
 * Notes:
 *   Not CL model: VramDraw->[Virtual Vram]->AGEventDraw2->drawUpdateTexture->[GL Texture]->Drawing
 *       CL Model: AGEvenDraw2 -> GLCL_DrawEventSub -> [GL/CL Texture] ->Drawing
 * History:
 *   Nov 01,2012: Initial.
 */
#include <SDL/SDL.h>
#ifdef _WINDOWS
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

#ifdef _USE_OPENCL
 #include <CL/cl.h>
 #include <CL/cl_gl.h>
 #include <CL/cl_gl_ext.h>


extern GLuint uVramTextureID;

class GLCLDraw {
 public:
   GLCLDraw();
   ~GLCLDraw();
   cl_int GetVram(int bmode);
   cl_int BuildFromSource(const char *p);
   cl_int SetupBuffer(GLuint *texid);
   cl_int SetupTable(void);
   cl_int InitContext(int platformnum, int processornum, int GLinterop);
   int GetPlatforms(void);
   int GetUsingDeviceNo(void);
   int GetDevices(void);
   void GetDeviceType(char *str, int maxlen, int num);
   void GetDeviceName(char *str, int maxlen, int num);
   GLuint GLCLDraw::GetPbo(void);
   int GetGLEnabled(void);
   Uint32 *GetPixelBuffer(void);
   int ReleasePixelBuffer(Uint32 *p);
   Uint8 *GetBufPtr(void);

   cl_context context = NULL;
   cl_command_queue command_queue = NULL;

   /* Program Object */
   const char *source = NULL;
   cl_program program = NULL;
   cl_int ret_num_devices;
   cl_int ret_num_platforms;
   cl_platform_id platform_id[8];
   cl_device_id device_id[8];

 private:
   CL_CALLBACK LogProgramExecute(cl_program program, void *userdata);
   CL_CALLBACK (*build_callback)(cl_program, void *);
   int w2 = 0;
   int h2 = 0;
   cl_event event_exec;
   cl_event event_uploadvram[4];
   cl_event event_copytotexture;
   cl_event event_release;
   cl_kernel kernels_array[8];
   cl_kernel *kernel_8colors = NULL;
   cl_kernel *kernel_4096colors = NULL;
   cl_kernel *kernel_256kcolors = NULL;
   cl_kernel *kernel_table = NULL;
   cl_uint nkernels;

   cl_mem inbuf = NULL;
   cl_mem outbuf = NULL;
   cl_mem palette = NULL;
   cl_mem internalpal = NULL;
   cl_mem table = NULL;
   cl_context_properties *properties = NULL;	
   GLuint pbo = 0;
   cl_int copy8(void);
   cl_int copy8_400l(void);
   cl_int copy4096(void);
   cl_int copysub(int hh);
   cl_int copy256k(void);
   int using_device = 0;
   int bCLEnableKhrGLShare = 0;
   Uint32 *pixelBuffer = NULL;
   Uint8 *TransferBuffer = NULL;
   int bModeOld = -1;
   cl_device_type device_type[8];
   cl_ulong local_memsize[8];
};

enum {
  CLKERNEL_8 = 0,
  CLKERNEL_4096,
  CLKERNEL_256K,
  CLKERNEL_END
};

#endif /* _USE_OPENCL */
