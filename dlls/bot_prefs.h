/***
*
*	Rocket Crowbar 1.9: bot_prefs.cpp, a config-line tokenizer and the CVEC growable array
*	(funcfile.tsv files CVEC's methods under "(header)"). Nothing in the game library calls
*	BP_GetConfArgLine or BP_ReadGenLine, or reads g_vTypeDefs; they look like the start of a
*	bot config reader that the bot (rcbot.dll) kept for itself.
*
*	CVEC is kept as the original wrote it, quirks included: the constructor memsets the
*	elements it has just constructed, and Resize grows by n % grow rather than to a multiple.
*
****/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 20 bytes in the original: data +0x0, alloc +0x4, count +0x8, grow +0xc, max +0x10.
template <class T>
class CVEC
{
public:
	CVEC();
	~CVEC();

	void Resize(int iSize);
	T& operator[](int i);

	T* m_pData;	 // orig +0x0
	int m_iAlloc; // orig +0x4, elements allocated
	int m_iCount; // orig +0x8, one past the highest index used
	int m_iGrow;  // orig +0xc
	int m_iMax;	 // orig +0x10, the most elements it will hold
};

// One tokenized line: the first token in szName, the rest in vArgs. 28 bytes in the original.
struct sEllem
{
	char* szName;		 // orig +0x0
	short iCount;		 // orig +0x4, number of tokens in vArgs
	CVEC<char*> vArgs;	 // orig +0x8
};

// 28 bytes in the original. VERIFY: nothing reads the first 8 bytes; laid out like sEllem.
struct sHeading
{
	char* szName;		 // orig +0x0, VERIFY name and type
	short iCount;		 // orig +0x4, VERIFY name and type
	CVEC<sEllem> vEllems; // orig +0x8
};

extern CVEC<sHeading> g_vTypeDefs;

int BP_GetConfArgLine(FILE* pFile, char* szBuf, int iSize, sEllem* pEllem, int* piUnused);
int BP_ReadGenLine(FILE* pFile, char* szBuf, int iSize);

// orig 0x2474b0 (CVEC<sHeading>), 0x247340 (CVEC<sEllem>), 0x2472c0 (CVEC<char*>)
template <class T>
CVEC<T>::CVEC()
{
	m_iGrow = 8;
	m_iAlloc = 16;
	m_iCount = 0;
	m_iMax = 0x8000;
	m_pData = new T[16];
	if (!m_pData)
	{
		fprintf(stderr, "CVEC::CVEC(): Alloc failed\n");
		exit(-1);
	}
	// As the original: this also zeroes the elements' own CVECs, leaking their arrays and
	// leaving them with no room (m_iMax 0), so operator[] on them returns the dummy.
	memset((void*)m_pData, 0, m_iAlloc * sizeof(T));
}

// orig 0x247250 (CVEC<sHeading>), 0x2471e0 (CVEC<sEllem>), 0x247190 (CVEC<char*>)
template <class T>
CVEC<T>::~CVEC()
{
	// The original frees with scalar delete (one element's destructor, then the array pointer
	// without its new[] cookie). delete[] here; the elements' CVECs are zeroed, so their
	// destructors do nothing either way.
	if (m_pData && m_iAlloc > 0)
		delete[] m_pData;
	m_iAlloc = 0;
	m_pData = NULL;
}

// orig 0x246fd0 (only CVEC<char*> is instantiated)
template <class T>
void CVEC<T>::Resize(int iSize)
{
	int iNewAlloc = m_iMax;
	if (iSize < m_iMax)
		iNewAlloc = iSize;

	T* pOld = m_pData;
	if (iSize == m_iAlloc)
		return;

	if (iSize < 1)
	{
		if (pOld)
			delete[] pOld;
		m_iAlloc = 0;
		m_pData = NULL;
		return;
	}

	if (m_iGrow > 1 && m_iGrow < iSize)
		iNewAlloc += iSize % m_iGrow;

	if (m_iMax < iNewAlloc)
	{
		fprintf(stderr, "CVEC::Resize(): Exceded max size of %i with %i\n", m_iMax, iNewAlloc);
		exit(-1);
	}

	m_pData = new T[iNewAlloc];
	if (!m_pData)
	{
		fprintf(stderr, "CVEC::Resize(): Alloc failed\n");
		exit(-1);
	}
	memset((void*)m_pData, 0, iNewAlloc * sizeof(T));

	if (!pOld || m_iAlloc < 1)
	{
		m_iAlloc = iNewAlloc;
		return;
	}

	int iCopy = m_iAlloc;
	if (iNewAlloc < m_iAlloc)
		iCopy = iNewAlloc;
	for (int i = 0; i < iCopy; i++)
		m_pData[i] = pOld[i];
	if (pOld)
		delete[] pOld;
	m_iCount = iSize;
	m_iAlloc = iNewAlloc;
}

// orig 0x247130 (only CVEC<char*> is instantiated). Out of range gives a shared dummy.
template <class T>
T& CVEC<T>::operator[](int i)
{
	static T dummy;
	if (i < 0 || i >= m_iMax)
		return dummy;
	if (i >= m_iAlloc)
		Resize(i + 1);
	if (m_iCount < i + 1)
		m_iCount = i + 1;
	return m_pData[i];
}
