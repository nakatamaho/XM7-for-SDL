/*
 * 
 * INIファイルアクセス ]
 */  
    
    
#ifndef _xw_inifile_h_
#define _xw_inifile_h_
    
    /*
     * 
     */
#define MAX_LEN 256
#define MAX_ENTRIES 256
    
    /*
     * 
     */
#ifdef __cplusplus
extern "C" {
#endif
void            INI_init(char *inifile);

    /*
     * 
     */ 
    BOOL INI_load(void);

    /*
     * 
     */ 
    BOOL INI_save(void);

    /*
     * 
     */ 
char           *INI_get(char *section, char *key);

    /*
     * 
     */ 
void            INI_set(char *section, char *key, char *value);

    /*
     * 
     */ 
void            INI_clearSection(char *section);

    /*
     * 
     */ 
void            INI_clearKey(char *section, char *key);

    /*
     * 
     */ 
    BOOL INI_getBool(char *section, char *key, BOOL value);

    /*
     * 
     */ 
void            INI_setBool(char *section, char *key, BOOL value);

    /*
     * 
     */ 
int             INI_getInt(char *section, char *key, int defvalue);

    /*
     * 
     */ 
void            INI_setInt(char *section, char *key, int value);

    /*
     * 
     */ 
char           *INI_getString(char *section, char *key, char *defvalue);

    /*
     * 
     */ 
void            INI_setString(char *section, char *key, char *value);
#ifdef __cplusplus
}
#endif
#endif	/* _xw_inifile_h_ */