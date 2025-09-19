#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"

/**
 * HIPS GUI Application Class
 * 
 * Main application class for the HIPS GUI using MFC
 */
class CHipsGuiApp : public CWinApp
{
public:
	CHipsGuiApp();

// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation
	DECLARE_MESSAGE_MAP()
};

extern CHipsGuiApp theApp;