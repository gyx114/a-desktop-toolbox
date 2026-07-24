// ContextMenuDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "ContextMenuDlg.h"
#include "afxdialogex.h"
#include <algorithm>
#include <shobjidl.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

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
	ON_BN_CLICKED(IDC_CHECK_CM_WIN11_CLASSIC, &CContextMenuDlg::OnBnClickedCheckWin11Classic)
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

// SEH-protected wrapper for IContextMenu::GetCommandString (Unicode)
static BOOL SafeGetCommandStringW(IContextMenu* pMenu, LPWSTR pszBuf, UINT cchBuf)
{
	__try
	{
		HRESULT hr = pMenu->GetCommandString(0, GCS_VERBW, nullptr,
			reinterpret_cast<LPSTR>(pszBuf), cchBuf * sizeof(WCHAR));
		return SUCCEEDED(hr) ? TRUE : FALSE;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		pszBuf[0] = 0;
		return FALSE;
	}
}

// SEH-protected wrapper for IContextMenu::GetCommandString (ANSI)
static BOOL SafeGetCommandStringA(IContextMenu* pMenu, LPSTR pszBuf, UINT cchBuf)
{
	__try
	{
		HRESULT hr = pMenu->GetCommandString(0, GCS_VERBA, nullptr, pszBuf, cchBuf);
		return SUCCEEDED(hr) ? TRUE : FALSE;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		pszBuf[0] = 0;
		return FALSE;
	}
}

// Get real display name from COM context menu handler via GetCommandString
// NOTE: QueryContextMenu is NOT called because most ShellEx handlers require
// a valid IDataObject/IShellFolder context; calling it without context causes
// crashes/hangs. We use GetCommandString which is safer.
CString CContextMenuDlg::GetShellExDisplayName(const CString& clsid, const CString& dllPath)
{
	if (clsid.IsEmpty())
		return _T("");

	CString displayName;
	CLSID clsidGuid;
	if (CLSIDFromString(clsid.GetString(), &clsidGuid) != S_OK)
		return _T("");

	// Use CoCreateInstance — COM is already initialized by AfxOleInit()
	// Use CLSCTX_INPROC_SERVER only: LOCAL_SERVER may launch Explorer/out-of-proc
	// servers which can deadlock the UI thread via window message pumps.
	IUnknown* pUnknown = nullptr;
	HRESULT hr = CoCreateInstance(clsidGuid, nullptr,
		CLSCTX_INPROC_SERVER,
		IID_IUnknown, reinterpret_cast<void**>(&pUnknown));
	if (FAILED(hr) || !pUnknown)
		return _T("");

	// Query IContextMenu interface
	IContextMenu* pContextMenu = nullptr;
	hr = pUnknown->QueryInterface(IID_IContextMenu, reinterpret_cast<void**>(&pContextMenu));

	if (SUCCEEDED(hr) && pContextMenu)
	{
		// SEH-protected call to GetCommandString (handler may crash)
		WCHAR szNameW[MAX_PATH] = { 0 };
		BOOL bOk = SafeGetCommandStringW(pContextMenu, szNameW, MAX_PATH);
		if (bOk && szNameW[0])
		{
			displayName = szNameW;
		}
		else
		{
			char szNameA[MAX_PATH] = { 0 };
			bOk = SafeGetCommandStringA(pContextMenu, szNameA, MAX_PATH);
			if (bOk && szNameA[0])
				displayName = szNameA;
		}
		pContextMenu->Release();
	}

	pUnknown->Release();
	return displayName;
}

// Resolve MUI resource reference string (e.g. @shell32.dll,-12345) to display text
CString CContextMenuDlg::ResolveMUIString(const CString& raw)
{
	if (raw.IsEmpty() || raw[0] != _T('@'))
		return raw;

	CString result;
	// Method 1: SHLoadIndirectString (handles language fallback automatically)
	TCHAR szResult[1024] = { 0 };
	HRESULT hr = SHLoadIndirectString(raw, szResult, 1024, nullptr);
	if (SUCCEEDED(hr) && szResult[0])
	{
		result = szResult;
		return result;
	}

	// Method 2: Manual LoadString fallback (@path,-id)
	int nPos = raw.ReverseFind(_T(','));
	if (nPos > 0)
	{
		CString strDll = raw.Mid(1, nPos - 1);  // skip leading @
		int nId = _ttoi(raw.Mid(nPos + 1));
		if (nId < 0) nId = -nId;

		HMODULE hMod = LoadLibraryEx(strDll, nullptr, LOAD_LIBRARY_AS_DATAFILE);
		if (hMod)
		{
			TCHAR szBuf[512] = { 0 };
			if (LoadString(hMod, nId, szBuf, 512) > 0)
				result = szBuf;
			FreeLibrary(hMod);
		}
	}

	return result.IsEmpty() ? raw : result;
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
					// Try to get real display name by loading the COM DLL
					CString realName = GetShellExDisplayName(clsid, szDll);
					if (!realName.IsEmpty())
						displayName = realName;
				}
				RegCloseKey(hInproc);
			}
		}

		m_entries.push_back({
			loc.name, szSubKey, displayName, command,
			loc.shellPath, HKEY_CLASSES_ROOT,
			true,  // bIsShellEx
			false, // bExtended
			false  // bDisabled
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
					(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
				{
					CString strRaw = szVal;
					if (strRaw[0] == _T('@'))
					{
						// Resolve MUI resource reference (@dll,-id)
						CString resolved = ResolveMUIString(strRaw);
						if (!resolved.IsEmpty() && resolved[0] != _T('@'))
						{
							displayName = resolved;
							bFound = TRUE;
						}
					}
					else
					{
						displayName = strRaw;
						bFound = TRUE;
					}
				}
				// Fall back to default value
				if (!bFound)
				{
					cbVal = sizeof(szVal);
					szVal[0] = 0;
					if (RegQueryValueEx(hVerb, nullptr, nullptr, &dwType,
						(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
					{
						CString strRaw = szVal;
						if (strRaw[0] == _T('@'))
						{
							CString resolved = ResolveMUIString(strRaw);
							if (!resolved.IsEmpty() && resolved[0] != _T('@'))
								displayName = resolved;
						}
						else
						{
							displayName = strRaw;
						}
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

				// Check visibility flags on the verb key
				BOOL bExtended = FALSE;
				BOOL bDisabled = FALSE;
				HKEY hVerbChk = nullptr;
				if (RegOpenKeyEx(HKEY_CLASSES_ROOT, subKeyPath, 0, KEY_READ, &hVerbChk) == ERROR_SUCCESS)
				{
					// Extended subkey presence => Shift+right-click only
					HKEY hExt = nullptr;
					if (RegOpenKeyEx(hVerbChk, _T("Extended"), 0, KEY_READ, &hExt) == ERROR_SUCCESS)
					{
						bExtended = TRUE;
						RegCloseKey(hExt);
					}
					// LegacyDisable value
					TCHAR szFlag[32] = { 0 };
					DWORD cbFlag = sizeof(szFlag);
					if (RegQueryValueEx(hVerbChk, _T("LegacyDisable"), nullptr, nullptr,
						(LPBYTE)szFlag, &cbFlag) == ERROR_SUCCESS)
						bDisabled = TRUE;
					// ProgrammaticAccessOnly value
					cbFlag = sizeof(szFlag);
					if (RegQueryValueEx(hVerbChk, _T("ProgrammaticAccessOnly"), nullptr, nullptr,
						(LPBYTE)szFlag, &cbFlag) == ERROR_SUCCESS)
						bDisabled = TRUE;
					RegCloseKey(hVerbChk);
				}

				m_entries.push_back({
				loc.name, szSubKey, displayName, command,
				loc.shellPath, HKEY_CLASSES_ROOT,
				false,          // bIsShellEx (static verb)
				bExtended != FALSE,  // bExtended
				bDisabled != FALSE   // bDisabled
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

		// Column 2: type
		pList->SetItemText(idx, 2, m_entries[i].bIsShellEx ? _T("ShellEx") : _T("静态"));

		// Column 3: visibility
		CString strVis;
		if (m_entries[i].bDisabled)
			strVis = _T("已禁用");
		else if (m_entries[i].bExtended)
			strVis = _T("Shift显示");
		else
			strVis = _T("正常");
		pList->SetItemText(idx, 3, strVis);

		// Column 4: key name
		pList->SetItemText(idx, 4, m_entries[i].keyName);

		// Column 5: command
		pList->SetItemText(idx, 5, m_entries[i].command);
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
		pList->InsertColumn(2, _T("类型"),     LVCFMT_LEFT, 60);
		pList->InsertColumn(3, _T("可见性"),   LVCFMT_LEFT, 70);
		pList->InsertColumn(4, _T("键名"),     LVCFMT_LEFT, 70);
		pList->InsertColumn(5, _T("命令"),     LVCFMT_LEFT, 130);
	}

	InitLocations();
	ScanEntries(_T(""));
	RefreshList();

	// Apply proportional column widths based on actual list width
	AdjustColumnWidths();

	// Load self context menu state
	LoadSelfContextMenuState();

	// Load Win11 classic menu state
	LoadWin11ClassicState();

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

void CContextMenuDlg::LoadWin11ClassicState()
{
	// Win11 new context menu is blocked by creating an empty InprocServer32
	// under HKCU\Software\Classes\CLSID\{86ca1aa0-34aa-4e8a-a939-50982eeb33a6}
	// When that key exists, Win11 falls back to the classic menu (Shift+right-click behavior).
	const CString strKey = _T("Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8a-a939-50982eeb33a6}\\InprocServer32");

	HKEY hKey = nullptr;
	BOOL bEnabled = FALSE;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, strKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		// Key exists => classic menu enabled (new menu blocked)
		bEnabled = TRUE;
		RegCloseKey(hKey);
	}
	CheckDlgButton(IDC_CHECK_CM_WIN11_CLASSIC, bEnabled ? BST_CHECKED : BST_UNCHECKED);
}

void CContextMenuDlg::SaveWin11ClassicState(bool bEnable)
{
	// The CLSID {86ca1aa0-34aa-4e8a-a939-50982eeb33a6} is the Win11 context menu
	// filibuster handler. Creating an empty InprocServer32 under HKCU overrides
	// it, forcing Explorer to show the classic menu (same as Shift+right-click).
	const CString strBase = _T("Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8a-a939-50982eeb33a6}");
	const CString strInproc = strBase + _T("\\InprocServer32");

	if (bEnable)
	{
		// Create the InprocServer32 subkey with an empty default value
		HKEY hKey = nullptr;
		DWORD dwDisp = 0;
		if (RegCreateKeyEx(HKEY_CURRENT_USER, strInproc, 0, nullptr, 0,
			KEY_WRITE, nullptr, &hKey, &dwDisp) == ERROR_SUCCESS)
		{
			// Set empty string as default value
			const TCHAR* szEmpty = _T("");
			RegSetValueEx(hKey, nullptr, 0, REG_SZ,
				(LPBYTE)szEmpty, sizeof(TCHAR));
			RegCloseKey(hKey);
		}
	}
	else
	{
		// Delete the entire CLSID key recursively under HKCU
		DeleteRegistryKeyRecursive(HKEY_CURRENT_USER, strBase);
	}

	// Notify the shell that file associations changed
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void CContextMenuDlg::OnBnClickedCheckWin11Classic()
{
	BOOL bCheck = IsDlgButtonChecked(IDC_CHECK_CM_WIN11_CLASSIC);
	SaveWin11ClassicState(bCheck == BST_CHECKED);

	CString msg;
	if (bCheck)
		msg = _T("已启用Win11经典菜单。需要重启Explorer或注销后生效。");
	else
		msg = _T("已恢复Win11新菜单。需要重启Explorer或注销后生效。");
	UpdateStatus(msg);
	AfxMessageBox(msg, MB_ICONINFORMATION | MB_OK);
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

	CRect rcList;
	pList->GetClientRect(&rcList);
	int nWidth = rcList.Width();
	if (nWidth <= 0) return;

	// Set a minimum total width so columns are wider than the list,
	// which triggers the horizontal scrollbar for long content.
	// 6 columns: position, name, type, visibility, key, command
	// Proportions: 8%, 20%, 8%, 10%, 12%, 42%
	int nMinTotal = (nWidth + 1 > 1200) ? (nWidth + 1) : 1200;
	pList->SetColumnWidth(0, nMinTotal * 8 / 100);
	pList->SetColumnWidth(1, nMinTotal * 20 / 100);
	pList->SetColumnWidth(2, nMinTotal * 8 / 100);
	pList->SetColumnWidth(3, nMinTotal * 10 / 100);
	pList->SetColumnWidth(4, nMinTotal * 12 / 100);
	pList->SetColumnWidth(5, nMinTotal * 42 / 100);
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
