/*
 * AgarKbdInterface.h
 *
 *  Created on: 2010/11/09
 *      Author: whatisthis
 */

#ifndef AGARKBDINTERFACE_H_
#define AGARKBDINTERFACE_H_

#include "SDLKbdInterface.h"
#include <agar/core/types.h>
#include <agar/core.h>
#include <agar/gui.h>

struct KeyCode2_AG {
	AG_KeySym sym;
	Uint8 code;
};


class AgarKbdInterface: public SDLKbdInterface {
public:
	AgarKbdInterface();
	virtual ~AgarKbdInterface();
	void OnPress(int sym, int mod, Uint32 unicode);
	void OnRelease(int sym, int mod, Uint32 unicode);
	void InitKeyTable(void);
	void ResetKeyMap(void);
	void LoadKeyTable(void *pMap);
	void SaveKeyTable(void *pMap);
	void GetKeyCode(Uint8 code, XM7KeyCode *p);
	void SetKeyCode(Uint8 code, Uint32 sym, Uint32 mod);
private:
	struct XM7KeyCode KeyCodeTable2[256];
	void InitLocalVar(void);
//	BOOL kbd_snooped;
	struct SpecialKey ResetKey;
	struct SpecialKey MouseCapture;
	struct SpecialKey ToggleMenu;
	AG_Mutex mutex;
	Uint32 oldsym;
	Uint32 oldmod;
};

#endif /* AGARKBDINTERFACE_H_ */
