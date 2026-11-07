/***
*
*	Rocket Crowbar 1.9: Preff.cpp, the rc.ini server prefs (sv_prefsfile).
*
*	rc.ini is "[HEADING]" lines followed by "Key=Value" lines. The whole file is held in
*	g_pHeadings, both levels sorted case-insensitively so lookups can binary search. Every
*	PREFS_KeyLoopUp* call creates the key with its default when it is missing, so writing the
*	table back out (PREFS_WriteFile) gives a complete rc.ini.
*
****/

#pragma once

// One "Key=Value" line. Both strings are malloc'd. 8 bytes in the original.
// VERIFY name: the original's struct names are not in the binary.
struct sPrefKey
{
	char* szKey;   // orig +0x0
	char* szValue; // orig +0x4
};

// One "[HEADING]" and its keys. 12 bytes in the original. VERIFY name, as sPrefKey.
struct sPrefHeading
{
	sPrefKey* pKeys; // orig +0x0, malloc'd array of iCount
	int iCount;		 // orig +0x4
	char* szName;	 // orig +0x8, malloc'd
};

extern sPrefHeading* g_pHeadings; // malloc'd array of g_iCountHeadings
extern int g_iCountHeadings;

int PREFS_BinSearchHead(const char* szHeading);
int PREFS_BinSearchKey(const char* szKey, int iHeading);
bool PREFS_SearchAll(const char* szHeading, const char* szKey, int* piHeading, int* piKey);
extern "C" DLLEXPORT char* PREFS_LookUpKey(const char* szHeading, const char* szKey);
const char* PREFS_LookUpWithDefualtCreate(const char* szHeading, const char* szKey, const char* szDefault);
void PREFS_SortAll();
int PREFS_AddHeading(const char* szHeading);
void PREFS_SetKey(int iHeading, int iKey, const char* szValue);
int PREFS_AddKey(const char* szKey, const char* szValue, int iHeading);
bool PREFS_LoadFile(const char* szFile);
bool PREFS_WriteFile(const char* szFile);

const char* PREFS_KeyLoopUpString(const char* szHeading, const char* szKey, const char* szDefault);
int PREFS_KeyLoopUpInt(const char* szHeading, const char* szKey, int iDefault);
float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault);
int PREFS_KeyLoopUpIntClamp(const char* szHeading, const char* szKey, int iDefault, int iMin, int iMax);
float PREFS_KeyLoopUpFloatClamp(const char* szHeading, const char* szKey, float flDefault, float flMin, float flMax);
