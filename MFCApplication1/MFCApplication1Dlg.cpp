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

// Overlay window procedure used for capture
static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CMFCApplication1Dlg* pDlg = (CMFCApplication1Dlg*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
        {
            POINTS pts = MAKEPOINTS(lParam);
            POINT pt = { pts.x, pts.y };
            ::ClientToScreen(hwnd, &pt);
            // hide our overlay first so WindowFromPoint returns the underlying window
            ::ShowWindow(hwnd, SW_HIDE);
            HWND hTarget = ::WindowFromPoint(pt);
            if (pDlg) pDlg->OnTargetSelected(hTarget, pt);
            if (::GetCapture() == hwnd) ::ReleaseCapture();
            ::DestroyWindow(hwnd);
            return 0;
        }
    default:
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

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
    if (!pWnd || !::IsWindow(pWnd->GetSafeHwnd())) return;
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

// Double-click on CListCtrl: copy command
void CMFCApplication1Dlg::OnNMDblclkList4(NMHDR* pNMHDR, LRESULT* pResult)
{
    // Use the item activation info to determine which row was double-clicked
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    int idx = (pItem) ? pItem->iItem : -1;
    if (idx >= 0)
    {
        CWnd* pWnd = GetDlgItem(IDC_LIST4);
        if (pWnd && ::IsWindow(pWnd->GetSafeHwnd()))
        {
            TCHAR cls[64] = {0};
            GetClassName(pWnd->GetSafeHwnd(), cls, _countof(cls));
            if (CString(cls).CompareNoCase(_T("SysListView32")) == 0)
            {
                CListCtrl* pListCtrl = (CListCtrl*)pWnd;
                // Prefer the command column (subitem 1). If empty, try to parse from
                // the first column (description) using common separators.
                CString cmd = pListCtrl->GetItemText(idx, 1);
                if (cmd.IsEmpty())
                {
                    CString combined = pListCtrl->GetItemText(idx, 0);
                    int sep = combined.Find(_T('|'));
                    if (sep != -1) cmd = combined.Mid(sep + 1);
                    else
                    {
                        sep = combined.Find(_T('\t'));
                        if (sep != -1) cmd = combined.Mid(sep + 1);
                        else cmd = combined; // fallback: copy whole text
                    }
                }
                if (!cmd.IsEmpty()) CopyToClipboard(m_hWnd, cmd);
            }
        }
    }
    *pResult = 0;
}

// Double-click on CListBox: copy command
void CMFCApplication1Dlg::OnLbnDblclkList4()
{
    CWnd* pWnd = GetDlgItem(IDC_LIST4);
    if (pWnd && ::IsWindow(pWnd->GetSafeHwnd()))
    {
        TCHAR cls[64] = {0};
        GetClassName(pWnd->GetSafeHwnd(), cls, _countof(cls));
        if (CString(cls).CompareNoCase(_T("ListBox")) == 0)
            OnCopyGitCommand();
    }
}













// The capture overlay window is created dynamically via CreateWindowEx using
// the class name "MyCaptureOverlayClass" and routed to OverlayWndProc.

// Delete selected dropped file (move to Recycle Bin)
void CMFCApplication1Dlg::OnBnClickedButton24()
{
    // Delete (move to Recycle Bin) the currently selected dropped file
    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(_T("请先拖入文件！"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    CString msg;
    msg.Format(_T("确定要将以下文件移到回收站？\n%s"), m_strDroppedFilePath);
    if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    // Prepare SHFILEOPSTRUCT for moving to recycle bin
    // SHFileOperation expects double-null-terminated strings
    CString pathDouble = m_strDroppedFilePath;
    pathDouble += _T('\0');
    pathDouble += _T('\0');

    SHFILEOPSTRUCT sh = {0};
    sh.hwnd = m_hWnd;
    sh.wFunc = FO_DELETE;
    sh.pFrom = pathDouble;
    sh.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION; // already confirmed by us

    int ret = SHFileOperation(&sh);
    if (ret == 0 && !sh.fAnyOperationsAborted)
    {
        MessageBox(_T("文件已移到回收站。"), _T("完成"), MB_OK | MB_ICONINFORMATION);
        m_strDroppedFilePath.Empty();
        SetDlgItemText(IDC_STATIC_PATH, _T("拖拽文件到此"));
        // clear rename edits
        SetDlgItemText(IDC_EDIT7, _T(""));
        SetDlgItemText(IDC_EDIT8, _T(""));
    }
    else
    {
        CString err;
        err.Format(_T("无法删除文件，错误代码：%d"), ret);
        MessageBox(err, _T("错误"), MB_OK | MB_ICONERROR);
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

void CMFCApplication1Dlg::OnBnClickedButton19()
{
    // If overlay exists, cancel it
    if (m_hCaptureWnd && ::IsWindow(m_hCaptureWnd))
    {
        ::DestroyWindow(m_hCaptureWnd);
        m_hCaptureWnd = NULL;
        // continue: recreate overlay so user can locate again
    }

    // Start capture overlay: create a simple full-screen window class on the fly
    WNDCLASS wc = {0};
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.lpszClassName = _T("MyCaptureOverlayClass");
    wc.hCursor = ::LoadCursor(NULL, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    // Register the class only if it isn't already registered to avoid leaking
    // repeated registrations.
    WNDCLASS existing = {0};
    if (!GetClassInfo(wc.hInstance, wc.lpszClassName, &existing))
    {
        RegisterClass(&wc);
    }

    HWND hOverlay = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, _T(""), WS_POPUP,
                                    0,0,::GetSystemMetrics(SM_CXSCREEN),::GetSystemMetrics(SM_CYSCREEN),
                                    m_hWnd, NULL, AfxGetInstanceHandle(), NULL);
    if (!hOverlay)
    {
        MessageBox(_T("无法进入定位模式。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }
    ::SetWindowLongPtr(hOverlay, GWLP_USERDATA, (LONG_PTR)this);
    // Ensure our wndproc is the overlay proc
    ::SetWindowLongPtr(hOverlay, GWLP_WNDPROC, (LONG_PTR)OverlayWndProc);
    ::ShowWindow(hOverlay, SW_SHOW);
    ::SetCapture(hOverlay);
    m_hCaptureWnd = hOverlay;
}

void CMFCApplication1Dlg::OnTargetSelected(HWND hTarget, POINT pt)
{
    // 将子控件提升为其顶层父窗口，避免记录按钮/编辑框等组件
    hTarget = ::GetAncestor(hTarget, GA_ROOT);

    m_hSelectedWnd = hTarget;
    if (!IsWindow(hTarget))
    {
        MessageBox(_T("未选中有效窗口。"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    // When a target window is selected via the overlay, treat it as "located":
    // add to history immediately so LIST7 gets populated on tab switch
    {
        auto it = std::find(m_historyWnds.begin(), m_historyWnds.end(), hTarget);
        if (it == m_historyWnds.end())
            m_historyWnds.push_back(hTarget);
    }

    // switch to the "窗口处理" tab and refresh the list to show this window's info.
    CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB1);
    if (pTab)
    {
        pTab->SetCurSel(3);
        LRESULT res = 0;
        OnTcnSelchangeTab1(NULL, &res);
    }

    // 弹出操作菜单
    CMenu menu;
    menu.CreatePopupMenu();
    // Provide only topmost and close options
    menu.AppendMenu(MF_STRING, 1, _T("置顶 (Topmost)"));
    menu.AppendMenu(MF_STRING, 3, _T("关闭窗口 (Close)"));
    menu.AppendMenu(MF_STRING, 0, _T("取消"));

    ::SetForegroundWindow(m_hWnd);
    int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, this);
    if (cmd == 1)
    {
        if (!::SetWindowPos(hTarget, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE))
            MessageBox(_T("置顶失败，可能权限不足或窗口不允许。"), _T("提示"), MB_OK | MB_ICONERROR);
        else
        {
            // 避免重复添加
            auto it = std::find(m_topmostWnds.begin(), m_topmostWnds.end(), hTarget);
            if (it == m_topmostWnds.end())
                m_topmostWnds.push_back(hTarget);

            // 如果置顶的是工具箱自身，同步选中复选框
            if (hTarget == m_hWnd)
            {
                CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
                if (pCheck) pCheck->SetCheck(BST_CHECKED);
            }

            // 刷新置顶列表显示
            CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
            if (pTab) UpdateTabVisibility(pTab->GetCurSel());
        }
    }
    else if (cmd == 3)
    {
        ::PostMessage(hTarget, WM_CLOSE, 0, 0);
    }
}

// Note: Button20 and Button21 were removed; their functionality is replaced by
// the locate button (IDC_BUTTON19) and the overlay/menu flow.



// Master volume helpers using Core Audio EndpointVolume (0-100)
// Async volume retrieval: run in background and post WM_VOLUME_UPDATED with percent in WPARAM
void CMFCApplication1Dlg::OnBnClickedButton10()
{
    CString url = AfxGetApp()->GetProfileString(_T("Sites"), _T("LocalServer"), _T("http://10.102.33.39/"));
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

/////////////////////////////////////////////////////////////////////////////
// CSettingsDlg dialog - configuration

class CSettingsDlg : public CDialogEx
{
public:
    CSettingsDlg(CWnd* pParent = nullptr) : CDialogEx(IDD_SETTINGS_DIALOG, pParent) {}
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    void OnBrowseBili() { BrowseFile(IDC_EDIT_BILI_PATH, _T("选择Bilibili可执行文件")); }
    void OnBrowseWeChat() { BrowseFile(IDC_EDIT_WECHAT_PATH, _T("选择微信可执行文件")); }
    void OnBrowseQQ() { BrowseFile(IDC_EDIT_QQ_PATH, _T("选择QQ可执行文件")); }
    void OnBrowseVSCode() { BrowseFile(IDC_EDIT_VSCODE_PATH, _T("选择VS Code可执行文件")); }
    void OnBrowseVS() { BrowseFile(IDC_EDIT_VS_PATH, _T("选择Visual Studio可执行文件")); }
    void OnBrowseGitBash() { BrowseFile(IDC_EDIT_GITBASH_PATH, _T("选择Git Bash可执行文件")); }
    void OnBrowseYuanbao() { BrowseFile(IDC_EDIT_YUANBAO_PATH, _T("选择元宝可执行文件")); }
    void OnBrowseStudy() { BrowseFolder(IDC_EDIT_STUDY_PATH, _T("选择学习文件夹")); }
    void OnBrowseDownload() { BrowseFolder(IDC_EDIT_DOWNLOAD_PATH, _T("选择下载文件夹")); }
    void OnBrowseScreenshot() { BrowseFolder(IDC_EDIT_SCREENSHOT_DIR, _T("选择截图保存目录")); }
private:
    void BrowseFile(UINT id, LPCTSTR title);
    void BrowseFolder(UINT id, LPCTSTR title);
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BROWSE_BILI, &CSettingsDlg::OnBrowseBili)
    ON_BN_CLICKED(IDC_BROWSE_WECHAT, &CSettingsDlg::OnBrowseWeChat)
    ON_BN_CLICKED(IDC_BROWSE_QQ, &CSettingsDlg::OnBrowseQQ)
    ON_BN_CLICKED(IDC_BROWSE_VSCODE, &CSettingsDlg::OnBrowseVSCode)
    ON_BN_CLICKED(IDC_BROWSE_VS, &CSettingsDlg::OnBrowseVS)
    ON_BN_CLICKED(IDC_BROWSE_GITBASH, &CSettingsDlg::OnBrowseGitBash)
    ON_BN_CLICKED(IDC_BROWSE_YUANBAO, &CSettingsDlg::OnBrowseYuanbao)
    ON_BN_CLICKED(IDC_BROWSE_STUDY, &CSettingsDlg::OnBrowseStudy)
    ON_BN_CLICKED(IDC_BROWSE_DOWNLOAD, &CSettingsDlg::OnBrowseDownload)
    ON_BN_CLICKED(IDC_BROWSE_SCREENSHOT, &CSettingsDlg::OnBrowseScreenshot)
END_MESSAGE_MAP()

BOOL CSettingsDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetDlgItemText(IDC_EDIT_DEFAULT_NAME, AfxGetApp()->GetProfileString(_T("Template"), _T("DefaultReportName"), _T("")));
    SetDlgItemText(IDC_EDIT_BILI_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("BiliPath"), _T("")));
    SetDlgItemText(IDC_EDIT_WECHAT_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("WeChatPath"), _T("")));
    SetDlgItemText(IDC_EDIT_QQ_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("QQPath"), _T("")));
    SetDlgItemText(IDC_EDIT_VSCODE_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("VSCodePath"), _T("")));
    SetDlgItemText(IDC_EDIT_VS_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("VSPath"), _T("")));
    SetDlgItemText(IDC_EDIT_GITBASH_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("GitBashPath"), _T("")));
    SetDlgItemText(IDC_EDIT_YUANBAO_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("YuanbaoPath"), _T("")));
    SetDlgItemText(IDC_EDIT_STUDY_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("StudyFolder"), _T("")));
    SetDlgItemText(IDC_EDIT_DOWNLOAD_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("DownloadFolder"), _T("")));
    SetDlgItemText(IDC_EDIT_MOOC_URL, AfxGetApp()->GetProfileString(_T("Sites"), _T("MoocUrl"), _T("")));
    SetDlgItemText(IDC_EDIT_SDUCS_URL, AfxGetApp()->GetProfileString(_T("Sites"), _T("Sducs"), _T("")));

    // 截图保存目录，默认桌面
    CString strDefaultScreenshot;
    TCHAR szDesktop[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, 0, szDesktop)))
        strDefaultScreenshot = szDesktop;
    SetDlgItemText(IDC_EDIT_SCREENSHOT_DIR,
        AfxGetApp()->GetProfileString(_T("Paths"), _T("ScreenshotDir"), strDefaultScreenshot));
    return TRUE;
}

void CSettingsDlg::OnOK()
{
    CString v;
    GetDlgItemText(IDC_EDIT_DEFAULT_NAME, v); AfxGetApp()->WriteProfileString(_T("Template"), _T("DefaultReportName"), v);
    GetDlgItemText(IDC_EDIT_BILI_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("BiliPath"), v);
    GetDlgItemText(IDC_EDIT_WECHAT_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("WeChatPath"), v);
    GetDlgItemText(IDC_EDIT_QQ_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("QQPath"), v);
    GetDlgItemText(IDC_EDIT_VSCODE_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("VSCodePath"), v);
    GetDlgItemText(IDC_EDIT_VS_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("VSPath"), v);
    GetDlgItemText(IDC_EDIT_GITBASH_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("GitBashPath"), v);
    GetDlgItemText(IDC_EDIT_YUANBAO_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("YuanbaoPath"), v);
    GetDlgItemText(IDC_EDIT_STUDY_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("StudyFolder"), v);
    GetDlgItemText(IDC_EDIT_DOWNLOAD_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("DownloadFolder"), v);
    GetDlgItemText(IDC_EDIT_SCREENSHOT_DIR, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("ScreenshotDir"), v);
    GetDlgItemText(IDC_EDIT_MOOC_URL, v); AfxGetApp()->WriteProfileString(_T("Sites"), _T("MoocUrl"), v);
    GetDlgItemText(IDC_EDIT_SDUCS_URL, v); AfxGetApp()->WriteProfileString(_T("Sites"), _T("Sducs"), v);
    CDialogEx::OnOK();
}

void CSettingsDlg::BrowseFile(UINT id, LPCTSTR title)
{
    CFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, NULL, this);
    dlg.m_ofn.lpstrTitle = title;
    if (dlg.DoModal() == IDOK) SetDlgItemText(id, dlg.GetPathName());
}

void CSettingsDlg::BrowseFolder(UINT id, LPCTSTR title)
{
    CFolderPickerDialog dlg(NULL, 0, this);
    dlg.m_ofn.lpstrTitle = title;
    if (dlg.DoModal() == IDOK) SetDlgItemText(id, dlg.GetPathName());
}



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
    ON_COMMAND(32775, &CMFCApplication1Dlg::OnUntopmostWindow)
    ON_COMMAND(32776, &CMFCApplication1Dlg::OnCopyStartupPath)
    ON_COMMAND(32777, &CMFCApplication1Dlg::OnDeleteList6Record)
    ON_COMMAND(32778, &CMFCApplication1Dlg::OnDeleteList7Record)
    ON_COMMAND(32779, &CMFCApplication1Dlg::OnTopmostFromHistory)
    ON_COMMAND(32780, &CMFCApplication1Dlg::OnUntopmostFromHistory)
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
			for (size_t i = 0; i < m_topmostWnds.size(); ++i)
			{
				HWND h = m_topmostWnds[i];
				if (!::IsWindow(h)) continue;

				TCHAR title[256] = {0};
				::GetWindowText(h, title, _countof(title));

				CString label;
				label.Format(_T("置顶[%zu]"), i + 1);
				int idx = pList6->InsertItem(static_cast<int>(i), label);
				pList6->SetItemText(idx, 1, CString(title));
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
				if (!::IsWindow(h)) continue;

				TCHAR title[256] = {0};
				::GetWindowText(h, title, _countof(title));

				CString label;
				label.Format(_T("%zu"), row + 1);
				int idx = pList7->InsertItem(row, label);
				pList7->SetItemText(idx, 1, CString(title));
				++row;
			}
		}
	}
}

// ========== 窗口处理新功能 ==========

void CMFCApplication1Dlg::OnForceKillProcess()
{
	if (!m_hSelectedWnd || !::IsWindow(m_hSelectedWnd))
	{
		MessageBox(_T("请先定位一个窗口。"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	DWORD pid = 0;
	GetWindowThreadProcessId(m_hSelectedWnd, &pid);
	if (pid == 0)
	{
		MessageBox(_T("无法获取进程ID。"), _T("错误"), MB_OK | MB_ICONERROR);
		return;
	}

	CString msg;
	msg.Format(_T("确定要强制结束进程 PID=%u 吗？\n未保存的数据可能会丢失。"), pid);
	if (MessageBox(msg, _T("确认强制结束"), MB_YESNO | MB_ICONWARNING) != IDYES)
		return;

	HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
	if (!hProc)
	{
		DWORD err = GetLastError();
		msg.Format(_T("无法打开进程 (错误: %u)，可能需要管理员权限。"), err);
		MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
		return;
	}

	if (TerminateProcess(hProc, 1))
	{
		MessageBox(_T("进程已强制结束。"), _T("完成"), MB_OK | MB_ICONINFORMATION);
		// 从置顶列表中移除已结束的窗口
		m_topmostWnds.erase(
			std::remove_if(m_topmostWnds.begin(), m_topmostWnds.end(),
				[](HWND h) { return !::IsWindow(h); }),
			m_topmostWnds.end());
		// 刷新显示
		CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
		if (pTab) UpdateTabVisibility(pTab->GetCurSel());
	}
	else
	{
		MessageBox(_T("强制结束进程失败。"), _T("错误"), MB_OK | MB_ICONERROR);
	}
	CloseHandle(hProc);
}

void CMFCApplication1Dlg::OnWindowScreenshot()
{
	if (!m_hSelectedWnd || !::IsWindow(m_hSelectedWnd))
	{
		MessageBox(_T("请先定位一个窗口。"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	// 获取窗口尺寸
	RECT rc;
	::GetWindowRect(m_hSelectedWnd, &rc);
	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;
	if (width <= 0 || height <= 0)
	{
		MessageBox(_T("窗口尺寸无效。"), _T("错误"), MB_OK | MB_ICONERROR);
		return;
	}

	// 创建设备上下文
	HDC hdcScreen = ::GetDC(NULL);
	HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
	HBITMAP hBitmap = ::CreateCompatibleBitmap(hdcScreen, width, height);
	HBITMAP hOldBmp = static_cast<HBITMAP>(::SelectObject(hdcMem, hBitmap));

	// 使用 PrintWindow 截取窗口
	if (!::PrintWindow(m_hSelectedWnd, hdcMem, PW_CLIENTONLY))
	{
		// PrintWindow 失败时尝试 BitBlt
		::SetForegroundWindow(m_hSelectedWnd);
		::Sleep(100);
		::BitBlt(hdcMem, 0, 0, width, height, hdcScreen, rc.left, rc.top, SRCCOPY);
	}

	// 复制到剪贴板
	if (::OpenClipboard(m_hWnd))
	{
		::EmptyClipboard();
		::SetClipboardData(CF_BITMAP, hBitmap);
		::CloseClipboard();
	}
	else
	{
		MessageBox(_T("无法打开剪贴板。"), _T("错误"), MB_OK | MB_ICONERROR);
		::DeleteObject(hBitmap);
	}

	// 保存到文件
	{
		CString sDir = AfxGetApp()->GetProfileString(_T("Paths"), _T("ScreenshotDir"), _T(""));
		if (sDir.IsEmpty())
		{
			TCHAR szDesktop[MAX_PATH];
			if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, 0, szDesktop)))
				sDir = szDesktop;
		}

		// 生成带时间戳的文件名
		SYSTEMTIME st;
		GetLocalTime(&st);
		CString sFilename;
		sFilename.Format(_T("\\screenshot_%04d%02d%02d_%02d%02d%02d.png"),
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

		CString sFullPath = sDir + sFilename;

		// 保存为 PNG（使用 ATL CImage）
		CImage img;
		img.Attach(hBitmap);
		img.Save(sFullPath);
		img.Detach();  // 避免 hBitmap 被 CImage 析构时释放

		CString sMsg;
		sMsg.Format(_T("截图已保存到:\n%s"), sFullPath);
		MessageBox(sMsg, _T("完成"), MB_OK | MB_ICONINFORMATION);
	}

	::SelectObject(hdcMem, hOldBmp);
	::DeleteDC(hdcMem);
	::ReleaseDC(NULL, hdcScreen);
}

// ========== 菜单命令处理函数 ==========

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

void CMFCApplication1Dlg::OnWindowLocate()
{
	// 切换到窗口处理标签页并触发定位
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) { pTab->SetCurSel(3); UpdateTabVisibility(3); }
	OnBnClickedButton19();
}

void CMFCApplication1Dlg::OnWindowUntopmost()
{
	for (HWND hWnd : m_topmostWnds)
	{
		if (::IsWindow(hWnd))
			::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
	}
	m_topmostWnds.clear();

	// 同步取消工具箱自身置顶复选框
	CButton* pCheck3 = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
	if (pCheck3) pCheck3->SetCheck(BST_UNCHECKED);

	// 刷新显示
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnWindowClose()
{
	if (m_hSelectedWnd && ::IsWindow(m_hSelectedWnd))
	{
		CString title;
		::GetWindowText(m_hSelectedWnd, title.GetBuffer(256), 256);
		title.ReleaseBuffer();

		CString msg;
		msg.Format(_T("确定要关闭窗口 \"%s\" 吗？"), title);
		if (MessageBox(msg, _T("确认关闭"), MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			::SendMessage(m_hSelectedWnd, WM_CLOSE, 0, 0);
			// 从置顶列表中移除
			m_topmostWnds.erase(
				std::remove_if(m_topmostWnds.begin(), m_topmostWnds.end(),
					[](HWND h) { return !::IsWindow(h); }),
				m_topmostWnds.end());
			m_hSelectedWnd = nullptr;
			CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
			if (pTab) UpdateTabVisibility(pTab->GetCurSel());
		}
	}
	else
	{
		MessageBox(_T("请先定位一个窗口。"), _T("提示"), MB_OK | MB_ICONWARNING);
	}
}

void CMFCApplication1Dlg::OnUntopmostWindow()
{
	CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
	if (!pList6) return;

	int nSel = pList6->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0 || static_cast<size_t>(nSel) >= m_topmostWnds.size()) return;

	HWND hWnd = m_topmostWnds[nSel];
	if (::IsWindow(hWnd))
		::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);

	m_topmostWnds.erase(m_topmostWnds.begin() + nSel);

	// 如果是工具箱自身，同步取消复选框
	if (hWnd == m_hWnd)
	{
		CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
		if (pCheck) pCheck->SetCheck(BST_UNCHECKED);
	}

	// 刷新列表显示
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnCopyStartupPath()
{
	CListCtrl* pList2 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST2));
	if (!pList2) return;

	int nSel = pList2->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) return;

	CString cmd = pList2->GetItemText(nSel, 1);
	if (!cmd.IsEmpty())
		CopyToClipboard(m_hWnd, cmd);
}

void CMFCApplication1Dlg::OnNMDblclkList2(NMHDR* pNMHDR, LRESULT* pResult)
{
	// 双击启动项列表直接复制路径
	OnCopyStartupPath();
	*pResult = 0;
}

void CMFCApplication1Dlg::OnHelpShortcuts()
{
	MessageBox(
		_T("快捷键列表:\n\n")
		_T("Ctrl+Alt+Space   - 显示/隐藏工具箱\n")
		_T("Alt+1~6          - 切换标签页\n")
		_T("Ctrl+Alt+T       - 定位窗口\n")
		_T("F3               - 连点器开关\n\n")
		_T("更多功能请查看视图/工具/窗口菜单。"),
		_T("快捷键列表"), MB_OK | MB_ICONINFORMATION);
}

void CMFCApplication1Dlg::OnHelpGithub()
{
	ShellExecute(m_hWnd, _T("open"), _T("https://github.com"), NULL, NULL, SW_SHOWNORMAL);
}

// ========== LIST6 辅助函数 ==========

void CMFCApplication1Dlg::LoadWindowDetailToList5(HWND hWnd)
{
	CListCtrl* pList5 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST5));
	if (!pList5) return;

	pList5->DeleteAllItems();

	if (!hWnd || !::IsWindow(hWnd)) return;

	DWORD pid = 0; GetWindowThreadProcessId(hWnd, &pid);

	CString procName = _T("");
	CString procPath = _T("");
	HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (hProc)
	{
		TCHAR buf[MAX_PATH] = {0};
		DWORD size = _countof(buf);
		if (QueryFullProcessImageName(hProc, 0, buf, &size)) procPath = buf;
		int psep = procPath.ReverseFind(_T('\\'));
		if (psep != -1) procName = procPath.Mid(psep + 1);
		CloseHandle(hProc);
	}

	CString title;
	::GetWindowText(hWnd, title.GetBuffer(512), 512);
	title.ReleaseBuffer();

	int row = 0;
	CString val;

	val.Format(_T("0x%08X"), reinterpret_cast<UINT_PTR>(hWnd));
	pList5->InsertItem(row, _T("句柄")); pList5->SetItemText(row++, 1, val);
	pList5->InsertItem(row, _T("进程名")); pList5->SetItemText(row++, 1, procName);
	val.Format(_T("%u"), pid);
	pList5->InsertItem(row, _T("PID")); pList5->SetItemText(row++, 1, val);
	pList5->InsertItem(row, _T("路径")); pList5->SetItemText(row++, 1, procPath);
	pList5->InsertItem(row, _T("窗口标题")); pList5->SetItemText(row++, 1, title);
}

void CMFCApplication1Dlg::OnClickList6(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
	if (!pList6) { *pResult = 0; return; }

	int nSel = pList6->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0 || static_cast<size_t>(nSel) >= m_topmostWnds.size()) { *pResult = 0; return; }

	HWND hWnd = m_topmostWnds[nSel];
	if (hWnd && ::IsWindow(hWnd))
	{
		m_hSelectedWnd = hWnd;
		LoadWindowDetailToList5(hWnd);
	}

	*pResult = 0;
}

void CMFCApplication1Dlg::OnClickList7(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (!pList7) { *pResult = 0; return; }

	int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0 || static_cast<size_t>(nSel) >= m_historyWnds.size()) { *pResult = 0; return; }

	HWND hWnd = m_historyWnds[nSel];
	if (hWnd && ::IsWindow(hWnd))
	{
		m_hSelectedWnd = hWnd;
		LoadWindowDetailToList5(hWnd);
	}

	*pResult = 0;
}

void CMFCApplication1Dlg::OnDeleteList6Record()
{
	CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
	if (!pList6) return;

	int nSel = pList6->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0 || static_cast<size_t>(nSel) >= m_topmostWnds.size()) return;

	HWND hWnd = m_topmostWnds[nSel];
	if (::IsWindow(hWnd))
		::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);

	if (hWnd == m_hWnd)
	{
		CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
		if (pCheck) pCheck->SetCheck(BST_UNCHECKED);
	}
	m_topmostWnds.erase(m_topmostWnds.begin() + nSel);

	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnDeleteList7Record()
{
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (!pList7) return;

	int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0 || static_cast<size_t>(nSel) >= m_historyWnds.size()) return;

	m_historyWnds.erase(m_historyWnds.begin() + nSel);

	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnTopmostFromHistory()
{
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (!pList7) return;

	int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0 || static_cast<size_t>(nSel) >= m_historyWnds.size()) return;

	HWND hWnd = m_historyWnds[nSel];
	if (!::IsWindow(hWnd)) return;

	if (!::SetWindowPos(hWnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE))
	{
		MessageBox(_T("置顶失败，可能权限不足或窗口不允许。"), _T("提示"), MB_OK | MB_ICONERROR);
		return;
	}

	auto it = std::find(m_topmostWnds.begin(), m_topmostWnds.end(), hWnd);
	if (it == m_topmostWnds.end())
		m_topmostWnds.push_back(hWnd);

	if (hWnd == m_hWnd)
	{
		CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
		if (pCheck) pCheck->SetCheck(BST_CHECKED);
	}

	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnUntopmostFromHistory()
{
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (!pList7) return;

	int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0 || static_cast<size_t>(nSel) >= m_historyWnds.size()) return;

	HWND hWnd = m_historyWnds[nSel];
	if (::IsWindow(hWnd))
		::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);

	m_topmostWnds.erase(
		std::remove(m_topmostWnds.begin(), m_topmostWnds.end(), hWnd),
		m_topmostWnds.end());

	if (hWnd == m_hWnd)
	{
		CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
		if (pCheck) pCheck->SetCheck(BST_UNCHECKED);
	}

	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
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

    // Ensure autoclicker threads are stopped (C++20: using CAutoClicker class)
    m_autoClicker.Stop();

    // ensure any background volume thread is stopped (CVolumeManager handles this automatically)
    // Drop helper and file management removed - no cleanup required here

    // Remove topmost status from all managed windows
    for (HWND hWnd : m_topmostWnds)
    {
        if (::IsWindow(hWnd))
            ::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
    }
    m_topmostWnds.clear();

    // Unregister drag-and-drop acceptance to be tidy
    ::DragAcceptFiles(m_hWnd, FALSE);

    // Destroy any lingering overlay capture window created for locating targets
    if (m_hCaptureWnd && ::IsWindow(m_hCaptureWnd))
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
        if (m_hCaptureWnd && ::IsWindow(m_hCaptureWnd))
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
    // 拦截键盘按下事件，且按下的是 F5
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_F5)
    {
        CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB1);
        // 判断当前是否停留在第一个选项卡 (进程管理)
        if (pTab)
        {
            int nSel = pTab->GetCurSel();
            if (nSel == 0)
            {
                RefreshProcessList();
                return TRUE; // 返回 TRUE 将事件处理掉，不往下传
            }
            else if (nSel == 1)
            {
                RefreshStartupList();
                return TRUE;
            }
        }
    }

    // 如果编辑框 IDC_EDIT6 拥有焦点，按回车等同于点击运行按钮
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* pEdit = this->GetDlgItem(IDC_EDIT6);
        if (pEdit && pEdit->GetSafeHwnd() == ::GetFocus())
        {
            // 调用按钮处理函数
            OnBnClickedButton17();
            return TRUE; // 吃掉回车，不让默认按钮处理
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

// Double-click handler for list3: copy selected item back to clipboard
void CMFCApplication1Dlg::OnNMDblclkList3(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    int nItem = pItem->iItem;
    if (nItem >= 0 && nItem < (int)m_clipHistory.size())
    {
        CString text = m_clipHistory[nItem];
        if (!text.IsEmpty())
        {
            if (::OpenClipboard(m_hWnd))
            {
                ::EmptyClipboard();
                int len = (text.GetLength() + 1);
                HGLOBAL hGlob = ::GlobalAlloc(GMEM_MOVEABLE, len * sizeof(WCHAR));
                if (hGlob)
                {
                    LPWSTR pBuf = (LPWSTR)::GlobalLock(hGlob);
                    if (pBuf)
                    {
                        wcscpy_s(pBuf, len, text);
                        ::GlobalUnlock(hGlob);
                        HGLOBAL hSet = ::SetClipboardData(CF_UNICODETEXT, hGlob);
                        if (hSet == NULL)
                        {
                            // SetClipboardData failed; free the allocation to avoid leak
                            ::GlobalFree(hGlob);
                        }
                    }
                    else
                    {
                        ::GlobalFree(hGlob);
                    }
                }
                ::CloseClipboard();
            }
        }
    }
    *pResult = 0;
}

// Background enumeration worker for processes
static UINT WINAPI EnumProcessesThread(LPVOID pParam)
{
    auto dlg = reinterpret_cast<CMFCApplication1Dlg*>(pParam);
    std::vector<CMFCApplication1Dlg::ProcInfo>* results = new std::vector<CMFCApplication1Dlg::ProcInfo>();

    // Try to enable SeDebugPrivilege so we can query protected processes when running elevated
    auto EnableDebugPrivilege = []() {
        HANDLE hToken = NULL;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;

        TOKEN_PRIVILEGES tp = {0};
        LUID luid;
        if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
        {
            CloseHandle(hToken);
            return false;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
        CloseHandle(hToken);
        return (ok && GetLastError() == ERROR_SUCCESS);
    };

    // best-effort enable
    EnableDebugPrivilege();

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe))
        {
            do
            {
                CMFCApplication1Dlg::ProcInfo pi;
                pi.name = pe.szExeFile;
                pi.pid = pe.th32ProcessID;
                pi.path = _T("");
                pi.memKB = 0;

                // Try to obtain process image path. Use QueryFullProcessImageName when possible,
                // otherwise fall back to module snapshot which often yields the path.
                TCHAR bufPath[MAX_PATH] = {0};

                HANDLE hProcPath = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.pid);
                if (hProcPath)
                {
                    DWORD size = _countof(bufPath);
                    if (QueryFullProcessImageName(hProcPath, 0, bufPath, &size))
                        pi.path = bufPath;
                    CloseHandle(hProcPath);
                }

                if (pi.path.IsEmpty())
                {
                    // Fallback: enumerate modules to get the executable path
                    HANDLE hModSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pi.pid);
                    if (hModSnap != INVALID_HANDLE_VALUE)
                    {
                        MODULEENTRY32 me;
                        me.dwSize = sizeof(me);
                        if (Module32First(hModSnap, &me))
                        {
                            pi.path = me.szExePath;
                        }
                        CloseHandle(hModSnap);
                    }
                }

                // Try to get memory info using permissions that allow querying memory counters
                HANDLE hProcMem = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pi.pid);
                if (hProcMem)
                {
                    PROCESS_MEMORY_COUNTERS pmc = {0};
                    if (GetProcessMemoryInfo(hProcMem, &pmc, sizeof(pmc)))
                        pi.memKB = pmc.WorkingSetSize / 1024;
                    CloseHandle(hProcMem);
                }

                results->push_back(pi);
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    // If the dialog window is still valid, post the results. Otherwise
    // free the results to avoid leaking memory when the UI has been destroyed.
    HWND hwnd = dlg->GetSafeHwnd();
    if (hwnd != NULL && ::IsWindow(hwnd))
    {
        // If posting the message fails for any reason, free the results to avoid leaking.
        if (!::PostMessage(hwnd, CMFCApplication1Dlg::WM_REFRESH_PROCESSES_DONE, (WPARAM)results, 0))
        {
            delete results;
        }
    }
    else
    {
        delete results;
    }
    return 0;
}

// Background enumeration worker for startup entries
static UINT WINAPI EnumStartupsThread(LPVOID pParam)
{
    auto dlg = reinterpret_cast<CMFCApplication1Dlg*>(pParam);
    std::vector<CMFCApplication1Dlg::StartupInfo>* results = new std::vector<CMFCApplication1Dlg::StartupInfo>();

    HKEY hKey = NULL;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        TCHAR valueName[256];
        TCHAR data[1024];
        DWORD valueNameSize = 0;
        DWORD dataSize = 0;
        DWORD type = 0;
        DWORD index = 0;

        while (TRUE)
        {
            valueNameSize = _countof(valueName);
            dataSize = sizeof(data);
            LONG ret = RegEnumValue(hKey, index, valueName, &valueNameSize, NULL, &type, (LPBYTE)data, &dataSize);
            if (ret != ERROR_SUCCESS) break;

            CMFCApplication1Dlg::StartupInfo si;
            si.name = valueName;
            si.cmd = data;
            results->push_back(si);

            index++;
        }

        RegCloseKey(hKey);
    }

    HWND hwnd = dlg->GetSafeHwnd();
    if (hwnd != NULL && ::IsWindow(hwnd))
    {
        if (!::PostMessage(hwnd, CMFCApplication1Dlg::WM_REFRESH_STARTUPS_DONE, (WPARAM)results, 0))
        {
            delete results;
        }
    }
    else
    {
        delete results;
    }
    return 0;
}

// Handlers for background completion messages
// EnumWindows callback helper to post WM_CLOSE to windows of a PID
static BOOL CALLBACK EnumWindowsCloseCallback(HWND hWnd, LPARAM lParam)
{
    DWORD pid = 0; GetWindowThreadProcessId(hWnd, &pid);
    if (pid == (DWORD)lParam)
    {
        ::PostMessage(hWnd, WM_CLOSE, 0, 0);
    }
    return TRUE;
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

void CMFCApplication1Dlg::RefreshProcessList()
{
    // Start background thread to enumerate processes
    AfxBeginThread(EnumProcessesThread, this);
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
            menu.AppendMenu(MF_STRING, 32776, _T("复制路径"));
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
            menu.AppendMenu(MF_STRING, 32775, _T("取消置顶"));
            menu.AppendMenu(MF_STRING, 32777, _T("删除"));
            menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        }
        return;
    }

    // 右键在历史窗口列表上 (置顶 / 删除)
    CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
    if (pList7 && hClicked == pList7->GetSafeHwnd())
    {
        int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
        if (nSel != -1 && static_cast<size_t>(nSel) < m_historyWnds.size())
        {
            HWND hWnd = m_historyWnds[nSel];
            bool bAlreadyTopmost = ::IsWindow(hWnd) &&
                std::find(m_topmostWnds.begin(), m_topmostWnds.end(), hWnd) != m_topmostWnds.end();

            CMenu menu;
            menu.CreatePopupMenu();
            if (bAlreadyTopmost)
                menu.AppendMenu(MF_STRING, 32780, _T("取消置顶"));
            else
                menu.AppendMenu(MF_STRING, 32779, _T("置顶"));
            menu.AppendMenu(MF_STRING, 32778, _T("删除"));
            menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
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

// 刷新启动项列表（Tab2）
void CMFCApplication1Dlg::RefreshStartupList()
{
    // Start background thread to enumerate startup entries
    AfxBeginThread(EnumStartupsThread, this);
}

// 添加启动项：通过文件对话框选择可执行文件，使用文件名作为条目名
void CMFCApplication1Dlg::OnAddStartup()
{
    CFileDialog dlg(TRUE, _T("exe"), NULL, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, _T("Executable Files (*.exe)|*.exe||"));
    if (dlg.DoModal() == IDOK)
    {
        CString path = dlg.GetPathName();
        int pos = path.ReverseFind('\\');
        CString name = (pos != -1) ? path.Mid(pos + 1) : path;

        // 尝试写入注册表
        HKEY hKey = NULL;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
        {
            LONG ret = RegSetValueEx(hKey, name, 0, REG_SZ, (const BYTE*)(LPCTSTR)path, (path.GetLength() + 1) * sizeof(TCHAR));
            RegCloseKey(hKey);
            if (ret == ERROR_SUCCESS)
            {
                MessageBox(_T("启动项已添加。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
                RefreshStartupList();
                return;
            }
        }

        MessageBox(_T("添加启动项失败！请检查权限。"), _T("错误"), MB_OK | MB_ICONERROR);
    }
}

// 删除选中的启动项
void CMFCApplication1Dlg::OnRemoveStartup()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST2);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    CString name = pList->GetItemText(idx, 0);

    CString msg;
    msg.Format(_T("确定要删除启动项\n%s 吗？"), name);
    if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    HKEY hKey = NULL;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        LONG ret = RegDeleteValue(hKey, name);
        RegCloseKey(hKey);
        if (ret == ERROR_SUCCESS)
        {
            MessageBox(_T("启动项已删除。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
            RefreshStartupList();
            return;
        }
    }

    MessageBox(_T("删除启动项失败！请检查权限。"), _T("错误"), MB_OK | MB_ICONERROR);
}

void CMFCApplication1Dlg::OnKillProcess()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    CString procName = pList->GetItemText(idx, 0);
    CString pidStr = pList->GetItemText(idx, 1);
    DWORD dwPID = _ttoi(pidStr);

    CString strMsg;
    strMsg.Format(_T("确定要结束进程\n%s (PID: %s) 吗？"), procName, pidStr);
    if (MessageBox(strMsg, _T("确认结束进程"), MB_YESNO | MB_ICONWARNING) != IDYES) return;

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwPID);
    if (hProcess)
    {
        // try to gracefully close: enumerate windows and send WM_CLOSE
        EnumWindows(EnumWindowsCloseCallback, (LPARAM)dwPID);

        // give it a short moment
        Sleep(200);

        if (TerminateProcess(hProcess, 0))
        {
            CloseHandle(hProcess);
            MessageBox(_T("进程已成功结束。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
            RefreshProcessList();
        }
        else
        {
            DWORD err = GetLastError();
            CString msg;
            msg.Format(_T("结束进程失败：%s"), FormatLastError(err));
            MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
        }
    }
    else
    {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(_T("无法打开进程以终止。错误：%s"), FormatLastError(err));
        // if access denied, prompt for elevation
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
                ::PostMessage(this->GetSafeHwnd(), WM_NULL, 0, 0);
        }
        MessageBox(msg, _T("权限不足"), MB_OK | MB_ICONERROR);
    }
}

// File management: handle files dropped onto the dialog
void CMFCApplication1Dlg::OnDropFiles(HDROP hDropInfo)
{
    TCHAR szFilePath[MAX_PATH] = {0};
    if (DragQueryFile(hDropInfo, 0, szFilePath, MAX_PATH) > 0)
    {
        m_strDroppedFilePath = szFilePath;
        // display path
        CWnd* pStatic = GetDlgItem(IDC_STATIC_PATH);
        if (pStatic)
            pStatic->SetWindowText(m_strDroppedFilePath);
        else
            ;
        // restore edit IDC_EDIT4 to default content whenever a new file is dropped
        SetDlgItemText(IDC_EDIT4, AfxGetApp()->GetProfileString(_T("Template"), _T("DefaultReportName"), _T("")));

        // Automatically switch to tab 5 (index 4) when a file is dropped
        CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB1);
        if (pTab)
        {
            pTab->SetCurSel(4);
            LRESULT res = 0;
            // call handler to update control visibility
            OnTcnSelchangeTab1(NULL, &res);
            // populate rename edits: IDC_EDIT7 (basename without ext), IDC_EDIT8 (extension without dot)
            int nSlash = m_strDroppedFilePath.ReverseFind(_T('\\'));
            int nDot = m_strDroppedFilePath.ReverseFind(_T('.'));
            CString base, ext;
            if (nDot != -1 && nDot > nSlash)
            {
                base = m_strDroppedFilePath.Mid(nSlash + 1, nDot - nSlash - 1);
                ext = m_strDroppedFilePath.Mid(nDot + 1); // without dot
            }
            else
            {
                base = m_strDroppedFilePath.Mid(nSlash + 1);
                ext = _T("");
            }
            SetDlgItemText(IDC_EDIT7, base);
            SetDlgItemText(IDC_EDIT8, ext);
        }
    }

    DragFinish(hDropInfo);
    CDialogEx::OnDropFiles(hDropInfo);
}

// File management helper removed

// Named pipe listener: receives one-line file paths from DropHelper and posts to main window
// DropHelper and named pipe listener removed

void CMFCApplication1Dlg::OnBnClickedButton3()
{
    // Read current file path and requested new name
    CString src = m_strDroppedFilePath;
    if (src.IsEmpty())
    {
        MessageBox(_T("请先拖入文件！"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    CString newName;
    GetDlgItemText(IDC_EDIT4, newName);
    newName.Trim();
    if (newName.IsEmpty())
    {
        newName = AfxGetApp()->GetProfileString(_T("Template"), _T("DefaultReportName"), _T(""));
        if (newName.IsEmpty())
        {
            MessageBox(_T("请先在 文件→设置→文件命名 中配置默认文件名。"), _T("提示"), MB_OK | MB_ICONWARNING);
            SetDlgItemText(IDC_EDIT4, newName);
            return;
        }
        SetDlgItemText(IDC_EDIT4, newName);
    }

    // build paths
    int nSlash = src.ReverseFind(_T('\\'));
    CString dir = (nSlash != -1) ? src.Left(nSlash + 1) : CString(_T(""));
    int nDot = src.ReverseFind(_T('.'));
    CString ext = (nDot != -1 && nDot > nSlash) ? src.Mid(nDot) : CString(_T(""));

    CString candidate = dir + newName + ext;
    CString baseName = newName;
    // avoid infinite loop: limit attempts
    int attempt = 0;
    while (GetFileAttributes(candidate) != INVALID_FILE_ATTRIBUTES && attempt < 1000)
    {
        attempt++;
        baseName = newName + _T("_副本");
        candidate = dir + baseName + ext;
        newName = baseName; // next iteration will append again if needed
    }

    if (attempt >= 1000)
    {
        MessageBox(_T("无法生成唯一文件名，请检查目标目录权限或手动选择不同名称。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    if (CopyFile(src, candidate, FALSE))
    {
        CString msg; msg.Format(_T("生成副本成功！\n保存路径：%s"), candidate);
        MessageBox(msg, _T("成功"), MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        CString err; err.Format(_T("生成副本失败，错误代码：%u"), GetLastError());
        MessageBox(err, _T("错误"), MB_OK | MB_ICONERROR);
    }
}


// Rename handler for IDC_BUTTON23
void CMFCApplication1Dlg::OnBnClickedButton23()
{
    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(_T("请先拖入文件！"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    CString newBase, newExt;
    GetDlgItemText(IDC_EDIT7, newBase);
    GetDlgItemText(IDC_EDIT8, newExt);
    newBase.Trim(); newExt.Trim();
    if (newBase.IsEmpty())
    {
        MessageBox(_T("文件名不能为空。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    // validate characters not allowed in Windows filenames
    CString invalid = _T("\\/:*?\"<>|");
    for (int i = 0; i < invalid.GetLength(); ++i)
    {
        if (newBase.Find(invalid[i]) != -1 || newExt.Find(invalid[i]) != -1)
        {
            MessageBox(_T("文件名或扩展名包含不合法字符。\\ / : * ? \" < > | 不允许。"), _T("错误"), MB_OK | MB_ICONERROR);
            return;
        }
    }

    int nSlash = m_strDroppedFilePath.ReverseFind(_T('\\'));
    int nDot = m_strDroppedFilePath.ReverseFind(_T('.'));
    CString dir = (nSlash != -1) ? m_strDroppedFilePath.Left(nSlash + 1) : CString(_T(""));
    CString srcExt = (nDot != -1 && nDot > nSlash) ? m_strDroppedFilePath.Mid(nDot) : CString(_T(""));

    CString dst;
    if (newExt.IsEmpty())
        dst.Format(_T("%s%s%s"), dir, newBase, srcExt);
    else
        dst.Format(_T("%s%s.%s"), dir, newBase, newExt);

    if (GetFileAttributes(dst) != INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(_T("目标文件已存在，请选择其他名称。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    if (MoveFile(m_strDroppedFilePath, dst))
    {
        CString msg; msg.Format(_T("重命名成功：%s"), dst);
        MessageBox(msg, _T("成功"), MB_OK | MB_ICONINFORMATION);
        m_strDroppedFilePath = dst;
        SetDlgItemText(IDC_STATIC_PATH, m_strDroppedFilePath);
        // do not modify IDC_EDIT4 here; user-managed copy name should remain as-is
    }
    else
    {
        DWORD err = GetLastError();
        CString emsg; emsg.Format(_T("重命名失败：%s (错误代码 %u)"), FormatLastError(err), err);
        MessageBox(emsg, _T("错误"), MB_OK | MB_ICONERROR);
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
            {
                // Close dialog to allow restarted elevated instance to run
                EndDialog(IDOK);
                return;
            }
        }
    }
}


void CMFCApplication1Dlg::OnStnClickedStaticPath()
{
    // TODO: 在此添加控件通知处理程序代码
}

// Copy selected dropped file to target directory specified in IDC_MFCEDITBROWSE2
void CMFCApplication1Dlg::OnBnClickedButton25()
{
    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(_T("请先拖入文件！"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    CString destDir;
    GetDlgItemText(IDC_MFCEDITBROWSE2, destDir);
    destDir.Trim();
    if (destDir.IsEmpty() || GetFileAttributes(destDir) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(_T("目标路径无效，请在下方输入或选择有效目录。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    DWORD attr = GetFileAttributes(destDir);
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        MessageBox(_T("目标路径不是目录，请选择文件夹路径。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    int pos = m_strDroppedFilePath.ReverseFind(_T('\\'));
    CString fileName = (pos != -1) ? m_strDroppedFilePath.Mid(pos + 1) : m_strDroppedFilePath;
    CString dst; dst.Format(_T("%s\\%s"), destDir, fileName);

    // If exists, generate unique name
    CString nameOnly = fileName;
    CString ext = _T("");
    int dot = fileName.ReverseFind('.');
    if (dot != -1) { nameOnly = fileName.Left(dot); ext = fileName.Mid(dot); }
    int attempt = 0;
    while (GetFileAttributes(dst) != INVALID_FILE_ATTRIBUTES && attempt < 1000)
    {
        attempt++;
        CString newName; newName.Format(_T("%s_copy%d%s"), nameOnly, attempt, ext);
        dst.Format(_T("%s\\%s"), destDir, newName);
    }

    if (attempt >= 1000)
    {
        MessageBox(_T("目标文件无法生成唯一名称，请检查目录或手动指定不同的名称。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    if (CopyFile(m_strDroppedFilePath, dst, FALSE))
    {
        CString msg; msg.Format(_T("复制成功：%s"), dst);
        MessageBox(msg, _T("完成"), MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        CString err; err.Format(_T("复制失败，错误代码：%u"), GetLastError());
        MessageBox(err, _T("错误"), MB_OK | MB_ICONERROR);
    }
}

// Move selected dropped file to target directory specified in IDC_MFCEDITBROWSE2
void CMFCApplication1Dlg::OnBnClickedButton26()
{
    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(_T("请先拖入文件！"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    CString destDir;
    GetDlgItemText(IDC_MFCEDITBROWSE2, destDir);
    destDir.Trim();
    if (destDir.IsEmpty() || GetFileAttributes(destDir) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(_T("目标路径无效，请在下方输入或选择有效目录。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    DWORD attr = GetFileAttributes(destDir);
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        MessageBox(_T("目标路径不是目录，请选择文件夹路径。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    int pos = m_strDroppedFilePath.ReverseFind(_T('\\'));
    CString fileName = (pos != -1) ? m_strDroppedFilePath.Mid(pos + 1) : m_strDroppedFilePath;
    CString dst; dst.Format(_T("%s\\%s"), destDir, fileName);

    if (GetFileAttributes(dst) != INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(_T("目标目录中已存在同名文件，请先移除或更改目标路径。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    if (MoveFile(m_strDroppedFilePath, dst))
    {
        CString msg; msg.Format(_T("移动成功：%s"), dst);
        MessageBox(msg, _T("完成"), MB_OK | MB_ICONINFORMATION);
        m_strDroppedFilePath = dst;
        SetDlgItemText(IDC_STATIC_PATH, m_strDroppedFilePath);
    }
    else
    {
        DWORD err = GetLastError();
        CString em; em.Format(_T("移动失败，错误代码：%u"), err);
        MessageBox(em, _T("错误"), MB_OK | MB_ICONERROR);
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
            {
                EndDialog(IDOK);
                return;
            }
        }
    }
}

// 复选按钮：设置或取消开机自启动（写入或删除当前用户 Run 注册表项）
void CMFCApplication1Dlg::OnBnClickedCheck1()
{
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK1);
    if (!pCheck) return;

    // 获取可执行文件名及路径
    TCHAR exePath[MAX_PATH] = {0};
    if (GetModuleFileName(NULL, exePath, MAX_PATH) == 0)
    {
        MessageBox(_T("无法获取程序路径。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    CString csExePath = exePath;
    int pos = csExePath.ReverseFind(_T('\\'));
    CString keyName = (pos != -1) ? csExePath.Mid(pos + 1) : csExePath; // 使用可执行文件名作为注册表项名

    HKEY hKey = NULL;
    LONG ret = RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    if (ret != ERROR_SUCCESS)
    {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(_T("无法打开注册表键，请检查权限。错误：%s"), FormatLastError(err));
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
                return;
        }
        MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    // 查询当前注册表中是否已有自启动项，以实现复选按钮的开关切换效果
    DWORD type = 0;
    TCHAR buf[MAX_PATH] = {0};
    DWORD bufSize = sizeof(buf);
    LONG checkRet = RegQueryValueEx(hKey, keyName, NULL, &type, (LPBYTE)buf, &bufSize);
    BOOL isAlreadyAutostart = (checkRet == ERROR_SUCCESS && type == REG_SZ);

    if (isAlreadyAutostart)
    {
        // 取消自启动：删除注册表值
        ret = RegDeleteValue(hKey, keyName);
        if (ret == ERROR_SUCCESS || ret == ERROR_FILE_NOT_FOUND)
        {
            MessageBox(_T("已取消开机自启动"), _T("提示"), MB_OK | MB_ICONINFORMATION);
            pCheck->SetCheck(BST_UNCHECKED);
            AfxGetApp()->WriteProfileInt(_T("Settings"), _T("AutoStart"), 0);
        }
        else
        {
            DWORD err = GetLastError();
            CString msg;
            msg.Format(_T("删除启动项失败，请检查权限。错误：%s"), FormatLastError(err));
            if (err == ERROR_ACCESS_DENIED)
            {
                if (PromptRestartElevated()) { RegCloseKey(hKey); return; }
            }
            MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
            pCheck->SetCheck(BST_CHECKED);
        }
    }
    else
    {
        // 设置自启动：写入注册表，值为可执行完整路径并附加我们约定的启动标记 --elevate
        CString runValue;
        // Quote path in case it contains spaces
        runValue.Format(_T("\"%s\" --elevate"), csExePath);
        LONG setRet = RegSetValueEx(hKey, keyName, 0, REG_SZ, (const BYTE*)(LPCTSTR)runValue, (runValue.GetLength() + 1) * sizeof(TCHAR));
        if (setRet == ERROR_SUCCESS)
        {
            MessageBox(_T("已设置为开机自启动。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
            pCheck->SetCheck(BST_CHECKED);
            AfxGetApp()->WriteProfileInt(_T("Settings"), _T("AutoStart"), 1);
        }
        else
        {
            CString msg;
            msg.Format(_T("添加启动项失败，请检查权限。错误：%s"), FormatLastError(GetLastError()));
            if (GetLastError() == ERROR_ACCESS_DENIED)
            {
                if (PromptRestartElevated()) { RegCloseKey(hKey); return; }
            }
            MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
            // 恢复为未选中
            pCheck->SetCheck(BST_UNCHECKED);
        }
    }

    RegCloseKey(hKey);
}

void CMFCApplication1Dlg::OnBnClickedCheck2()
{
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK2);
    if (!pCheck) return;

    // 更新内部标志
    m_bMinimizeOnClose = (pCheck->GetCheck() == BST_CHECKED);
}

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

void CMFCApplication1Dlg::OnBnClickedButton20()
{
    // Try to launch Task Manager. ShellExecute usually finds it in PATH/System32.
    HINSTANCE h = ::ShellExecute(m_hWnd, _T("open"), _T("taskmgr.exe"), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        // Fallback: try explicit system directory path
        TCHAR sysPath[MAX_PATH] = {0};
        if (GetSystemDirectory(sysPath, MAX_PATH) > 0)
        {
            CString full;
            full.Format(_T("%s\\taskmgr.exe"), sysPath);
            HINSTANCE h2 = ::ShellExecute(m_hWnd, _T("open"), full, NULL, NULL, SW_SHOWNORMAL);
            if ((INT_PTR)h2 <= 32)
            {
                MessageBox(_T("无法启动任务管理器，请手动运行 taskmgr.exe"), _T("错误"), MB_OK | MB_ICONERROR);
            }
        }
        else
        {
            MessageBox(_T("无法启动任务管理器，请手动运行 taskmgr.exe"), _T("错误"), MB_OK | MB_ICONERROR);
        }
    }
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

            if (m_hSelectedWnd && ::IsWindow(m_hSelectedWnd))
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

void CMFCApplication1Dlg::OnLocateProcess()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    // 路径在第3列 (index 为 2)
    CString path = pList->GetItemText(idx, 2);
    if (path.IsEmpty())
    {
        MessageBox(_T("无法获取该进程的路径或路径为空。"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
    {
        CString args;
        args.Format(_T("/select,\"%s\""), path);
        ::ShellExecute(NULL, _T("open"), _T("explorer.exe"), args, NULL, SW_SHOWNORMAL);
    }
    else
    {
        MessageBox(_T("找不到该文件，可能无权访问或进程已退出。"), _T("提示"), MB_OK | MB_ICONWARNING);
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

void CMFCApplication1Dlg::OnBnClickedCheck3()
{
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK3);
    if (!pCheck) return;

    if (pCheck->GetCheck() == BST_CHECKED)
    {
        // 置顶
        SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        // 避免重复添加
        auto it = std::find(m_topmostWnds.begin(), m_topmostWnds.end(), m_hWnd);
        if (it == m_topmostWnds.end())
            m_topmostWnds.push_back(m_hWnd);
    }
    else
    {
        // 取消置顶
        SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        m_topmostWnds.erase(
            std::remove(m_topmostWnds.begin(), m_topmostWnds.end(), m_hWnd),
            m_topmostWnds.end());
    }

    // 刷新置顶窗口列表
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
    if (pTab && pTab->GetCurSel() == 3)
        UpdateTabVisibility(3);
}

void CMFCApplication1Dlg::OnBnClickedButton18()
{
    SetDlgItemText(IDC_EDIT6, _T(""));
}


// Handler for autoclick checkbox in main dialog (C++20: using CAutoClicker class)
void CMFCApplication1Dlg::OnBnClickedCheck4()
{
    CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK4));
    if (!pCheck) return;

    if (pCheck->GetCheck() == BST_CHECKED)
    {
        m_autoClicker.Start(100, m_hWnd);
    }
    else
    {
        m_autoClicker.Stop();
    }
}

// Message handler invoked when autoclick stops due to B key
afx_msg LRESULT CMFCApplication1Dlg::OnAutoClickStopped(WPARAM wParam, LPARAM lParam)
{
    // ensure checkbox is unchecked and show message
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK4);
    if (pCheck) pCheck->SetCheck(BST_UNCHECKED);
    MessageBox(_T("连点停止"), _T("提示"), MB_OK | MB_ICONINFORMATION);
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
    if (pStatic && ::IsWindow(pStatic->GetSafeHwnd()))
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


