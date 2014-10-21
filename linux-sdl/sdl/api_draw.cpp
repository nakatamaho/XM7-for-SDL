/*
 *  FM-7 EMULATOR "XM7"
 *  Copyright (C) 1999-2003 ＰＩ．(ytanaka@ipc-tokai.or.jp)
 *  Copyright (C) 2001-2003 Ryu Takegami
 *  Copyright (C) 2004 GIMONS
 *  Copyright (C) 2010 K.Ohta
 *  [SDL 表示 ]
 *  2010.10.28 sdl_draw.c から移動
 */



#include <agar/core.h>
#include <agar/core/types.h>
#include <agar/gui.h>
#ifdef _OPENMP
#include <omp.h>
#endif // _OPENMP

#include "xm7.h"
#include "multipag.h"
#include "ttlpalet.h"
#include "apalet.h"
#include "subctrl.h"
#include "display.h"
#include "device.h"

#include "agar_xm7.h"
#include "agar_draw.h"
#include "agar_cfg.h"
#ifdef USE_OPENGL
  #include "agar_gldraw.h"
  #include "agar_glutil.h"
  #ifdef _USE_OPENCL
    #include "agar_glcl.h"
    extern class GLCLDraw *cldraw;
  #endif
#endif /* USE_OPENGL */

//#include "sdl.h"
#include "sdl_cpuid.h"

#include "api_vram.h"
#include "api_draw.h"
//#include "api_scaler.h"
#include "cache_wrapper.h"

/*
 *  グローバル ワーク
 */
extern "C" {
   DWORD   rgbTTLGDI[16];	/* デジタルパレット */
   DWORD   rgbAnalogGDI[4096];	/* アナログパレット */
   struct DrawPieces SDLDrawFlag;
   
   BOOL            bFullScan;		/* フルスキャン(Window) */
   BOOL            bDirectDraw;		/* 直接書き込みフラグ */
   AG_Surface     *realDrawArea;	/* 実際に書き込むSurface(DirectDrawやOpenGLを考慮する) */
   WORD            nDrawTop;			/* 描画範囲上 */
   WORD            nDrawBottom;		/* 描画範囲下 */
   WORD            nDrawLeft;		/* 描画範囲左 */
   WORD            nDrawRight;		/* 描画範囲右 */
   WORD            nDrawWidth;
   WORD            nDrawHeight;
   BOOL            bPaletFlag;		/* パレット変更フラグ */
   BOOL            bClearFlag;
   int             nOldVideoMode;
   WORD			nDrawFPS;   /* FPS値 20100913 */
   WORD 			nEmuFPS; /* エミュレーションFPS値 20110123 */
   BOOL			bSyncToVSYNC; /* VSYNC同期(OpenGLのみ) */
   BOOL 			bSmoosing; /* スムージング処理する(GLのみ?) */
   BOOL            bOldFullScan;	/* クリアフラグ(過去) */
   BOOL bFullScanFS;							/* フルスキャン(FullScreen) */
   BOOL bDoubleSize;							/* 2倍拡大フラグ */

   BOOL  bUseOpenGL; /* OPENGLを描画に使う */
#if XM7_VER == 1
   BOOL bGreenMonitor;							/* グリーンモニタフラグ */
#endif
   BOOL bRasterRendering;						/* ラスタレンダリングフラグ */
   Api_Vram_FuncList *pVirtualVramBuilder;
   BOOL bDirtyLine[400];				/* 要書き換えフラグ */
   BOOL bDrawLine[400]; /* ラスタ時、書き替え指示するフラグ */
   extern struct  XM7_CPUID *pCpuID;
}
   SDL_semaphore *DrawInitSem;

/*
 *	スタティック ワーク
 */
static BOOL bNextFrameRender;				/* 次フレーム描画フラグ */

#if XM7_VER >= 3
BYTE    bMode;		/* 画面モード */

#else				/*  */
static BOOL     bAnalog;	/* アナログモードフラグ */

#endif				/*  */
static BYTE     bNowBPP;	/* 現在のビット深度 */
static WORD    nOldDrawWidth;
static WORD    nOldDrawHeight;


#if XM7_VER >= 3
static BOOL     bWindowOpen;	/* ハードウェアウィンドウ状態
 */
static WORD    nWindowDx1;	/* ウィンドウ左上X座標 */
static WORD    nWindowDy1;	/* ウィンドウ左上Y座標 */
static WORD    nWindowDx2;	/* ウィンドウ右下X座標 */
static WORD    nWindowDy2;	/* ウィンドウ右下Y座標 */

#endif				/*  */

/*
 * マルチスレッド向け定義
 */
BOOL DrawINGFlag;
BOOL DrawSHUTDOWN;
BOOL DrawWaitFlag;


AG_Thread DrawThread;

int newDrawWidth;
int newDrawHeight;
BOOL newResize;

extern Uint32 nDrawTick1D;
extern Uint32 nDrawTick1E;
#ifdef USE_OPENGL
extern GLuint uVramTextureID;
#endif /* USE_OPENGL */


/*
 *  プロトタイプ宣言
 */

/*
 * ビデオドライバ関連
 */


void ResizeWindow(int w, int h)
{
//    char          EnvMainWindow[64]; /* メインウィンドウのIDを取得して置く環境変数 */
//    SDL_SysWMinfo sdlinfo;
            ResizeWindow_Agar(w, h);
}

static void initsub(void);
static void detachsub(void);


void AG_DrawInitsub(void)
{
//	initsub();
}
void AG_DrawDetachsub(void)
{
//	detachsub();
}

void Flip(void)
{

}
/*
 *  BITBLT
 */
BOOL XM7_BitBlt(int nDestLeft, int nDestTop, int nWidth, int nHeight,
			int nSrcLeft, int nSrcTop)
{

	AG_Rect srcrect, dstrect;

	if(bUseOpenGL) {
		Flip();
		return TRUE;
	}
	srcrect.x = nSrcLeft;
	srcrect.y = nSrcTop;
	srcrect.w = (Uint16) nWidth;
	srcrect.h = (Uint16) nHeight;

	dstrect.x = nDestLeft;
	dstrect.y = nDestTop;
	dstrect.w = (Uint16) nWidth;
	dstrect.h = (Uint16) nHeight;
		/*
		 * データ転送
		 */
		/*
		 * 擬似インタレース設定をここでやる
		 */
		if (bOldFullScan != bFullScan) {
			if (!bFullScan) {
				RenderSetOddLine();
			} else {
				RenderFullScan();
			}
		}
		bOldFullScan = bFullScan;
		return TRUE;
}

static void detachsub(void)
{

//    detachsub_scaler();
	// 最後にVRAMハンドラ
}


static void initsub()
{

//	initsub_scaler();
	//	b256kFlag = FALSE;
    SetVram_200l(vram_dptr);

//	init_scaler();

}


extern "C"
{

void ResizeGL(int w, int h)
{
	newDrawWidth = w;
	newDrawHeight = h;
	newResize = TRUE;
}

/*
 *  初期化
 */



void	InitDraw(void)
{
		/*
		 * ワークエリア初期化
		 */
#if XM7_VER >= 3
		bMode = SCR_200LINE;
#else				/*  */
		bAnalog = FALSE;

#endif				/*  */
		bNowBPP = 24;
                pVirtualVramBuilder = NULL;
                _prefetch_data_write_permanent(&rgbTTLGDI, sizeof(rgbTTLGDI));
                memset(rgbTTLGDI, 0, sizeof(rgbTTLGDI));
                _prefetch_data_write_permanent(&rgbAnalogGDI, sizeof(rgbAnalogGDI));
		memset(rgbAnalogGDI, 0, sizeof(rgbAnalogGDI));
                _prefetch_data_write_permanent(&SDLDrawFlag, sizeof(SDLDrawFlag));
		memset((void *)&SDLDrawFlag, 0, sizeof(SDLDrawFlag));
   
		nDrawTop = 0;
		nDrawBottom = 400;
		nDrawLeft = 0;
		nDrawRight = 640;
		nOldDrawHeight = 480;
		nOldDrawWidth = 640;
		nDrawHeight = 480;
		nDrawWidth = 640;
		nDrawFPS = 25;
		nEmuFPS = 20;
		bSyncToVSYNC = TRUE;
		bSmoosing = FALSE;

		bFullScan = TRUE;
		bPaletFlag = FALSE;
		bOldFullScan = bFullScan;
#if XM7_VER >= 3
		nOldVideoMode = SCR_200LINE;
#else
		nOldVideoMode = FALSE;
#endif
	        bFullScanFS = FALSE;
	        bDoubleSize = FALSE;
#if XM7_VER == 1
	        bGreenMonitor = FALSE;
#endif
                bRasterRendering = FALSE;
                bNextFrameRender = FALSE;
                _prefetch_data_write_permanent(bDirtyLine, sizeof(bDirtyLine));
                memset(bDirtyLine, 0, sizeof(bDirtyLine));
                _prefetch_data_write_permanent(bDrawLine, sizeof(bDrawLine));
                memset(bDrawLine, 0, sizeof(bDrawLine));
		SetDrawFlag(FALSE);
		nDrawTick1D = 0;
		nDrawTick1E = 0;
		newResize = FALSE;


#if XM7_VER >= 3
		bWindowOpen = FALSE;
		nWindowDx1 = 640;
		nWindowDy1 = 400;
		nWindowDx2 = 0;
		nWindowDy2 = 0;
#endif				/*  */
		bDirectDraw = TRUE;
		DrawINGFlag = FALSE;
		DrawSHUTDOWN = FALSE;
		DrawWaitFlag = FALSE;
//		bUseOpenGL = TRUE;

		/*
		 * 直接書き込み→間接書き込み
		 */

//        realDrawArea = GetDrawSurface();
		//AG_MutexInit(&DrawMutex);
		//AG_CondInit(&DrawCond);
		//AG_MutexUnlock(&DrawMutex);
		AG_ThreadCreate(&DrawThread, DrawThreadMain, NULL);
		if(!DrawInitSem) {
			DrawInitSem = SDL_CreateSemaphore(1);
			SDL_SemPost(DrawInitSem);
		}
		/*
		 *  VRAMテクスチャ生成
		 */
#ifdef USE_OPENGL
                 uVramTextureID = 0;
#endif /* USE_OPENGL */
   //		 initvramtbl_8_vec();
		 initvramtbl_4096_vec();
}


	/*
	 *  クリーンアップ
	 */
void	CleanDraw(void)
{

		DrawSHUTDOWN = TRUE;
		//		AG_CondSignal(&DrawCond);
		AG_ThreadJoin(DrawThread, NULL);
//                DrawThread = NULL;

		if(DrawInitSem != NULL) {
			SDL_DestroySemaphore(DrawInitSem);
		}

   //		 detachvramtbl_8_vec();
		 detachvramtbl_4096_vec();
		detachsub();
}



	/*
	 *  全ての再描画フラグを設定
	 */
void SetDrawFlag(BOOL flag)
{
    int x;
    int y;
    int ip;
   
//#ifdef _OPENMP
//       #pragma omp parallel for shared(SDLDrawFlag, flag) private(x)
//#endif
   SDLDrawFlag.Drawn = flag;
   SDLDrawFlag.APaletteChanged = flag;
   SDLDrawFlag.DPaletteChanged = flag;
   SDLDrawFlag.ForcaReDraw = flag;
   if((nRenderMethod != RENDERING_RASTER) && (bCLEnabled == FALSE)) return; 
   for(y = 0; y < 50 ; y++) {
        for(x = 0; x < 80; x++){
           SDLDrawFlag.read[x][y] =
           SDLDrawFlag.write[x][y] = flag;
        }
    }
}

/*
 *  640x200、デジタルモード セレクト
 */
BOOL Select640(void)
{
   /*
    * 全領域無効
    */
   LockVram();
   nDrawTop = 0;
   nDrawBottom = 200;
   nDrawLeft = 0;
   nDrawRight = 640;
   bPaletFlag = TRUE;
   Palet640();
   
#if defined(USE_SSE2)
    if(pCpuID->use_sse2) {
       pVirtualVramBuilder = &api_vram8_sse2;
    } else {
       pVirtualVramBuilder = &api_vram8_generic;
    }
#else
   pVirtualVramBuilder = &api_vram8_generic;
#endif
   if((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
      SetDirtyFlag(0, 400, TRUE);
   } else {
      SetDrawFlag(TRUE);
   }
   
#if XM7_VER >= 3
   /*
    * デジタル/200ラインモード
    */
   bMode = SCR_200LINE;
#else				/*  */
   /*
    * デジタルモード
    */
   bAnalog = FALSE;
#endif				/*  */
   SDLDrawFlag.Drawn = TRUE;
   UnlockVram();
   return TRUE;
}


#if XM7_VER >= 3
    /*
     *  640x400、デジタルモード セレクト
     */
BOOL Select400l(void)
{
   /*
    * 全領域無効
    */
   LockVram();
   nDrawTop = 0;
   nDrawBottom = 400;
   nDrawLeft = 0;
   nDrawRight = 640;
   bPaletFlag = TRUE;
   Palet640();
#if defined(USE_SSE2)
    if(pCpuID->use_sse2) {
       pVirtualVramBuilder = &api_vram8_sse2;
    } else {
       pVirtualVramBuilder = &api_vram8_generic;
    }
#else
   pVirtualVramBuilder = &api_vram8_generic;
#endif

   if((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
      SetDirtyFlag(0, 400, TRUE);
   } else {
      SetDrawFlag(TRUE);
   }
   /*
    * デジタル/400ラインモード
    */
   bMode = SCR_400LINE;
   SDLDrawFlag.Drawn = TRUE;
   UnlockVram();
   return TRUE;
}


#endif				/*  */

    /*
     *  320x200、アナログモード セレクト
     */
BOOL Select320(void)
{
/*
 * 全領域無効
 */
   LockVram();
   nDrawTop = 0;
   nDrawBottom = 200;
   nDrawLeft = 0;
   nDrawRight = 320;
   bPaletFlag = TRUE;
   
#if defined(USE_SSE2)
    if(pCpuID->use_sse2) {
       pVirtualVramBuilder = &api_vram4096_sse2;
    } else {
       pVirtualVramBuilder = &api_vram4096_generic;
    }
#else
   pVirtualVramBuilder = &api_vram4096_generic;
#endif

   Palet320();
   if((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
      SetDirtyFlag(0, 400, TRUE);
   } else {
      SetDrawFlag(TRUE);
   }
#if XM7_VER >= 3
   /*
    * アナログ/200ラインモード
    */
   bMode = SCR_4096;

#else				/*  */
   /*
    * アナログモード
    */
   bAnalog = TRUE;
#endif				/*  */
   SDLDrawFlag.Drawn = TRUE;
   UnlockVram();
   return TRUE;
}


#if XM7_VER >= 3
   /*
    *  320x200、26万色モード セレクト
    */
BOOL Select256k()
{
   /*
    * 全領域無効
    */
   LockVram();
   nDrawTop = 0;
   nDrawBottom = 200;
   nDrawLeft = 0;
   nDrawRight = 320;
   bPaletFlag = TRUE;
#if defined(USE_SSE2)
    if(pCpuID->use_sse2) {
       pVirtualVramBuilder = &api_vram256k_sse2;
    } else {
       pVirtualVramBuilder = &api_vram256k_generic;
    }
#else
   pVirtualVramBuilder = &api_vram256k_generic;
#endif

   Palet320();
   if((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
      SetDirtyFlag(0, 200, TRUE);
   } else {
      SetDrawFlag(TRUE);
   }
   
   /*
    * アナログ(26万色)/200ラインモード
    */
   bMode = SCR_262144;
   SDLDrawFlag.Drawn = TRUE;
   UnlockVram();
   return TRUE;
}
#endif
   
/*
 *  セレクト
 */
BOOL SelectDraw(void)
{
   BOOL ret;
   AG_Color nullcolor;
   AG_Rect rect;
   AG_Widget *w;

   if (SelectCheck()) {
      return TRUE;
   }

   rect.h = nDrawWidth;
   rect.w = nDrawHeight;
   rect.x = 0;
   rect.y = 0;
   DrawSHUTDOWN = FALSE;
   /*
    * すべてクリア
    */
   if(!bUseOpenGL) {
    if(DrawArea != NULL) {
       AG_ObjectLock(DrawArea);
       nullcolor.r = 0;
       nullcolor.g = 0;
       nullcolor.b = 0;
       nullcolor.a = 255;
       AG_FillRect(AGWIDGET(DrawArea)->drv->sRef , &rect, nullcolor);
       AG_ObjectUnlock(DrawArea);
    }
   } else { // OpenGLのとき
   }
   /*
    * すべてクリア
    */
   bOldFullScan = bFullScan;
	 /*
	  * セレクト
	  */
#if XM7_VER >= 3
	 switch (screen_mode) {
	 case SCR_400LINE:
		 ret =  Select400l();
	 case SCR_262144:
		 ret = Select256k();
	 case SCR_4096:
		 ret = Select320();
	 default:
		 ret = Select640();
	 }
#else				/*  */
if (mode320) {
	ret = Select320();
} else {
	ret =  Select640();
}
} else {
	ret = TRUE;
}
#endif				/*  */
	return ret;
}

/*
 *  オールクリア
 */
void AllClear(void)
{
   int x;
   int y;
   AG_Color nullcolor;
   AG_Rect rect;
   AG_Driver *drv;
   AG_Widget *w;

#if 0   
   LockVram();
   if((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
      SetDirtyFlag(0, 400, TRUE);
      if((!bCLEnabled)) {
	Palet640();
	Palet320();
      }
   } else {
      SetDrawFlag(TRUE);
   }
   UnlockVram();
#endif
   
#ifdef USE_OPENGL
   if(DrawArea != NULL) {
        drv = AGWIDGET(DrawArea)->drv;
        if(drv == NULL) return;
        w = AGWIDGET(DrawArea);
    } else if(GLDrawArea != NULL) {
        drv = AGWIDGET(GLDrawArea)->drv;
        if(drv == NULL) return;
        w = AGWIDGET(GLDrawArea);
    } else {
        return;
    }
#else /* USE_OPENGL */
   if(DrawArea != NULL) {
        drv = AGWIDGET(DrawArea)->drv;
        if(drv == NULL) return;
        w = AGWIDGET(DrawArea);
    } else {
        return;
    }
#endif /* USE_OPENGL */
   if(!bUseOpenGL) {
      rect.h = nDrawHeight;
      rect.w = nDrawWidth;
      rect.x = 0;
      rect.y = 0;
      /*
       * すべてクリア
       */
      AG_ObjectLock(w);
      nullcolor.r = 0;
      nullcolor.g = 0;
      nullcolor.b = 0;
      nullcolor.a = 255;
      AG_FillRect(drv->sRef, &rect, nullcolor);
      AG_ObjectUnlock(w);
   } else {
      // OpenGL
   }
   /*
    * 全領域をレンダリング対象とする
    */
   LockVram();
   nDrawTop = 0;
   nDrawBottom = 400;
   nDrawLeft = 0;
   nDrawRight = 640;
   if((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)) {
      SetDirtyFlag(0, 400, TRUE);
      if(!(bCLEnabled)) {
	Palet640();
	Palet320();
      }
   } else {
      SetDrawFlag(TRUE);
   }
   bClearFlag = FALSE;
   UnlockVram();
}


/*
 *  フルスキャン
 */
void RenderFullScan(void)
{
   return; // Scalerがやるので
}


/*
 *  奇数ライン設定
 */
void RenderSetOddLine(void)
{
   return; // Scalerがやるので
}


/*
 *  描画(通常)
 */
void OnDraw(void)
{
   /*
    * 描画スレッドのKICKを(1/60sec * nFrameSkip)ごとにする。
    */
  //   AG_CondSignal(&DrawCond);
}


/*
 *  描画(PAINT) *GTK依存だが、ダミー。
 */

int OnPaint(void)
{
    return 1;
}


/*-[ VMとの接続 ]-----------------------------------------------------------*/
/*
 *	再描画ラスタ一括設定
 */
void FASTCALL SetDirtyFlag(int top, int bottom, BOOL flag)
{
	int y;

	if ((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
		for (y = top; y < bottom; y++) {
			bDirtyLine[y] = flag;
		}
	}
	//SDLDrawFlag.Drawn = TRUE;
}

/*
 *  VRAMセット
 */
void vram_notify(WORD addr, BYTE dat)
{
	WORD x;
	WORD y;
	WORD xx;
	WORD yy;
	WORD ww;
	/*
	 * y座標算出
	 */
	if((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
#if XM7_VER >= 3
        switch (bMode) {
                case SCR_400LINE        :       addr &= 0x7fff;
                                                x = (WORD)((addr % 80) << 3);
                                                y = (WORD)(addr / 80);
						yy = y;
                                                break;
                case SCR_262144         :
                case SCR_4096           :       addr &= 0x1fff;
                                                x = (WORD)((addr % 40) << 4);
                                                y = (WORD)((addr / 40) << 1);
						yy = y >> 1;
                                                break;
                case SCR_200LINE        :       addr &= 0x3fff;
                                                x = (WORD)((addr % 80) << 3);
                                                y = (WORD)((addr / 80) << 1);
						yy = y >> 1;
                                                break;
        }
#else				/*  */
	if (bAnalog) {
		addr &= 0x1fff;
		x = 0;
		y = (WORD) ((addr / 40) << 1);
	}
	else {
		addr &= 0x3fff;
	        x = 0;
		y = (WORD) ((addr / 80) << 1);
	}
#endif				/*  */
      /*
       * オーバーチェック
       */
      LockVram();
	/*
	 * オーバーチェック
	 */
      if ((x >= 640) || (y >= 400)) {
		UnlockVram();
		return;
      }
      if(yy < 400) {
		bDirtyLine[yy] = TRUE;
      }
	   UnlockVram();
        /* 垂直方向 */
	   if (nDrawTop > y) {
                nDrawTop = y;
        }
        if (nDrawBottom <= y) {
#if XM7_VER >= 3
                if (bMode == SCR_400LINE) {
                        nDrawBottom = (WORD)(y + 1);
                }
                else {
                        nDrawBottom = (WORD)(y + 2);
                }
#else
                nDrawBottom = (WORD)(y + 2);
#endif
        }

        /* 水平方向 */
        if (nDrawLeft > x) {
                nDrawLeft = x;
        }
        if (nDrawRight <= x) {
#if XM7_VER >= 2
#if XM7_VER >= 3
                if (bMode & SCR_ANALOG) {
#else
                if (bAnalog) {
#endif
                        nDrawRight = (WORD)(x + 16);
                }
                else {
                        nDrawRight = (WORD)(x + 8);
                }
#else
                nDrawRight = (WORD)(x + 8);
#endif
        }
	UnlockVram();
	return;      
	} else {
	
#if XM7_VER >= 3
	switch (bMode) {
	case SCR_400LINE:
		addr &= 0x7fff;
		x = addr % 80;
		y = (addr / 80) >>3;
		xx = (addr % 80) << 3;
		yy = addr / 80;
		break;
	case SCR_262144:
	case SCR_4096:
		addr &= 0x1fff;
		x = addr % 40;
		y = (addr / 40) >> 3;
		xx = (addr % 40) << 4;
		yy = (addr / 40) << 1;
		break;
	case SCR_200LINE:
		addr &= 0x3fff;
		x = addr % 80;
		y = (addr / 80) >>3;
		xx = (addr % 80) << 3;
		yy = (addr / 80) << 1;
		break;
	}

#else				/*  */
	if (bAnalog) {
		addr &= 0x1fff;
		x = (WORD) ((addr % 40) << 4);
		y = (WORD) ((addr / 40) << 1);
	}

	else {
		addr &= 0x3fff;
		x = (WORD) ((addr % 80) << 3);
		y = (WORD) ((addr / 80) << 1);
	}

#endif				/*  */
	/*
	 * オーバーチェック
	 */
	if ((xx >= 640) || (yy >= 400)) {
		return;
	}
	/*
	 * 再描画フラグを設定
	 */
        LockVram();
        SDLDrawFlag.read[x][y] = TRUE;
#ifdef _USE_OPENCL
	//       if(cldraw != NULL) SDLDrawFlag.Drawn = TRUE; // OK?
#endif	   
	/*
	 * 垂直方向更新
	 */
	if (nDrawTop > yy) {
		nDrawTop = yy;
	}
	if (nDrawBottom <= yy) {

#if XM7_VER >= 3
		if (bMode == SCR_400LINE) {
			nDrawBottom = (WORD) (yy + 1);
		}

		else {
			nDrawBottom = (WORD) (yy + 2);
		}

#else				/*  */
		nDrawBottom = (WORD) (yy + 2);

#endif				/*  */
	}

	/*
	 * 水平方向更新
	 */
	if (nDrawLeft > xx) {
		nDrawLeft = xx;
	}
	if (nDrawRight <= xx) {

#if XM7_VER >= 3
		if (bMode & SCR_ANALOG) {

#else				/*  */
			if (bAnalog) {

#endif				/*  */
				nDrawRight = (WORD) (xx + 16);
			}

			else {
				nDrawRight = (WORD) (xx + 8);
			}
		}

		}
        UnlockVram();
   }
      
}


	/*
	 *  TTLパレットセット
	 */
void	ttlpalet_notify(void)
{

   /*
    * 不要なレンダリングを抑制するため、領域設定は描画時に行う
    */
   LockVram();
   if ((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
      bNextFrameRender = TRUE;
      if(bPaletFlag != TRUE) {
	 SetDirtyFlag(now_raster, 400, TRUE);
      }
   } else {
      if(bPaletFlag != TRUE) {
	 SetDrawFlag(TRUE);
      }
   }
   if(bPaletFlag != TRUE) {
      SDLDrawFlag.DPaletteChanged = TRUE; // Palette changed
      Palet640();
   }
   bPaletFlag = TRUE;
   UnlockVram();
}

/*
 *  アナログパレットセット
 */
void 	apalet_notify(void)
{
   LockVram();
   if ((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
      bNextFrameRender = TRUE;
      if(bPaletFlag != TRUE) {
	 SetDirtyFlag(now_raster, 400, TRUE);
      }
   } else {
      if(bPaletFlag != TRUE) {
	 SetDrawFlag(TRUE);
      }
   }
   if(bPaletFlag != TRUE) {
      SetDirtyFlag(now_raster, 400, TRUE);
      Palet320();
      SDLDrawFlag.APaletteChanged = TRUE; // Palette changed
      //SDLDrawFlag.Drawn = TRUE; // Palette changed
   }
   bPaletFlag = TRUE;
   UnlockVram();
}

/*
 *  再描画要求
 */
void 	display_notify(void)
{

   int i;
   int raster;
   /*
    * 再描画
    */
   LockVram();
   nDrawTop = 0;
   nDrawBottom = 400;
   nDrawLeft = 0;
   nDrawRight = 640;
   bPaletFlag = TRUE;
   if ((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
	bNextFrameRender = TRUE;
	SetDirtyFlag(0, 400, TRUE);
        if(bPaletFlag) {
	  bPaletFlag = FALSE;
	  if(bCLEnabled == FALSE) {
	    Palet640();
	    Palet320();
	  }
	   //	   SDLDrawFlag.Drawn = TRUE; // Palette changed
	}
      
	if (!run_flag) {
		raster = now_raster;
		for (i = 0; i < 400; i++) {
			now_raster = i;
			hblank_notify();
		}
		now_raster = raster;
	}
   } else {
      SetDrawFlag(TRUE);
   }
   UnlockVram();
}

static void Transfer_1Line(Uint8 *dst, int line);
   
/*
 *	VBLANK終了要求通知
 */
void FASTCALL vblankperiod_notify(void)
{
	BOOL flag;
	int y;
	int ymax;

	if ((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
		/* 次のフレームを強制的に書き換えるか */
		if (bNextFrameRender) {
			bNextFrameRender = FALSE;
			SetDirtyFlag(0, 400, TRUE);
		}
		else {
			/* 書き換えが必要かチェック */
#if XM7_VER >= 3
			if (screen_mode == SCR_400LINE) {
				ymax = 400;
			}
			else {
				ymax = 200;
			}
#elif XM7_VER == 1 && defined(L4CARD)
			if (enable_400line) {
				ymax = 400;
			}
			else {
				ymax = 200;
			}
#else
			ymax = 200;
#endif
			flag = FALSE;
			for (y = 0; y < ymax; y++) {
				flag |= bDirtyLine[y];
			}
			if (!flag) {
				return;
			}
		   _prefetch_data_read_l1(bDirtyLine, sizeof(bDirtyLine));
		   _prefetch_data_read_l1(aPlanes, sizeof(aPlanes));
		   if((bCLEnabled) && (cldraw != NULL)) {
		     for(y = 0; y < ymax; y++) {
		       if (bDirtyLine[y]) {
			 Transfer_1Line(cldraw->GetBufPtr(), y);
			 bDirtyLine[y] = FALSE;
			 
		       }
		     }
		   } else if(nRenderMethod == RENDERING_RASTER) {
		     for(y = 0; y < ymax; y++) {
		       if (bDirtyLine[y]) {
			 Draw_1Line(y);
			 bDirtyLine[y] = FALSE;
		       }
		     }
		   } 
		}
	}
}



static inline void Transfer_Main(Uint8 *dst, int src, int mode, int w)
{
  int ofset;
  int planes;
  int i;
  Uint8 *q;
  Uint8 *p;

  if((w <= 0) || (w > 80)) return;
  switch(mode) {
  case SCR_200LINE:
    ofset = 0x4000;
    planes = 1;
    break;
  case SCR_400LINE:
    ofset = 0x8000;
    planes = 1;
    break;
  case SCR_4096:
    ofset = 0x2000;
    planes = 4;
    break;
  case SCR_262144:
    ofset = 0x2000;
    planes = 6;
    break;
  default:
    return;
    break;
  }

  q = dst;
 
  p = (Uint8 *)vram_pb;
  p = &p[src];
  for(i = 0; i < planes; i++){
    memcpy(q, p, w);
    q = &q[ofset];
    p = &p[ofset];
  }

  p = (Uint8 *)vram_pr;
  p = &p[src];
  for(i = 0; i < planes; i++){
    memcpy(q, p, w);
    q = &q[ofset];
    p = &p[ofset];
  }
    //memset(q, 0xff, w);
  p = (Uint8 *)vram_pg;
  p = &p[src];
  for(i = 0; i < planes; i++){
    memcpy(q, p, w);
    //memset(q, 0xff, w);
    q = &q[ofset];
    p = &p[ofset];
  }
}

static void Transfer_1Line(Uint8 *dst, int line)
{
  int top, bottom;
  int src;
  Uint8 *d1, *d2;
  int wdbtm, wdtop;
  int w, h;

  if(SDLDrawFlag.APaletteChanged) {
	 if(bCLEnabled == FALSE) Palet320();
         SDLDrawFlag.APaletteChanged = FALSE;
	 bPaletFlag = FALSE;
   }
   if(SDLDrawFlag.DPaletteChanged) {
	 if(bCLEnabled == FALSE) Palet640();
         SDLDrawFlag.DPaletteChanged = FALSE;
	 bPaletFlag = FALSE;
   }
   switch(bMode) {
   case SCR_200LINE:
     w = 80;
     h = 200;
     break;
   case SCR_400LINE:
     w = 80;
     h = 400;
     break;
   case SCR_4096:
   case SCR_262144:
     w = 40;
     h = 200;
     break;
   default:
     return;
     break;
   }
   if(dst == NULL) return;
   if(bMode == SCR_200LINE) {
     top = nDrawTop >> 1;
     bottom = nDrawBottom >> 1;
   } else {
     top = nDrawTop;
     bottom = nDrawBottom;
   }
   line = line % h;
   if((line < 0) || (line >= 400)) return;
   //SDLDrawFlag.Drawn = TRUE;
   if(vram_dptr == NULL) return;
   if(vram_bdptr == NULL) return;

   d1 = &dst[w * line];
   d2 = d1;
   if((bWindowOpen) && (bMode != SCR_262144)) { // ハードウェアウインドウ開いてる
      if((nDrawTop >= nDrawBottom) && (nDrawLeft >= nDrawRight)) return;	 /* ウィンドウ内の描画 */
	 if (top  > window_dy1) {
	    wdtop = top ;
	 } else {
	    wdtop = window_dy1;
	 }
	 
	 if (bottom < window_dy2) {
	    wdbtm = bottom ;
	 }
	 else {
	    wdbtm = window_dy2;
	 }
         //LockVram();
	 if ((wdbtm > wdtop) && (wdtop < line) && (wdbtm > line)) { // Inside window.
	   /*
	    * Outside of Window
	    */
	    src = line * w;
	    SetVram_200l(vram_dptr);
	    src = w * line;
	    Transfer_Main(d1, src, bMode, window_dx1 >> 3);

	    src = src + (window_dx2 >> 3);
	    d1 = &d1[window_dx2 >> 3];
	    Transfer_Main(d1, src, bMode, w - (window_dx2 >> 3));

	    SetVram_200l(vram_bdptr);
	    src = w * line + (window_dx1 >> 3);
	    d2 = &d2[window_dx1 >> 3];
	    Transfer_Main(d2, src, bMode, (window_dx2 >> 3) - (window_dx1 >> 3));
	 } else if(wdbtm > wdtop) {
	   SetVram_200l(vram_dptr);
	   src = w * line;
	   Transfer_Main(d1, src, bMode, w);
	 }
         //UnlockVram();
      } else { // 256K Colors, not using window.
        SetVram_200l(vram_dptr);
        src = w * line;
	Transfer_Main(d1, src, bMode, w);
	 //UnlockVram();
    }
   bDrawLine[line] = TRUE;
   SDLDrawFlag.Drawn = TRUE;
   return;
}
/*
 *	HBLANK要求通知
 */
void FASTCALL hblank_notify(void)
{
           if((bCLEnabled) && (cldraw != NULL)){
	     if(now_raster >= 400) return;
	     if(bDirtyLine[now_raster]) {
	       Transfer_1Line(cldraw->GetBufPtr(), now_raster);
	       bDirtyLine[now_raster] = FALSE;
	     }

	   } else if (nRenderMethod == RENDERING_RASTER) {
//	   LockVram();
	   if(now_raster >= 400) return;

//	   _prefetch_data_read_l1(bDirtyLine, sizeof(bDirtyLine));
	   if (bDirtyLine[now_raster]) {
//	           _prefetch_data_read_l1(aPlanes, sizeof(aPlanes));
	           Draw_1Line(now_raster);
		}
//	   UnlockVram();
	}
}

/*
 *  ディジタイズ要求通知
 */
void	digitize_notify(void)
{

}
#if XM7_VER >= 3
/*
 *  ハードウェアウィンドウ通知
 */
void window_notify(void)
{
	WORD tmpLeft, tmpRight;
	WORD tmpTop, tmpBottom;
	WORD tmpDx1, tmpDx2;
	WORD tmpDy1, tmpDy2;
	int x, y;
	/*
	 * 26万色モード時は何もしない
	 */
	if (bMode == SCR_262144) {
		return;
	}

	/*
	 * 前もってクリッピングする
	 */
	window_clip(bMode);

	/*
	 * ウィンドウサイズを補正
	 */
	tmpDx1 = window_dx1;
	tmpDy1 = window_dy1;
	tmpDx2 = window_dx2;
	tmpDy2 = window_dy2;
	if (bMode != SCR_400LINE) {
		tmpDy1 <<= 1;
		tmpDy2 <<= 1;
	}
	if (bMode == SCR_4096) {
		tmpDx1 <<= 1;
		tmpDx2 <<= 1;
	}
	if (bWindowOpen != window_open) {
		if (window_open) {

			/*
			 * ウィンドウを開いた場合
			 */
			 tmpLeft = tmpDx1;
			 tmpRight = tmpDx2;
			 tmpTop = tmpDy1;
			 tmpBottom = tmpDy2;
		} else {

			/*
			 * ウィンドウを閉じた場合
			 */
			tmpLeft = nWindowDx1;
			tmpRight = nWindowDx2;
			tmpTop = nWindowDy1;
			tmpBottom = nWindowDy2;
		        bWindowOpen = FALSE;
		}
	} else {
		if (window_open) {

			/*
			 * 更新領域サイズを現在のものに設定
			 */
			tmpTop = nDrawTop;
			tmpBottom = nDrawBottom;
			tmpLeft = nDrawLeft;
			tmpRight = nDrawRight;

			/*
			 * 座標変更チェック
			 */
			 if (!((nWindowDx1 == tmpDx1) &&
					 (nWindowDy1 == tmpDy1) &&
					 (nWindowDx2 == tmpDx2) && (nWindowDy2 == tmpDy2))) {

				 /*
				  * 左上X
				  */
				  if (nWindowDx1 < tmpDx1) {
					  tmpLeft = nWindowDx1;
				  } else {
					  tmpLeft = tmpDx1;
				  }

				  /*
				   * 右下X
				   */
				  if (nWindowDx2 > tmpDx2) {
					  tmpRight = nWindowDx2;
				  } else {
					  tmpRight = tmpDx2;
				  }

				  /*
				   * 左上Y
				   */
				  if (nWindowDy1 < tmpDy1) {
					  tmpTop = nWindowDy1;
				  } else {
					  tmpTop = tmpDy1;
				  }

				  /*
				   * 右下Y
				   */
				  if (nWindowDy2 > tmpDy2) {
					  tmpBottom = nWindowDy2;
				  } else {
					  tmpBottom = tmpDy2;
				  }
			 }
		} else {
		        bWindowOpen = FALSE;
			/*
			 * ウィンドウが開いていないので何もしない
			 */
			return;
		}
	}
   

	/*
	 * 処理前の再描画領域と比較して広ければ領域を更新
	 */
	if (tmpLeft < nDrawLeft) {
		nDrawLeft = tmpLeft;
	}
	if (tmpRight > nDrawRight) {
		nDrawRight = tmpRight;
	}
	if (tmpTop < nDrawTop) {
		nDrawTop = tmpTop;
	}
	if (tmpBottom > nDrawBottom) {
		nDrawBottom = tmpBottom;
	}
	/*
	 * 再描画フラグを更新
	 */
	if((nRenderMethod == RENDERING_RASTER) || (bCLEnabled)){
	    if ((nDrawLeft < nDrawRight) && (nDrawTop < nDrawBottom)) {
	       for(y = nDrawTop; y < nDrawBottom; y++) {
		  bDirtyLine[y] = TRUE;
	       }
	    }
	 } else if ((nDrawLeft < nDrawRight) && (nDrawTop < nDrawBottom)) {
	     for(y = nDrawTop >> 3; y < ((nDrawBottom + 7) >> 3); y++) {
	         for(x = nDrawLeft >> 3; x < (nDrawRight >>3); x ++){
		    SDLDrawFlag.read[x][y] = TRUE;
	         }
	     }
	 }
	 /*
	  * ウィンドウオープン状態を保存
	  */
	 bWindowOpen = window_open;
	 nWindowDx1 = tmpDx1;
	 nWindowDy1 = tmpDy1;
	 nWindowDx2 = tmpDx2;
	 nWindowDy2 = tmpDy2;
}
#endif				/*  */
void OnFullScreen(void)
{
   return; // Full Screenは実装考える 20121128
}
void	OnWindowedScreen(void)
{
   return; // Full Screenは実装考える 20121128
}



/*
 * パレット関連処理
 */

static void Palet640Sub(Uint32 i, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    CalcPalette_8colors((int)i, r, g, b, a);
}

#ifdef __cplusplus
extern "C"
{
#endif

void Palet640(void)
{
	int i;
	int             vpage;
	BYTE          tmp;
	BYTE         g,r,b;
	BYTE         a = 255; // Alpha

	vpage = (~(multi_page >> 4)) & 0x07;
	for (i = 0; i < 8; i++) {
		if (crt_flag) {
			/*
			 * CRT ON
			 */
			tmp = ttl_palet[i & vpage] & 0x07;
			b = ((tmp & 0x01)==0)?0:255;
			r = ((tmp & 0x02)==0)?0:255;
			g = ((tmp & 0x04)==0)?0:255;
			Palet640Sub(i & 7, r, g, b, a);
		} else {
			/*
			 * CRT OFF
			 */
			r = 0;
			g = 0;
			b = 0;
			Palet640Sub(i & 7, r, g, b, a);
		}
	}
	/*
	 * 奇数ライン用
	 */
	Palet640Sub(8, 0, 0, 0, a);
	Palet640Sub(9, 0, 255, 0, a);
}

static void Palet320Sub(Uint32 i, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
//	CalcPalette_AG_GL(rgbAnalogGDI, i, r, g, b, a);
    CalcPalette_4096Colors(i, r, g, b, a);
}

void Palet320(void)
{

	int     i,
	j;
	Uint8   r,
	g,
	b;
	int     amask;

	/*
	 * アナログマスクを作成
	 */
	 amask = 0;
	 if (!(multi_page & 0x10)) {
		 amask |= 0x000f;
	 }
	 if (!(multi_page & 0x20)) {
		 amask |= 0x00f0;
	 }
	 if (!(multi_page & 0x40)) {
		 amask |= 0x0f00;
	 }
//     LockVram();
	 for (i = 0; i < 4096; i++) {
		 /*
		  * 最下位から5bitづつB,G,R
		  */
		  if (crt_flag) {
		     j = i & amask;
		     r = apalet_r[j] <<4;
		     g = apalet_g[j] <<4;
		     b = apalet_b[j] <<4;
		  } else {
		     r = 0;
		     g = 0;
		     b = 0;
		  }
		  Palet320Sub(i, r, g, b, 255);
	 }
//     UnlockVram();

}



#ifdef __cplusplus
}
#endif

/*
 * 描画処理
 */


extern "C"
{

void Draw640All(void)
{
   void (*PutVramFunc)(AG_Surface *, int, int, int, int, Uint32);
   WORD wdtop, wdbtm;
   AG_Surface *p;

   if(agDriverOps == NULL) return;
   p = GetDrawSurface();
   /*
    * パレット設定
    */
   /*
    *描画モードを変えたら強制的にPalet640すること。
    */
   if(bPaletFlag) { // 描画モードでVRAM変更
      LockVram();
      if(bCLEnabled) Palet640();
      bPaletFlag = FALSE;
      nDrawTop = 0;
      nDrawBottom = 400;
      nDrawLeft = 0;
      nDrawRight = 640;
      SetDrawFlag(TRUE);
      UnlockVram();
   }
   if(SDLDrawFlag.APaletteChanged) {
	 if(bCLEnabled == FALSE) Palet320();
         SDLDrawFlag.APaletteChanged = FALSE;
         bPaletFlag = TRUE;
   }
   if(SDLDrawFlag.DPaletteChanged) {
	 if(bCLEnabled == FALSE) Palet640();
         SDLDrawFlag.DPaletteChanged = FALSE;
         bPaletFlag = TRUE;
   }
   /*
    * クリア処理
    */
   if (bClearFlag) {
      AllClear();
   }
   if(nRenderMethod == RENDERING_FULL) {
      LockVram();
      SetDrawFlag(TRUE);
      PutVramFunc = &PutVram_AG_SP;
      UnlockVram();
   } else if(nRenderMethod == RENDERING_RASTER) {
      return;
   } else if(nRenderMethod == RENDERING_BLOCK) {
     PutVramFunc = &PutVram_AG_SP;
   } else {
     PutVramFunc = &PutVram_AG_SP;
   }
   
   
   /*
    * レンダリング
    */
   if(PutVramFunc == NULL) return;
   if((nDrawTop < nDrawBottom) && (nDrawLeft < nDrawRight)) {
      if(bWindowOpen) { // ハードウェアウインドウ開いてる
	 if ((nDrawTop >> 1) < window_dy1) {
	    SetVram_200l(vram_dptr);
	    PutVramFunc(p, 0, nDrawTop >> 1, 640, window_dy1, multi_page);
	 }
	 /* ウィンドウ内の描画 */
	 if ((nDrawTop >> 1) > window_dy1) {
	    wdtop = nDrawTop >> 1;
	 } else {
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
	    PutVramFunc(p, window_dx1, wdtop, window_dx2 - window_dx1 , wdbtm - wdtop , multi_page);
	 }
	 /* ハードウェアウインドウ外下部 */
	 if ((nDrawBottom >> 1) > window_dy2) {
	    //	vramhdr->SetVram(vram_dptr, 80, 200);
	    SetVram_200l(vram_dptr);
	    PutVramFunc(p, 0 , wdbtm, 640, (nDrawBottom >> 1) - wdbtm, multi_page);
	 }
      } else { // ハードウェアウィンドウ開いてない
	 //	vramhdr->SetVram(vram_dptr, 80, 200);
	 SetVram_200l(vram_dptr);
	 PutVramFunc(p, 0, 0, 640, 200, multi_page);
      }
   }
}



void Draw_1Line(int line)
{
   WORD wdtop, wdbtm;
   Uint32 *pp;
   int ww;
   int hh;
   int top;
   int bottom;
   pp = pVram2;

   bDirtyLine[line] = FALSE;
   if(pp == NULL) return;
   if(bPaletFlag) {
     if(bCLEnabled == FALSE) {
	Palet320();
        Palet640();
     }
     bPaletFlag = FALSE;
   }
   if(SDLDrawFlag.APaletteChanged) {
     //	 Palet320();
         SDLDrawFlag.APaletteChanged = FALSE;
   }
   if(SDLDrawFlag.DPaletteChanged) {
     //	 Palet640();
         SDLDrawFlag.DPaletteChanged = FALSE;
   }

   
   switch(bMode) {
    case SCR_200LINE:
      ww = 80;
      hh = 200;
      top = nDrawTop >> 1;
      bottom = nDrawBottom >> 1;
      _prefetch_data_read_permanent(rgbTTLGDI, sizeof(Uint32) * 8);
      break;
    case SCR_400LINE:
      ww = 80;
      hh = 400;
      top = nDrawTop;
      bottom = nDrawBottom;
      _prefetch_data_read_permanent(rgbTTLGDI, sizeof(Uint32) * 8);
      break;
    case SCR_4096:
      ww = 40;
      hh = 200;
      top = nDrawTop;
      bottom = nDrawBottom;
      _prefetch_data_read_permanent(rgbAnalogGDI, sizeof(Uint32) * 4096);
      break;
    case SCR_262144:
      ww = 40;
      hh = 200;
      top = nDrawTop;
      bottom = nDrawBottom;
      break;
   }
   line = line % hh;
   if((line < 0) || (line >= hh)) return;
   if(vram_dptr == NULL) return;
   if(vram_bdptr == NULL) return;
#ifdef _USE_OPENCL
   if((bCLEnabled)) {
      SDLDrawFlag.Drawn = TRUE;
      bDrawLine[line] = TRUE;
      return;
   }
#endif
   if((bWindowOpen) && (bMode != SCR_262144)) { // ハードウェアウインドウ開いてる
      if((nDrawTop >= nDrawBottom) && (nDrawLeft >= nDrawRight)) return;	 /* ウィンドウ内の描画 */
	 if (top  > window_dy1) {
	    wdtop = top ;
	 } else {
	    wdtop = window_dy1;
	 }
	 
	 if (bottom < window_dy2) {
	    wdbtm = bottom ;
	 }
	 else {
	    wdbtm = window_dy2;
	 }
         //LockVram();
	 if ((wdbtm > wdtop) && (wdtop < line) && (wdbtm > line)) { // Inside window.
	    SetVram_200l(vram_bdptr);
	    BuildVirtualVram_RasterWindow(pp, window_dx1 >> 3 , window_dx2 >> 3, line, multi_page);
	    SetVram_200l(vram_dptr);
	    BuildVirtualVram_RasterWindow(pp, 0, window_dx1 >> 3 , line, multi_page);
	    BuildVirtualVram_RasterWindow(pp, window_dx2 >> 3 , ww, line, multi_page);
	 } else if(wdbtm > wdtop) {
	    SetVram_200l(vram_dptr);
	    BuildVirtualVram_Raster(pp, line, multi_page);
	 }
         //UnlockVram();
      } else { // ハードウェアウィンドウ開いてない
	 //LockVram();
	 SetVram_200l(vram_dptr);
	 BuildVirtualVram_Raster(pp,  line,  multi_page);
	 //UnlockVram();
    }
   SDLDrawFlag.Drawn = TRUE;
   bDrawLine[line] = TRUE;
   return;
}
   


void Draw400l(void)
{

   void (*PutVramFunc)(AG_Surface *, int, int, int, int, Uint32);
   WORD wdtop, wdbtm;
   AG_Surface *p;
   if(agDriverOps == NULL) return;
   p = GetDrawSurface();
   /*
    * パレット設定
    */
   if(SDLDrawFlag.APaletteChanged) {
         if(bCLEnabled == FALSE) Palet320();
         SDLDrawFlag.APaletteChanged = FALSE;
         bPaletFlag = TRUE;
   }
   if(SDLDrawFlag.DPaletteChanged) {
	 if(bCLEnabled == FALSE) Palet640();
         SDLDrawFlag.DPaletteChanged = FALSE;
         bPaletFlag = TRUE;
   }
   if (bPaletFlag) {
      LockVram();
//      Palet640();
      nDrawTop = 0;
      nDrawBottom = 400;
      nDrawLeft = 0;
      nDrawRight = 640;
      SetDrawFlag(TRUE);
      bPaletFlag = FALSE;
      UnlockVram();
   }

   /*
    * クリア処理
    */
   if (bClearFlag) {
      AllClear();
   }
   
   /*
    * レンダリング
    */
   if(nRenderMethod == RENDERING_FULL) {
      LockVram();
      SetDrawFlag(TRUE);
      PutVramFunc = &PutVram_AG_SP;
      UnlockVram();
   } else if(nRenderMethod == RENDERING_RASTER) {
      return;
   } else if(nRenderMethod == RENDERING_BLOCK) {
     PutVramFunc = &PutVram_AG_SP;
   } else {
     PutVramFunc = &PutVram_AG_SP;
   }
   
   
   /*
    * レンダリング
    */
   if(PutVramFunc == NULL) return;
   if((nDrawTop < nDrawBottom) && (nDrawLeft < nDrawRight)) {
      if(bWindowOpen) { // ハードウェアウインドウ開いてる
	 if ((nDrawTop >> 1) < window_dy1) {
	    SetVram_200l(vram_dptr);
	    PutVramFunc(p, 0, nDrawTop, 640, window_dy1, multi_page);
	 }
	 /* ウィンドウ内の描画 */
	 if (nDrawTop > window_dy1) {
	    wdtop = nDrawTop;
	 } else {
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
	    PutVramFunc(p, window_dx1, wdtop, window_dx2 - window_dx1 , wdbtm - wdtop , multi_page);
	 }
	 /* ハードウェアウインドウ外下部 */
	 if (nDrawBottom > window_dy2) {
	    //	vramhdr->SetVram(vram_dptr, 80, 200);
	    SetVram_200l(vram_dptr);
	    PutVramFunc(p, 0 , wdbtm, 640, nDrawBottom  - wdbtm, multi_page);
	 }
      } else { // ハードウェアウィンドウ開いてない
	 //	vramhdr->SetVram(vram_dptr, 80, 200);
	 SetVram_200l(vram_dptr);
	 PutVramFunc(p, 0, 0, 640, 400, multi_page);
      }
   }

}


void Draw320(void)
{
	void (*PutVramFunc)(AG_Surface *, int, int, int, int, Uint32);
	AG_Surface *p;
	WORD wdtop, wdbtm;

	if(agDriverOps == NULL) return;
	p = GetDrawSurface();
   /*
    * パレット設定
    */
   PutVramFunc = &PutVram_AG_SP;
   if(SDLDrawFlag.APaletteChanged) {
	 if(bCLEnabled == FALSE) Palet320();
         SDLDrawFlag.APaletteChanged = FALSE;
         bPaletFlag = TRUE;
   }
   if(SDLDrawFlag.DPaletteChanged) {
	 if(bCLEnabled == FALSE) Palet640();
         SDLDrawFlag.DPaletteChanged = FALSE;
         bPaletFlag = TRUE;
   }
   if(bPaletFlag) {
      LockVram();
//      Palet320();
      nDrawTop = 0;
      nDrawBottom = 200;
      nDrawLeft = 0;
      nDrawRight = 320;
//      SDLDrawFlag.APaletteChanged = TRUE;
      SetDrawFlag(TRUE);
      bPaletFlag = FALSE;
      UnlockVram();
   }
   /*
    * クリア処理
    */
   if (bClearFlag) {
      AllClear();
   }
   /*
    * レンダリング
    */
   /*
    * レンダリング
    */
   if(nRenderMethod == RENDERING_FULL) {
      LockVram();
      SetDrawFlag(TRUE);
      PutVramFunc = &PutVram_AG_SP;
      UnlockVram();
   } else if(nRenderMethod == RENDERING_RASTER) {
      return;
   } else if(nRenderMethod == RENDERING_BLOCK) {
     PutVramFunc = &PutVram_AG_SP;
   } else {
     PutVramFunc = &PutVram_AG_SP;
   }
   
   if(PutVramFunc == NULL) return;
   if((nDrawTop < nDrawBottom) && (nDrawLeft < nDrawRight)) {
      if(bWindowOpen) { // ハードウェアウインドウ開いてる
	 if (nDrawTop < window_dy1) {
            SetVram_200l(vram_dptr);
	    PutVramFunc(p, 0, nDrawTop, 320, window_dy1, multi_page);
	 }
	 /* ウィンドウ内の描画 */
	 if (nDrawTop > window_dy1) {
	    wdtop = nDrawTop;
	 } else {
	    wdtop = window_dy1;
	 }
	 
	 if (nDrawBottom < window_dy2) {
	    wdbtm = nDrawBottom;
	 } else {
	    wdbtm = window_dy2;
	 }
	 
	 if (wdbtm > wdtop) {
	    SetVram_200l(vram_bdptr);
	    PutVramFunc(p, window_dx1, wdtop, window_dx2 - window_dx1 , wdbtm - wdtop , multi_page);
	 }
	 /* ハードウェアウインドウ外下部 */
	 if (nDrawBottom  > window_dy2) {
	    SetVram_200l(vram_dptr);
	    PutVramFunc(p, 0 , wdbtm, 320, nDrawBottom - wdbtm, multi_page);
	 }
      } else { // ハードウェアウィンドウ開いてない
	 SetVram_200l(vram_dptr);
	 PutVramFunc(p, 0, 0, 320, 200, multi_page);
      }
   }
}


void Draw256k(void)
{
   AG_Surface *p;
   void (*PutVramFunc)(AG_Surface *, int, int, int, int, Uint32);
   if(agDriverOps == NULL) return;
   p = GetDrawSurface();

   if(nRenderMethod == RENDERING_FULL) {
      LockVram();
      SetDrawFlag(TRUE);
      PutVramFunc = &PutVram_AG_SP;
      UnlockVram();
   } else if(nRenderMethod == RENDERING_RASTER) {
      return;
   } else if(nRenderMethod == RENDERING_BLOCK) {
     PutVramFunc = &PutVram_AG_SP;
   } else {
     PutVramFunc = &PutVram_AG_SP;
   }
   LockVram();
   nDrawTop = 0;
   nDrawBottom = 200;
   nDrawLeft = 0;
   nDrawRight = 320;
   bPaletFlag = FALSE;
   UnlockVram();
   //    SetDrawFlag(TRUE);
   // 
   /*
    * クリア処理
    */
   if (bClearFlag) {
      AllClear();
   }
   if(SDLDrawFlag.APaletteChanged) {
	 if(bCLEnabled == FALSE) Palet320();
         SDLDrawFlag.APaletteChanged = FALSE;
//         bPaletFlag = TRUE;
   }
   if(SDLDrawFlag.DPaletteChanged) {
	 if(bCLEnabled == FALSE) Palet640();
         SDLDrawFlag.DPaletteChanged = FALSE;
//         bPaletFlag = TRUE;
   }
   
   /*
    * レンダリング
    */
   if(PutVramFunc == NULL) return;
   /*
    * 26万色モードの時は、ハードウェアウィンドウを考慮しない。
    */
   PutVramFunc(p, 0, 0, 320, 200, multi_page);
}

}

