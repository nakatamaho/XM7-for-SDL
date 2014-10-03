/*
 * agar_gui_pref.cpp
 *
 * Preferences Page
 *
 *  Created on: 2010/11/25
 *      Author: whatisthis
 */

#include <libintl.h>
#include <agar/core.h>
#include <agar/core/types.h>
#include <agar/gui.h>
#include <SDL/SDL.h>

#include "xm7.h"
#include "device.h"
#include "fdc.h"
#include "tapelp.h"
#include "opn.h"
#include "keyboard.h"
#include "mmr.h"
#include "mouse.h"
#include "aluline.h"
#include "multipag.h"
#include "ttlpalet.h"
#include "apalet.h"
#include "subctrl.h"
#include "display.h"
#include "device.h"

#ifdef USE_AGAR
#include "agar_xm7.h"
#else
#include "xm7_sdl.h"
#endif

#include "sdl_inifile.h"
#include "agar_cfg.h"
#include "xm7.h"

configdat_t localconfig;


extern void OnPushCancel2(AG_Event *event);
extern void ConfigMenuOpenCL(configdat_t *cfg, AG_NotebookTab *parent);

void OnConfigApplyMain(configdat_t *p, AG_Button *self)
{

        int ver;
	LockVM();
        
	memcpy(&configdat, p, sizeof(configdat_t));
	ver = fm7_ver;
	ApplyCfg();
	/*
	 * VMヴァージョンが違ったら強制リセット
	 */
	if(ver != fm7_ver){
		system_reset();
	}
	/*
	 * ここにアイコン変更入れる
	 */

	/*
	 * 終了処理
	 */
	UnlockVM();
	AG_WindowHide(self->wid.window);
	AG_ObjectDetach(self);
//	AG_WindowDetach(self->wid.window);
}


void OnConfigApply(AG_Event *event)
{
	int ver;
        int i;
	AG_Button *self = (AG_Button *)AG_SELF();

        OnConfigApplyMain(&localconfig, self);

}



void OnConfigApply2(AG_Event *event)
{
	int ver;
        int i;
	AG_Button *self = (AG_Button *)AG_SELF();
        configdat_t *cfg = AG_PTR(1);

        if(cfg == NULL) return;
         OnConfigApplyMain(cfg, self);
        free(cfg);
}



static const char *EmuTypeName[] =
{
		"FM-7",
		"FM-77AV",
		"FM-77AVEX",
		NULL
};
enum EmuTypeNum  {
		FM7=1,
		FM77AV,
		FM77AVEX
};

static const char *EmuSpeedName[] =
{
		"SLOW Mode",
		"FAST Mode",
		NULL
};
enum EmuSpeedNum  {
		EMUSP_SLOW=0,
		EMUSP_FAST,
};
static const char *EmuCycleName[] =
{
		"Main CPU",
		"Main CPU using MMR",
		"Main CPU using MMR Fast",
		"Sub CPU",
		NULL
};

enum EmuCycleNum  {
	EMUCYCLE_MAIN = 0,
	EMUCYCLE_MAIN_MMR,
	EMUCYCLE_MAIN_FASTMMR,
	EMUCYCLE_SUB
};


static void OnSetEmulationMode(AG_Event *event)
{
        configdat_t *cfg = AG_PTR(1);
	int number = AG_INT(2);

        if(cfg == NULL) return;
	cfg->fm7_ver = number + 1;
}

static void OnSetCyclestealMode(AG_Event *event)
{
        configdat_t *cfg = AG_PTR(1);
   	int number = AG_INT(2);

        if(cfg == NULL) return;
	cfg->cycle_steal = number;
}

void ConfigMenuEmulation(configdat_t *cfg, AG_NotebookTab *parent)
{
	AG_Radio *radio;
	AG_Checkbox *check;
	AG_Box *box;
	AG_Label *lbl;
        int n;

	box = AG_BoxNewVert(AGWIDGET(parent), 0);
	{
//	cfg->EmuVMTypeSelected = cfg->localconfig.fm7_ver - 1;
	n = cfg->fm7_ver - 1;
	lbl = AG_LabelNew(AGWIDGET(box), 0, gettext("Emulation Type"));
	radio = AG_RadioNewFn(AGWIDGET(box), 0, EmuTypeName, OnSetEmulationMode, "%p", cfg);
	AG_SetInt(radio, "value", n);
//	AG_BindInt(radio, "value", &(cfg->EmuVMTypeSelected));

//	cfg->EmuCyclestealMode = cfg->localconfig.cycle_steal;
	n = cfg->cycle_steal;
	lbl = AG_LabelNew(AGWIDGET(box), 0, gettext("Cycle Steal"));
	radio = AG_RadioNewFn(AGWIDGET(box), 0, EmuSpeedName, OnSetCyclestealMode, "%p", cfg);
	AG_SetInt(radio, "value", n);
//	AG_BindInt(radio, "value", &(cfg->EmuCyclestealMode));
	}

	box = AG_BoxNewVert(AGWIDGET(parent), 0);
	{
		check = AG_CheckboxNewInt (AGWIDGET(box), 0, gettext("Auto Speed Adjust"), &(cfg->bSpeedAdjust));
		check = AG_CheckboxNewInt (AGWIDGET(box), 0, gettext("Full Speed"), &(cfg->bCPUFull));
		check = AG_CheckboxNewInt (AGWIDGET(box), 0, gettext("Disable Speed adjust when motor on"), &(cfg->bTapeMode));
		check = AG_CheckboxNewInt (AGWIDGET(box), 0, gettext("Full Speed adjust when motor on"), &(cfg->bTapeFull));
		check = AG_CheckboxNewInt (AGWIDGET(box), 0, gettext("Wait on accessing FDD"), &(cfg->bFddWait));
	}

}


static void OnResetCycles(AG_Event *event)
{
   AG_Widget *self = AG_SELF();
   configdat_t *cfg = AG_PTR(1);
   
   if(cfg == NULL) return;
   cfg->main_speed = MAINCYCLES;
   cfg->sub_speed = SUBCYCLES;
   cfg->mmr_speed = MAINCYCLES_MMR;
   cfg->fmmr_speed = MAINCYCLES_FMMR;

//   printf("Reset!\n");
}


static void CheckCycleRange(AG_Event *event)
{
   AG_Numerical *self = AG_SELF();
   Uint32 *bind = AG_PTR(1);
   if(bind == NULL) return;
   if(self == NULL) return;
   
   if(*bind < 2) *bind = 2;
   if(*bind > 9999) *bind = 9999;
   AG_NumericalUpdate(self);
}


static AG_Numerical *MakeCycleDialog(AG_Widget *parent, char *label, Uint32 *bind)
{
   AG_Numerical *r;
   r = AG_NumericalNewUintR(parent, 
			    AG_NUMERICAL_INT, gettext("cycles"), 
			    label, bind, 2, 9999);
   if(r == NULL) return NULL;
   AG_AddEvent(r, "numerical-return", CheckCycleRange, "%p", bind);
   AG_WidgetShow(r);
}


void ConfigMenuVMSpeed(configdat_t *cfg, AG_NotebookTab *parent)
{
	AG_Box *box;
	AG_Label *lbl;
	AG_Button *btn;
        AG_Event *ev;
        AG_Numerical *main, *sub, *mmr, *fmmr;

	box = AG_BoxNewVert(AGWIDGET(parent), AG_BOX_VFILL);
        main = MakeCycleDialog(AGWIDGET(box), gettext("Main CPU"), &(cfg->main_speed));
        sub  = MakeCycleDialog(AGWIDGET(box), gettext("Sub CPU"), &(cfg->sub_speed));
        mmr  = MakeCycleDialog(AGWIDGET(box), gettext("Main MMR"), &(cfg->mmr_speed));
        fmmr = MakeCycleDialog(AGWIDGET(box), gettext("Main CPU Fast MMR"), &(cfg->fmmr_speed));
   
	btn = AG_ButtonNewFn(AGWIDGET(parent), 0, gettext("Reset Default"), OnResetCycles, "%p", cfg);
}


void ConfigMenuVMConfig(configdat_t *cfg, AG_NotebookTab *parent)
{

   AG_Box *box;
   AG_Label *lbl;
   AG_Button *btn;
   AG_Event *ev;
   
}

void OnConfigEmulationMenu(AG_Event *event)
{
   AG_MenuItem *self = (AG_MenuItem *)AG_SELF();
   AG_Window *win;
   AG_Notebook *note;
   AG_Notebook *note2;

   AG_NotebookTab *tab;
   AG_NotebookTab *tab2;
   AG_Box *box;
   AG_Button *btn;
   configdat_t *p;
   
   p = malloc(sizeof(configdat_t));
   if(p == NULL) return;
   memcpy(p, &configdat, sizeof(configdat_t));

    win= AG_WindowNew(DIALOG_WINDOW_DEFAULT);
    note = AG_NotebookNew(AGWIDGET(win), AG_NOTEBOOK_HFILL);
    {
    	/*
    	 * 
    	 */
    	tab = AG_NotebookAddTab(note, gettext("Emulation"), AG_BOX_HORIZ);
    	ConfigMenuEmulation(p, tab);

    	tab = AG_NotebookAddTab(note, gettext("Cycle"), AG_BOX_HORIZ);
    	ConfigMenuVMSpeed(p, tab);

    	tab = AG_NotebookAddTab(note, gettext("VM"), AG_BOX_HORIZ);
    	ConfigMenuVMConfig(p, tab);
    }
   
    box = AG_BoxNewHoriz(AGWIDGET(win), AG_BOX_HFILL);
    AG_WidgetSetSize(AGWIDGET(box), 320, 24);
    {
    	AG_Box *vbox;
        vbox = AG_BoxNewVert(AGWIDGET(box), AG_BOX_VFILL);
    	btn = AG_ButtonNewFn(AGWIDGET(box), 0, gettext("OK"), OnConfigApply2, "%p", p);
        vbox = AG_BoxNewVert(AGWIDGET(box), AG_BOX_VFILL);
        AG_WidgetSetSize(AGWIDGET(vbox), 80, 24);
        vbox = AG_BoxNewVert(AGWIDGET(box), AG_BOX_VFILL);
    	btn = AG_ButtonNewFn(AGWIDGET(box), 0, gettext("Cancel"), OnPushCancel2, "%p", p);
    }
	AG_WindowSetCaption(win, gettext("Preferences"));
	AG_WindowShow(win);
}

