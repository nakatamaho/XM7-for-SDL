/*
 * agar_gui_tapemenu.cpp
 *
 *  Created on: 2010/11/24
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
#include "tapelp.h"


#ifdef USE_AGAR
#include "agar_xm7.h"
#include "agar_cfg.h"
#else
#include "xm7_sdl.h"
#include "sdl_cfg.h"
#endif

#include "agar_toolbox.h"

extern void OnPushCancel(AG_Event *event);

static void OnOpenTapeSub(char *sFilename)
{
	char *p;
    /*
     * セット
     */
	LockVM();
    tape_setfile(sFilename);
    ResetSch();
    UnlockVM();
#ifdef _WINDOWS
    p = _mbsrchr(sFilename, '\\');
#else
    p = strrchr(sFilename, '/');
#endif   
    if (p != NULL) {
	p[1] = '\0';
	strcpy(InitialDir[1], sFilename);
    }
}


static void OnOpenTapeSubEv(AG_Event *event)
{
    AG_FileDlg *dlg = (AG_FileDlg *)AG_SELF();
    char  *sFilename = AG_STRING(1);
    AG_MenuItem *parent;
    OnOpenTapeSub(sFilename);
}


static void OnTapeOpen(AG_Event *event)
{
	AG_MenuItem *self = (AG_MenuItem *)AG_SELF();
	AG_Window *dlgWin;
	AG_FileDlg *dlg;
        char s[MAXPATHLEN + 1];

	dlgWin = AG_WindowNew(DIALOG_WINDOW_DEFAULT);
	if(dlgWin == NULL) return;
	AG_WindowSetCaption(dlgWin, "%s", gettext("Open Tape Image"));
    dlg = AG_FileDlgNew(dlgWin, FILEDLG_DEFAULT | AG_FILEDLG_LOAD);
	if(dlg == NULL) return;
   if(InitialDir[1] != NULL) {
      strcpy(s, InitialDir[1]);
   } else {
      strcpy(s, ".");
   }
#ifdef _WINDOWS
     { // Subst '\\' to '/', needed AG_FileDlgSetDirectory().
	int i;
	i = 0; 
	while((s[i] != '\0') && (i <= MAXPATHLEN)) {
	   if(s[i] == '\\') s[i] = '/';
	   i++;
	}
     }
#endif   /* _WINDOWS */
	AG_FileDlgSetDirectory (dlg, "%s", s);
	AG_WidgetFocus(dlg);
	AG_FileDlgAddType(dlg, "T77 CMT Image File", "*.t77,*.T77", OnOpenTapeSubEv, NULL);
    AG_ActionFn(AGWIDGET(dlgWin), "window-close", OnPushCancel, NULL);
    AG_ActionFn(AGWIDGET(dlg), "window-close", OnPushCancel, NULL);

    AG_FileDlgCancelAction (dlg, OnPushCancel,NULL);

    AG_WindowShow(dlgWin);

}

/*
 *  テープイジェクト
 */
static void OnTapeEject(AG_Event *event)
{
/*
 * イジェクト
 */
        LockVM();
        tape_setfile(NULL);
        UnlockVM();
}
/*
 *  巻き戻し
 */
static void OnRew(AG_Event *event)
{

/*
 * 巻き戻し
 */
        LockVM();
        StopSnd();
        tape_rew();
        PlaySnd();
        ResetSch();
        UnlockVM();
}
/*
 *  早送り
 */
static void OnFF(AG_Event *event)
{

/*
 * 巻き戻し
 */
    LockVM();
    StopSnd();
    tape_ff();
    PlaySnd();
    ResetSch();
    UnlockVM();
}

/*
 *  録音
 */
static void OnRec(AG_Event *event)
{
	AG_Button *self = (AG_Button *)AG_SELF();
	int flag = AG_INT(1);
/*
 * 録音
 */
        LockVM();
        if (flag) {
                tape_setrec(TRUE);
        } else {
                tape_setrec(FALSE);
        }
        UnlockVM();

    	AG_WindowHide(self->wid.window);
    	AG_ObjectDetach(self->wid.window);
}

/*
 * 書き込み保護
 */
static void OnWriteProtectTape(AG_Event *event)
{
   AG_Menu *parent = AG_SELF();
   AG_MenuItem *my = AG_SENDER();
   BOOL flag = (BOOL)AG_INT(1);
   tape_writep = flag;
}

static void OnTapeRec(AG_Event *event)
{
	AG_Window *dlgWin;

	AG_Menu *self = (AG_Menu *)AG_SELF();
	AG_MenuItem *item = (AG_MenuItem *)AG_SENDER();
	AG_Window *w;
	AG_Button   *btn;
	AG_Box *box;
	AG_Box *box2;
	char *caption;
	AG_Label *lbl;
	int id;

	if(tape_fname[0] == '\0') {
		caption = gettext("NO Tape Exists");
	} else 	if(tape_writep) {
		caption = gettext("Tape: Write Protected");
	} else {
		caption = gettext("Recording to Virtual Tape");
	}

	w = AG_WindowNew(AG_WINDOW_NOMINIMIZE | AG_WINDOW_NOMAXIMIZE | FILEDIALOG_WINDOW_DEFAULT);
	AG_WindowSetMinSize(w, 230, 80);
	box = AG_BoxNewHorizNS(w, AG_BOX_HFILL);
	AG_WidgetSetSize(box, 230, 32);
	lbl = AG_LabelNew(AGWIDGET(box), AG_LABEL_EXPAND, "%s", caption );
	box = AG_BoxNewHoriz(w, AG_BOX_HFILL);
	AG_WidgetSetSize(box, 230, 8);
	box = AG_BoxNewHoriz(w, AG_BOX_HFILL);
	AG_WidgetSetSize(box, 230, 32);
	box2 = AG_BoxNewVert(box, 0);
	if(tape_writep || (tape_fname[0] == '\0')) {
		btn = AG_ButtonNewFn (AGWIDGET(box2), 0, gettext("OK"), OnPushCancel, NULL);
	} else {
	   if (!tape_rec) {
	      btn = AG_ButtonNewFn (AGWIDGET(box2), 0, gettext("Start Recording"), OnRec, "%i", TRUE);
	   } else {
	      btn = AG_ButtonNewFn (AGWIDGET(box2), 0, gettext("Stop Recording"), OnRec, "%i", FALSE);
	   }
	   box2 = AG_BoxNewVert(box, 0);
	   box2 = AG_BoxNewVert(box, 0);
	   btn = AG_ButtonNewFn (AGWIDGET(box2), 0, gettext("Cancel"), OnPushCancel, NULL);
	}
	AG_WindowSetCaption(w, gettext("Recording to Tape"));
	AG_WindowShow(w);

}

void Create_TapeMenu(AG_MenuItem *self)
{
	AG_MenuItem *item;
	AG_MenuItem *subitem;
	AG_Toolbar *toolbar;

	item = AG_MenuAction(self, gettext("Open"), NULL, OnTapeOpen,NULL);
	AG_MenuSeparator(self);
	item = AG_MenuAction(self, gettext("Eject"), NULL, OnTapeEject, NULL);
	AG_MenuSeparator(self);
	/*
	 * ライトプロテクト
	 */
        item = AG_MenuNode(self, gettext("Write Protect"), NULL); 
        AG_MenuAction(item, gettext("ON"),  NULL, OnWriteProtectTape, "%i", TRUE);
        AG_MenuAction(item, gettext("OFF"), NULL, OnWriteProtectTape, "%i", FALSE);
        AG_MenuToolbar(item, NULL);
	/*
	 * 巻き戻し
	 */
	AG_MenuSeparator(self);
	item = AG_MenuAction(self, gettext("Rewind"), NULL, OnRew, NULL);
	item = AG_MenuAction(self, gettext("Fast Forward"), NULL, OnFF, NULL);
//	item = AG_MenuAction(self, gettext("Tape Reset"), NULL, OnResetTape, NULL);
	AG_MenuSeparator(self);
	item = AG_MenuAction(self, gettext("Record Tape"), NULL, OnTapeRec, NULL);
}

