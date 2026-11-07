/***
*
*	Rocket Crowbar 1.9: bot_prefs.cpp, config-line readers (bot_prefs.h).
*
****/

#include <ctype.h>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "bot_prefs.h"

// orig 0xf45a0 (_GLOBAL_.I) and 0xf4570 (_GLOBAL_.D): its static constructor and destructor.
CVEC<sHeading> g_vTypeDefs;

// orig 0xf4170. Reads a line and splits it into tokens: whitespace separates, "quotes" group,
// "//" outside quotes ends the line, and \n \t \/ are escapes. The first token goes to
// pEllem->szName, the rest to pEllem->vArgs. Returns -1 at end of file, 1 when a token was
// stored, else 0. piUnused is never read.
//
// VERIFY: as the original (reads at 0xf4290 and 0xf43b8), each token is copied from the write
// pointer, which by then sits on the terminator, so every stored token is "" and the last one is
// stored only when an unknown escape left a '\' under the pointer. Nothing calls it.
int BP_GetConfArgLine(FILE* pFile, char* szBuf, int iSize, sEllem* pEllem, int* piUnused)
{
	if (feof(pFile) || !fgets(szBuf, iSize - 1, pFile))
		return -1;

	if (strlen(szBuf) <= 1)
		return 0;

	szBuf[iSize - 2] = '\0';

	char* szTok = (char*)malloc(strlen(szBuf) + 1);
	char* p = szTok;
	bool bSpace = false;
	bool bHaveChar = false;
	bool bQuote = false;
	int iTok = -1;
	pEllem->iCount = 0;
	*szTok = '\0';

	while (*szBuf)
	{
		if (*szBuf == '\n' || (*szBuf == '/' && szBuf[1] == '/' && !bQuote))
			break;

		if (isspace(*szBuf) && !bQuote)
		{
			bSpace = true;
			szBuf++;
			continue;
		}

		if (bQuote && *szBuf == '"')
		{
			szBuf++;
			bQuote = false;
		}

		if (bHaveChar && (bSpace || *szBuf == '"'))
		{
			if (*szBuf == '"')
				bQuote = true;

			if (iTok < 0)
			{
				pEllem->szName = (char*)malloc(strlen(p) + 1);
				strcpy(pEllem->szName, p);
			}
			else
			{
				pEllem->vArgs[pEllem->iCount] = (char*)malloc(strlen(p) + 1);
				strcpy(pEllem->vArgs[pEllem->iCount], p);
				pEllem->iCount++;
			}
			iTok++;
			p = szTok;
			*szTok = '\0';
		}

		p[1] = '\0';
		bSpace = false;
		bHaveChar = true;

		if (*szBuf != '\\')
		{
			*p++ = *szBuf++;
			continue;
		}

		szBuf++;
		if (*szBuf == 'n')
			*p++ = '\n';
		else if (*szBuf == 't')
			*p++ = '\t';
		else if (*szBuf == '/')
			*p++ = '/';
		else
		{
			// An unknown escape keeps the '\' unadvanced and reads this character again.
			*p = '\\';
			continue;
		}
		szBuf++;
	}

	if (strlen(p) != 0)
	{
		if (iTok < 0)
		{
			pEllem->szName = (char*)malloc(strlen(p) + 1);
			strcpy(pEllem->szName, p);
		}
		else
		{
			pEllem->vArgs[pEllem->iCount] = (char*)malloc(strlen(p) + 1);
			strcpy(pEllem->vArgs[pEllem->iCount], p);
			pEllem->iCount++;
		}
		iTok++;
	}

	free(szTok);
	return iTok >= 0 ? 1 : 0;
}

// orig 0xf4470. Reads a line, drops a "//" comment, and collapses each whitespace run between
// words into one space (leading and trailing whitespace removed). Returns -1 at end of file, 1
// when what is left is longer than one character, else 0.
int BP_ReadGenLine(FILE* pFile, char* szBuf, int iSize)
{
	if (feof(pFile) || !fgets(szBuf, iSize - 1, pFile))
		return -1;

	if (strlen(szBuf) <= 1)
		return 0;

	szBuf[iSize - 2] = '\0';

	bool bSpace = false;
	bool bHaveChar = false;
	char* pIn = szBuf;
	char* pOut = szBuf;
	while (*pIn && *pIn != '\n' && (*pIn != '/' || pIn[1] != '/'))
	{
		if (!isspace(*pIn))
		{
			if (bSpace && bHaveChar)
				*pOut++ = ' ';
			bHaveChar = true;
			bSpace = false;
			*pOut++ = *pIn;
		}
		else
			bSpace = true;
		pIn++;
	}
	*pOut = '\0';

	if (strlen(szBuf) > 1)
		return 1;
	return 0;
}
