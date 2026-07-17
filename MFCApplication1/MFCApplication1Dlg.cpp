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
#include "AutoClickerSpeedDlg.h"
#include "SettingsDlg.h"
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

// Trigger next track in Bilibili player if found; otherwise send global media next key as fallback
void CMFCApplication1Dlg::OnBiliNext()
{
    // Helper: find candidate window(s) that likely belong to Bilibili. Collect all candidates
    // and prefer the one with the largest HWND value when multiple exist.
    auto FindBiliWindow = [this]() -> HWND {
        std::vector<HWND> candidates;
        for (HWND h = ::GetTopWindow(NULL); h != NULL; h = ::GetNextWindow(h, GW_HWNDNEXT))
        {
            if (!::IsWindowVisible(h)) continue;

            TCHAR title[512] = {0};
            ::GetWindowText(h, title, _countof(title));
            CString sTitle = title;
            sTitle.MakeLower();
            bool matched = false;
            if (sTitle.Find(_T("哔哩")) != -1 || sTitle.Find(_T("bilibili")) != -1 || sTitle.Find(_T("b站")) != -1)
                matched = true;

            // check process image path for name containing bilibili
            if (!matched)
            {
                DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
                if (pid != 0)
                {
                    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                    if (hProc)
                    {
                        TCHAR buf[MAX_PATH] = {0};
                        DWORD sz = _countof(buf);
                        if (QueryFullProcessImageName(hProc, 0, buf, &sz))
                        {
                            CString p = buf; p.MakeLower();
                            if (p.Find(_T("bilibili")) != -1 || p.Find(_T("bili")) != -1)
                                matched = true;
                        }
                        CloseHandle(hProc);
                    }
                }
            }

            if (matched)
                candidates.push_back(h);
        }

        if (candidates.empty()) return NULL;

        // If only one candidate, return it. If multiple, pick the one with smallest HWND value.
        HWND best = candidates.front();
        if (candidates.size() > 1)
        {
            for (HWND c : candidates)
            {
                if ((UINT_PTR)c < (UINT_PTR)best) best = c;
            }
        }
        return best;
    };

    // Do NOT prefer user-selected window; instead select candidate by smallest HWND value
    HWND hBili = FindBiliWindow();
    if (hBili)
    {
        // Bring the player window to foreground (restore if minimized), send ']' key, then re-minimize.
        // Try to set foreground safely by attaching thread input.
        if (::IsIconic(hBili)) ::ShowWindow(hBili, SW_RESTORE);

        DWORD curTid = GetCurrentThreadId();
        DWORD targetPid = 0; DWORD targetTid = GetWindowThreadProcessId(hBili, &targetPid);
        // attach input threads to allow SetForegroundWindow
        AttachThreadInput(curTid, targetTid, TRUE);
        ::SetForegroundWindow(hBili);
        ::SetActiveWindow(hBili);
        AttachThreadInput(curTid, targetTid, FALSE);

        // small delay to ensure window receives focus
        Sleep(120);

        // Determine virtual-key and modifier from current layout for ']'
        SHORT vkAndState = VkKeyScanW((WCHAR)']');
        BYTE vk = LOBYTE(vkAndState);
        BYTE shiftState = HIBYTE(vkAndState);

        // Build inputs: press modifiers, keydown, keyup, release modifiers
        std::vector<INPUT> inputs;
        inputs.reserve(6);
        auto pushKey = [&](WORD vkCode, DWORD flags){ INPUT in = {}; in.type = INPUT_KEYBOARD; in.ki.wVk = vkCode; in.ki.dwFlags = flags; inputs.push_back(in); };

        // modifiers: SHIFT (1), CTRL (2), ALT (4) per VkKeyScan return
        if (shiftState & 1) pushKey(VK_SHIFT, 0);
        if (shiftState & 2) pushKey(VK_CONTROL, 0);
        if (shiftState & 4) pushKey(VK_MENU, 0);

        // key down + up
        if (vk != 0xFF)
        {
            pushKey(vk, 0);
            pushKey(vk, KEYEVENTF_KEYUP);
        }

        // release modifiers in reverse order
        if (shiftState & 4) pushKey(VK_MENU, KEYEVENTF_KEYUP);
        if (shiftState & 2) pushKey(VK_CONTROL, KEYEVENTF_KEYUP);
        if (shiftState & 1) pushKey(VK_SHIFT, KEYEVENTF_KEYUP);

        if (!inputs.empty()) SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));

        // give the app a moment to process and then minimize it again
        Sleep(80);
        ::ShowWindow(hBili, SW_MINIMIZE);
        return;
    }

    // Fallback: send a global media next key via SendInput
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_MEDIA_NEXT_TRACK;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_MEDIA_NEXT_TRACK;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

// Command handler for context menu "复制命令"
void CMFCApplication1Dlg::OnCopyGitCommand()
{
    CWnd* pWnd = GetDlgItem(IDC_LIST4);
    if (!pWnd || !IsValidWindow(pWnd->GetSafeHwnd())) return;
    TCHAR cls[64] = {0};
    GetClassName(pWnd->GetSafeHwnd(), cls, _countof(cls));
    CString className = cls;
    if (className.CompareNoCase(_T("SysListView32")) == 0)
    {
        CListCtrl* pListCtrl = (CListCtrl*)pWnd;
        int idx = pListCtrl->GetNextItem(-1, LVNI_SELECTED);
        if (idx != -1)
        {
            CString cmd = pListCtrl->GetItemText(idx, 1);
            if (cmd.IsEmpty())
            {
                CString combined = pListCtrl->GetItemText(idx, 0);
                int sep = combined.Find(_T('|'));
                if (sep != -1) cmd = combined.Mid(sep + 1);
            }
            CopyToClipboard(m_hWnd, cmd);
        }
    }
    else if (className.CompareNoCase(_T("ListBox")) == 0)
    {
        CListBox* pBox = (CListBox*)pWnd;
        int sel = pBox->GetCurSel();
        if (sel != LB_ERR)
        {
            CString item; pBox->GetText(sel, item);
            int sep = item.Find(_T('|'));
            CString cmd;
            if (sep != -1) cmd = item.Mid(sep + 1);
            else
            {
                sep = item.Find(_T('\t'));
                if (sep != -1) cmd = item.Mid(sep + 1);
                else cmd = item;
            }
            CopyToClipboard(m_hWnd, cmd);
        }
    }
}

// File management (drag/drop) and DropHelper removed

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Define message constants declared in header (now constexpr in header, so no need to redefine here)
// Autoclicker message is defined in the dialog header as WM_AUTOCLICK_STOPPED

// Autoclicker functionality moved to CAutoClicker class (AutoClicker.h/cpp)

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}



// Master volume helpers using Core Audio EndpointVolume (0-100)
// Async volume retrieval: run in background and post WM_VOLUME_UPDATED with percent in WPARAM
void CMFCApplication1Dlg::OnBnClickedButton10()
{
    CString url = AfxGetApp()->GetProfileString(_T("Sites"), _T("Sducs"), _T(""));
    ::ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}

// Next-track feature removed

void CMFCApplication1Dlg::OnBnClickedButton1()
{
    // Execute: determine selection
    CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO1);
    int sel = 0;
    if (pCombo) sel = pCombo->GetCurSel();

    if (sel == 0)
    {
        // 1分钟后重启
        ::ShellExecute(NULL, _T("open"), _T("shutdown.exe"), _T("/r /t 60"), NULL, SW_HIDE);
    }
    else if (sel == 1)
    {
        // 默认 3 分钟关机
        ::ShellExecute(NULL, _T("open"), _T("shutdown.exe"), _T("/s /t 180"), NULL, SW_HIDE);
    }
    else if (sel == 2)
    {
        // 设定时间关机
        int seconds = ParseShutdownSeconds(this);
        CString cmd;
        cmd.Format(_T("/s /t %d"), seconds);
        ::ShellExecute(NULL, _T("open"), _T("shutdown.exe"), cmd, NULL, SW_HIDE);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton8()
{
    CString strPath = GetOrAskPath(this, _T("BiliPath"), _T("选择哔哩哔哩可执行文件"), false);

    if (!strPath.IsEmpty() && GetFileAttributes(strPath) != INVALID_FILE_ATTRIBUTES)
    {
        ::ShellExecute(NULL, _T("open"), strPath, NULL, NULL, SW_SHOWNORMAL);
    }
    else
    {
        MessageBox(_T("指定的哔哩哔哩可执行文件未找到或未设置。请先设置路径。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton2()
{
    // 解除关机/重启
    ::ShellExecute(NULL, _T("open"), _T("shutdown.exe"), _T("/a"), NULL, SW_HIDE);
}

void CMFCApplication1Dlg::OnCbnSelchangeCombo1()
{
    // Use the selected text instead of index because the combo may be
    // created with CBS_SORT or items may reorder. Enable edits only when
    // the selected string indicates "设定时间".
    CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO1);
    CString selText;
    if (pCombo)
    {
        int sel = pCombo->GetCurSel();
        if (sel != CB_ERR)
            pCombo->GetLBText(sel, selText);
    }

    CEdit* pE1 = (CEdit*)GetDlgItem(IDC_EDIT1);
    CEdit* pE2 = (CEdit*)GetDlgItem(IDC_EDIT2);
    CEdit* pE3 = (CEdit*)GetDlgItem(IDC_EDIT3);

    BOOL enable = (selText.Find(_T("设定")) != -1);

    if (pE1)
    {
        pE1->SetReadOnly(!enable);
        pE1->EnableWindow(enable);
        if (enable) pE1->SetFocus();
    }
    if (pE2)
    {
        pE2->SetReadOnly(!enable);
        pE2->EnableWindow(enable);
    }
    if (pE3)
    {
        pE3->SetReadOnly(!enable);
        pE3->EnableWindow(enable);
    }
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()



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
    // Drop helper message removed
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
    ON_COMMAND(ID_WINDOW_LOCATE,    &CMFCApplication1Dlg::OnWindowLocate)
    ON_COMMAND(ID_WINDOW_UNTOPMOST, &CMFCApplication1Dlg::OnWindowUntopmost)
    ON_COMMAND(ID_WINDOW_CLOSE,     &CMFCApplication1Dlg::OnWindowClose)
    ON_COMMAND(ID_HELP_SHORTCUTS,   &CMFCApplication1Dlg::OnHelpShortcuts)
    ON_COMMAND(ID_HELP_GITHUB,      &CMFCApplication1Dlg::OnHelpGithub)
    ON_BN_CLICKED(IDC_BUTTON21, &CMFCApplication1Dlg::OnBnClickedButton21)
    ON_BN_CLICKED(IDC_BUTTON22, &CMFCApplication1Dlg::OnBnClickedButton22)
    ON_BN_CLICKED(IDC_BUTTON23, &CMFCApplication1Dlg::OnBnClickedButton23)
    ON_BN_CLICKED(IDC_BUTTON24, &CMFCApplication1Dlg::OnBnClickedButton24)
    ON_BN_CLICKED(IDC_BUTTON25, &CMFCApplication1Dlg::OnBnClickedButton25)
    ON_BN_CLICKED(IDC_BUTTON26, &CMFCApplication1Dlg::OnBnClickedButton26)
    // ON_BN_CLICKED(IDC_BUTTON27, &CMFCApplication1Dlg::OnBnClickedButton27) // This button handler has been removed
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

// ========== 标签页初始化辅助函数 ==========

void CMFCApplication1Dlg::InitTabControl()
{
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (!pTab) return;

	pTab->InsertItem(0, _T("进程管理"));
	pTab->InsertItem(1, _T("启动项管理"));
	pTab->InsertItem(2, _T("剪贴板"));
	pTab->InsertItem(3, _T("窗口处理"));
	pTab->InsertItem(4, _T("文件管理"));
	pTab->InsertItem(5, _T("git工具箱"));
}

void CMFCApplication1Dlg::InitProcessTab()
{
	CListCtrl* pList1 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST1));
	if (!pList1) return;

	pList1->ModifyStyle(0, LVS_REPORT);
	pList1->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
	pList1->InsertColumn(0, _T("进程名"), LVCFMT_LEFT, 200);
	pList1->InsertColumn(1, _T("PID"), LVCFMT_LEFT, 160);
	pList1->InsertColumn(2, _T("路径"), LVCFMT_LEFT, 600);
	pList1->InsertColumn(3, _T("内存(KB)"), LVCFMT_RIGHT, 200);
}

void CMFCApplication1Dlg::InitStartupTab()
{
	CListCtrl* pList2 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST2));
	if (!pList2) return;

	pList2->ModifyStyle(0, LVS_REPORT);
	pList2->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
	pList2->InsertColumn(0, _T("启动项名"), LVCFMT_LEFT, 200);
	pList2->InsertColumn(1, _T("命令(路径)"), LVCFMT_LEFT, 840);
}

void CMFCApplication1Dlg::InitClipboardTab()
{
	CListCtrl* pList3 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST3));
	if (!pList3) return;

	pList3->ModifyStyle(0, LVS_REPORT);
	pList3->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
	pList3->InsertColumn(0, _T("文本内容(双击复制)"), LVCFMT_LEFT, 780);
}

void CMFCApplication1Dlg::InitWindowTab()
{
	CListCtrl* pList5 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST5));
	if (pList5)
	{
		pList5->ModifyStyle(0, LVS_REPORT);
		pList5->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
		pList5->InsertColumn(0, _T("字段"), LVCFMT_LEFT, 84);
		pList5->InsertColumn(1, _T("值"), LVCFMT_LEFT, 980);
	}

	// 初始化透明度滑块（资源控件）
	CSliderCtrl* pSlider2 = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER2));
	if (pSlider2)
	{
		pSlider2->SetRange(10, 255);
		pSlider2->SetPos(255);
		pSlider2->SetTicFreq(25);
	}
	SetDlgItemText(IDC_STATIC18, _T("透明度: 100%"));

	// 初始化置顶窗口列表
	CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
	if (pList6)
	{
		pList6->ModifyStyle(0, LVS_REPORT);
		pList6->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		pList6->InsertColumn(0, _T("类型"), LVCFMT_LEFT, 80);
		pList6->InsertColumn(1, _T("窗口标题"), LVCFMT_LEFT, 280);
	}

	// 初始化历史定位窗口列表
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (pList7)
	{
		pList7->ModifyStyle(0, LVS_REPORT);
		pList7->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		pList7->InsertColumn(0, _T("序号"), LVCFMT_LEFT, 60);
		pList7->InsertColumn(1, _T("窗口标题"), LVCFMT_LEFT, 280);
	}
}

void CMFCApplication1Dlg::InitFileTab()
{
	// 文件管理标签页的控件在资源编辑器中定义，此处仅做占位
	// 实际可见性由 UpdateTabVisibility 管理
}

void CMFCApplication1Dlg::InitGitTab()
{
	CListCtrl* pList4 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST4));
	if (!pList4) return;

	pList4->ModifyStyle(0, LVS_REPORT);
	pList4->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
	pList4->InsertColumn(0, _T("说明"), LVCFMT_LEFT, 190);
	pList4->InsertColumn(1, _T("命令"), LVCFMT_LEFT, 400);

	// 如果 config.ini 不存在，创建默认 Git 命令
	const TCHAR* section = _T("GitCommands");
	TCHAR exePath[MAX_PATH] = {0};
	GetModuleFileName(NULL, exePath, MAX_PATH);
	CString exeDir = exePath;
	int p = exeDir.ReverseFind(_T('\\'));
	CString configPath;
	if (p != -1) configPath.Format(_T("%s\\config.ini"), exeDir.Left(p));
	else configPath = _T("config.ini");

	if (GetFileAttributes(configPath) == INVALID_FILE_ATTRIBUTES)
	{
		WritePrivateProfileString(section, _T("Cmd1"),  _T("初始化本地仓库|git init"), configPath);
		WritePrivateProfileString(section, _T("Cmd2"),  _T("添加所有文件到暂存区|git add ."), configPath);
		WritePrivateProfileString(section, _T("Cmd3"),  _T("提交到本地仓库|git commit -m \"第一次提交\""), configPath);
		WritePrivateProfileString(section, _T("Cmd4"),  _T("添加远程仓库地址|git remote add origin <地址>"), configPath);
		WritePrivateProfileString(section, _T("Cmd5"),  _T("重命名分支为main|git branch -M main"), configPath);
		WritePrivateProfileString(section, _T("Cmd6"),  _T("首次推送并建立关联|git push -u origin main"), configPath);
		WritePrivateProfileString(section, _T("Cmd7"),  _T("拉取远程更新|git pull"), configPath);
		WritePrivateProfileString(section, _T("Cmd8"),  _T("克隆远程仓库到本地|git clone <地址>"), configPath);
		WritePrivateProfileString(section, _T("Cmd9"),  _T("查看当前状态|git status"), configPath);
		WritePrivateProfileString(section, _T("Cmd10"), _T("添加所有修改到暂存区|git add ."), configPath);
		WritePrivateProfileString(section, _T("Cmd11"), _T("提交到本地仓库|git commit -m \"说明\""), configPath);
		WritePrivateProfileString(section, _T("Cmd12"), _T("推送到远程仓库|git push"), configPath);
		WritePrivateProfileString(section, _T("Cmd13"), _T("查看所有分支含远程|git branch -a"), configPath);
		WritePrivateProfileString(section, _T("Cmd14"), _T("创建并切换分支|git checkout -b <分支名>"), configPath);
		WritePrivateProfileString(section, _T("Cmd15"), _T("切换分支|git checkout <分支名>"), configPath);
		WritePrivateProfileString(section, _T("Cmd16"), _T("合并指定分支到当前分支|git merge <分支名>"), configPath);
		WritePrivateProfileString(section, _T("Cmd17"), _T("删除已合并的分支|git branch -d <分支名>"), configPath);
		WritePrivateProfileString(section, _T("Cmd18"), _T("查看简洁版提交历史|git log --oneline"), configPath);
		WritePrivateProfileString(section, _T("Cmd19"), _T("撤销工作区修改|git restore <文件>"), configPath);
		WritePrivateProfileString(section, _T("Cmd20"), _T("把暂存区文件撤回来|git restore --staged <文件>"), configPath);
	}

	// 从 INI 加载命令列表
	pList4->DeleteAllItems();
	for (int i = 1; i <= 99; ++i)
	{
		CString key; key.Format(_T("Cmd%d"), i);
		TCHAR buf[1024] = {0};
		GetPrivateProfileString(section, key, _T(""), buf, 1024, configPath);
		CString val = buf;
		if (val.IsEmpty()) break;
		int sep = val.Find(_T('|'));
		CString desc, cmd;
		if (sep != -1) { desc = val.Left(sep); cmd = val.Mid(sep + 1); }
		else { desc = val; cmd = _T(""); }
		int idx = pList4->InsertItem(i - 1, desc);
		pList4->SetItemText(idx, 1, cmd);
	}
}

// ========== 统一标签页可见性管理 ==========

void CMFCApplication1Dlg::UpdateTabVisibility(int nTab)
{
	// 列表控件
	CListCtrl* pList1 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST1));
	CListCtrl* pList2 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST2));
	CListCtrl* pList3 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST3));
	CListCtrl* pList4 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST4));
	CListCtrl* pList5 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST5));

	if (pList1) pList1->ShowWindow(nTab == 0 ? SW_SHOW : SW_HIDE);
	if (pList2) pList2->ShowWindow(nTab == 1 ? SW_SHOW : SW_HIDE);
	if (pList3) pList3->ShowWindow(nTab == 2 ? SW_SHOW : SW_HIDE);
	if (pList4) pList4->ShowWindow(nTab == 5 ? SW_SHOW : SW_HIDE);
	if (pList5)
	{
		pList5->ShowWindow(nTab == 3 ? SW_SHOW : SW_HIDE);
		if (nTab == 3)
		{
			::SetWindowPos(pList5->GetSafeHwnd(), HWND_TOP, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
			pList5->BringWindowToTop();
		}
	}

	// 窗口处理标签页按钮
	CWnd* pBtn19 = GetDlgItem(IDC_BUTTON19);
	CWnd* pStatic12 = GetDlgItem(IDC_STATIC12);
	if (pBtn19) pBtn19->ShowWindow(nTab == 3 ? SW_SHOW : SW_HIDE);
	if (pStatic12) pStatic12->ShowWindow(nTab == 3 ? SW_SHOW : SW_HIDE);

	// Git 工具按钮
	CWnd* pBtn30 = GetDlgItem(IDC_BUTTON30);
	CWnd* pBtn31 = GetDlgItem(IDC_BUTTON31);
	CWnd* pBtn32 = GetDlgItem(IDC_BUTTON32);
	if (pBtn30) pBtn30->ShowWindow(nTab == 5 ? SW_SHOW : SW_HIDE);
	if (pBtn31) pBtn31->ShowWindow(nTab == 5 ? SW_SHOW : SW_HIDE);
	if (pBtn32)
	{
		if (nTab == 5)
		{
			pBtn32->ShowWindow(SW_SHOW);
			pBtn32->EnableWindow(TRUE);
			pBtn32->BringWindowToTop();
			pBtn32->SetWindowPos(&wndTop, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
		}
		else
		{
			pBtn32->ShowWindow(SW_HIDE);
		}
	}

	// 文件管理控件
	BOOL showFile = (nTab == 4);
	CWnd* pStaticPath = GetDlgItem(IDC_STATIC_PATH);
	CWnd* pEdit4 = GetDlgItem(IDC_EDIT4);
	CWnd* pBtn3 = GetDlgItem(IDC_BUTTON3);
	CWnd* pStatic7 = GetDlgItem(IDC_STATIC7);
	CWnd* pStatic13 = GetDlgItem(IDC_STATIC13);
	CWnd* pEdit7 = GetDlgItem(IDC_EDIT7);
	CWnd* pEdit8 = GetDlgItem(IDC_EDIT8);
	CWnd* pBtn23 = GetDlgItem(IDC_BUTTON23);
	CWnd* pBtn24 = GetDlgItem(IDC_BUTTON24);
	CWnd* pStatic14 = GetDlgItem(IDC_STATIC14);
	CWnd* pBtn25 = GetDlgItem(IDC_BUTTON25);
	CWnd* pBtn26 = GetDlgItem(IDC_BUTTON26);
	CWnd* pBrowse = GetDlgItem(IDC_MFCEDITBROWSE2);

	if (pStaticPath) pStaticPath->ShowWindow((nTab == 4 || nTab == 5) ? SW_SHOW : SW_HIDE);
	if (pEdit4) pEdit4->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
	if (pBtn3) pBtn3->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
	if (pStatic7) pStatic7->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
	if (pStatic13) pStatic13->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
	if (pEdit7) pEdit7->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
	if (pEdit8) pEdit8->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
	if (pBtn23) pBtn23->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
	if (pBtn24) pBtn24->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
	if (pStatic14) pStatic14->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
	if (pBtn25) pBtn25->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
	if (pBtn26) pBtn26->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
	if (pBrowse) pBrowse->ShowWindow(showFile ? SW_SHOW : SW_HIDE);

	if (showFile || nTab == 5)
	{
		CString stDisplay = m_strDroppedFilePath.IsEmpty() ? CString(_T("拖拽文件到此")) : m_strDroppedFilePath;
		SetDlgItemText(IDC_STATIC_PATH, stDisplay);
	}

	// 窗口处理标签页：填充窗口信息
	if (nTab == 3 && pList5)
	{
		LoadWindowDetailToList5(m_hSelectedWnd);
	}

	// 窗口处理标签页的新控件可见性
	BOOL showWindowTab = (nTab == 3);
	CWnd* pSlider2 = GetDlgItem(IDC_SLIDER2);
	CWnd* pStatic18 = GetDlgItem(IDC_STATIC18);
	CWnd* pBtn15 = GetDlgItem(IDC_BUTTON15);
	CWnd* pBtn16 = GetDlgItem(IDC_BUTTON16);
	if (pSlider2) pSlider2->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);
	if (pStatic18) pStatic18->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);
	if (pBtn15) pBtn15->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);
	if (pBtn16) pBtn16->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);

	// 切换到窗口管理tab时，同步工具箱自身置顶复选框状态
	if (showWindowTab)
	{
		CButton* pCheck3 = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
		if (pCheck3)
		{
			auto it = std::find(m_topmostWnds.begin(), m_topmostWnds.end(), m_hWnd);
			pCheck3->SetCheck(it != m_topmostWnds.end() ? BST_CHECKED : BST_UNCHECKED);
		}
	}

	// 置顶窗口列表 (LIST6)
	CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
	if (pList6)
	{
		pList6->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);
		if (showWindowTab)
		{
			pList6->DeleteAllItems();
			int row = 0;
			for (size_t i = 0; i < m_topmostWnds.size(); ++i)
			{
				HWND h = m_topmostWnds[i];
				if (!IsValidWindow(h)) continue;

				TCHAR title[256] = {0};
				::GetWindowText(h, title, _countof(title));

				CString label;
				label.Format(_T("置顶[%zu]"), i + 1);
				int idx = pList6->InsertItem(row, label);
				pList6->SetItemText(idx, 1, CString(title));
				pList6->SetItemData(idx, static_cast<DWORD_PTR>(i));
				++row;
			}
		}
	}

	// 历史定位窗口列表 (LIST7)
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (pList7)
	{
		pList7->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);
		if (showWindowTab)
		{
			pList7->DeleteAllItems();
			int row = 0;
			for (size_t i = 0; i < m_historyWnds.size(); ++i)
			{
				HWND h = m_historyWnds[i];
				if (!IsValidWindow(h)) continue;

				TCHAR title[256] = {0};
				::GetWindowText(h, title, _countof(title));

				CString label;
				label.Format(_T("%zu"), row + 1);
				int idx = pList7->InsertItem(row, label);
				pList7->SetItemText(idx, 1, CString(title));
				pList7->SetItemData(idx, static_cast<DWORD_PTR>(i));  // 存储真实索引
				++row;
			}
		}
	}
}

// ========== 窗口处理新功能 ==========


void CMFCApplication1Dlg::OnViewProcess()
{
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) { pTab->SetCurSel(0); UpdateTabVisibility(0); }
}

void CMFCApplication1Dlg::OnViewStartup()
{
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) { pTab->SetCurSel(1); UpdateTabVisibility(1); RefreshStartupList(); }
}

void CMFCApplication1Dlg::OnViewClipboard()
{
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) { pTab->SetCurSel(2); UpdateTabVisibility(2); }
}

void CMFCApplication1Dlg::OnViewWindow()
{
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) { pTab->SetCurSel(3); UpdateTabVisibility(3); }
}

void CMFCApplication1Dlg::OnViewFile()
{
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) { pTab->SetCurSel(4); UpdateTabVisibility(4); }
}

void CMFCApplication1Dlg::OnViewGit()
{
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) { pTab->SetCurSel(5); UpdateTabVisibility(5); }
}



void CMFCApplication1Dlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    // 当窗口被最小化时，隐藏主窗口并显示托盘图标
    if (nType == SIZE_MINIMIZED)
    {
        // 创建托盘图标
        if (!m_bTrayVisible)
        {
            ZeroMemory(&m_nid, sizeof(m_nid));
            m_nid.cbSize = sizeof(m_nid);
            m_nid.hWnd = m_hWnd;
            m_nid.uID = 1001;
            m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
            m_nid.uCallbackMessage = WM_TRAYICON;
            m_nid.hIcon = m_hIcon;
            _tcscpy_s(m_nid.szTip, _countof(m_nid.szTip), _T("MFCApplication1"));
            Shell_NotifyIcon(NIM_ADD, &m_nid);
            m_bTrayVisible = true;
        }

        ShowWindow(SW_HIDE);
    }
}

LRESULT CMFCApplication1Dlg::OnTrayNotification(WPARAM wParam, LPARAM lParam)
{
    if (wParam != 1001) return 0;

    if (lParam == WM_RBUTTONUP)
    {
        // 弹出托盘菜单
        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, 2001, _T("显示窗口"));
        menu.AppendMenu(MF_STRING, 2002, _T("退出程序"));

        POINT pt;
        GetCursorPos(&pt);
        ::SetForegroundWindow(m_hWnd);
        menu.TrackPopupMenu(TPM_RIGHTBUTTON, pt.x, pt.y, this);
        PostMessage(WM_NULL, 0, 0);
    }
    else if (lParam == WM_LBUTTONDBLCLK)
    {
        // 双击恢复
        ShowWindow(SW_SHOW);
        ShowWindow(SW_RESTORE);
        if (m_bTrayVisible)
        {
            Shell_NotifyIcon(NIM_DELETE, &m_nid);
            m_bTrayVisible = false;
        }
    }

    return 0;
}

void CMFCApplication1Dlg::OnTrayShowWindow()
{
    ShowWindow(SW_SHOW);
    ShowWindow(SW_RESTORE);
    if (m_bTrayVisible)
    {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        m_bTrayVisible = false;
    }
}

void CMFCApplication1Dlg::OnTrayExit()
{
    // 删除托盘图标并退出程序
    if (m_bTrayVisible)
    {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        m_bTrayVisible = false;
    }
    m_bExiting = true;
    EndDialog(IDOK);
}

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

void CMFCApplication1Dlg::OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2)
{
    if (nHotKeyId == 1001)
    {
        // Toggle minimize/restore when Ctrl+Alt+Space pressed.
        // If currently not minimized to tray, create tray icon and hide window.
        if (!m_bTrayVisible && ::IsWindowVisible(m_hWnd))
        {
            ZeroMemory(&m_nid, sizeof(m_nid));
            m_nid.cbSize = sizeof(m_nid);
            m_nid.hWnd = m_hWnd;
            m_nid.uID = 1001;
            m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
            m_nid.uCallbackMessage = WM_TRAYICON;
            m_nid.hIcon = m_hIcon;
            _tcscpy_s(m_nid.szTip, _countof(m_nid.szTip), _T("MFCApplication1"));
            Shell_NotifyIcon(NIM_ADD, &m_nid);
            m_bTrayVisible = true;
            // hide window (minimized to tray)
            ShowWindow(SW_HIDE);
        }
        else
        {
            // If already in tray (or window not visible), restore and remove tray icon
            ShowWindow(SW_SHOW);
            ShowWindow(SW_RESTORE);
            if (m_bTrayVisible)
            {
                Shell_NotifyIcon(NIM_DELETE, &m_nid);
                m_bTrayVisible = false;
            }
            ::SetForegroundWindow(m_hWnd);
        }
        return; // handled
    }
    CDialogEx::OnHotKey(nHotKeyId, nKey1, nKey2);
}

void CMFCApplication1Dlg::OnClose()
{
    // 根据 m_bMinimizeOnClose 决定关闭时是最小化到托盘还是直接退出
    if (m_bMinimizeOnClose)
    {
        ShowWindow(SW_MINIMIZE);
    }
    else
    {
        // 清理托盘图标并调用默认关闭流程
        if (m_bTrayVisible)
        {
            Shell_NotifyIcon(NIM_DELETE, &m_nid);
            m_bTrayVisible = false;
        }
        // 取消剪贴板监听
        ::RemoveClipboardFormatListener(m_hWnd);
        // 清理可能存在的捕获窗口
        if (m_hCaptureWnd && IsValidWindow(m_hCaptureWnd))
        {
            ::DestroyWindow(m_hCaptureWnd);
            m_hCaptureWnd = NULL;
        }
        // clear prevent-lock state if set
        if (m_bPreventLockScreen)
        {
            SetThreadExecutionState(ES_CONTINUOUS);
            m_bPreventLockScreen = false;
        }
        CDialogEx::OnClose();
    }
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

void CMFCApplication1Dlg::OnTcnSelchangeTab1(NMHDR *pNMHDR, LRESULT *pResult)
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
    if (pTab)
    {
        int nSel = pTab->GetCurSel();
        UpdateTabVisibility(nSel);
        if (nSel == 1)
            RefreshStartupList();
    }
    *pResult = 0;
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

void CMFCApplication1Dlg::OnBnClickedButton4()
{
    // 1. 通过遍历进程检查微信是否在运行
    bool bIsWeChatRunning = false;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hProcessSnap, &pe32))
        {
            do
            {
                CString strExeFile = pe32.szExeFile;
                strExeFile.MakeLower(); // 转换为小写以便匹配
                // 兼容旧版 WeChat.exe 和新版 Weixin.exe
                if (strExeFile == _T("wechat.exe") || strExeFile == _T("weixin.exe"))
                {
                    bIsWeChatRunning = true;
                    break;
                }
            } while (Process32Next(hProcessSnap, &pe32));
        }
        CloseHandle(hProcessSnap);
    }

    if (bIsWeChatRunning)
    {
        // 微信已运行，模拟发送微信默认的全局快捷键 Ctrl + Alt + W
        INPUT inputs[6] = { 0 };

        // 1. 按下 Ctrl
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_CONTROL;

        // 2. 按下 Alt
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_MENU;

        // 3. 按下 W
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'W';

        // 4. 松开 W
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = 'W';
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

        // 5. 松开 Alt
        inputs[4].type = INPUT_KEYBOARD;
        inputs[4].ki.wVk = VK_MENU;
        inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;

        // 6. 松开 Ctrl
        inputs[5].type = INPUT_KEYBOARD;
        inputs[5].ki.wVk = VK_CONTROL;
        inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

        // 发送输入快捷键唤醒/隐藏微信
        SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
    }
    else
    {
        // 如果没有运行，通过保存的路径启动它
        CString strWeChatPath = _T("D:\\微信\\Weixin\\Weixin.exe");

        // 检查文件是否存在
        if (GetFileAttributes(strWeChatPath) != INVALID_FILE_ATTRIBUTES)
        {
            ::ShellExecute(NULL, _T("open"), strWeChatPath, NULL, NULL, SW_SHOWNORMAL);
        }
        else
        {
        // try to ask user to locate and save
        CString path = GetOrAskPath(this, _T("WeChatPath"), _T("选择微信可执行文件"), false);
        if (!path.IsEmpty()) ::ShellExecute(NULL, _T("open"), path, NULL, NULL, SW_SHOWNORMAL);
        else MessageBox(_T("指定的微信可执行文件未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
        }
    }
}

void CMFCApplication1Dlg::OnBnClickedButton5()
{
    // 1. 通过遍历进程检查QQ是否在运行
    bool bIsQQRunning = false;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hProcessSnap, &pe32))
        {
            do
            {
                CString strExeFile = pe32.szExeFile;
                strExeFile.MakeLower(); // 转换为小写以便匹配
                if (strExeFile == _T("qq.exe"))
                {
                    bIsQQRunning = true;
                    break;
                }
            } while (Process32Next(hProcessSnap, &pe32));
        }
        CloseHandle(hProcessSnap);
    }

    if (bIsQQRunning)
    {
        // QQ已运行，模拟发送快捷键 Ctrl + Alt + X
        INPUT inputs[6] = { 0 };

        // 1. 按下 Ctrl
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_CONTROL;

        // 2. 按下 Alt
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_MENU;

        // 3. 按下 X
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'X';

        // 4. 松开 X
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = 'X';
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

        // 5. 松开 Alt
        inputs[4].type = INPUT_KEYBOARD;
        inputs[4].ki.wVk = VK_MENU;
        inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;

        // 6. 松开 Ctrl
        inputs[5].type = INPUT_KEYBOARD;
        inputs[5].ki.wVk = VK_CONTROL;
        inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

        // 发送输入
        SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
    }
    else
    {
        // 如果没有运行，通过保存的路径启动它
        CString strQQPath = _T("D:\\QQ\\QQ.exe");

        // 检查文件是否存在
        if (GetFileAttributes(strQQPath) != INVALID_FILE_ATTRIBUTES)
        {
            ::ShellExecute(NULL, _T("open"), strQQPath, NULL, NULL, SW_SHOWNORMAL);
        }
        else
        {
        CString path = GetOrAskPath(this, _T("QQPath"), _T("选择QQ可执行文件"), false);
        if (!path.IsEmpty()) ::ShellExecute(NULL, _T("open"), path, NULL, NULL, SW_SHOWNORMAL);
        else MessageBox(_T("指定的QQ可执行文件未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
        }
    }
}

void CMFCApplication1Dlg::OnBnClickedButton6()
{
    CString strPath = _T("D:\\Microsoft VS Code\\Code.exe");

    // 检查文件是否存在
    if (GetFileAttributes(strPath) != INVALID_FILE_ATTRIBUTES)
    {
        ::ShellExecute(NULL, _T("open"), strPath, NULL, NULL, SW_SHOWNORMAL);
    }
    else
    {
        MessageBox(_T("指定的VS Code可执行文件未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton7()
{
    CString strPath = _T("C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\Common7\\IDE\\devenv.exe");

    // 检查文件是否存在
    if (GetFileAttributes(strPath) != INVALID_FILE_ATTRIBUTES)
    {
        ::ShellExecute(NULL, _T("open"), strPath, NULL, NULL, SW_SHOWNORMAL);
    }
    else
    {
        CString path = GetOrAskPath(this, _T("VSPath"), _T("选择 Visual Studio 可执行文件"), false);
        if (!path.IsEmpty()) ::ShellExecute(NULL, _T("open"), path, NULL, NULL, SW_SHOWNORMAL);
        else MessageBox(_T("指定的Visual Studio可执行文件未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}




void CMFCApplication1Dlg::OnBnClickedButton9()
{
    CString strPath = _T("C:\\Users\\guany\\Desktop\\学习");

    // 检查文件夹是否存在
    DWORD dwAttr = GetFileAttributes(strPath);
    if (dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        ::ShellExecute(NULL, _T("open"), strPath, NULL, NULL, SW_SHOWNORMAL);
    }
    else
    {
        CString path = GetOrAskPath(this, _T("StudyFolder"), _T("选择学习文件夹"), true);
        if (!path.IsEmpty()) ::ShellExecute(NULL, _T("open"), path, NULL, NULL, SW_SHOWNORMAL);
        else MessageBox(_T("指定的学习文件夹未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton11()
{
    CString url = AfxGetApp()->GetProfileString(_T("Sites"), _T("MoocUrl"), _T("https://www.icourse163.org/home.htm?userId=1595641987#/home/course"));
    ::ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}

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

void CMFCApplication1Dlg::OnBnClickedButton12()
{
    // Apply numeric value from IDC_EDIT5 if valid
    CEdit* pEditVol = (CEdit*)GetDlgItem(IDC_EDIT5);
    CSliderCtrl* pSlider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER1);
    if (!pEditVol) return;
    CString s; pEditVol->GetWindowText(s);
    int v = _ttoi(s);
    if (s.IsEmpty() || v < 0 || v > 100)
    {
        // invalid -> restore display to current volume
        int cur = CVolumeManager::GetMasterVolumePercent();
        CString cs; cs.Format(_T("%d"), cur);
        pEditVol->SetWindowText(cs);
        if (pSlider) pSlider->SetPos(cur);
        return;
    }

    // set volume
    if (CVolumeManager::SetMasterVolumePercent(v))
    {
        if (pSlider) pSlider->SetPos(v);
    }
    else
    {
        // on failure, restore displayed
        int cur = CVolumeManager::GetMasterVolumePercent();
        CString cs; cs.Format(_T("%d"), cur);
        pEditVol->SetWindowText(cs);
        if (pSlider) pSlider->SetPos(cur);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton13()
{
    // set to 0
    if (CVolumeManager::SetMasterVolumePercent(0))
    {
        CEdit* pEditVol = (CEdit*)GetDlgItem(IDC_EDIT5);
        CSliderCtrl* pSlider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER1);
        if (pEditVol) pEditVol->SetWindowText(_T("0"));
        if (pSlider) pSlider->SetPos(0);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton14()
{
    // set to 10
    if (CVolumeManager::SetMasterVolumePercent(10))
    {
        CEdit* pEditVol = (CEdit*)GetDlgItem(IDC_EDIT5);
        CSliderCtrl* pSlider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER1);
        if (pEditVol) pEditVol->SetWindowText(_T("10"));
        if (pSlider) pSlider->SetPos(10);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton17()
{
    CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT6);
    if (!pEdit) return;

    CString cmd;
    pEdit->GetWindowText(cmd);
    if (cmd.IsEmpty())
    {
        MessageBox(_T("请输入要运行的指令。"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    // Use ShellExecute to run commands similar to Win+R. Support 'runas' for elevated commands if user prefixes 'runas:'
    // Trim whitespace
    cmd.Trim();

    // If the command looks like a path with arguments, split appropriately
    // ShellExecute can accept entire string as lpParameters if lpFile is executable. We'll try ShellExecuteEx via SHELLEXECUTEINFO
    // Try to execute directly using CreateProcess for proper command line handling when target is an executable.
    CString firstToken = cmd;
    int sp = firstToken.Find(' ');
    if (sp != -1) firstToken = firstToken.Left(sp);

    bool triedCreate = false;
    // Heuristic: if it contains a backslash or ends with .exe, try CreateProcess
    int dot = firstToken.ReverseFind('.');
    CString ext = (dot != -1) ? firstToken.Mid(dot) : CString(_T(""));
    if (firstToken.Find(_T('\\')) != -1 || (!ext.IsEmpty() && ext.CompareNoCase(_T(".exe")) == 0))
    {
        triedCreate = true;
        // CreateProcess requires a modifiable buffer
        CString cmdLine = cmd;
        TCHAR* lpCmd = _tcsdup(cmdLine.GetBuffer());
        STARTUPINFO si = {0}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {0};
        BOOL ok = CreateProcess(NULL, lpCmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
        free(lpCmd);
        if (ok)
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return;
        }
    }

    // Next try ShellExecute (handles URLs, file associations, apps without explicit .exe)
    HINSTANCE h = ShellExecute(m_hWnd, NULL, cmd, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h > 32)
    {
        return; // success
    }

    // Final fallback: run through cmd.exe so builtins and complex commands work. Quote the entire command.
    SHELLEXECUTEINFO sei = {0};
    CString params;
    params.Format(_T("/C %s"), cmd);
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOASYNC;
    sei.hwnd = m_hWnd;
    sei.lpVerb = NULL;
    sei.lpFile = _T("cmd.exe");
    sei.lpParameters = params;
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteEx(&sei))
    {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(_T("执行命令失败：%u"), err);
        MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
    }
}


void CMFCApplication1Dlg::OnBnClickedButton18()
{
    SetDlgItemText(IDC_EDIT6, _T(""));
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

void CMFCApplication1Dlg::OnBnClickedButton21()
{
    // TODO: 在此添加控件通知处理程序代码
    CString strPath = _T("C:\\Users\\guany\\Desktop\\下载");

    // 检查文件夹是否存在
    DWORD dwAttr = GetFileAttributes(strPath);
    if (dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        ::ShellExecute(NULL, _T("open"), strPath, NULL, NULL, SW_SHOWNORMAL);
    }
    else
    {
        CString path = GetOrAskPath(this, _T("downloadFolder"), _T("选择下载文件夹"), true);
        if (!path.IsEmpty()) ::ShellExecute(NULL, _T("open"), path, NULL, NULL, SW_SHOWNORMAL);
        else MessageBox(_T("指定的下载文件夹未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton22()
{
    CString strPath = GetOrAskPath(this, _T("YuanbaoPath"), _T("选择元宝可执行文件"), false);

    if (!strPath.IsEmpty() && GetFileAttributes(strPath) != INVALID_FILE_ATTRIBUTES)
    {
        ::ShellExecute(NULL, _T("open"), strPath, NULL, NULL, SW_SHOWNORMAL);
    }
    else
    {
        MessageBox(_T("指定的元宝可执行文件未找到或未设置。请先设置路径。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}


// Handler for IDC_CHECK5: prevent automatic lock/screen-off while checked
void CMFCApplication1Dlg::OnBnClickedCheck5()
{
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK5);
    if (!pCheck) return;

    if (pCheck->GetCheck() == BST_CHECKED)
    {
        EXECUTION_STATE es = SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
        if (es == 0)
        {
            MessageBox(_T("无法设置防止锁屏的系统状态。"), _T("错误"), MB_OK | MB_ICONERROR);
            pCheck->SetCheck(BST_UNCHECKED);
            m_bPreventLockScreen = false;
        }
        else
        {
            m_bPreventLockScreen = true;
        }
    }
    else
    {
        SetThreadExecutionState(ES_CONTINUOUS);
        m_bPreventLockScreen = false;
    }
}

void CMFCApplication1Dlg::OnBnClickedCheck6()
{
    // No special UI action required when the checkbox is toggled. The
    // launch behavior is handled when the user clicks IDC_BUTTON27.
}

void CMFCApplication1Dlg::OnBnClickedButton27()
{
    // Launch PowerShell. If IDC_CHECK6 is checked, launch as the shell
    // (interactive) user (typically non-elevated). If unchecked, request
    // elevation via ShellExecute "runas".
    CButton* pCheck6 = (CButton*)GetDlgItem(IDC_CHECK6);
    bool nonElevated = true;
    if (pCheck6 && pCheck6->GetCheck() == BST_UNCHECKED)
        nonElevated = false;

    CString exe = _T("powershell.exe");
    CString params = _T("-NoExit"); // keep window open

    if (nonElevated)
    {
        CString err;
        if (!LaunchProcessAsShellUser(exe, params, &err))
        {
            CString msg;
            msg.Format(_T("以非管理员权限启动 PowerShell 失败：%s"), err.IsEmpty() ? FormatLastError(GetLastError()) : CString(err));
            MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
        }
    }
    else
    {
        SHELLEXECUTEINFO sei = {0};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = _T("runas");
        sei.lpFile = exe;
        sei.lpParameters = params;
        sei.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteEx(&sei))
        {
            DWORD err = GetLastError();
            CString msg;
            msg.Format(_T("以管理员权限启动 PowerShell 失败：%s"), FormatLastError(err));
            MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
        }
    }
}

void CMFCApplication1Dlg::OnBnClickedButton28()
{
    // Launch WSL (wsl.exe). Try ShellExecute first; fallback to system directory path.
    HINSTANCE h = ::ShellExecute(m_hWnd, _T("open"), _T("wsl.exe"), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        TCHAR sysPath[MAX_PATH] = {0};
        if (GetSystemDirectory(sysPath, MAX_PATH) > 0)
        {
            CString full;
            full.Format(_T("%s\\wsl.exe"), sysPath);
            HINSTANCE h2 = ::ShellExecute(m_hWnd, _T("open"), full, NULL, NULL, SW_SHOWNORMAL);
            if ((INT_PTR)h2 <= 32)
            {
                MessageBox(_T("无法启动 WSL，请手动运行 wsl.exe"), _T("错误"), MB_OK | MB_ICONERROR);
            }
        }
        else
        {
            MessageBox(_T("无法启动 WSL，请手动运行 wsl.exe"), _T("错误"), MB_OK | MB_ICONERROR);
        }
    }
}

void CMFCApplication1Dlg::OnBnClickedButton29()
{
    // Open LeetCode CN problemset in default browser
    CString url = _T("https://leetcode.cn/problemset/");
    HINSTANCE h = ::ShellExecute(m_hWnd, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        MessageBox(_T("无法打开链接，请手动访问 https://leetcode.cn/problemset/"), _T("错误"), MB_OK | MB_ICONERROR);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton30()
{
    // Open GitHub
    CString url = _T("https://github.com/");
    HINSTANCE h = ::ShellExecute(m_hWnd, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        MessageBox(_T("无法打开链接，请手动访问 https://github.com/"), _T("错误"), MB_OK | MB_ICONERROR);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton31()
{
    // Launch Git Bash. If a valid path is displayed in IDC_STATIC_PATH, use it as
    // the process working directory so the shell starts there. Otherwise fall back
    // to the default behaviour.
    CString exe = _T("D:\\Git\\git-bash.exe");
    if (GetFileAttributes(exe) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(_T("找不到 git-bash.exe，请检查路径 D:\\Git\\git-bash.exe 是否存在。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    // Read displayed path from static control. It may be a file path (dropped file)
    // or a directory. If it's a file, use its parent directory.
    CString displayed;
    CWnd* pStatic = GetDlgItem(IDC_STATIC_PATH);
    if (pStatic) pStatic->GetWindowText(displayed);

    CString workDir;
    if (!displayed.IsEmpty())
    {
        DWORD attr = GetFileAttributes(displayed);
        if (attr != INVALID_FILE_ATTRIBUTES)
        {
            if (attr & FILE_ATTRIBUTE_DIRECTORY)
                workDir = displayed;
            else
            {
                int pos = displayed.ReverseFind(_T('\\'));
                if (pos != -1)
                    workDir = displayed.Left(pos);
            }
        }
    }

    // If we have a valid working directory, try CreateProcess with lpCurrentDirectory
    if (!workDir.IsEmpty() && GetFileAttributes(workDir) != INVALID_FILE_ATTRIBUTES)
    {
        STARTUPINFO si = {0}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {0};
        // CreateProcess expects writable command line buffer if provided; we pass NULL.
        BOOL ok = CreateProcess(exe, NULL, NULL, NULL, FALSE, 0, NULL, workDir, &si, &pi);
        if (ok)
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return;
        }
        // on failure, fall through to ShellExecute as a fallback
    }

    // Fallback: let ShellExecute open git-bash (uses default start directory)
    ::ShellExecute(NULL, _T("open"), exe, NULL, NULL, SW_SHOWNORMAL);
}

// Clear the displayed dropped file path when IDC_BUTTON32 is clicked
void CMFCApplication1Dlg::OnBnClickedButton32()
{
    // Clear stored path and update static control text
    m_strDroppedFilePath.Empty();
    CWnd* pStatic = GetDlgItem(IDC_STATIC_PATH);
    if (pStatic && IsValidWindow(pStatic->GetSafeHwnd()))
    {
        // Show placeholder to indicate no file
        pStatic->SetWindowText(_T("拖拽文件到此"));
    }
}

/////////////////////////////////////////////////////////////////////////////
// CMFCApplication1Dlg menu command handlers

void CMFCApplication1Dlg::OnFileSettings()
{
    CSettingsDlg dlg(this);
    dlg.DoModal();
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

void CMFCApplication1Dlg::OnNMDblclkList5(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST5);
    if (pList && pItem->iItem >= 0)
    {
        CString val = pList->GetItemText(pItem->iItem, 1);
        if (!val.IsEmpty()) CopyToClipboard(m_hWnd, val);
    }
    *pResult = 0;
}


