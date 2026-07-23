// ContextMenuDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "ContextMenuDlg.h"
#include "afxdialogex.h"
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Menu command IDs for right-click context menu in the list
#define ID_MENU_CM_DELETE   50020
#define ID_MENU_CM_LOCATE   50021

IMPLEMENT_DYNAMIC(CContextMenuDlg, CDialogEx)

CContextMenuDlg::CContextMenuDlg(CWnd* pParent)
	: CDialogEx(IDD_CONTEXT_MENU_DLG, pParent)
	, m_listLeft(0), m_listTop(0), m_statusTop(0)
{
}

CContextMenuDlg::~CContextMenuDlg()
{
}

void CContextMenuDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CContextMenuDlg, CDialogEx)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BTN_CM_REFRESH, &CContextMenuDlg::OnBnClickedRefresh)
	ON_CBN_SELCHANGE(IDC_COMBO_CM_LOCATION, &CContextMenuDlg::OnCbnSelchangeLocation)
	ON_BN_CLICKED(IDC_BTN_CM_DELETE, &CContextMenuDlg::OnBnClickedDelete)
	ON_BN_CLICKED(IDC_BTN_CM_LOCATE, &CContextMenuDlg::OnBnClickedLocate)
	ON_BN_CLICKED(IDC_CHECK_CM_FOLDER, &CContextMenuDlg::OnBnClickedCheckFolder)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_CM_ENTRIES, &CContextMenuDlg::OnNMRClickList)
	ON_COMMAND(ID_MENU_CM_DELETE, &CContextMenuDlg::OnMenuDelete)
	ON_COMMAND(ID_MENU_CM_LOCATE, &CContextMenuDlg::OnMenuLocate)
END_MESSAGE_MAP()

BOOL CContextMenuDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_F5)
	{
		OnBnClickedRefresh();
		return TRUE;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

void CContextMenuDlg::InitLocations()
{
	m_locations.clear();
	// Common shell context menu locations in HKCR (static shell verbs)
	m_locations.push_back({ _T("全部"),                                 _T(""), false });
	m_locations.push_back({ _T("文件 (*)"),                             _T("*\\shell"), false });
	m_locations.push_back({ _T("文件夹 (Directory)"),                   _T("Directory\\shell"), false });
	m_locations.push_back({ _T("文件夹 (Folder)"),                      _T("Folder\\shell"), false });
	m_locations.push_back({ _T("文件夹背景 (Directory\\Background)"),   _T("Directory\\Background\\shell"), false });
	m_locations.push_back({ _T("桌面背景"),                             _T("DesktopBackground\\shell"), false });
	m_locations.push_back({ _T("驱动器"),                               _T("Drive\\shell"), false });
	m_locations.push_back({ _T("所有文件 (AllFilesystemObjects)"),      _T("AllFilesystemObjects\\shell"), false });
	m_locations.push_back({ _T("可执行文件 (exefile)"),                 _T("exefile\\shell"), false });
	m_locations.push_back({ _T("文本文件 (txtfile)"),                   _T("txtfile\\shell"), false });
	// Common file extensions (static shell verbs registered per-extension)
	m_locations.push_back({ _T("文本文件 (.txt)"),                      _T(".txt\\shell"), false });
	m_locations.push_back({ _T("图片 (.png)"),                          _T(".png\\shell"), false });
	m_locations.push_back({ _T("图片 (.jpg)"),                          _T(".jpg\\shell"), false });
	m_locations.push_back({ _T("PDF (.pdf)"),                           _T(".pdf\\shell"), false });
	m_locations.push_back({ _T("可执行文件 (.exe)"),                    _T(".exe\\shell"), false });
	m_locations.push_back({ _T("压缩包 (.zip)"),                        _T(".zip\\shell"), false });
	m_locations.push_back({ _T("Word (.docx)"),                         _T(".docx\\shell"), false });
	m_locations.push_back({ _T("批处理 (.bat)"),                        _T(".bat\\shell"), false });
	m_locations.push_back({ _T("注册表 (.reg)"),                        _T(".reg\\shell"), false });
	// COM-based context menu handlers (shellex\ContextMenuHandlers)
	m_locations.push_back({ _T("文件 Shellex (COM)"),                   _T("*\\shellex\\ContextMenuHandlers"), true });
	m_locations.push_back({ _T("文件夹 Shellex (Directory COM)"),       _T("Directory\\shellex\\ContextMenuHandlers"), true });
	m_locations.push_back({ _T("文件夹背景 Shellex (COM)"),             _T("Directory\\Background\\shellex\\ContextMenuHandlers"), true });
	m_locations.push_back({ _T("文件夹 Shellex (Folder COM)"),          _T("Folder\\shellex\\ContextMenuHandlers"), true });
	m_locations.push_back({ _T("所有文件 Shellex (COM)"),               _T("AllFilesystemObjects\\shellex\\ContextMenuHandlers"), true });

	CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO_CM_LOCATION);
	if (pCombo)
	{
		for (size_t i = 0; i < m_locations.size(); ++i)
			pCombo->AddString(m_locations[i].name);
		pCombo->SetCurSel(0);
	}
}

CString CContextMenuDlg::ResolveClsidName(const CString& clsid)
{
	// Look up HKCR\CLSID\{guid} default value for a friendly name
	CString friendly;
	CString clsidKey = _T("CLSID\\") + clsid;
	HKEY hKey = nullptr;
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, clsidKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		TCHAR szVal[MAX_PATH] = { 0 };
		DWORD cbVal = sizeof(szVal);
		DWORD dwType = 0;
		if (RegQueryValueEx(hKey, nullptr, nullptr, &dwType,
			(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
		{
			friendly = szVal;
		}
		RegCloseKey(hKey);
	}
	return friendly;
}

void CContextMenuDlg::ScanShellExLocation(const LocationFilter& loc)
{
	HKEY hShell = nullptr;
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, loc.shellPath, 0, KEY_READ, &hShell) != ERROR_SUCCESS)
		return;

	DWORD dwIndex = 0;
	TCHAR szSubKey[MAX_PATH];
	DWORD cbSubKey = MAX_PATH;

	while (RegEnumKeyEx(hShell, dwIndex, szSubKey, &cbSubKey,
		nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
	{
		CString subKeyPath = loc.shellPath + _T("\\") + szSubKey;

		// The default value of a shellex handler entry is a CLSID
		CString clsid;
		CString command;
		HKEY hVerb = nullptr;
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, subKeyPath, 0, KEY_READ, &hVerb) == ERROR_SUCCESS)
		{
			TCHAR szVal[MAX_PATH] = { 0 };
			DWORD cbVal = sizeof(szVal);
			DWORD dwType = 0;
			if (RegQueryValueEx(hVerb, nullptr, nullptr, &dwType,
				(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
			{
				clsid = szVal;
			}
			RegCloseKey(hVerb);
		}

		// Resolve the CLSID to a friendly name and the InprocServer32 DLL path
		CString displayName = szSubKey;
		if (!clsid.IsEmpty())
		{
			CString friendly = ResolveClsidName(clsid);
			if (!friendly.IsEmpty())
				displayName = friendly;

			// Read the DLL path from CLSID\InprocServer32 as the "command"
			CString inprocKey = _T("CLSID\\") + clsid + _T("\\InprocServer32");
			HKEY hInproc = nullptr;
			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, inprocKey, 0, KEY_READ, &hInproc) == ERROR_SUCCESS)
			{
				TCHAR szDll[MAX_PATH * 2] = { 0 };
				DWORD cbDll = sizeof(szDll);
				DWORD dwType = 0;
				if (RegQueryValueEx(hInproc, nullptr, nullptr, &dwType,
					(LPBYTE)szDll, &cbDll) == ERROR_SUCCESS && szDll[0])
				{
					command = szDll;
				}
				RegCloseKey(hInproc);
			}
		}

		m_entries.push_back({
			loc.name, szSubKey, displayName, command,
			loc.shellPath, HKEY_CLASSES_ROOT
		});

		dwIndex++;
		cbSubKey = MAX_PATH;
	}
	RegCloseKey(hShell);
}

void CContextMenuDlg::ScanEntries(const CString& filter)
{
	m_entries.clear();

	for (const auto& loc : m_locations)
	{
		if (loc.shellPath.IsEmpty()) continue; // skip "全部"
		if (!filter.IsEmpty() && loc.name != filter) continue;

		// COM-based shellex handlers use a different structure
		if (loc.isShellEx)
		{
			ScanShellExLocation(loc);
			continue;
		}

		HKEY hShell = nullptr;
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, loc.shellPath, 0, KEY_READ, &hShell) == ERROR_SUCCESS)
		{
			DWORD dwIndex = 0;
			TCHAR szSubKey[MAX_PATH];
			DWORD cbSubKey = MAX_PATH;

			while (RegEnumKeyEx(hShell, dwIndex, szSubKey, &cbSubKey,
				nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
			{
				CString subKeyPath = loc.shellPath + _T("\\") + szSubKey;

				// Read display name: prefer MUIVerb, fall back to default value, then key name
				CString displayName = szSubKey;
				HKEY hVerb = nullptr;
				if (RegOpenKeyEx(HKEY_CLASSES_ROOT, subKeyPath, 0, KEY_READ, &hVerb) == ERROR_SUCCESS)
				{
					TCHAR szVal[MAX_PATH] = { 0 };
					DWORD cbVal = sizeof(szVal);
					DWORD dwType = 0;
					BOOL bFound = FALSE;
					// Try MUIVerb first (Windows shell preference)
					if (RegQueryValueEx(hVerb, _T("MUIVerb"), nullptr, &dwType,
						(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0] && szVal[0] != _T('@'))
					{
						displayName = szVal;
						bFound = TRUE;
					}
					// Fall back to default value
					if (!bFound)
					{
						cbVal = sizeof(szVal);
						szVal[0] = 0;
						if (RegQueryValueEx(hVerb, nullptr, nullptr, &dwType,
							(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0] && szVal[0] != _T('@'))
						{
							displayName = szVal;
						}
					}
					RegCloseKey(hVerb);
				}

				// Read command from "command" subkey
				CString command;
				CString cmdPath = subKeyPath + _T("\\command");
				HKEY hCmd = nullptr;
				if (RegOpenKeyEx(HKEY_CLASSES_ROOT, cmdPath, 0, KEY_READ, &hCmd) == ERROR_SUCCESS)
				{
					TCHAR szCmd[MAX_PATH * 2] = { 0 };
					DWORD cbCmd = sizeof(szCmd);
					DWORD dwType = 0;
					if (RegQueryValueEx(hCmd, nullptr, nullptr, &dwType,
						(LPBYTE)szCmd, &cbCmd) == ERROR_SUCCESS)
					{
						command = szCmd;
					}
					RegCloseKey(hCmd);
				}

				m_entries.push_back({
					loc.name, szSubKey, displayName, command,
					loc.shellPath, HKEY_CLASSES_ROOT
				});

				dwIndex++;
				cbSubKey = MAX_PATH;
			}
			RegCloseKey(hShell);
		}
	}
}

void CContextMenuDlg::RefreshList()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList) return;

	pList->SetRedraw(FALSE);
	pList->DeleteAllItems();

	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		int idx = pList->InsertItem((int)i, m_entries[i].location);
		pList->SetItemText(idx, 1, m_entries[i].displayName);
		pList->SetItemText(idx, 2, m_entries[i].keyName);
		pList->SetItemText(idx, 3, m_entries[i].command);
	}
	pList->SetRedraw(TRUE);

	CString status;
	status.Format(_T("共找到 %d 个右键菜单项"), (int)m_entries.size());
	UpdateStatus(status);
}

void CContextMenuDlg::UpdateStatus(const CString& text)
{
	SetDlgItemText(IDC_STATIC_CM_STATUS, text);
}

BOOL CContextMenuDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Read layout
	auto ReadRect = [&](int id) -> CRect {
		CRect rc(0, 0, 0, 0);
		CWnd* pWnd = GetDlgItem(id);
		if (pWnd) { pWnd->GetWindowRect(&rc); ScreenToClient(&rc); }
		return rc;
	};

	CRect rc = ReadRect(IDC_LIST_CM_ENTRIES);
	m_listLeft = rc.left; m_listTop = rc.top;

	rc = ReadRect(IDC_STATIC_CM_STATUS);
	m_statusTop = rc.top;

	// Initialize list columns
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (pList)
	{
		pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		pList->InsertColumn(0, _T("位置"),     LVCFMT_LEFT, 50);
		pList->InsertColumn(1, _T("显示名称"), LVCFMT_LEFT, 150);
		pList->InsertColumn(2, _T("键名"),     LVCFMT_LEFT, 70);
		pList->InsertColumn(3, _T("命令"),     LVCFMT_LEFT, 130);
	}

	InitLocations();
	ScanEntries(_T(""));
	RefreshList();

	// Apply proportional column widths based on actual list width
	AdjustColumnWidths();

	// Load self context menu state
	LoadSelfContextMenuState();

	return TRUE;
}

void CContextMenuDlg::PostNcDestroy()
{
	CDialogEx::PostNcDestroy();
	delete this;
}

void CContextMenuDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType != SIZE_MINIMIZED && IsWindow(m_hWnd))
	{
		CRect rcClient;
		GetClientRect(&rcClient);

		CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
		if (pList)
		{
			pList->SetWindowPos(nullptr, m_listLeft, m_listTop,
				rcClient.Width() - m_listLeft - 15,
				m_statusTop - m_listTop - 5, SWP_NOZORDER);
			// Re-apply proportional column widths after resize
			AdjustColumnWidths();
		}

		// Status label
		CWnd* pStatus = GetDlgItem(IDC_STATIC_CM_STATUS);
		if (pStatus)
			pStatus->SetWindowPos(nullptr, m_listLeft, m_statusTop,
				0, 0, SWP_NOSIZE | SWP_NOZORDER);

		// Group box and checkbox at bottom
		CWnd* pGroup = GetDlgItem(IDC_STATIC);
		// These stay fixed, no repositioning needed
	}
}

bool CContextMenuDlg::DeleteRegistryKeyRecursive(HKEY hParent, const CString& subKey)
{
	// Use SHDeleteKey for recursive deletion
	return (SHDeleteKey(hParent, subKey) == ERROR_SUCCESS);
}

void CContextMenuDlg::OpenRegEditToPath(HKEY hRoot, const CString& path)
{
	// Save the desired key path in regedit's last-opened registry
	// regedit remembers the last selected key in:
	// HKCU\Software\Microsoft\Windows\CurrentVersion\Applets\Regedit\LastKey
	CString fullRegPath;
	if (hRoot == HKEY_CLASSES_ROOT)
		fullRegPath = _T("HKEY_CLASSES_ROOT\\") + path;
	else
		fullRegPath = path;

	HKEY hKey = nullptr;
	CString regeditKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit");
	if (RegOpenKeyEx(HKEY_CURRENT_USER, regeditKey, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
	{
		RegSetValueEx(hKey, _T("LastKey"), 0, REG_SZ,
			(LPBYTE)fullRegPath.GetString(),
			(fullRegPath.GetLength() + 1) * sizeof(TCHAR));
		RegCloseKey(hKey);
	}

	// Launch regedit
	ShellExecute(nullptr, _T("open"), _T("regedit.exe"), nullptr, nullptr, SW_SHOWNORMAL);
}

void CContextMenuDlg::LoadSelfContextMenuState()
{
	// Check if our folder context menu entry exists
	HKEY hKey = nullptr;
	CString path = _T("Directory\\shell\\MFCApplication1");
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, path, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		CheckDlgButton(IDC_CHECK_CM_FOLDER, BST_CHECKED);
	}
	else
	{
		CheckDlgButton(IDC_CHECK_CM_FOLDER, BST_UNCHECKED);
	}
}

void CContextMenuDlg::SaveSelfContextMenuState(bool bEnable)
{
	// Get our exe path
	TCHAR szExe[MAX_PATH];
	GetModuleFileName(nullptr, szExe, MAX_PATH);
	CString exePath = szExe;

	CString baseKey = _T("Directory\\shell\\MFCApplication1");

	if (bEnable)
	{
		// Create the shell verb key
		HKEY hKey = nullptr;
		DWORD dwDisp = 0;
		if (RegCreateKeyEx(HKEY_CLASSES_ROOT, baseKey, 0, nullptr, 0,
			KEY_WRITE, nullptr, &hKey, &dwDisp) == ERROR_SUCCESS)
		{
			RegSetValueEx(hKey, nullptr, 0, REG_SZ,
				(LPBYTE)_T("用 MFC工具箱打开"),
				(DWORD)((_tcslen(_T("用 MFC工具箱打开")) + 1) * sizeof(TCHAR)));
			RegSetValueEx(hKey, _T("Icon"), 0, REG_SZ,
				(LPBYTE)exePath.GetString(),
				(exePath.GetLength() + 1) * sizeof(TCHAR));
			RegCloseKey(hKey);
		}

		// Create the command subkey
		CString cmdKey = baseKey + _T("\\command");
		if (RegCreateKeyEx(HKEY_CLASSES_ROOT, cmdKey, 0, nullptr, 0,
			KEY_WRITE, nullptr, &hKey, &dwDisp) == ERROR_SUCCESS)
		{
			CString cmd;
			cmd.Format(_T("\"%s\" \"%s\""), exePath, _T("%1"));
			RegSetValueEx(hKey, nullptr, 0, REG_SZ,
				(LPBYTE)cmd.GetString(),
				(cmd.GetLength() + 1) * sizeof(TCHAR));
			RegCloseKey(hKey);
		}
	}
	else
	{
		// Delete recursively
		DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, baseKey);
	}
}

void CContextMenuDlg::OnBnClickedRefresh()
{
	CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO_CM_LOCATION);
	if (!pCombo) return;

	int sel = pCombo->GetCurSel();
	if (sel < 0) return;

	CString filter;
	if (sel > 0)
		filter = m_locations[sel].name;

	ScanEntries(filter);
	RefreshList();
}

void CContextMenuDlg::OnCbnSelchangeLocation()
{
	// Refresh the list when the filter dropdown selection changes
	OnBnClickedRefresh();
}

void CContextMenuDlg::AdjustColumnWidths()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList || !pList->GetSafeHwnd()) return;

	// Compute the list client width (account for vertical scrollbar)
	CRect rcList;
	pList->GetClientRect(&rcList);
	int nWidth = rcList.Width();
	if (nWidth <= 0) return;

	// Distribute as 1/5, 2/5, 1/5, 1/5
	pList->SetColumnWidth(0, nWidth / 5);
	pList->SetColumnWidth(1, nWidth * 2 / 5);
	pList->SetColumnWidth(2, nWidth / 5);
	pList->SetColumnWidth(3, nWidth / 5);
}

void CContextMenuDlg::OnBnClickedDelete()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList) return;

	std::vector<int> selected;
	POSITION pos = pList->GetFirstSelectedItemPosition();
	while (pos)
		selected.push_back(pList->GetNextSelectedItem(pos));

	if (selected.empty())
	{
		MessageBox(_T("请先选择要删除的项。"), _T("提示"), MB_ICONINFORMATION);
		return;
	}

	CString msg;
	msg.Format(_T("确定要删除选中的 %d 个右键菜单项吗？\n此操作将修改注册表，不可撤销。"),
		(int)selected.size());
	if (MessageBox(msg, _T("确认删除"), MB_ICONWARNING | MB_YESNO) != IDYES)
		return;

	int deleted = 0;
	// Sort descending for safe list deletion
	std::sort(selected.rbegin(), selected.rend());
	for (int idx : selected)
	{
		const auto& entry = m_entries[idx];
		CString fullKey = entry.regPath + _T("\\") + entry.keyName;
		if (DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, fullKey))
		{
			pList->DeleteItem(idx);
			m_entries.erase(m_entries.begin() + idx);
			deleted++;
		}
	}

	CString status;
	status.Format(_T("已删除 %d 项"), deleted);
	UpdateStatus(status);

	// Refresh the list from registry to ensure consistency
	if (deleted > 0)
	{
		OnBnClickedRefresh();
		// Update self context menu checkbox in case we deleted our own entry
		LoadSelfContextMenuState();
	}
}

void CContextMenuDlg::OnBnClickedLocate()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList) return;

	POSITION pos = pList->GetFirstSelectedItemPosition();
	if (!pos)
	{
		MessageBox(_T("请先选择要定位的项。"), _T("提示"), MB_ICONINFORMATION);
		return;
	}

	int idx = pList->GetNextSelectedItem(pos);
	if (idx < 0 || idx >= (int)m_entries.size()) return;

	const auto& entry = m_entries[idx];
	CString fullRegPath = entry.regPath + _T("\\") + entry.keyName;
	OpenRegEditToPath(HKEY_CLASSES_ROOT, fullRegPath);
}

void CContextMenuDlg::OnBnClickedCheckFolder()
{
	BOOL bChecked = IsDlgButtonChecked(IDC_CHECK_CM_FOLDER);
	SaveSelfContextMenuState(bChecked == BST_CHECKED);

	CString msg = bChecked ? _T("已添加文件夹右键菜单项") : _T("已移除文件夹右键菜单项");
	UpdateStatus(msg);

	// Refresh the list to show the change
	OnBnClickedRefresh();
}

void CContextMenuDlg::OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList) return;

	POSITION pos = pList->GetFirstSelectedItemPosition();
	if (!pos) return;

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, ID_MENU_CM_DELETE, _T("删除选中"));
	menu.AppendMenu(MF_STRING, ID_MENU_CM_LOCATE, _T("定位(注册表)"));

	CPoint pt;
	GetCursorPos(&pt);
	menu.TrackPopupMenu(TPM_RIGHTBUTTON, pt.x, pt.y, this);
}

void CContextMenuDlg::OnMenuDelete()
{
	OnBnClickedDelete();
}

void CContextMenuDlg::OnMenuLocate()
{
	OnBnClickedLocate();
}
