#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "resource.h"
#include "Utils.h"

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

void CMFCApplication1Dlg::OnBnClickedButton19()
{
    // If overlay exists, cancel it
    if (m_hCaptureWnd && IsValidWindow(m_hCaptureWnd))
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

void CMFCApplication1Dlg::OnForceKillProcess()
{
	if (!m_hSelectedWnd || !IsValidWindow(m_hSelectedWnd))
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
				[](HWND h) { return !IsValidWindow(h); }),
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
	if (!m_hSelectedWnd || !IsValidWindow(m_hSelectedWnd))
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

	// 使用 PrintWindow 截取窗口（多级回退）
	// PW_RENDERFULLCONTENT(0x2): 对 DWM 合成窗口效果最好，支持 GPU 渲染内容
	BOOL bPrintOK = ::PrintWindow(m_hSelectedWnd, hdcMem, 0x2);
	if (!bPrintOK)
	{
		// 回退1: 不带标志的 PrintWindow
		bPrintOK = ::PrintWindow(m_hSelectedWnd, hdcMem, 0);
	}
	if (!bPrintOK)
	{
		// 回退2: BitBlt 从屏幕直接拷贝
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

		// 确保目录存在
		::SHCreateDirectoryEx(NULL, sDir, NULL);

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
		if (IsValidWindow(hWnd))
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
	if (m_hSelectedWnd && IsValidWindow(m_hSelectedWnd))
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
					[](HWND h) { return !IsValidWindow(h); }),
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
	if (nSel < 0) return;

	size_t idx = static_cast<size_t>(pList6->GetItemData(nSel));
	if (idx >= m_topmostWnds.size()) return;

	HWND hWnd = m_topmostWnds[idx];
	if (IsValidWindow(hWnd))
		::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);

	m_topmostWnds.erase(m_topmostWnds.begin() + idx);

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
		_T("F5               - 刷新当前列表\n")
		_T("Ctrl+Alt+D       - 定位窗口\n\n")
		_T("更多功能请查看视图/工具/窗口菜单。"),
		_T("快捷键列表"), MB_OK | MB_ICONINFORMATION);
}

void CMFCApplication1Dlg::OnHelpGithub()
{
	ShellExecute(m_hWnd, _T("open"), _T("https://github.com"), NULL, NULL, SW_SHOWNORMAL);
}

void CMFCApplication1Dlg::LoadWindowDetailToList5(HWND hWnd)
{
	CListCtrl* pList5 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST5));
	if (!pList5) return;

	pList5->DeleteAllItems();

	if (!hWnd || !IsValidWindow(hWnd)) return;

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
	if (nSel < 0) { *pResult = 0; return; }

	size_t idx = static_cast<size_t>(pList6->GetItemData(nSel));
	if (idx >= m_topmostWnds.size()) { *pResult = 0; return; }

	HWND hWnd = m_topmostWnds[idx];
	if (hWnd && IsValidWindow(hWnd))
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
	if (nSel < 0) { *pResult = 0; return; }

	size_t idx = static_cast<size_t>(pList7->GetItemData(nSel));
	if (idx >= m_historyWnds.size()) { *pResult = 0; return; }

	HWND hWnd = m_historyWnds[idx];
	if (hWnd && IsValidWindow(hWnd))
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
	if (nSel < 0) return;

	size_t idx = static_cast<size_t>(pList6->GetItemData(nSel));
	if (idx >= m_topmostWnds.size()) return;

	HWND hWnd = m_topmostWnds[idx];
	if (IsValidWindow(hWnd))
		::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);

	if (hWnd == m_hWnd)
	{
		CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
		if (pCheck) pCheck->SetCheck(BST_UNCHECKED);
	}
	m_topmostWnds.erase(m_topmostWnds.begin() + idx);

	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnDeleteList7Record()
{
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (!pList7) return;

	int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) return;

	size_t idx = static_cast<size_t>(pList7->GetItemData(nSel));
	if (idx >= m_historyWnds.size()) return;

	m_historyWnds.erase(m_historyWnds.begin() + idx);

	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnTopmostFromHistory()
{
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (!pList7) return;

	int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) return;

	size_t idx = static_cast<size_t>(pList7->GetItemData(nSel));
	if (idx >= m_historyWnds.size()) return;

	HWND hWnd = m_historyWnds[idx];
	if (!IsValidWindow(hWnd)) return;

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
	if (nSel < 0) return;

	size_t idx = static_cast<size_t>(pList7->GetItemData(nSel));
	if (idx >= m_historyWnds.size()) return;

	HWND hWnd = m_historyWnds[idx];
	if (IsValidWindow(hWnd))
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