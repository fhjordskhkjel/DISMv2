#pragma once

#include "DriverInterface.h"

/**
 * HIPS Main Dialog Class
 * 
 * Main user interface dialog for HIPS configuration and monitoring
 */
class CHipsMainDialog : public CDialogEx
{
// Construction
public:
	CHipsMainDialog(CWnd* pParent = NULL);

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_HIPS_MAIN_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	HICON m_hIcon;
	CDriverInterface m_driverInterface;
	
	// Timer for status updates
	UINT_PTR m_updateTimer;
	
	// Status tracking
	BOOL m_driverConnected;
	BOOL m_monitoringEnabled;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	
	// Button handlers
	afx_msg void OnBnClickedStartMonitoring();
	afx_msg void OnBnClickedStopMonitoring();
	afx_msg void OnBnClickedConnectDriver();
	afx_msg void OnBnClickedDisconnectDriver();
	afx_msg void OnBnClickedRefreshStatus();
	afx_msg void OnBnClickedViewEvents();
	afx_msg void OnBnClickedConfiguration();
	afx_msg void OnBnClickedAbout();
	
	DECLARE_MESSAGE_MAP()

private:
	void UpdateStatus();
	void UpdateControls();
	void LogMessage(const CString& message);
	void RefreshEventList();
	void CreateThreatLevelIcons();
	void ApplyEnhancedStyling();
	COLORREF GetThreatLevelColor(DWORD threatLevel);
	int GetThreatLevelIcon(DWORD threatLevel);

public:
	// Control variables
	CStatic m_statusText;
	CStatic m_driverStatusText;
	CButton m_startButton;
	CButton m_stopButton;
	CButton m_connectButton;
	CButton m_disconnectButton;
	CListCtrl m_eventList;
	CEdit m_logEdit;
	
	// Enhanced UI elements
	CImageList m_threatImageList;
};