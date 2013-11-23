/*
 * agar_guicore.cpp
 *
 *  Created on: 2010/11/08
 *      Author: whatisthis
 */
#include <SDL/SDL.h>
#include <libintl.h>
extern "C" {
#include <agar/core.h>
#include <agar/core/types.h>
#include <agar/gui.h>
#include <agar/gui/opengl.h>
}
#include "xm7.h"
#include "fdc.h"

#ifdef USE_AGAR
#include "agar_xm7.h"
#include "agar_cfg.h"
#include "agar_toolbox.h"
#include "agar_gldraw.h"
#include "agar_draw.h"
#include "agar_sdlview.h"
#include "agar_osd.h"
#else
#include "xm7_sdl.h"
#include "sdl_cfg.h"
#endif

#include "api_kbd.h"
#include "sdl_inifile.h"
#include "api_draw.h"
#include "sdl_cpuid.h"
extern struct XM7_CPUID *pCpuID;

extern "C" {
void InitInstance(void);
extern void OnDestroy(AG_Event *event);
extern DWORD XM7_timeGetTime(void);	/* timeGetTime互換関数 */
extern void  XM7_Sleep(DWORD t);	/* Sleep互換関数 */
}

Uint32 nDrawTick1D;
BOOL   bResizeGUIFlag;
extern Uint32 nDrawTick1E;

extern void Create_AGMainBar(AG_Widget *Parent);
extern void InitGridVertexs(void);


BOOL EventGuiSingle(AG_Driver *drv, AG_DriverEvent *ev)
{
   int w;
   int h;
   BOOL bi;
   
   bi = FALSE;
   if(ev == NULL) return TRUE;
   /* Retrieve the next queued event. */
   if (AG_ProcessEvent(drv, ev) == -1) 	return FALSE;
   //	if(drv == NULL) return;
   /* Forward the event to Agar. */
   return TRUE;
}

BOOL EventGUI(AG_Driver *drv)
{
	AG_DriverEvent dev;
	BOOL r;
	if(AG_PendingEvents(drv) <= 0) return FALSE;
	do {
		if (AG_GetNextEvent(drv, &dev) == 1) {
		   r = EventGuiSingle(drv, &dev);
		   if(!r) return FALSE;
		}
	} while (AG_PendingEvents(drv) > 0);
	return TRUE;
}


void AGDrawTaskEvent(BOOL flag)
{
   Uint32 nDrawTick2D;
   AG_Window *win;
   AG_Driver *drv;
   Uint32 fps;
   Uint32 oldfps = nDrawFPS;
   AG_EventSource *src;
   AG_EventSink *es;
   
   bResizeGUIFlag = FALSE;
   nDrawTick2D = XM7_timeGetTime();

   src = AG_GetEventSource();
   
   AG_TAILQ_FOREACH(es, &src->prologues, sinks){
	                es->fn(es, &es->fnArgs);
   }
   if(nDrawFPS > 2) {
      fps = 1000 / nDrawFPS;
   } else {
      fps = 500;
   }
   
   for(;;) {
      if(oldfps != nDrawFPS){ // FPS Change 20120120
	 oldfps = nDrawFPS;
	 if(nDrawFPS > 2) {
	    fps = 1000 / nDrawFPS;
	 } else {
	    fps = 500;
	 }
      }
      if(EventSDL(NULL) == FALSE) return;
      nDrawTick2D = XM7_timeGetTime();

      if(nDrawTick2D < nDrawTick1D) nDrawTick1D = 0; // オーバーフロー対策
      if((nDrawTick2D - nDrawTick1D) > fps) {
	 // Force-Redraw mainwindow, workaround of glx driver.

#ifdef USE_OPENGL
	   if(GLDrawArea != NULL){
	      AG_Redraw(GLDrawArea);
	   } else 
#else
	   if(DrawArea != NULL) {
	      XM7_SDLViewUpdateSrc(DrawArea, NULL);
	      AG_Redraw(DrawArea);
	   }
#endif
	 AG_WindowDrawQueued();
	 nDrawTick1D = nDrawTick2D;
	 XM7_Sleep(1);
	 //EventSDL(NULL);
      } else if(AG_PendingEvents(NULL) != 0) {
      //if(AG_PendingEvents(NULL) != 0) {
	 AG_DriverEvent dev;
	 if(EventSDL(NULL) == FALSE) return;
	 if(AG_GetNextEvent(NULL, &dev) == 1) AG_ProcessEvent(NULL, &dev);
	 XM7_Sleep(1);
      } else { // Timeout
	 Uint32 tim = 0;
	 AG_TAILQ_FOREACH(es, &src->spinners, sinks){
	    es->fn(es, &es->fnArgs);
	 }
	 if (src->sinkFn() == -1) {
	    return ;
	 }
	 AG_TAILQ_FOREACH(es, &src->epilogues, sinks) {
	    es->fn(es, &es->fnArgs);
	 }
	 if (src->breakReq) break;
	 tim = AG_GetTicks();
//	 AG_ProcessTimeouts(tim);
//	 Code got from core/event.c
	 XM7_Sleep(1);
      }	// Process Event per 1Ticks;

      AG_WindowProcessQueued();
      
   }
}

        




void ProcessKeyDown(AG_Event *event)
{
	// キーハンドラー
	int sym = AG_INT(1);
	int mod = AG_INT(2);
	Uint32  unicode = (Uint32)AG_ULONG(3);
	OnKeyPressAG(sym, mod, unicode);
}



void ProcessKeyUp(AG_Event *event)
{
	// キーハンドラー
	int sym = AG_INT(1);
	int mod = AG_INT(2);
	Uint32  unicode = (Uint32)AG_ULONG(3);
	OnKeyReleaseAG(sym, mod, unicode);
}

extern void AG_initsub(void);
extern void AG_detachsub(void);
extern Uint32 nDrawTick1;




extern "C" {

extern void OnMouseMotionSDL(AG_Event *event);
extern void OnMouseButtonUpSDL(AG_Event *event);
extern void OnMouseButtonDownSDL(AG_Event *event);
extern void OnMouseMotionGL(AG_Event *event);
extern void OnMouseButtonUpGL(AG_Event *event);
extern void OnMouseButtonDownGL(AG_Event *event);



static void InitFont(void)
{
}

BOOL LoadWindowIconPng(AG_Window *win, char *path, char *filename)
     {
	
   char fullpath[MAXPATHLEN + 1];
   int len;
   
   if(win == NULL) return FALSE;   
   if(filename == NULL) return FALSE;
   if(strlen(filename) >= MAXPATHLEN) return FALSE;
   fullpath[0] = '\0';
   if(path == NULL) {
#ifdef RSSDIR
      strcpy(fullpath, RSSDIR);
#else
      strcpy(fullpath, "./.xm7/");
#endif
   } else {
      if(strlen(path) >= MAXPATHLEN) return FALSE;
      strcpy(fullpath, path); 
   }
   
   len = strlen(fullpath) + strlen(filename);
   if(len >= MAXPATHLEN) return FALSE;
   strcat(fullpath, filename);
   
   if(!AG_FileExists(fullpath)) { // Fallback
      return FALSE;
   } else {
      AG_Surface *mark;
      mark = AG_SurfaceFromPNG(fullpath);
      if(mark != NULL) {
	 AG_WindowSetIcon(win, mark);
	 AG_SurfaceFree(mark);
	 return TRUE;
      } else {
	 return FALSE; // Illegal PNG.
      }
   }

}


   
   
   
void InitInstance(void)
{
	AG_HBox *hb;
        AG_VBox *vb;
	AG_Window *win;
//	AG_Driver *drv;

#ifdef USE_OPENGL
    GLDrawArea = NULL;
#endif /* USE_OPENGL */
    DrawArea = NULL;
    pStatusBar = NULL;

	InitFont();

	//  最初にカスタムウイジェットをつける
    AG_RegisterClass(&XM7_SDLViewClass);
   if(agDriverSw) {
      MainWindow = AG_WindowNew(AG_WINDOW_NOTITLE |  AG_WINDOW_NOBORDERS | AG_WINDOW_KEEPBELOW | AG_WINDOW_NOBACKGROUND | AG_WINDOW_MODKEYEVENTS);
   } else {
      MainWindow = AG_WindowNew(AG_WINDOW_MODKEYEVENTS );
//        MainWindow = AG_WindowNew(AG_WINDOW_NOTITLE |  AG_WINDOW_NOBORDERS | AG_WINDOW_KEEPBELOW | AG_WINDOW_NOBACKGROUND | AG_WINDOW_MODKEYEVENTS);
   }
//   AGWIDGET(MainWindow)->flags |= AG_WIDGET_NOSPACING;
   AG_WindowSetGeometry (MainWindow, 0, 0 , nDrawWidth, nDrawHeight);
   AG_SetEvent(MainWindow , "window-close", OnDestroy, NULL);
   AG_WindowSetCloseAction(MainWindow, AG_WINDOW_DETACH);
   
   
   MenuBar = AG_MenuNew(AGWIDGET(MainWindow), 0);
   Create_AGMainBar(AGWIDGET(NULL));
   AG_WidgetSetPosition(MenuBar, 0, 0);
   AG_WidgetShow(AGWIDGET(MenuBar));
   
   AG_WindowShow(MainWindow);
   AG_WindowFocus(MainWindow);
#if USE_OPENGL
    if(AG_UsingGL(NULL) != 0) {

       hb = AG_HBoxNew(AGWIDGET(MainWindow), AG_BOX_HFILL);
          /*
         * OpenGL Capability
         */
       
        GLDrawArea = AG_GLViewNew(AGWIDGET(hb) , 0);
        GLDrawArea->wid.flags |= AG_WIDGET_CATCH_TAB;
        //if(!AG_UsingSDL(AGDRIVER(GLDrawArea))) {
	//   InitFBO(); // 拡張の有無を調べてからFBOを初期化する。
	   // FBOの有無を受けて、拡張の有無変数を変更する（念のために）
	//   InitGLExtensionVars();
	//   InitGridVertexs(); // Grid初期化
	//}
       
        AG_WidgetSetSize(GLDrawArea, nDrawWidth, nDrawHeight);
        AG_GLViewSizeHint(GLDrawArea, nDrawWidth, nDrawHeight);
        AG_GLViewDrawFn (GLDrawArea, AGEventDrawGL2, NULL);
        AG_GLViewKeydownFn (GLDrawArea, AGEventKeyDownGL, NULL);
        AG_GLViewKeyupFn (GLDrawArea, AGEventKeyUpGL, NULL);
        AG_GLViewScaleFn (GLDrawArea, AGEventScaleGL, NULL);
        //AG_GLViewOverlayFn (GLDrawArea, AGEventOverlayGL, NULL);
        AG_GLViewMotionFn(GLDrawArea, OnMouseMotionGL, NULL);
        AG_GLViewButtondownFn(GLDrawArea, OnMouseButtonDownGL, NULL);
        AG_GLViewButtonupFn(GLDrawArea, OnMouseButtonUpGL, NULL);
	bUseOpenGL = TRUE;
	DrawArea = NULL;
	//AGWIDGET(GLDrawArea)->flags |= AG_WIDGET_NOSPACING;
        AG_WidgetShow(GLDrawArea);
        AG_WidgetFocus(AGWIDGET(GLDrawArea));
        ResizeWindow_Agar2(nDrawWidth, nDrawHeight);

    } else
#endif /* USE_OPENGL */
     {
        // Non-GL
        hb = AG_HBoxNew(AGWIDGET(MainWindow), AG_BOX_HFILL);
#if 1
        DrawArea = XM7_SDLViewNew(AGWIDGET(hb), NULL, NULL);
        AGWIDGET(DrawArea)->flags |= AG_WIDGET_CATCH_TAB;
        XM7_SDLViewDrawFn(DrawArea, XM7_SDLViewUpdateSrc, "%p", NULL);
        XM7_SDLViewSurfaceNew(DrawArea, 640, 400);
#else
        DrawArea = AG_PixmapNew(AGWIDGET(hb), 0, 640, 400);
        AGWIDGET(DrawArea)->flags |= (AG_WIDGET_CATCH_TAB | AG_WIDGET_USE_MOUSEOVER | AG_WIDGET_FOCUSABLE);
	DrawSurface = AG_SurfaceRGBA(640, 400, 32, AG_SRCALPHA, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
	if(DrawSurface != NULL) {
	   if(DrawArea != NULL) {
	      DrawSurfaceId = AG_PixmapAddSurface(DrawArea, DrawSurface);
	      AG_PixmapSetSurface(DrawArea, DrawSurfaceId);
	   }
	}
#endif
        AG_SetEvent(DrawArea, "key-up", ProcessKeyUp, NULL);
        AG_SetEvent(DrawArea, "key-down", ProcessKeyDown, NULL);
        AG_SetEvent(DrawArea, "mouse-motion", OnMouseMotionSDL, NULL);
        AG_SetEvent(DrawArea, "mouse-button-down", OnMouseButtonDownSDL, NULL);
        AG_SetEvent(DrawArea, "mouse-button-up", OnMouseButtonUpSDL, NULL);

        AG_WidgetSetSize(DrawArea, 640, 400);
        bUseOpenGL = FALSE;
#ifdef USE_OPENGL
        GLDrawArea = NULL;
#endif /* USE_OPENGL */
//	AGWIDGET(DrawArea)->flags |= AG_WIDGET_NOSPACING;
        AG_WidgetShow(DrawArea);
        AG_WidgetFocus(AGWIDGET(DrawArea));
        ResizeWindow_Agar2(nDrawWidth, nDrawHeight);
    }
    {
      // hb = AG_HBoxNew(AGWIDGET(MainWindow), 0);
       pStatusBar = AG_HBoxNew(AGWIDGET(MainWindow), AG_BOX_VFILL | AG_WIDGET_NOSPACING);
       AG_WidgetSetSize(pStatusBar, 640, 40);
       CreateStatus(AGWIDGET(pStatusBar));
//       AGWIDGET(pStatusBar)->flags |= (AG_WIDGET_HFILL | AG_WIDGET_NOSPACING);
       AG_WidgetShow(pStatusBar);
    }
#if 0 // GUI-DEBUG
     {
     win = AG_GuiDebugger(AGWIDGET(MainWindow));
     AG_WindowShow(win);
    }
#endif
   
}

/*
 *  Update UI
 */
void ui_update(void)
{
	AG_WindowUpdate(MainWindow);
}


}

