// MFCApplication1Dlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "MFCApplication1Dlg.h"
#include "afxdialogex.h"
#include "Utils.h"
#include "AutoClicker.h"
#include "VolumeManager.h"
#include "ProcessManager.h"
#include "AboutDlg.h"
#include "AutoClickerSpeedDlg.h"
#include "SettingsDlg.h"
#include "QRCodeGenDlg.h"
#include "ScreenshotOCRDlg.h"
#include "BatchRenameDlg.h"
#include "RegexGuideDlg.h"
#include <TlHelp32.h>
#include <Shellapi.h>
#include <Psapi.h>
#include <afxdlgs.h>
#include <Mmdeviceapi.h>
#include <Endpointvolume.h>
#include <atomic>
#include <algorithm>
#include <stdio.h>
#include <string>
#include <afxole.h>
#pragma comment(lib, "Ole32.lib")

#include <windows.h>
#include <processthreadsapi.h>
#include <userenv.h>
#pragma comment(lib, "Userenv.lib")

// capture overlay is implemented via a window class registered at runtime

// Bilibili-specific window-finding helpers removed per user request.





// File management (drag/drop) and DropHelper removed

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Define message constants declared in header (now constexpr in header, so no need to redefine here)
// Autoclicker message is defined in the dialog header as WM_AUTOCLICK_STOPPED

// Autoclicker functionality moved to CAutoClicker class (AutoClicker.h/cpp)

// Master volume helpers using Core Audio EndpointVolume (0-100)
// Async volume retrieval: run in background and post WM_VOLUME_UPDATED with percent in WPARAM


// Next-track feature removed



// CMFCApplication1Dlg 对话框



CMFCApplication1Dlg::CMFCApplication1Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MFCAPPLICATION1_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    m_bTrayVisible = false;
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_bExiting = false;
    m_bMinimizeOnClose = true; // 默认选中

    m_hCaptureWnd = NULL;
    m_hSelectedWnd = nullptr;
    m_fileTabIndex = 4;
    m_bPreventLockScreen = false;
}

void CMFCApplication1Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMFCApplication1Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, &CMFCApplication1Dlg::OnTcnSelchangeTab1)
    ON_WM_CONTEXTMENU()
    ON_COMMAND(32771, &CMFCApplication1Dlg::OnKillProcess)
    ON_COMMAND(32772, &CMFCApplication1Dlg::OnAddStartup)
    ON_COMMAND(32773, &CMFCApplication1Dlg::OnRemoveStartup)
    ON_COMMAND(32774, &CMFCApplication1Dlg::OnLocateProcess)
    ON_COMMAND(32805, &CMFCApplication1Dlg::OnUntopmostWindow)
    ON_COMMAND(32806, &CMFCApplication1Dlg::OnCopyStartupPath)
    ON_COMMAND(32807, &CMFCApplication1Dlg::OnDeleteList6Record)
    ON_COMMAND(32808, &CMFCApplication1Dlg::OnDeleteList7Record)
    ON_COMMAND(32809, &CMFCApplication1Dlg::OnTopmostFromHistory)
    ON_COMMAND(32810, &CMFCApplication1Dlg::OnUntopmostFromHistory)
    ON_BN_CLICKED(IDC_BUTTON1, &CMFCApplication1Dlg::OnBnClickedButton1)
    ON_BN_CLICKED(IDC_BUTTON2, &CMFCApplication1Dlg::OnBnClickedButton2)
    ON_CBN_SELCHANGE(IDC_COMBO1, &CMFCApplication1Dlg::OnCbnSelchangeCombo1)
    ON_WM_DROPFILES()
    ON_BN_CLICKED(IDC_BUTTON3, &CMFCApplication1Dlg::OnBnClickedButton3)
    ON_BN_CLICKED(IDC_BUTTON4, &CMFCApplication1Dlg::OnBnClickedButton4)
    ON_BN_CLICKED(IDC_CHECK1, &CMFCApplication1Dlg::OnBnClickedCheck1)
    ON_MESSAGE(WM_TRAYICON, &CMFCApplication1Dlg::OnTrayNotification)
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_COMMAND(2001, &CMFCApplication1Dlg::OnTrayShowWindow)
    ON_COMMAND(2002, &CMFCApplication1Dlg::OnTrayExit)
    ON_STN_CLICKED(IDC_STATIC_PATH, &CMFCApplication1Dlg::OnStnClickedStaticPath)
    ON_BN_CLICKED(IDC_CHECK2, &CMFCApplication1Dlg::OnBnClickedCheck2)
    ON_BN_CLICKED(IDC_BUTTON5, &CMFCApplication1Dlg::OnBnClickedButton5)
    ON_BN_CLICKED(IDC_BUTTON6, &CMFCApplication1Dlg::OnBnClickedButton6)
    ON_BN_CLICKED(IDC_BUTTON7, &CMFCApplication1Dlg::OnBnClickedButton7)
    ON_BN_CLICKED(IDC_BUTTON8, &CMFCApplication1Dlg::OnBnClickedButton8)
    ON_BN_CLICKED(IDC_BUTTON9, &CMFCApplication1Dlg::OnBnClickedButton9)
    ON_BN_CLICKED(IDC_BUTTON10, &CMFCApplication1Dlg::OnBnClickedButton10)
    ON_BN_CLICKED(IDC_BUTTON11, &CMFCApplication1Dlg::OnBnClickedButton11)
    ON_BN_CLICKED(IDC_BUTTON20, &CMFCApplication1Dlg::OnBnClickedButton20)
    ON_BN_CLICKED(IDC_BUTTON12, &CMFCApplication1Dlg::OnBnClickedButton12)
    ON_BN_CLICKED(IDC_BUTTON13, &CMFCApplication1Dlg::OnBnClickedButton13)
    ON_BN_CLICKED(IDC_BUTTON14, &CMFCApplication1Dlg::OnBnClickedButton14)
    ON_WM_HSCROLL()
    ON_MESSAGE(WM_REFRESH_PROCESSES_DONE, &CMFCApplication1Dlg::OnRefreshProcessesDone)
    ON_MESSAGE(WM_REFRESH_STARTUPS_DONE, &CMFCApplication1Dlg::OnRefreshStartupsDone)
    ON_WM_HOTKEY()
    ON_BN_CLICKED(IDC_CHECK3, &CMFCApplication1Dlg::OnBnClickedCheck3)
    ON_BN_CLICKED(IDC_CHECK4, &CMFCApplication1Dlg::OnBnClickedCheck4)
    ON_BN_CLICKED(IDC_CHECK5, &CMFCApplication1Dlg::OnBnClickedCheck5)
    ON_BN_CLICKED(IDC_BUTTON17, &CMFCApplication1Dlg::OnBnClickedButton17)
    ON_BN_CLICKED(IDC_BUTTON18, &CMFCApplication1Dlg::OnBnClickedButton18)
    ON_MESSAGE(WM_CLIPBOARDUPDATE, &CMFCApplication1Dlg::OnClipboardUpdate)
    ON_MESSAGE(CMFCApplication1Dlg::WM_AUTOCLICK_STOPPED, &CMFCApplication1Dlg::OnAutoClickStopped)
    ON_MESSAGE(CMFCApplication1Dlg::WM_SPEED_DLG_CLOSED, &CMFCApplication1Dlg::OnSpeedDlgClosed)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST3, &CMFCApplication1Dlg::OnNMDblclkList3)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST2, &CMFCApplication1Dlg::OnNMDblclkList2)
    ON_NOTIFY(NM_CLICK,  IDC_LIST6, &CMFCApplication1Dlg::OnClickList6)
    ON_NOTIFY(NM_CLICK,  IDC_LIST7, &CMFCApplication1Dlg::OnClickList7)
    ON_BN_CLICKED(IDC_BUTTON19, &CMFCApplication1Dlg::OnBnClickedButton19)
    // 窗口处理控件
    ON_BN_CLICKED(IDC_BUTTON15, &CMFCApplication1Dlg::OnForceKillProcess)
    ON_BN_CLICKED(IDC_BUTTON16, &CMFCApplication1Dlg::OnWindowScreenshot)
    // 菜单栏扩展
    ON_COMMAND(ID_VIEW_PROCESS,     &CMFCApplication1Dlg::OnViewProcess)
    ON_COMMAND(ID_VIEW_STARTUP,     &CMFCApplication1Dlg::OnViewStartup)
    ON_COMMAND(ID_VIEW_CLIPBOARD,   &CMFCApplication1Dlg::OnViewClipboard)
    ON_COMMAND(ID_VIEW_WINDOW,      &CMFCApplication1Dlg::OnViewWindow)
    ON_COMMAND(ID_VIEW_FILE,        &CMFCApplication1Dlg::OnViewFile)
    ON_COMMAND(ID_VIEW_GIT,         &CMFCApplication1Dlg::OnViewGit)
    ON_COMMAND(ID_TOOLS_WECHAT,     &CMFCApplication1Dlg::OnBnClickedButton4)
    ON_COMMAND(ID_TOOLS_QQ,         &CMFCApplication1Dlg::OnBnClickedButton5)
    ON_COMMAND(ID_TOOLS_VSCODE,     &CMFCApplication1Dlg::OnBnClickedButton6)
    ON_COMMAND(ID_TOOLS_VS,         &CMFCApplication1Dlg::OnBnClickedButton7)
    ON_COMMAND(ID_TOOLS_BILIBILI,   &CMFCApplication1Dlg::OnBnClickedButton8)
    ON_COMMAND(ID_TOOLS_STUDY,      &CMFCApplication1Dlg::OnBnClickedButton9)
    ON_COMMAND(ID_TOOLS_DOWNLOAD,   &CMFCApplication1Dlg::OnBnClickedButton21)
    ON_COMMAND(ID_TOOLS_POWERSHELL, &CMFCApplication1Dlg::OnBnClickedButton27)
    ON_COMMAND(ID_TOOLS_WSL,        &CMFCApplication1Dlg::OnBnClickedButton28)
    ON_COMMAND(ID_TOOLS_GITBASH,    &CMFCApplication1Dlg::OnBnClickedButton31)
    ON_COMMAND(ID_TOOLS_QRCODE,         &CMFCApplication1Dlg::OnToolsQRCode)
    ON_COMMAND(ID_TOOLS_SCREENSHOT_OCR, &CMFCApplication1Dlg::OnToolsScreenshotOCR)
    ON_COMMAND(ID_TOOLS_BATCH_RENAME,   &CMFCApplication1Dlg::OnToolsBatchRename)
    ON_COMMAND(ID_WINDOW_LOCATE,    &CMFCApplication1Dlg::OnWindowLocate)
    ON_COMMAND(ID_WINDOW_UNTOPMOST, &CMFCApplication1Dlg::OnWindowUntopmost)
    ON_COMMAND(ID_WINDOW_CLOSE,     &CMFCApplication1Dlg::OnWindowClose)
    ON_COMMAND(ID_HELP_SHORTCUTS,   &CMFCApplication1Dlg::OnHelpShortcuts)
    ON_COMMAND(ID_HELP_GITHUB,      &CMFCApplication1Dlg::OnHelpGithub)
    ON_COMMAND(ID_HELP_REGEX_GUIDE, &CMFCApplication1Dlg::OnHelpRegexGuide)
    ON_BN_CLICKED(IDC_BUTTON21, &CMFCApplication1Dlg::OnBnClickedButton21)
    ON_BN_CLICKED(IDC_BUTTON22, &CMFCApplication1Dlg::OnBnClickedButton22)
    ON_BN_CLICKED(IDC_BUTTON23, &CMFCApplication1Dlg::OnBnClickedButton23)
    ON_BN_CLICKED(IDC_BUTTON24, &CMFCApplication1Dlg::OnBnClickedButton24)
    ON_BN_CLICKED(IDC_BUTTON25, &CMFCApplication1Dlg::OnBnClickedButton25)
    ON_BN_CLICKED(IDC_BUTTON26, &CMFCApplication1Dlg::OnBnClickedButton26)
    ON_MESSAGE(CMFCApplication1Dlg::WM_VOLUME_UPDATED, &CMFCApplication1Dlg::OnVolumeUpdated)
    ON_BN_CLICKED(IDC_CHECK6, &CMFCApplication1Dlg::OnBnClickedCheck6)
    ON_BN_CLICKED(IDC_BUTTON27, &CMFCApplication1Dlg::OnBnClickedButton27)
    ON_BN_CLICKED(IDC_BUTTON28, &CMFCApplication1Dlg::OnBnClickedButton28)
    ON_BN_CLICKED(IDC_BUTTON29, &CMFCApplication1Dlg::OnBnClickedButton29)
    ON_BN_CLICKED(IDC_BUTTON30, &CMFCApplication1Dlg::OnBnClickedButton30)
    ON_BN_CLICKED(IDC_BUTTON31, &CMFCApplication1Dlg::OnBnClickedButton31)
    ON_BN_CLICKED(IDC_BUTTON32, &CMFCApplication1Dlg::OnBnClickedButton32)
    // Bind IDC_BUTTON33 to the bilibili "next" handler if the control exists
#ifdef IDC_BUTTON33
    ON_BN_CLICKED(IDC_BUTTON33, &CMFCApplication1Dlg::OnBiliNext)
#endif
    ON_COMMAND(41001, &CMFCApplication1Dlg::OnBiliNext)
    ON_COMMAND(40001, &CMFCApplication1Dlg::OnCopyGitCommand)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST4, &CMFCApplication1Dlg::OnNMDblclkList4)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST5, &CMFCApplication1Dlg::OnNMDblclkList5)
    ON_LBN_DBLCLK(IDC_LIST4, &CMFCApplication1Dlg::OnLbnDblclkList4)
    ON_COMMAND(ID_FILE_SETTINGS, &CMFCApplication1Dlg::OnFileSettings)
    ON_COMMAND(ID_FILE_EXIT, &CMFCApplication1Dlg::OnFileExit)
    ON_COMMAND(ID_HELP_ABOUT, &CMFCApplication1Dlg::OnHelpAbout)
END_MESSAGE_MAP()


// CMFCApplication1Dlg 消息处理程序


BOOL CMFCApplication1Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将"关于..."菜单项添加到系统菜单中。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	// 菜单栏
	CMenu menu;
	menu.LoadMenu(IDR_MAIN_MENU);
	SetMenu(&menu);
	menu.Detach();

	m_bMinimizeOnClose = true;
	m_strDroppedFilePath.Empty();
	SetDlgItemText(IDC_EDIT4, AfxGetApp()->GetProfileString(_T("Template"), _T("DefaultReportName"), _T("")));

	// 初始化标签页和各标签页控件
	InitTabControl();
	InitProcessTab();
	InitStartupTab();
	InitClipboardTab();
	InitWindowTab();
	InitFileTab();
	InitGitTab();

	// 根据当前选中标签页统一更新控件可见性
	int nCur = 0;
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) nCur = pTab->GetCurSel();
	UpdateTabVisibility(nCur);

	// 首屏加载数据
	if (nCur == 0)
		RefreshProcessList();
	else
		RefreshStartupList();

	// 关机/重启 combo 初始化
	CComboBox* pCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO1));
	if (pCombo)
	{
		pCombo->ResetContent();
		pCombo->AddString(_T("1分钟后重启"));
		pCombo->AddString(_T("默认3分钟关机"));
		pCombo->AddString(_T("设定时间关机"));
		int idx = pCombo->FindStringExact(-1, _T("默认3分钟关机"));
		if (idx != CB_ERR) pCombo->SetCurSel(idx);
		else pCombo->SetCurSel(0);
		OnCbnSelchangeCombo1();
	}

	// 开机自启动复选按钮
	CButton* pCheck1 = static_cast<CButton*>(GetDlgItem(IDC_CHECK1));
	if (pCheck1)
	{
		TCHAR exePath[MAX_PATH] = {0};
		if (GetModuleFileName(NULL, exePath, MAX_PATH) > 0)
		{
			CString csExePath = exePath;
			int pos = csExePath.ReverseFind(_T('\\'));
			CString keyName = (pos != -1) ? csExePath.Mid(pos + 1) : csExePath;

			HKEY hKey = NULL;
			if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
			{
				DWORD type = 0;
				TCHAR buf[MAX_PATH] = {0};
				DWORD bufSize = sizeof(buf);
				LONG ret = RegQueryValueEx(hKey, keyName, NULL, &type, reinterpret_cast<LPBYTE>(buf), &bufSize);
				if (ret == ERROR_SUCCESS && type == REG_SZ)
					pCheck1->SetCheck(BST_CHECKED);
				else
					pCheck1->SetCheck(BST_UNCHECKED);
				RegCloseKey(hKey);
			}
			else
			{
				pCheck1->SetCheck(BST_UNCHECKED);
			}
		}
	}

	// 拖放接收
	CWnd* pCtrl = nullptr;
	pCtrl = GetDlgItem(IDC_STATIC_PATH); if (pCtrl) ::DragAcceptFiles(pCtrl->GetSafeHwnd(), TRUE);
	pCtrl = GetDlgItem(IDC_EDIT4);       if (pCtrl) ::DragAcceptFiles(pCtrl->GetSafeHwnd(), TRUE);
	pCtrl = GetDlgItem(IDC_BUTTON3);     if (pCtrl) ::DragAcceptFiles(pCtrl->GetSafeHwnd(), TRUE);
	pCtrl = GetDlgItem(IDC_STATIC7);     if (pCtrl) ::DragAcceptFiles(pCtrl->GetSafeHwnd(), TRUE);

	// 最小化到托盘复选按钮
	CButton* pCheckMin = static_cast<CButton*>(GetDlgItem(IDC_CHECK2));
	if (pCheckMin) pCheckMin->SetCheck(m_bMinimizeOnClose ? BST_CHECKED : BST_UNCHECKED);

	// 非管理员权限 PowerShell 复选按钮默认选中
	CButton* pCheck6 = static_cast<CButton*>(GetDlgItem(IDC_CHECK6));
	if (pCheck6) pCheck6->SetCheck(BST_CHECKED);

	// 剪贴板监听
	::AddClipboardFormatListener(m_hWnd);

	// 文件拖放
	::DragAcceptFiles(m_hWnd, TRUE);
	AllowUIPIMessage(m_hWnd, WM_DROPFILES, TRUE);
	AllowUIPIMessage(m_hWnd, WM_COPYDATA, TRUE);
	AllowUIPIMessage(m_hWnd, 0x0049, TRUE);

	// 管理员权限提升
	if (!IsProcessElevated())
	{
		auto RelaunchElevatedNoPrompt = []() -> bool {
			TCHAR path[MAX_PATH];
			if (GetModuleFileName(NULL, path, MAX_PATH) > 0)
			{
				SHELLEXECUTEINFO sei = { sizeof(sei) };
				sei.fMask = SEE_MASK_FLAG_NO_UI;
				sei.lpVerb = _T("runas");
				sei.lpFile = path;
				sei.nShow = SW_SHOWNORMAL;
				return ShellExecuteEx(&sei) != FALSE;
			}
			return false;
		};

		if (RelaunchElevatedNoPrompt())
		{
			EndDialog(IDOK);
			return FALSE;
		}
		else
		{
			MessageBox(_T("无法以管理员权限重新启动。请手动以管理员身份运行程序。"), _T("提示"), MB_OK | MB_ICONWARNING);
		}
	}

	// 全局快捷键
	CString strTitle;
	GetWindowText(strTitle);
	strTitle += _T(" (ctrl+alt+空格唤起此窗口)");
	SetWindowText(strTitle);
	RegisterHotKey(m_hWnd, 1001, MOD_CONTROL | MOD_ALT, VK_SPACE);

	// 音量滑块
	CSliderCtrl* pSlider = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER1));
	CEdit* pEditVol = static_cast<CEdit*>(GetDlgItem(IDC_EDIT5));
	if (pSlider)
	{
		pSlider->SetRange(0, 100);
		pSlider->SetPos(100);
		if (pEditVol) pEditVol->SetWindowText(_T("100"));
		CVolumeManager::FetchVolumeAsync(m_hWnd);
	}

	return TRUE;
}

// ========== 窗口处理新功能 ==========


void CMFCApplication1Dlg::OnDestroy()
{
    UnregisterHotKey(m_hWnd, 1001);

    if (m_bTrayVisible)
    {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        m_bTrayVisible = false;
    }

    // Ensure we unregister clipboard listener if we registered it in OnInitDialog.
    ::RemoveClipboardFormatListener(m_hWnd);

    // 销毁连点器速度调节窗口
    if (m_pSpeedDlg)
    {
        m_pSpeedDlg->DestroyWindow();
        m_pSpeedDlg.reset();
    }

    // Ensure autoclicker threads are stopped (C++20: using CAutoClicker class)
    m_autoClicker.Stop();

    // ensure any background volume thread is stopped (CVolumeManager handles this automatically)
    // Drop helper and file management removed - no cleanup required here

    // Remove topmost status from all managed windows
    for (HWND hWnd : m_topmostWnds)
    {
        if (IsValidWindow(hWnd))
            ::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
    }
    m_topmostWnds.clear();

    // Unregister drag-and-drop acceptance to be tidy
    ::DragAcceptFiles(m_hWnd, FALSE);

    // Destroy any lingering overlay capture window created for locating targets
    if (m_hCaptureWnd && IsValidWindow(m_hCaptureWnd))
    {
        ::DestroyWindow(m_hCaptureWnd);
        m_hCaptureWnd = NULL;
    }

    // If we dynamically registered a window class for the overlay, unregister it.
    // This avoids leaving class registrations around across repeated runs in
    // environments that reload the module without process exit.
    WNDCLASS wc = {0};
    if (GetClassInfo(AfxGetInstanceHandle(), _T("MyCaptureOverlayClass"), &wc))
    {
        UnregisterClass(_T("MyCaptureOverlayClass"), AfxGetInstanceHandle());
    }

    // Clear global autoclick owner handle to avoid referencing freed window
    // Autoclicker cleanup now handled by m_autoClicker.Stop() above

    // Revoke UIPI allowances
    AllowUIPIMessage(m_hWnd, WM_DROPFILES, FALSE);
    AllowUIPIMessage(m_hWnd, WM_COPYDATA, FALSE);
    AllowUIPIMessage(m_hWnd, 0x0049, FALSE);

    // Ensure we clear execution state if we had prevented lock
    if (m_bPreventLockScreen)
    {
        // clear previously set flags by calling with ES_CONTINUOUS only
        SetThreadExecutionState(ES_CONTINUOUS);
        m_bPreventLockScreen = false;
    }

    CDialogEx::OnDestroy();
}

void CMFCApplication1Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CMFCApplication1Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CMFCApplication1Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

BOOL CMFCApplication1Dlg::PreTranslateMessage(MSG* pMsg)
{
    // ===== 全局快捷键 =====

    if (pMsg->message == WM_KEYDOWN)
    {
        // F5: 刷新当前tab列表
        if (pMsg->wParam == VK_F5)
        {
            CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
            if (pTab)
            {
                int nSel = pTab->GetCurSel();
                if (nSel == 0) { RefreshProcessList(); return TRUE; }
                if (nSel == 1) { RefreshStartupList(); return TRUE; }
            }
        }

        // Ctrl+Alt+D: 窗口定位
        if (pMsg->wParam == 'D' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_MENU) & 0x8000))
        {
            OnWindowLocate();
            return TRUE;
        }

        // Enter: 当焦点在编辑框时触发运行
        if (pMsg->wParam == VK_RETURN)
        {
            CWnd* pEdit = GetDlgItem(IDC_EDIT6);
            if (pEdit && pEdit->GetSafeHwnd() == ::GetFocus())
            {
                OnBnClickedButton17();
                return TRUE;
            }
        }
    }

    // Alt+1~6: 切换标签页
    if (pMsg->message == WM_SYSKEYDOWN)
    {
        if (pMsg->wParam >= '1' && pMsg->wParam <= '6')
        {
            int nTab = static_cast<int>(pMsg->wParam - '1');
            CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
            if (pTab)
            {
                pTab->SetCurSel(nTab);
                UpdateTabVisibility(nTab);
            }
            return TRUE;
        }
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}

// Handle clipboard update messages
LRESULT CMFCApplication1Dlg::OnClipboardUpdate(WPARAM wParam, LPARAM lParam)
{
    // Only handle text
    if (!::IsClipboardFormatAvailable(CF_UNICODETEXT))
        return 0;

    if (!::OpenClipboard(m_hWnd))
        return 0;

    HGLOBAL hData = ::GetClipboardData(CF_UNICODETEXT);
    if (hData)
    {
        LPCWSTR pszText = (LPCWSTR)::GlobalLock(hData);
        if (pszText)
        {
            CString s = pszText;
            ::GlobalUnlock(hData);

            // Trim and ignore empty
            s.Trim();
            if (!s.IsEmpty())
            {
                // Avoid duplicate consecutive entries
                if (m_clipHistory.empty() || m_clipHistory.front() != s)
                {
                    m_clipHistory.insert(m_clipHistory.begin(), s);
                    if ((int)m_clipHistory.size() > CLIP_HISTORY_MAX)
                        m_clipHistory.pop_back();

                    // update list control
                    CListCtrl* pList3 = (CListCtrl*)GetDlgItem(IDC_LIST3);
                    if (pList3)
                    {
                        pList3->DeleteAllItems();
                        int idx = 0;
                        for (auto &item : m_clipHistory)
                        {
                            pList3->InsertItem(idx, item);
                            idx++;
                        }
                    }
                }
            }
        }
    }

    ::CloseClipboard();
    return 0;
}

afx_msg LRESULT CMFCApplication1Dlg::OnRefreshProcessesDone(WPARAM wParam, LPARAM lParam)
{
    auto vec = reinterpret_cast<std::vector<ProcInfo>*>(wParam);
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (pList)
    {
        pList->DeleteAllItems();
        // Ensure report style and reasonable column widths are set
        pList->ModifyStyle(0, LVS_REPORT);
        pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
        pList->SetColumnWidth(0, 200);
        pList->SetColumnWidth(1, 160);
        pList->SetColumnWidth(2, 600);
        pList->SetColumnWidth(3, 200);

        int idx = 0;
        for (auto &pi : *vec)
        {
            CString pidStr;
            pidStr.Format(_T("%u"), pi.pid);
            CString memStr;
            memStr.Format(_T("%llu"), (unsigned long long)pi.memKB);

            LVITEM li = {0};
            li.mask = LVIF_TEXT;
            li.iItem = idx;
            li.iSubItem = 0;
            li.pszText = const_cast<LPTSTR>((LPCTSTR)pi.name);
            pList->InsertItem(&li);

            // Explicitly set subitem texts
            pList->SetItemText(idx, 0, pi.name);
            pList->SetItemText(idx, 1, pidStr);
            pList->SetItemText(idx, 2, pi.path);
            pList->SetItemText(idx, 3, memStr);
            idx++;
        }
    }
    delete vec;
    return 0;
}

afx_msg LRESULT CMFCApplication1Dlg::OnVolumeUpdated(WPARAM wParam, LPARAM lParam)
{
    int pct = (int)wParam;
    CSliderCtrl* pSlider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER1);
    CEdit* pEditVol = (CEdit*)GetDlgItem(IDC_EDIT5);
    if (pSlider) pSlider->SetPos(pct);
    if (pEditVol)
    {
        CString s; s.Format(_T("%d"), pct);
        pEditVol->SetWindowText(s);
    }
    return 0;
}

afx_msg LRESULT CMFCApplication1Dlg::OnRefreshStartupsDone(WPARAM wParam, LPARAM lParam)
{
    auto vec = reinterpret_cast<std::vector<StartupInfo>*>(wParam);
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST2);
    if (pList)
    {
        pList->DeleteAllItems();
        int idx = 0;
        for (auto &si : *vec)
        {
            pList->InsertItem(idx, si.name);
            pList->SetItemText(idx, 1, si.cmd);
            idx++;
        }
    }
    delete vec;
    return 0;
}

void CMFCApplication1Dlg::OnContextMenu(CWnd* pWnd, CPoint point)
{
    CListCtrl* pList1 = (CListCtrl*)GetDlgItem(IDC_LIST1);
    CListCtrl* pList2 = (CListCtrl*)GetDlgItem(IDC_LIST2);

    HWND hClicked = pWnd ? pWnd->GetSafeHwnd() : ::WindowFromPoint(point);

    // 右键在进程列表上
    if (pList1 && hClicked == pList1->GetSafeHwnd())
    {
        int nSel = pList1->GetNextItem(-1, LVNI_SELECTED);
        if (nSel != -1)
        {
            CMenu menu;
            menu.CreatePopupMenu();
            menu.AppendMenu(MF_STRING, 32771, _T("结束进程"));
            menu.AppendMenu(MF_STRING, 32774, _T("打开程序所在位置"));
            menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        }
        return;
    }

    // 右键在启动项管理列表上
    if (pList2 && hClicked == pList2->GetSafeHwnd())
    {
        int nSel = pList2->GetNextItem(-1, LVNI_SELECTED);
        CMenu menu;
        menu.CreatePopupMenu();
        // 添加和删除命令
        menu.AppendMenu(MF_STRING, 32772, _T("添加启动项"));
        if (nSel != -1)
        {
            menu.AppendMenu(MF_STRING, 32773, _T("删除启动项"));
            menu.AppendMenu(MF_STRING, 32806, _T("复制路径"));
        }

        menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        return;
    }


    // 右键在窗口处理列表上 (复制值)
    CListCtrl* pList5 = (CListCtrl*)GetDlgItem(IDC_LIST5);
    if (pList5 && hClicked == pList5->GetSafeHwnd())
    {
        int nSel = pList5->GetNextItem(-1, LVNI_SELECTED);
        if (nSel != -1)
        {
            CMenu menu;
            menu.CreatePopupMenu();
            menu.AppendMenu(MF_STRING, 40002, _T("复制值"));
            int cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, this);
            if (cmd == 40002)
            {
                CString val = pList5->GetItemText(nSel, 1);
                if (!val.IsEmpty()) CopyToClipboard(m_hWnd, val);
            }
        }
        return;
    }
    // 右键在置顶窗口列表上 (取消置顶 / 删除)
    CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
    if (pList6 && hClicked == pList6->GetSafeHwnd())
    {
        int nSel = pList6->GetNextItem(-1, LVNI_SELECTED);
        if (nSel != -1)
        {
            CMenu menu;
            menu.CreatePopupMenu();
            menu.AppendMenu(MF_STRING, 32805, _T("取消置顶"));
            menu.AppendMenu(MF_STRING, 32807, _T("删除"));
            menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        }
        return;
    }

    // 右键在历史窗口列表上 (置顶 / 删除)
    CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
    if (pList7 && hClicked == pList7->GetSafeHwnd())
    {
        int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
        if (nSel != -1)
        {
            size_t idx = static_cast<size_t>(pList7->GetItemData(nSel));
            if (idx < m_historyWnds.size())
            {
                HWND hWnd = m_historyWnds[idx];
                bool bAlreadyTopmost = IsValidWindow(hWnd) &&
                    std::find(m_topmostWnds.begin(), m_topmostWnds.end(), hWnd) != m_topmostWnds.end();

                CMenu menu;
                menu.CreatePopupMenu();
                if (bAlreadyTopmost)
                    menu.AppendMenu(MF_STRING, 32810, _T("取消置顶"));
                else
                    menu.AppendMenu(MF_STRING, 32809, _T("置顶"));
                menu.AppendMenu(MF_STRING, 32808, _T("删除"));
                menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
            }
        }
        return;
    }
    // 右键在 git 工具列表上 (复制指令)
    CWnd* pList4 = GetDlgItem(IDC_LIST4);
    if (pList4 && hClicked == pList4->GetSafeHwnd())
    {
        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, 40001, _T("复制指令"));
        menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        return;
    }
}

// File management helper removed

// Named pipe listener: receives one-line file paths from DropHelper and posts to main window
// DropHelper and named pipe listener removed
















void CMFCApplication1Dlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CWnd* pWnd = CWnd::FromHandle(pScrollBar ? pScrollBar->m_hWnd : NULL);
    if (pWnd && pScrollBar && pScrollBar->GetSafeHwnd() == GetDlgItem(IDC_SLIDER1)->GetSafeHwnd())
    {
        CSliderCtrl* pSlider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER1);
        CEdit* pEditVol = (CEdit*)GetDlgItem(IDC_EDIT5);
        if (pSlider && pEditVol)
        {
            int pos = pSlider->GetPos();
            CString s; s.Format(_T("%d"), pos);
            pEditVol->SetWindowText(s);
            CVolumeManager::SetMasterVolumePercent(pos);
        }
    }
    else if (pScrollBar && pScrollBar->GetSafeHwnd() == GetDlgItem(IDC_SLIDER2)->GetSafeHwnd())
    {
        CSliderCtrl* pSlider2 = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER2));
        if (pSlider2)
        {
            int pos = pSlider2->GetPos();
            int percent = (pos * 100) / 255;
            CString label;
            label.Format(_T("透明度: %d%%"), percent);
            SetDlgItemText(IDC_STATIC18, label);

            if (m_hSelectedWnd && IsValidWindow(m_hSelectedWnd))
            {
                // 设置分层窗口透明度
                LONG style = ::GetWindowLong(m_hSelectedWnd, GWL_EXSTYLE);
                ::SetWindowLong(m_hSelectedWnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
                ::SetLayeredWindowAttributes(m_hSelectedWnd, 0, static_cast<BYTE>(pos), LWA_ALPHA);
            }
        }
    }

    CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}













// ===== 连点器统一启动/停止 =====

void CMFCApplication1Dlg::StartAutoClicker()
{
    int interval = AfxGetApp()->GetProfileInt(_T("AutoClicker"), _T("IntervalMs"), 100);

    // 读取触发键配置，默认 A/B
    CString strKey = AfxGetApp()->GetProfileString(_T("AutoClicker"), _T("KeyStart"), _T("A"));
    char keyStart = static_cast<char>(strKey.IsEmpty() ? 'A' : strKey[0]);
    strKey = AfxGetApp()->GetProfileString(_T("AutoClicker"), _T("KeyStop"), _T("B"));
    char keyStop = static_cast<char>(strKey.IsEmpty() ? 'B' : strKey[0]);
    m_autoClicker.SetKeys(keyStart, keyStop);
    m_autoClicker.Start(interval, m_hWnd);

    // 显示速度调节窗口
    if (!m_pSpeedDlg)
    {
        auto pDlg = std::make_unique<CAutoClickerSpeedDlg>(&m_autoClicker);
        pDlg->SetInterval(interval);
        pDlg->Create(IDD_CLICK_SPEED_DIALOG, this);
        pDlg->ShowWindow(SW_SHOW);
        m_pSpeedDlg = std::move(pDlg);
    }
}

void CMFCApplication1Dlg::StopAutoClicker()
{
    m_autoClicker.Stop();

    // 销毁速度调节窗口
    if (m_pSpeedDlg)
    {
        m_pSpeedDlg->DestroyWindow();
        m_pSpeedDlg.reset();
    }
}

// Handler for autoclick checkbox in main dialog (C++20: using CAutoClicker class)
void CMFCApplication1Dlg::OnBnClickedCheck4()
{
    CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK4));
    if (!pCheck) return;

    if (pCheck->GetCheck() == BST_CHECKED)
        StartAutoClicker();
    else
        StopAutoClicker();
}

// Message handler invoked when autoclick stops due to B key
afx_msg LRESULT CMFCApplication1Dlg::OnAutoClickStopped(WPARAM wParam, LPARAM lParam)
{
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK4);
    if (pCheck) pCheck->SetCheck(BST_UNCHECKED);

    // 销毁速度调节窗口
    if (m_pSpeedDlg)
    {
        m_pSpeedDlg->DestroyWindow();
        m_pSpeedDlg.reset();
    }

    // 托盘气泡通知（托盘已初始化才发送）
    if (m_bTrayVisible && m_nid.hWnd != NULL)
    {
        m_nid.uFlags = NIF_INFO;
        m_nid.dwInfoFlags = NIIF_INFO;
        _tcscpy_s(m_nid.szInfoTitle, _T("连点器"));
        _tcscpy_s(m_nid.szInfo, _T("连点已停止"));
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
    }

    return 0;
}

// 速度调节窗口关闭回调：清空指针
afx_msg LRESULT CMFCApplication1Dlg::OnSpeedDlgClosed(WPARAM wParam, LPARAM lParam)
{
    // 同步 CHECK4 状态
    CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK4));
    if (pCheck) pCheck->SetCheck(BST_UNCHECKED);

    m_pSpeedDlg.reset();
    return 0;
}






















/////////////////////////////////////////////////////////////////////////////
// CMFCApplication1Dlg menu command handlers

void CMFCApplication1Dlg::OnFileSettings()
{
    auto* pDlg = new CSettingsDlg(nullptr);
    pDlg->Create(IDD_SETTINGS_DIALOG, nullptr);
    pDlg->ShowWindow(SW_SHOW);
}

void CMFCApplication1Dlg::OnFileExit()
{
    DestroyWindow();
}

void CMFCApplication1Dlg::OnHelpAbout()
{
    CAboutDlg dlgAbout;
    dlgAbout.DoModal();
}

void CMFCApplication1Dlg::OnHelpRegexGuide()
{
    auto* pDlg = new CRegexGuideDlg(nullptr);
    pDlg->Create(IDD_REGEX_GUIDE_DLG, nullptr);
    pDlg->ShowWindow(SW_SHOW);
}

void CMFCApplication1Dlg::OnToolsQRCode()
{
    auto* pDlg = new CQRCodeGenDlg(nullptr);
    pDlg->Create(IDD_QRCODE_DLG, nullptr);
    pDlg->ShowWindow(SW_SHOW);
}

void CMFCApplication1Dlg::OnToolsScreenshotOCR()
{
    auto* pDlg = new CScreenshotOCRDlg(nullptr, &m_autoClicker);
    pDlg->Create(IDD_SCREENSHOT_OCR_DLG, nullptr);
    pDlg->ShowWindow(SW_SHOW);
}

void CMFCApplication1Dlg::OnToolsBatchRename()
{
    auto* pDlg = new CBatchRenameDlg(nullptr);
    pDlg->Create(IDD_BATCH_RENAME_DLG, nullptr);
    pDlg->ShowWindow(SW_SHOW);
}




