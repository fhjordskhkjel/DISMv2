#include "stdafx.h"
#include "HipsGuiApp.h"
#include "HipsMainDialog.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CHipsMainDialog dialog

CHipsMainDialog::CHipsMainDialog(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_HIPS_MAIN_DIALOG, pParent)
	, m_updateTimer(0)
	, m_driverConnected(FALSE)
	, m_monitoringEnabled(FALSE)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CHipsMainDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATUS_TEXT, m_statusText);
	DDX_Control(pDX, IDC_DRIVER_STATUS_TEXT, m_driverStatusText);
	DDX_Control(pDX, IDC_START_MONITORING, m_startButton);
	DDX_Control(pDX, IDC_STOP_MONITORING, m_stopButton);
	DDX_Control(pDX, IDC_CONNECT_DRIVER, m_connectButton);
	DDX_Control(pDX, IDC_DISCONNECT_DRIVER, m_disconnectButton);
	DDX_Control(pDX, IDC_EVENT_LIST, m_eventList);
	DDX_Control(pDX, IDC_LOG_EDIT, m_logEdit);
}

BEGIN_MESSAGE_MAP(CHipsMainDialog, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_START_MONITORING, &CHipsMainDialog::OnBnClickedStartMonitoring)
	ON_BN_CLICKED(IDC_STOP_MONITORING, &CHipsMainDialog::OnBnClickedStopMonitoring)
	ON_BN_CLICKED(IDC_CONNECT_DRIVER, &CHipsMainDialog::OnBnClickedConnectDriver)
	ON_BN_CLICKED(IDC_DISCONNECT_DRIVER, &CHipsMainDialog::OnBnClickedDisconnectDriver)
	ON_BN_CLICKED(IDC_REFRESH_STATUS, &CHipsMainDialog::OnBnClickedRefreshStatus)
	ON_BN_CLICKED(IDC_VIEW_EVENTS, &CHipsMainDialog::OnBnClickedViewEvents)
	ON_BN_CLICKED(IDC_CONFIGURATION, &CHipsMainDialog::OnBnClickedConfiguration)
	ON_BN_CLICKED(IDC_ABOUT, &CHipsMainDialog::OnBnClickedAbout)
END_MESSAGE_MAP()

// CHipsMainDialog message handlers

BOOL CHipsMainDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// Initialize event list
	m_eventList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_eventList.InsertColumn(0, _T("Time"), LVCFMT_LEFT, 120);
	m_eventList.InsertColumn(1, _T("Event Type"), LVCFMT_LEFT, 120);
	m_eventList.InsertColumn(2, _T("Threat Level"), LVCFMT_LEFT, 100);
	m_eventList.InsertColumn(3, _T("Process"), LVCFMT_LEFT, 150);
	m_eventList.InsertColumn(4, _T("Target"), LVCFMT_LEFT, 200);

	// Set window title
	SetWindowText(_T("HIPS - Host Intrusion Prevention System v1.2.0"));

	// Initialize driver interface
	LogMessage(_T("HIPS GUI started. Ready to connect to driver."));
	
	// Set up timer for status updates
	m_updateTimer = SetTimer(1, 2000, NULL); // Update every 2 seconds
	
	// Initial status update
	UpdateStatus();
	UpdateControls();

	return TRUE;
}

void CHipsMainDialog::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CHipsMainDialog::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CHipsMainDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1)
	{
		UpdateStatus();
		RefreshEventList();
	}
	CDialogEx::OnTimer(nIDEvent);
}

void CHipsMainDialog::OnDestroy()
{
	if (m_updateTimer)
	{
		KillTimer(m_updateTimer);
		m_updateTimer = 0;
	}
	
	if (m_driverConnected)
	{
		m_driverInterface.DisconnectFromDriver();
	}
	
	CDialogEx::OnDestroy();
}

void CHipsMainDialog::OnBnClickedStartMonitoring()
{
	if (m_driverInterface.StartDriverMonitoring())
	{
		LogMessage(_T("Driver monitoring started successfully."));
		m_monitoringEnabled = TRUE;
	}
	else
	{
		LogMessage(_T("Failed to start driver monitoring."));
	}
	UpdateControls();
}

void CHipsMainDialog::OnBnClickedStopMonitoring()
{
	if (m_driverInterface.StopDriverMonitoring())
	{
		LogMessage(_T("Driver monitoring stopped successfully."));
		m_monitoringEnabled = FALSE;
	}
	else
	{
		LogMessage(_T("Failed to stop driver monitoring."));
	}
	UpdateControls();
}

void CHipsMainDialog::OnBnClickedConnectDriver()
{
	if (m_driverInterface.ConnectToDriver())
	{
		LogMessage(_T("Connected to HIPS kernel driver successfully."));
		m_driverConnected = TRUE;
		
		// Get driver version
		DWORD version = 0;
		if (m_driverInterface.GetDriverVersion(version))
		{
			CString versionStr;
			versionStr.Format(_T("Driver version: %d.%d.%d.%d"), 
				(version >> 24) & 0xFF,
				(version >> 16) & 0xFF,
				(version >> 8) & 0xFF,
				version & 0xFF);
			LogMessage(versionStr);
		}
	}
	else
	{
		LogMessage(_T("Failed to connect to HIPS kernel driver. Make sure the driver is installed and running."));
		m_driverConnected = FALSE;
	}
	UpdateControls();
}

void CHipsMainDialog::OnBnClickedDisconnectDriver()
{
	m_driverInterface.DisconnectFromDriver();
	LogMessage(_T("Disconnected from HIPS kernel driver."));
	m_driverConnected = FALSE;
	m_monitoringEnabled = FALSE;
	UpdateControls();
}

void CHipsMainDialog::OnBnClickedRefreshStatus()
{
	UpdateStatus();
	RefreshEventList();
	LogMessage(_T("Status refreshed."));
}

void CHipsMainDialog::OnBnClickedViewEvents()
{
	RefreshEventList();
	LogMessage(_T("Event list refreshed."));
}

void CHipsMainDialog::OnBnClickedConfiguration()
{
	// TODO: Open configuration dialog
	MessageBox(_T("Configuration dialog will be implemented in a future update."), 
		_T("HIPS Configuration"), MB_ICONINFORMATION);
}

void CHipsMainDialog::OnBnClickedAbout()
{
	CString aboutText;
	aboutText = _T("HIPS - Host Intrusion Prevention System\n")
				_T("Version 1.2.0\n\n")
				_T("Advanced Windows security solution with kernel-level protection.\n\n")
				_T("Features:\n")
				_T("• Real-time file system monitoring\n")
				_T("• Process creation/termination tracking\n")
				_T("• Registry protection\n")
				_T("• Memory injection detection\n")
				_T("• Network traffic analysis\n\n")
				_T("Built with Visual C++ and MFC");
	
	MessageBox(aboutText, _T("About HIPS"), MB_ICONINFORMATION);
}

void CHipsMainDialog::UpdateStatus()
{
	// Update connection status
	CString statusText;
	if (m_driverConnected)
	{
		statusText = _T("Connected");
		m_driverStatusText.SetWindowText(_T("Driver: Connected"));
	}
	else
	{
		statusText = _T("Disconnected");
		m_driverStatusText.SetWindowText(_T("Driver: Disconnected"));
	}

	// Update monitoring status
	if (m_monitoringEnabled)
	{
		statusText += _T(" - Monitoring Active");
		m_statusText.SetWindowText(_T("Status: Monitoring Active"));
	}
	else
	{
		statusText += _T(" - Monitoring Inactive");
		m_statusText.SetWindowText(_T("Status: Monitoring Inactive"));
	}
}

void CHipsMainDialog::UpdateControls()
{
	// Update button states based on connection and monitoring status
	m_connectButton.EnableWindow(!m_driverConnected);
	m_disconnectButton.EnableWindow(m_driverConnected);
	m_startButton.EnableWindow(m_driverConnected && !m_monitoringEnabled);
	m_stopButton.EnableWindow(m_driverConnected && m_monitoringEnabled);
}

void CHipsMainDialog::LogMessage(const CString& message)
{
	// Get current time
	CTime currentTime = CTime::GetCurrentTime();
	CString timeStr = currentTime.Format(_T("[%H:%M:%S] "));
	
	// Add message to log
	CString logText;
	m_logEdit.GetWindowText(logText);
	
	if (!logText.IsEmpty())
	{
		logText += _T("\r\n");
	}
	
	logText += timeStr + message;
	m_logEdit.SetWindowText(logText);
	
	// Scroll to bottom
	m_logEdit.LineScroll(m_logEdit.GetLineCount());
}

void CHipsMainDialog::RefreshEventList()
{
	if (!m_driverConnected)
		return;

	// Get events from driver
	std::vector<SecurityEvent> events;
	if (m_driverInterface.GetEventsFromDriver(events))
	{
		// Clear existing items
		m_eventList.DeleteAllItems();
		
		// Add new events
		for (size_t i = 0; i < events.size(); ++i)
		{
			const SecurityEvent& event = events[i];
			
			// Format timestamp
			CString timeStr;
			// Convert timestamp to readable format
			timeStr.Format(_T("%I64d"), event.timestamp);
			
			// Insert item
			int index = m_eventList.InsertItem(i, timeStr);
			
			// Set event type
			CString eventType;
			switch (event.event_type)
			{
			case EventType::FILE_ACCESS:
				eventType = _T("File Access");
				break;
			case EventType::PROCESS_CREATED:
				eventType = _T("Process Created");
				break;
			case EventType::PROCESS_TERMINATED:
				eventType = _T("Process Terminated");
				break;
			case EventType::REGISTRY_MODIFIED:
				eventType = _T("Registry Modified");
				break;
			default:
				eventType = _T("Unknown");
				break;
			}
			m_eventList.SetItemText(index, 1, eventType);
			
			// Set threat level
			CString threatLevel;
			switch (event.threat_level)
			{
			case ThreatLevel::LOW:
				threatLevel = _T("Low");
				break;
			case ThreatLevel::MEDIUM:
				threatLevel = _T("Medium");
				break;
			case ThreatLevel::HIGH:
				threatLevel = _T("High");
				break;
			case ThreatLevel::CRITICAL:
				threatLevel = _T("Critical");
				break;
			default:
				threatLevel = _T("Unknown");
				break;
			}
			m_eventList.SetItemText(index, 2, threatLevel);
			
			// Set process path
			CString processPath(event.process_path.c_str());
			m_eventList.SetItemText(index, 3, processPath);
			
			// Set target path
			CString targetPath(event.target_path.c_str());
			m_eventList.SetItemText(index, 4, targetPath);
		}
	}
}