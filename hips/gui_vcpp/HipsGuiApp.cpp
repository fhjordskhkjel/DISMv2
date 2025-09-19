#include "stdafx.h"
#include "HipsGuiApp.h"
#include "HipsMainDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CHipsGuiApp

BEGIN_MESSAGE_MAP(CHipsGuiApp, CWinApp)
END_MESSAGE_MAP()

// CHipsGuiApp construction

CHipsGuiApp::CHipsGuiApp()
{
	// TODO: add construction code here
	// Place all significant initialization in InitInstance
}

// The one and only CHipsGuiApp object
CHipsGuiApp theApp;

// CHipsGuiApp initialization

BOOL CHipsGuiApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	// Standard initialization
	SetRegistryKey(_T("HIPS - Host Intrusion Prevention System"));

	CHipsMainDialog dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	
	// Since the dialog has been closed, return FALSE so that we exit the
	// application, rather than start the application's message pump.
	return FALSE;
}

int CHipsGuiApp::ExitInstance()
{
	return CWinApp::ExitInstance();
}