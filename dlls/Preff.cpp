/***
*
*	Rocket Crowbar 1.9: Preff.cpp, the rc.ini server prefs reader (Preff.h).
*
****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "Preff.h"

sPrefHeading* g_pHeadings = NULL;
int g_iCountHeadings = 0;

// orig 0xe1350
int PREFS_BinSearchHead(const char* szHeading)
{
	if (!g_pHeadings || g_iCountHeadings < 1)
		return -1;

	int iLow = 0;
	int iHigh = g_iCountHeadings - 1;
	while (iLow < iHigh)
	{
		if (!stricmp(g_pHeadings[iLow].szName, szHeading))
			return iLow;
		if (!stricmp(g_pHeadings[iHigh].szName, szHeading))
			return iHigh;
		if (iHigh <= iLow + 1)
			return -1;

		int iMid = (iLow + iHigh) / 2;
		int iCmp = stricmp(g_pHeadings[iMid].szName, szHeading);
		if (!iCmp)
			return iMid;
		if (iCmp < 1)
		{
			iHigh--;
			iLow = iMid + 1;
		}
		else
		{
			iLow++;
			iHigh = iMid - 1;
		}
	}

	if (iLow >= g_iCountHeadings || stricmp(g_pHeadings[iLow].szName, szHeading))
		return -1;
	return iLow;
}

// orig 0xe14c0. The same search over one heading's keys.
int PREFS_BinSearchKey(const char* szKey, int iHeading)
{
	if (iHeading < 0 || !g_pHeadings || g_iCountHeadings < 1 || !g_pHeadings[iHeading].pKeys || g_pHeadings[iHeading].iCount < 1)
		return -1;

	sPrefKey* pKeys = g_pHeadings[iHeading].pKeys;
	int iLow = 0;
	int iHigh = g_pHeadings[iHeading].iCount - 1;
	while (iLow < iHigh)
	{
		if (!stricmp(pKeys[iLow].szKey, szKey))
			return iLow;
		if (!stricmp(pKeys[iHigh].szKey, szKey))
			return iHigh;
		if (iHigh <= iLow + 1)
			return -1;

		int iMid = (iLow + iHigh) / 2;
		int iCmp = stricmp(pKeys[iMid].szKey, szKey);
		if (!iCmp)
			return iMid;
		if (iCmp > 0)
		{
			iLow++;
			iHigh = iMid - 1;
		}
		else
		{
			iLow = iMid + 1;
			iHigh--;
		}
	}

	if (iLow >= g_pHeadings[iHeading].iCount || stricmp(pKeys[iLow].szKey, szKey))
		return -1;
	return iLow;
}

// orig 0xe1670
bool PREFS_SearchAll(const char* szHeading, const char* szKey, int* piHeading, int* piKey)
{
	*piKey = -1;
	*piHeading = -1;
	*piHeading = PREFS_BinSearchHead(szHeading);
	if (*piHeading >= 0)
	{
		*piKey = PREFS_BinSearchKey(szKey, *piHeading);
		if (*piKey >= 0)
			return true;
	}
	return false;
}

// orig 0xe16e0. The original exports it unmangled (extern "C").
extern "C" DLLEXPORT char* PREFS_LookUpKey(const char* szHeading, const char* szKey)
{
	int iHeading, iKey;
	if (!PREFS_SearchAll(szHeading, szKey, &iHeading, &iKey))
		return NULL;
	return g_pHeadings[iHeading].pKeys[iKey].szValue;
}

// orig 0xe1730. Returns the stored value, or adds szDefault (and re-sorts) and returns szDefault.
const char* PREFS_LookUpWithDefualtCreate(const char* szHeading, const char* szKey, const char* szDefault)
{
	int iHeading = PREFS_BinSearchHead(szHeading);
	if (iHeading < 0)
	{
		iHeading = PREFS_AddHeading(szHeading);
		PREFS_AddKey(szKey, szDefault, iHeading);
	}
	else
	{
		int iKey = PREFS_BinSearchKey(szKey, iHeading);
		if (iKey >= 0)
			return g_pHeadings[iHeading].pKeys[iKey].szValue;
		PREFS_AddKey(szKey, szDefault, iHeading);
	}
	PREFS_SortAll();
	return szDefault;
}

// orig 0xe17d0. An exchange sort of the headings, then of each heading's keys.
void PREFS_SortAll()
{
	if (!g_pHeadings || g_iCountHeadings < 1)
		return;

	for (int i = 0; i < g_iCountHeadings; i++)
	{
		for (int j = 0; j < i; j++)
		{
			if (stricmp(g_pHeadings[i].szName, g_pHeadings[j].szName) < 0)
			{
				sPrefHeading tmp = g_pHeadings[i];
				g_pHeadings[i] = g_pHeadings[j];
				g_pHeadings[j] = tmp;
			}
		}

		if (g_pHeadings[i].iCount > 0 && g_pHeadings[i].pKeys)
		{
			for (int k = 0; k < g_pHeadings[i].iCount; k++)
			{
				sPrefKey* pKeys = g_pHeadings[i].pKeys;
				for (int l = 0; l < k; l++)
				{
					if (stricmp(pKeys[k].szKey, pKeys[l].szKey) < 0)
					{
						sPrefKey tmp = pKeys[k];
						pKeys[k] = pKeys[l];
						pKeys[l] = tmp;
					}
				}
			}
		}
	}
}

// orig 0xe19f0. Returns the heading's index, adding it (unsorted, at the end) when missing.
int PREFS_AddHeading(const char* szHeading)
{
	if (g_pHeadings && g_iCountHeadings > 0)
	{
		for (int i = 0; i < g_iCountHeadings; i++)
		{
			if (!stricmp(g_pHeadings[i].szName, szHeading))
				return i;
		}
	}

	sPrefHeading* pOld = g_pHeadings;
	g_pHeadings = (sPrefHeading*)malloc((g_iCountHeadings + 1) * sizeof(sPrefHeading));
	if (pOld && g_iCountHeadings > 0)
	{
		memcpy(g_pHeadings, pOld, g_iCountHeadings * sizeof(sPrefHeading));
		free(pOld);
	}

	int iNew = g_iCountHeadings;
	g_pHeadings[iNew].iCount = 0;
	g_pHeadings[iNew].pKeys = NULL;
	g_pHeadings[iNew].szName = (char*)malloc(strlen(szHeading) + 1);
	strcpy(g_pHeadings[iNew].szName, szHeading);
	g_iCountHeadings++;
	return iNew;
}

// orig 0xe1b40. No upper bound check on either index, as in the original.
void PREFS_SetKey(int iHeading, int iKey, const char* szValue)
{
	if (iHeading < 0 || iKey < 0 || !g_pHeadings || g_iCountHeadings < 1)
		return;

	sPrefHeading* pHeading = &g_pHeadings[iHeading];
	if (!pHeading->pKeys || pHeading->iCount < 1)
		return;

	free(pHeading->pKeys[iKey].szValue);
	pHeading->pKeys[iKey].szValue = (char*)malloc(strlen(szValue) + 1);
	strcpy(pHeading->pKeys[iKey].szValue, szValue);
}

// orig 0xe1c10. Replaces the value of an existing key (case-insensitive) or appends the key
// (unsorted). Returns its index, -1 when the heading is bad.
int PREFS_AddKey(const char* szKey, const char* szValue, int iHeading)
{
	if (iHeading < 0 || !g_pHeadings || g_iCountHeadings < 1)
		return -1;

	sPrefHeading* pHeading = &g_pHeadings[iHeading];
	if (pHeading->pKeys && pHeading->iCount > 0)
	{
		for (int i = 0; i < pHeading->iCount; i++)
		{
			if (!stricmp(pHeading->pKeys[i].szKey, szKey))
			{
				free(pHeading->pKeys[i].szValue);
				pHeading->pKeys[i].szValue = (char*)malloc(strlen(szValue) + 1);
				strcpy(pHeading->pKeys[i].szValue, szValue);
				return i;
			}
		}
	}

	sPrefKey* pOld = pHeading->pKeys;
	pHeading->pKeys = (sPrefKey*)malloc((pHeading->iCount + 1) * sizeof(sPrefKey));
	if (pOld && pHeading->iCount > 0)
	{
		memcpy(pHeading->pKeys, pOld, pHeading->iCount * sizeof(sPrefKey));
		free(pOld);
	}

	int iNew = pHeading->iCount;
	pHeading->pKeys[iNew].szKey = (char*)malloc(strlen(szKey) + 1);
	pHeading->pKeys[iNew].szValue = (char*)malloc(strlen(szValue) + 1);
	strcpy(pHeading->pKeys[iNew].szKey, szKey);
	strcpy(pHeading->pKeys[iNew].szValue, szValue);
	pHeading->iCount++;
	return iNew;
}

// orig 0xe1e60. Frees the whole table, then reads szFile. A heading line is "[NAME]" with the
// ']' just before the line's last character (its newline); key lines before the first heading
// are dropped, as are lines with no value ("Key=" or "Key"). CRLF line ends are accepted.
bool PREFS_LoadFile(const char* szFile)
{
	FILE* pFile = fopen(szFile, "r");
	if (!pFile)
		return false;

	if (g_pHeadings)
	{
		for (int i = 0; i < g_iCountHeadings; i++)
		{
			if (g_pHeadings[i].pKeys)
			{
				for (int j = 0; j < g_pHeadings[i].iCount; j++)
				{
					free(g_pHeadings[i].pKeys[j].szKey);
					free(g_pHeadings[i].pKeys[j].szValue);
					g_pHeadings[i].pKeys[j].szValue = NULL;
					g_pHeadings[i].pKeys[j].szKey = NULL;
				}
				free(g_pHeadings[i].pKeys);
				g_pHeadings[i].pKeys = NULL;
			}
			g_pHeadings[i].iCount = 0;
			free(g_pHeadings[i].szName);
			g_pHeadings[i].szName = NULL;
		}
		free(g_pHeadings);
		g_pHeadings = NULL;
	}
	g_iCountHeadings = 0;

	int iHeading = -1;
	char szKey[256];
	char szLine[1024];
	while (!feof(pFile))
	{
		if (!fgets(szLine, 1024, pFile))
			continue;
		szLine[1023] = '\0';

		// Deviation (DEVIATIONS.md): accept CRLF. The original Linux build keeps the '\r', so the
		// CRLF rc.ini RC shipped never matched a heading and loaded nothing; Windows servers, whose
		// text-mode reads drop it, did load it.
		size_t iLen = strlen(szLine);
		if (iLen >= 2 && szLine[iLen - 2] == '\r' && szLine[iLen - 1] == '\n')
		{
			szLine[iLen - 2] = '\n';
			szLine[iLen - 1] = '\0';
		}
		else if (iLen >= 1 && szLine[iLen - 1] == '\r')
			szLine[iLen - 1] = '\0';

		if (strlen(szLine) <= 1)
			continue;

		if (szLine[0] == '[' && szLine[strlen(szLine) - 2] == ']')
		{
			szLine[strlen(szLine + 1) - 1] = '\0';
			iHeading = PREFS_AddHeading(szLine + 1);
			continue;
		}

		if (iHeading < 0)
			continue;

		char* pTok = strtok(szLine, "=\n");
		// VERIFY: guard, the original crashes here: a line of only '=' gives a NULL token.
		if (!pTok)
			continue;
		strncpy(szKey, pTok, 256);
		// VERIFY: the original terminates the token, not szKey (movb $0,0xff(%esi) at 0xe211c,
		// esi = the strtok result).
		pTok[255] = '\0';
		pTok = strtok(NULL, "=\n");
		if (pTok)
			PREFS_AddKey(szKey, pTok, iHeading);
	}

	fclose(pFile);
	PREFS_SortAll();
	return true;
}

// orig 0xe21a0
bool PREFS_WriteFile(const char* szFile)
{
	if (!g_pHeadings || g_iCountHeadings < 1)
		return false;

	FILE* pFile = fopen(szFile, "w");
	if (!pFile)
		return false;

	for (int i = 0; i < g_iCountHeadings; i++)
	{
		fprintf(pFile, "[%s]\n", g_pHeadings[i].szName);
		for (int j = 0; j < g_pHeadings[i].iCount; j++)
			fprintf(pFile, "%s=%s\n", g_pHeadings[i].pKeys[j].szKey, g_pHeadings[i].pKeys[j].szValue);
		fprintf(pFile, "\n");
		fflush(pFile);
	}
	fclose(pFile);
	return true;
}

// orig 0xe22e0
const char* PREFS_KeyLoopUpString(const char* szHeading, const char* szKey, const char* szDefault)
{
	return PREFS_LookUpWithDefualtCreate(szHeading, szKey, szDefault);
}

// orig 0xe2310
int PREFS_KeyLoopUpInt(const char* szHeading, const char* szKey, int iDefault)
{
	char szDefault[32];
	sprintf(szDefault, "%i", iDefault);
	return atoi(PREFS_LookUpWithDefualtCreate(szHeading, szKey, szDefault));
}

// orig 0xe2450
float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault)
{
	char szDefault[48];
	sprintf(szDefault, "%f", (double)flDefault);
	return (float)atof(PREFS_LookUpWithDefualtCreate(szHeading, szKey, szDefault));
}

// orig 0xe25a0. "true", "1" and "yes" (any case) are true, anything else false.
bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault)
{
	char szDefault[16];
	sprintf(szDefault, "%s", bDefault ? "true" : "false");
	const char* szValue = PREFS_LookUpWithDefualtCreate(szHeading, szKey, szDefault);
	return !stricmp(szValue, "true") || !stricmp(szValue, "1") || !stricmp(szValue, "yes");
}

// orig 0xe2650. The default is clamped before it is written; the stored value is clamped on
// read (and looked up again for each test, as in the original).
int PREFS_KeyLoopUpIntClamp(const char* szHeading, const char* szKey, int iDefault, int iMin, int iMax)
{
	int iClamped = iMin;
	if (iDefault >= iMin)
	{
		iClamped = iDefault;
		if (iDefault > iMax)
			iClamped = iMax;
	}

	char szDefault[32];
	sprintf(szDefault, "%i", iClamped);
	if (atoi(PREFS_LookUpWithDefualtCreate(szHeading, szKey, szDefault)) < iMin)
		return iMin;
	if (atoi(PREFS_LookUpWithDefualtCreate(szHeading, szKey, szDefault)) > iMax)
		return iMax;
	return atoi(PREFS_LookUpWithDefualtCreate(szHeading, szKey, szDefault));
}

// orig 0xe29e0. As PREFS_KeyLoopUpIntClamp. A stored "nan" passes the min test and fails the
// max test, so it reads as flMax.
float PREFS_KeyLoopUpFloatClamp(const char* szHeading, const char* szKey, float flDefault, float flMin, float flMax)
{
	float flClamped = flMin;
	if (flMin <= flDefault)
	{
		flClamped = flDefault;
		if (flMax < flDefault)
			flClamped = flMax;
	}

	char szDefault[48];
	sprintf(szDefault, "%f", (double)flClamped);
	if ((float)atof(PREFS_LookUpWithDefualtCreate(szHeading, szKey, szDefault)) < flMin)
		return flMin;
	if (!((float)atof(PREFS_LookUpWithDefualtCreate(szHeading, szKey, szDefault)) <= flMax))
		return flMax;
	return (float)atof(PREFS_LookUpWithDefualtCreate(szHeading, szKey, szDefault));
}
