// ContextMenuDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "ContextMenuDlg.h"
#include "afxdialogex.h"
#include <algorithm>
#include <set>
#include <shobjidl.h>
#include <shlwapi.h>
#include <shlobj.h>
#pragma comment(lib, "shlwapi.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Menu command IDs for right-click context menu in the list
#define ID_MENU_CM_DELETE   50020
#define ID_MENU_CM_LOCATE   50021
#define ID_MENU_CM_TOGGLE   50022

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
	ON_COMMAND(ID_MENU_CM_TOGGLE, &CContextMenuDlg::OnMenuToggle)
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
	m_locations.push_back({ _T("驱动器 Shellex (COM)"),                 _T("Drive\\shellex\\ContextMenuHandlers"), true });
	m_locations.push_back({ _T("桌面 Shellex (COM)"),                   _T("DesktopBackground\\shellex\\ContextMenuHandlers"), true });
	// Additional static verb locations
	m_locations.push_back({ _T("快捷方式 (lnkfile)"),                   _T("lnkfile\\shell"), false });
	m_locations.push_back({ _T("URL快捷方式"),                          _T("InternetShortcut\\shell"), false });
	m_locations.push_back({ _T("库文件夹"),                             _T("LibraryFolder\\shell"), false });

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

CString CContextMenuDlg::ResolveProgID(const CString& ext)
{
	// Read the ProgID from an extension key (e.g. ".png" → "pngfile")
	// Windows shell verbs are registered under the ProgID, not the extension itself.
	CString progID;
	HKEY hExt = nullptr;
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, ext, 0, KEY_READ, &hExt) == ERROR_SUCCESS)
	{
		TCHAR szVal[MAX_PATH] = { 0 };
		DWORD cbVal = sizeof(szVal);
		if (RegQueryValueEx(hExt, nullptr, nullptr, nullptr,
			(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
		{
			progID = szVal;
		}
		RegCloseKey(hExt);
	}
	return progID;
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
			false, // bDisabled
			!IsKeyDisabledByPrefix(szSubKey)  // bEnabled
		});

		dwIndex++;
		cbSubKey = MAX_PATH;
	}
	RegCloseKey(hShell);
}

void CContextMenuDlg::ScanAllExtensions(const CString& filterExt)
{
	// Enumerate all file extensions in HKCR and scan their shell verbs.
	// Resolves ProgID chains (e.g. ".png" → "pngfile" → "pngfile\shell")
	// so that per-extension context menus are correctly detected.
	HKEY hCR = nullptr;
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, nullptr, 0,
		KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hCR) != ERROR_SUCCESS)
		return;

	// Dedup set: track (regPath|keyName) to avoid duplicates
	std::set<CString> seen;
	for (const auto& e : m_entries)
		seen.insert(e.regPath + _T("|") + e.keyName);

	// Helper lambda: scan verbs from a single shell path and add to m_entries
	auto scanShellVerbs = [&](const CString& shellPath, const CString& locName) {
		HKEY hShell = nullptr;
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, shellPath, 0, KEY_READ, &hShell) != ERROR_SUCCESS)
			return;

		DWORD dwVerb = 0;
		TCHAR szVerb[MAX_PATH];
		DWORD cbVerb = MAX_PATH;
		while (RegEnumKeyEx(hShell, dwVerb, szVerb, &cbVerb,
			nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
		{
			CString dedupKey = shellPath + _T("|") + szVerb;
			if (seen.find(dedupKey) != seen.end())
			{
				dwVerb++;
				cbVerb = MAX_PATH;
				continue;
			}
			seen.insert(dedupKey);

			CString verbPath = shellPath + _T("\\") + szVerb;

			// Read display name
			CString displayName = szVerb;
			HKEY hVerb = nullptr;
			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, verbPath, 0, KEY_READ, &hVerb) == ERROR_SUCCESS)
			{
				TCHAR szVal[MAX_PATH] = { 0 };
				DWORD cbVal = sizeof(szVal);
				DWORD dwType = 0;
				BOOL bFound = FALSE;
				if (RegQueryValueEx(hVerb, _T("MUIVerb"), nullptr, &dwType,
					(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
				{
					CString strRaw = szVal;
					if (strRaw[0] == _T('@'))
					{
						CString resolved = ResolveMUIString(strRaw);
						if (!resolved.IsEmpty() && resolved[0] != _T('@'))
						{ displayName = resolved; bFound = TRUE; }
					}
					else
					{ displayName = strRaw; bFound = TRUE; }
				}
				if (!bFound)
				{
					cbVal = sizeof(szVal); szVal[0] = 0;
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
							displayName = strRaw;
					}
				}
				RegCloseKey(hVerb);
			}

			// Read command
			CString command;
			CString cmdPath = verbPath + _T("\\command");
			HKEY hCmd = nullptr;
			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, cmdPath, 0, KEY_READ, &hCmd) == ERROR_SUCCESS)
			{
				TCHAR szCmd[MAX_PATH * 2] = { 0 };
				DWORD cbCmd = sizeof(szCmd);
				DWORD dwType = 0;
				if (RegQueryValueEx(hCmd, nullptr, nullptr, &dwType,
					(LPBYTE)szCmd, &cbCmd) == ERROR_SUCCESS)
					command = szCmd;
				RegCloseKey(hCmd);
			}

			// Check visibility flags
			BOOL bDisabled = FALSE;
			HKEY hVerbChk = nullptr;
			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, verbPath, 0, KEY_READ, &hVerbChk) == ERROR_SUCCESS)
			{
				TCHAR szFlag[32] = { 0 };
				DWORD cbFlag = sizeof(szFlag);
				if (RegQueryValueEx(hVerbChk, _T("LegacyDisable"), nullptr, nullptr,
					(LPBYTE)szFlag, &cbFlag) == ERROR_SUCCESS)
					bDisabled = TRUE;
				cbFlag = sizeof(szFlag);
				if (RegQueryValueEx(hVerbChk, _T("ProgrammaticAccessOnly"), nullptr, nullptr,
					(LPBYTE)szFlag, &cbFlag) == ERROR_SUCCESS)
					bDisabled = TRUE;
				RegCloseKey(hVerbChk);
			}

			m_entries.push_back({
				locName, szVerb, displayName, command,
				shellPath, HKEY_CLASSES_ROOT,
				false, false, bDisabled != FALSE,
				!bDisabled
			});

			dwVerb++;
			cbVerb = MAX_PATH;
		}
		RegCloseKey(hShell);
	};

	// Helper lambda: scan ShelEx handlers from a path
	auto scanShellexHandlers = [&](const CString& shellexPath, const CString& locName) {
		HKEY hShell = nullptr;
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, shellexPath, 0, KEY_READ, &hShell) != ERROR_SUCCESS)
			return;

		DWORD dwIndex2 = 0;
		TCHAR szHandler[MAX_PATH];
		DWORD cbHandler = MAX_PATH;
		while (RegEnumKeyEx(hShell, dwIndex2, szHandler, &cbHandler,
			nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
		{
			CString dedupKey = shellexPath + _T("|") + szHandler;
			if (seen.find(dedupKey) != seen.end())
			{
				dwIndex2++;
				cbHandler = MAX_PATH;
				continue;
			}
			seen.insert(dedupKey);

			CString displayName = szHandler;
			CString clsid;
			HKEY hVerb = nullptr;
			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, shellexPath + _T("\\") + szHandler,
				0, KEY_READ, &hVerb) == ERROR_SUCCESS)
			{
				TCHAR szVal[MAX_PATH] = { 0 };
				DWORD cbVal = sizeof(szVal);
				if (RegQueryValueEx(hVerb, nullptr, nullptr, nullptr,
					(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
					clsid = szVal;
				RegCloseKey(hVerb);
			}
			if (!clsid.IsEmpty())
			{
				CString friendly = ResolveClsidName(clsid);
				if (!friendly.IsEmpty())
					displayName = friendly;
			}

			bool bEnabled = !IsKeyDisabledByPrefix(szHandler);

			m_entries.push_back({
				locName, szHandler, displayName, _T(""),
				shellexPath, HKEY_CLASSES_ROOT,
				true, false, false, bEnabled
			});

			dwIndex2++;
			cbHandler = MAX_PATH;
		}
		RegCloseKey(hShell);
	};

	DWORD dwIndex = 0;
	TCHAR szKey[MAX_PATH];
	DWORD cbKey = MAX_PATH;

	while (RegEnumKeyEx(hCR, dwIndex, szKey, &cbKey,
		nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
	{
		// Only process file extension keys (start with '.')
		if (szKey[0] == _T('.') && szKey[1] != _T('\0'))
		{
			// If a specific extension filter is set, skip non-matching extensions
			if (!filterExt.IsEmpty() && _tcsicmp(szKey, filterExt) != 0)
			{
				dwIndex++;
				cbKey = MAX_PATH;
				continue;
			}

			CString ext = szKey;
			CString locName = CString(_T("扩展名 ")) + ext;

			// 1. Resolve ProgID and scan ProgID\shell + ProgID\shellex
			CString progID = ResolveProgID(ext);
			if (!progID.IsEmpty())
			{
				scanShellVerbs(progID + _T("\\shell"), locName);
				scanShellexHandlers(progID + _T("\\shellex\\ContextMenuHandlers"), locName);
			}

			// 2. Scan .ext\shell directly (some extensions have shell verbs directly)
			scanShellVerbs(ext + _T("\\shell"), locName);

			// 3. Scan SystemFileAssociations\.ext\shell (Windows 8+)
			scanShellVerbs(_T("SystemFileAssociations\\") + ext + _T("\\shell"), locName);
		}
		dwIndex++;
		cbKey = MAX_PATH;
	}
	RegCloseKey(hCR);
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
				bDisabled != FALSE,   // bDisabled
				!bDisabled            // bEnabled
			});

				dwIndex++;
				cbSubKey = MAX_PATH;
			}
			RegCloseKey(hShell);
		}
	}

	// Scan all file extensions for their shell verbs
	if (filter.IsEmpty())
	{
		ScanAllExtensions();
	}
	else
	{
		// If filter is a per-extension location, scan that extension with ProgID resolution
		for (const auto& loc : m_locations)
		{
			if (loc.name == filter && !loc.isShellEx && loc.shellPath.Find(_T('.')) == 0)
			{
				int slashPos = loc.shellPath.Find(_T('\\'));
				CString ext = (slashPos > 0) ? loc.shellPath.Left(slashPos) : loc.shellPath;
				ScanAllExtensions(ext);
				break;
			}
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
		if (!m_entries[i].bEnabled)
			strVis = _T("已禁用");
		else if (m_entries[i].bDisabled)
			strVis = _T("系统禁用");
		else if (m_entries[i].bExtended)
			strVis = _T("Shift显示");
		else
			strVis = _T("正常");
		pList->SetItemText(idx, 3, strVis);

		// Column 4: key name
		pList->SetItemText(idx, 4, m_entries[i].keyName);

		// Column 5: command
		pList->SetItemText(idx, 5, m_entries[i].command);

		// Gray out disabled items
		if (!m_entries[i].bEnabled)
			pList->SetItemState(idx, LVIS_CUT, LVIS_CUT);
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

	// Use precise scan on initial load
	ScanWithShellAPI();
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
	// under HKCU\Software\Classes\CLSID\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}
	// When that key exists, Win11 falls back to the classic menu (Shift+right-click behavior).
	const CString strKey = _T("Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\\InprocServer32");

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
	// The CLSID {86ca1aa0-34aa-4e8b-a509-50c905bae2a2} is the Win11 context menu
	// filibuster handler. Creating an empty InprocServer32 under HKCU overrides
	// it, forcing Explorer to show the classic menu (same as Shift+right-click).
	const CString strBase = _T("Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}");
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

	// Persist the configuration so it survives across app restarts
	AfxGetApp()->WriteProfileInt(_T("ContextMenu"), _T("Win11Classic"), bEnable ? 1 : 0);

	// Notify the shell that file associations changed
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

	// Restart Explorer to apply the change immediately
	RestartExplorer();
}

void CContextMenuDlg::OnBnClickedCheckWin11Classic()
{
	BOOL bCheck = IsDlgButtonChecked(IDC_CHECK_CM_WIN11_CLASSIC);
	SaveWin11ClassicState(bCheck == BST_CHECKED);

	CString msg;
	if (bCheck)
		msg = _T("已启用Win11经典菜单，正在重启资源管理器...");
	else
		msg = _T("已恢复Win11新菜单，正在重启资源管理器...");
	UpdateStatus(msg);
}

void CContextMenuDlg::OnBnClickedRefresh()
{
	ScanWithShellAPI();
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

		// Check if this key is in HKLM (requires admin rights)
		CString hklmPath = _T("Software\\Classes\\") + fullKey;
		HKEY hTest = nullptr;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, hklmPath, 0, KEY_READ, &hTest) == ERROR_SUCCESS)
		{
			RegCloseKey(hTest);
			if (!IsRunningAsAdmin())
			{
				CString errMsg;
				errMsg.Format(_T("跳过：%s\n\n此项位于 HKLM，需要管理员权限才能删除。"),
					entry.displayName);
				MessageBox(errMsg, _T("权限不足"), MB_ICONWARNING);
				continue;
			}
		}

		if (DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, fullKey))
		{
			pList->DeleteItem(idx);
			m_entries.erase(m_entries.begin() + idx);
			deleted++;
		}
		else
		{
			CString errMsg;
			errMsg.Format(_T("无法删除：%s\n\n可能原因：系统保护、权限不足、或该项正在被使用。"),
				entry.displayName);
			MessageBox(errMsg, _T("删除失败"), MB_ICONERROR);
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

	int idx = pList->GetNextSelectedItem(pos);
	if (idx < 0 || idx >= (int)m_entries.size()) return;

	CMenu menu;
	menu.CreatePopupMenu();

	// Toggle enable/disable
	if (m_entries[idx].bEnabled)
		menu.AppendMenu(MF_STRING, ID_MENU_CM_TOGGLE, _T("禁用"));
	else
		menu.AppendMenu(MF_STRING, ID_MENU_CM_TOGGLE, _T("启用"));

	menu.AppendMenu(MF_SEPARATOR);
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

void CContextMenuDlg::OnMenuToggle()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList) return;

	POSITION pos = pList->GetFirstSelectedItemPosition();
	if (!pos) return;

	int idx = pList->GetNextSelectedItem(pos);
	if (idx < 0 || idx >= (int)m_entries.size()) return;

	ToggleEntry(idx);
}

void CContextMenuDlg::RestartExplorer()
{
	// Gracefully terminate explorer.exe; it will auto-restart
	// This is necessary for registry-based context menu changes to take effect
	HWND hwndShell = ::FindWindow(_T("Progman"), nullptr);
	if (hwndShell)
	{
		::PostMessage(hwndShell, WM_QUIT, 0, 0);
		Sleep(500);
	}
	// Also terminate explorer.exe directly
	system("taskkill /f /im explorer.exe >nul 2>&1");
	Sleep(1000);
	system("start explorer.exe");
}

bool CContextMenuDlg::IsKeyDisabledByPrefix(const CString& keyName)
{
	// ShellEx handlers are disabled by prefixing with '_' (e.g. _{CLSID})
	return keyName.GetLength() > 0 && keyName[0] == _T('_');
}

void CContextMenuDlg::ToggleEntry(int index)
{
	if (index < 0 || index >= (int)m_entries.size()) return;

	auto& entry = m_entries[index];
	CString parentPath = entry.regPath;
	HKEY hRoot = entry.hRoot;

	// Check if this registry key is under HKLM (requires admin rights)
	if (hRoot == HKEY_CLASSES_ROOT)
	{
		// HKCR is a merged view. Check if the key actually lives in HKLM.
		CString hklmPath = _T("Software\\Classes\\") + parentPath;
		HKEY hTest = nullptr;
		LONG lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, hklmPath, 0, KEY_READ, &hTest);
		if (lResult == ERROR_SUCCESS)
		{
			RegCloseKey(hTest);
			// Key exists in HKLM - need admin rights to modify
			if (!IsRunningAsAdmin())
			{
				MessageBox(
					_T("此项位于 HKLM（本地计算机），需要管理员权限才能修改。\n\n")
					_T("请以管理员身份重新运行本程序。"),
					_T("权限不足"), MB_ICONWARNING);
				return;
			}
		}
	}

	if (entry.bIsShellEx)
	{
		// ShellEx: disable by renaming key (add/remove '_' prefix)
		// e.g. {CLSID} <-> _{CLSID}
		CString oldName = entry.keyName;
		CString newName;
		if (entry.bEnabled)
		{
			// Disable: prefix with '_'
			if (!IsKeyDisabledByPrefix(oldName))
				newName = _T("_") + oldName;
		}
		else
		{
			// Enable: remove '_' prefix
			if (IsKeyDisabledByPrefix(oldName))
				newName = oldName.Mid(1);
		}

		if (!newName.IsEmpty() && newName != oldName)
		{
			HKEY hParent = nullptr;
			LONG lResult = RegOpenKeyEx(hRoot, parentPath, 0, KEY_WRITE | KEY_READ, &hParent);
			if (lResult != ERROR_SUCCESS)
			{
				CString errMsg;
				errMsg.Format(_T("无法打开注册表项：%s\n错误代码：%d"), parentPath, lResult);
				MessageBox(errMsg, _T("操作失败"), MB_ICONERROR);
				return;
			}

			// Delete the target key if it already exists (shouldn't happen normally)
			SHDeleteKey(hParent, newName);
			// Rename the key
			lResult = RegRenameKey(hParent, oldName, newName);
			if (lResult == ERROR_SUCCESS)
			{
				entry.keyName = newName;
				entry.bEnabled = !entry.bEnabled;
			}
			else
			{
				CString errMsg;
				errMsg.Format(_T("无法重命名注册表项：%s\n错误代码：%d\n\n")
					_T("可能原因：系统保护、权限不足、或该项正在被使用。"),
					oldName, lResult);
				MessageBox(errMsg, _T("操作失败"), MB_ICONERROR);
				RegCloseKey(hParent);
				return;
			}
			RegCloseKey(hParent);
		}
	}
	else
	{
		// Static verb: disable by adding/removing LegacyDisable value
		CString verbPath = parentPath + _T("\\") + entry.keyName;
		HKEY hVerb = nullptr;
		LONG lResult = RegOpenKeyEx(hRoot, verbPath, 0, KEY_WRITE | KEY_READ, &hVerb);
		if (lResult != ERROR_SUCCESS)
		{
			CString errMsg;
			if (lResult == ERROR_ACCESS_DENIED)
				errMsg.Format(_T("无法修改：%s\n\n访问被拒绝。此项可能位于 HKLM，需要管理员权限。"),
					entry.displayName);
			else
				errMsg.Format(_T("无法打开注册表项：%s\n错误代码：%d"), verbPath, lResult);
			MessageBox(errMsg, _T("操作失败"), MB_ICONERROR);
			return;
		}

		if (entry.bEnabled)
		{
			// Disable: add LegacyDisable
			const TCHAR* szEmpty = _T("");
			lResult = RegSetValueEx(hVerb, _T("LegacyDisable"), 0, REG_SZ,
				(LPBYTE)szEmpty, sizeof(TCHAR));
			if (lResult == ERROR_SUCCESS)
			{
				entry.bEnabled = false;
				entry.bDisabled = true;
			}
			else
			{
				CString errMsg;
				errMsg.Format(_T("无法设置 LegacyDisable：%s\n错误代码：%d"),
					entry.displayName, lResult);
				MessageBox(errMsg, _T("操作失败"), MB_ICONERROR);
				RegCloseKey(hVerb);
				return;
			}
		}
		else
		{
			// Enable: remove LegacyDisable
			lResult = RegDeleteValue(hVerb, _T("LegacyDisable"));
			if (lResult == ERROR_SUCCESS || lResult == ERROR_FILE_NOT_FOUND)
			{
				entry.bEnabled = true;
				entry.bDisabled = false;
			}
			else
			{
				CString errMsg;
				errMsg.Format(_T("无法删除 LegacyDisable：%s\n错误代码：%d"),
					entry.displayName, lResult);
				MessageBox(errMsg, _T("操作失败"), MB_ICONERROR);
				RegCloseKey(hVerb);
				return;
			}
		}
		RegCloseKey(hVerb);
	}

	// Notify shell of changes
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

	// Refresh the list to show updated state
	RefreshList();

	CString msg;
	msg.Format(_T("已%s: %s"), entry.bEnabled ? _T("启用") : _T("禁用"), entry.displayName);
	UpdateStatus(msg);
}

bool CContextMenuDlg::IsRunningAsAdmin()
{
	BOOL bIsAdmin = FALSE;
	PSID pAdminGroup = nullptr;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	if (AllocateAndInitializeSid(&NtAuthority, 2,
		SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0, &pAdminGroup))
	{
		CheckTokenMembership(nullptr, pAdminGroup, &bIsAdmin);
		FreeSid(pAdminGroup);
	}
	return bIsAdmin != FALSE;
}

// SEH-protected helper: try to get display name from a ShellEx handler via COM
// Must be a standalone function (no objects with destructors in scope) for __try
static void SafeGetComDisplayName(const CString& clsid, IShellFolder* pFolder, IDataObject* pDataObj, LPCITEMIDLIST pidlFolder, LPCITEMIDLIST pidlChild, CString& outDisplayName)
{
	outDisplayName.Empty();
	CLSID clsidGuid;
	if (CLSIDFromString(clsid.GetString(), &clsidGuid) != S_OK)
		return;

	__try
	{
		IShellExtInit* pInit = nullptr;
		HRESULT hr = CoCreateInstance(clsidGuid, nullptr,
			CLSCTX_INPROC_SERVER, IID_IShellExtInit,
			(void**)&pInit);
		if (SUCCEEDED(hr) && pInit)
		{
			hr = pInit->Initialize(
				pidlChild ? pidlFolder : nullptr,
				pDataObj, nullptr);
			if (SUCCEEDED(hr))
			{
				IContextMenu* pMenu = nullptr;
				hr = pInit->QueryInterface(IID_IContextMenu,
					(void**)&pMenu);
				if (SUCCEEDED(hr) && pMenu)
				{
					HMENU hMenu = CreatePopupMenu();
					hr = pMenu->QueryContextMenu(hMenu, 0,
						1, 500, CMF_NORMAL);
					if (SUCCEEDED(hr) && hr > 0)
					{
						TCHAR szText[256] = { 0 };
						MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
						mii.fMask = MIIM_STRING;
						mii.dwTypeData = szText;
						mii.cch = 256;
						if (GetMenuItemInfo(hMenu, 0, TRUE, &mii) && szText[0])
						{
							outDisplayName = szText;
							outDisplayName.Replace(_T("&"), _T(""));
						}
					}
					DestroyMenu(hMenu);
					pMenu->Release();
				}
			}
			pInit->Release();
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		// Handler crashed - return empty string
	}
}

void CContextMenuDlg::ScanShellExWithCom(const LocationFilter& loc)
{
	// Precise scanning using IShellFolder + IDataObject + QueryContextMenu
	// This creates a real file/folder context and calls each ShellEx handler's
	// IContextMenu interface to get the actual menu items it would add.

	// Determine the context type from the location
	bool bIsFolder = (loc.name.Find(_T("文件夹")) >= 0 || loc.name.Find(_T("Directory")) >= 0);
	bool bIsBackground = (loc.name.Find(_T("背景")) >= 0 || loc.name.Find(_T("Background")) >= 0);
	bool bIsDrive = (loc.name.Find(_T("驱动器")) >= 0 || loc.name.Find(_T("Drive")) >= 0);

	// Get the desktop IShellFolder
	IShellFolder* pDesktopFolder = nullptr;
	if (FAILED(SHGetDesktopFolder(&pDesktopFolder)) || !pDesktopFolder)
		return;

	// Create a temp file or use a known folder for context
	CString strTempPath;
	TCHAR szTempDir[MAX_PATH];
	GetTempPath(MAX_PATH, szTempDir);

	LPITEMIDLIST pidlFolder = nullptr;
	LPCITEMIDLIST pidlChild = nullptr;
	IShellFolder* pParentFolder = nullptr;
	IDataObject* pDataObj = nullptr;

	if (bIsBackground)
	{
		// For background context, use the desktop or a folder itself
		if (bIsDrive)
		{
			// Use C:\ as the folder
			SHParseDisplayName(_T("C:\\"), nullptr, &pidlFolder, 0, nullptr);
		}
		else
		{
			// Use temp directory as the folder
			SHParseDisplayName(szTempDir, nullptr, &pidlFolder, 0, nullptr);
		}
		// For background, pidlFolder is the folder, no child pidl
		SHBindToParent(pidlFolder, IID_IShellFolder, (void**)&pParentFolder, &pidlChild);
		if (pParentFolder && pidlChild)
		{
			pParentFolder->GetUIObjectOf(nullptr, 1, (LPCITEMIDLIST*)&pidlChild,
				IID_IDataObject, nullptr, (void**)&pDataObj);
		}
	}
	else
	{
		// Create a temp file for file context
		CString strTempFile;
		if (bIsFolder)
		{
			// Create a temp folder
			strTempFile = CString(szTempDir) + _T("MFC_ContextMenu_Test\\");
			CreateDirectory(strTempFile, nullptr);
			SHParseDisplayName(strTempFile, nullptr, &pidlFolder, 0, nullptr);
		}
		else
		{
			// Create a temp .txt file
			strTempFile = CString(szTempDir) + _T("MFC_ContextMenu_Test.txt");
			HANDLE hFile = CreateFile(strTempFile, GENERIC_WRITE, 0, nullptr,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				CloseHandle(hFile);
				SHParseDisplayName(strTempFile, nullptr, &pidlFolder, 0, nullptr);
			}
		}

		// Get parent IShellFolder and child PIDL
		if (pidlFolder)
		{
			SHBindToParent(pidlFolder, IID_IShellFolder, (void**)&pParentFolder, &pidlChild);
			if (pParentFolder && pidlChild)
			{
				pParentFolder->GetUIObjectOf(nullptr, 1, &pidlChild,
					IID_IDataObject, nullptr, (void**)&pDataObj);
			}
		}

		// Clean up temp file
		if (!bIsFolder && !strTempFile.IsEmpty())
			DeleteFile(strTempFile);
		if (bIsFolder && !strTempFile.IsEmpty())
			RemoveDirectory(strTempFile);
	}

	// Now scan each ShellEx handler with the real context
	HKEY hShell = nullptr;
	CString hkcrPath = loc.shellPath;
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, hkcrPath, 0, KEY_READ, &hShell) != ERROR_SUCCESS)
	{
		if (pDataObj) pDataObj->Release();
		if (pParentFolder) pParentFolder->Release();
		if (pidlFolder) CoTaskMemFree(pidlFolder);
		pDesktopFolder->Release();
		return;
	}

	DWORD dwIndex = 0;
	TCHAR szSubKey[MAX_PATH];
	DWORD cbSubKey = MAX_PATH;

	while (RegEnumKeyEx(hShell, dwIndex, szSubKey, &cbSubKey,
		nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
	{
		CString subKeyPath = hkcrPath + _T("\\") + szSubKey;
		CString keyName = szSubKey;
		bool bEnabled = !IsKeyDisabledByPrefix(keyName);

		// Read CLSID from default value
		CString clsid;
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

		CString displayName = keyName;
		if (!clsid.IsEmpty())
		{
			CString friendly = ResolveClsidName(clsid);
			if (!friendly.IsEmpty())
				displayName = friendly;

			// Try COM-based scanning if we have valid context
			if (pDataObj && pParentFolder)
			{
				CString comName;
				SafeGetComDisplayName(clsid, pParentFolder, pDataObj, pidlFolder, pidlChild, comName);
				if (!comName.IsEmpty())
					displayName = comName;
			}
		}

		// Check if this entry already exists from static scan
		bool bDuplicate = false;
		for (auto& e : m_entries)
		{
			if (e.regPath == loc.shellPath && e.keyName == keyName)
			{
				// Update display name if COM scan found a better one
				if (displayName != keyName && e.displayName == keyName)
					e.displayName = displayName;
				bDuplicate = true;
				break;
			}
		}

		if (!bDuplicate)
		{
			m_entries.push_back({
				loc.name, keyName, displayName, _T(""),
				loc.shellPath, HKEY_CLASSES_ROOT,
				true,    // bIsShellEx
				false,   // bExtended
				false,   // bDisabled
				bEnabled // bEnabled
			});
		}

		dwIndex++;
		cbSubKey = MAX_PATH;
	}

	RegCloseKey(hShell);

	if (pDataObj) pDataObj->Release();
	if (pParentFolder) pParentFolder->Release();
	if (pidlFolder) CoTaskMemFree(pidlFolder);
	pDesktopFolder->Release();
}

// SEH-safe helper: get verb string from IContextMenu (no CString in __try)
static bool SafeGetVerb(IContextMenu* pCM, UINT cmdId, CString& outVerb)
{
	WCHAR szVerbW[256] = { 0 };
	BOOL bOk = FALSE;
	__try
	{
		HRESULT hr = pCM->GetCommandString(cmdId, GCS_VERBW, nullptr,
			reinterpret_cast<LPSTR>(szVerbW), sizeof(szVerbW));
		if (SUCCEEDED(hr) && szVerbW[0])
		{
			outVerb = szVerbW;
			bOk = TRUE;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}

	if (bOk) return true;

	char szVerbA[256] = { 0 };
	__try
	{
		HRESULT hr = pCM->GetCommandString(cmdId, GCS_VERBA, nullptr, szVerbA, sizeof(szVerbA));
		if (SUCCEEDED(hr) && szVerbA[0])
		{
			outVerb = szVerbA;
			bOk = TRUE;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}

	return bOk;
}

// Helper: enumerate menu items from an IContextMenu into m_entries
static void EnumerateShellMenu(HMENU hMenu, IContextMenu* pCM, UINT idCmdFirst,
	const CString& ctxName, const CString& regBasePath,
	std::vector<CContextMenuDlg::MenuEntry>& entries)
{
	int nCount = GetMenuItemCount(hMenu);
	for (int i = 0; i < nCount; i++)
	{
		MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
		mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_ID | MIIM_SUBMENU;
		mii.dwTypeData = nullptr;
		mii.cch = 0;

		GetMenuItemInfo(hMenu, i, TRUE, &mii);
		mii.cch++;

		TCHAR* szText = new TCHAR[mii.cch + 1];
		mii.dwTypeData = szText;
		GetMenuItemInfo(hMenu, i, TRUE, &mii);
		szText[mii.cch] = 0;

		CString displayName = szText;
		delete[] szText;

		// Skip separators
		if (mii.fType & MFT_SEPARATOR)
			continue;

		displayName.Replace(_T("&"), _T(""));

		// Try to get the verb string via SEH-safe helper
		CString verb;
		UINT cmdId = mii.wID - idCmdFirst;
		if (cmdId < 30000)
		{
			SafeGetVerb(pCM, cmdId, verb);
		}

		// Determine if ShellEx or static
		bool bIsShellEx = (!verb.IsEmpty() && verb[0] == _T('{'));
		bool bIsSubmenu = (mii.hSubMenu != nullptr);
		bool bFoundRegKey = false;
		CString regKeyName = verb;
		CString regPath = regBasePath;

		if (!verb.IsEmpty())
		{
			if (bIsShellEx)
			{
				// Search ALL known ShellEx locations for the CLSID
				static const TCHAR* s_shellexLocs[] = {
					_T("*\\shellex\\ContextMenuHandlers"),
					_T("Directory\\shellex\\ContextMenuHandlers"),
					_T("Directory\\Background\\shellex\\ContextMenuHandlers"),
					_T("Folder\\shellex\\ContextMenuHandlers"),
					_T("AllFilesystemObjects\\shellex\\ContextMenuHandlers"),
					_T("Drive\\shellex\\ContextMenuHandlers"),
					_T("DesktopBackground\\shellex\\ContextMenuHandlers"),
				};
				for (const auto* locPath : s_shellexLocs)
				{
					CString fullPath = CString(locPath) + _T("\\") + verb;
					HKEY hTest = nullptr;
					if (RegOpenKeyEx(HKEY_CLASSES_ROOT, fullPath, 0, KEY_READ, &hTest) == ERROR_SUCCESS)
					{
						RegCloseKey(hTest);
						regPath = locPath;
						regKeyName = verb;
						bFoundRegKey = true;
						break;
					}
				}
			}
			else
			{
				// Static verb: check if key exists
				CString fullPath = regBasePath + _T("\\") + verb;
				HKEY hTest = nullptr;
				if (RegOpenKeyEx(HKEY_CLASSES_ROOT, fullPath, 0, KEY_READ, &hTest) == ERROR_SUCCESS)
				{
					RegCloseKey(hTest);
					regKeyName = verb;
					bFoundRegKey = true;
				}
			}
		}

		// Check if disabled
		bool bEnabled = true;
		bool bDisabled = false;
		if (bFoundRegKey && !bIsShellEx)
		{
			CString verbPath = regPath + _T("\\") + regKeyName;
			HKEY hVerb = nullptr;
			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, verbPath, 0, KEY_READ, &hVerb) == ERROR_SUCCESS)
			{
				TCHAR szFlag[32] = { 0 };
				DWORD cbFlag = sizeof(szFlag);
				if (RegQueryValueEx(hVerb, _T("LegacyDisable"), nullptr, nullptr,
					(LPBYTE)szFlag, &cbFlag) == ERROR_SUCCESS)
				{
					bDisabled = true;
					bEnabled = false;
				}
				RegCloseKey(hVerb);
			}
		}
		if (bIsShellEx && regKeyName.GetLength() > 0 && regKeyName[0] == _T('_'))
			bEnabled = false;

		// Get command
		CString command;
		if (bFoundRegKey)
		{
			CString cmdPath = regPath + _T("\\") + regKeyName + _T("\\command");
			HKEY hCmd = nullptr;
			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, cmdPath, 0, KEY_READ, &hCmd) == ERROR_SUCCESS)
			{
				TCHAR szCmd[MAX_PATH * 2] = { 0 };
				DWORD cbCmd = sizeof(szCmd);
				DWORD dwType = 0;
				if (RegQueryValueEx(hCmd, nullptr, nullptr, &dwType,
					(LPBYTE)szCmd, &cbCmd) == ERROR_SUCCESS)
					command = szCmd;
				RegCloseKey(hCmd);
			}
		}

		// Check duplicates
		bool bDuplicate = false;
		for (const auto& e : entries)
		{
			if (e.displayName == displayName && e.location == ctxName)
			{
				bDuplicate = true;
				break;
			}
		}

		if (!bDuplicate)
		{
			entries.push_back({
				ctxName,
				bFoundRegKey ? regKeyName : verb,
				displayName,
				command,
				bFoundRegKey ? regPath : regBasePath,
				HKEY_CLASSES_ROOT,
				bIsShellEx,
				false,  // bExtended
				bDisabled,
				bEnabled
			});
		}
	}
}

void CContextMenuDlg::ScanWithShellAPI()
{
	// Use SHCreateDefaultContextMenu to get the EXACT menu that Explorer shows.
	// This is the same API Explorer uses internally — it merges static verbs
	// and ShellEx handlers, eliminates duplicates, and handles dynamic menus.
	m_entries.clear();

	CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO_CM_LOCATION);
	int sel = pCombo ? pCombo->GetCurSel() : 0;
	CString filter;
	if (sel > 0) filter = m_locations[sel].name;

	// Determine if filtering by a specific extension
	CString filterExt;
	bool bFilterByExt = false;
	if (!filter.IsEmpty() && filter != _T("全部"))
	{
		for (const auto& loc : m_locations)
		{
			if (loc.name == filter && !loc.isShellEx && loc.shellPath.Find(_T('.')) == 0)
			{
				int slashPos = loc.shellPath.Find(_T('\\'));
				filterExt = (slashPos > 0) ? loc.shellPath.Left(slashPos) : loc.shellPath;
				bFilterByExt = true;
				break;
			}
		}
	}

	TCHAR szTempDir[MAX_PATH];
	GetTempPath(MAX_PATH, szTempDir);

	// --- Extension-specific: create temp file with the extension and scan via Shell API ---
	if (bFilterByExt && !filterExt.IsEmpty())
	{
		CString strTempFile = CString(szTempDir) + _T("MFC_CM_Scan") + filterExt;
		HANDLE hFile = CreateFile(strTempFile, GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFile);
			IShellFolder* pDesktop = nullptr;
			if (SUCCEEDED(SHGetDesktopFolder(&pDesktop)) && pDesktop)
			{
				LPITEMIDLIST pidlFolder = nullptr;
				if (SUCCEEDED(SHParseDisplayName(strTempFile, nullptr, &pidlFolder, 0, nullptr)) && pidlFolder)
				{
					IShellFolder* pFolder = nullptr;
					LPCITEMIDLIST pidlChild = nullptr;
					if (SUCCEEDED(SHBindToParent(pidlFolder, IID_IShellFolder, (void**)&pFolder, &pidlChild)) && pFolder && pidlChild)
					{
						DEFCONTEXTMENU dcm = {};
						dcm.hwnd = m_hWnd;
						dcm.psf = pFolder;
						dcm.cidl = 1;
						dcm.apidl = &pidlChild;

						IContextMenu* pCM = nullptr;
						HRESULT hr = SHCreateDefaultContextMenu(&dcm, IID_IContextMenu, (void**)&pCM);
						if (SUCCEEDED(hr) && pCM)
						{
							HMENU hMenu = CreatePopupMenu();
							hr = pCM->QueryContextMenu(hMenu, 0, 1, 30000, CMF_NORMAL | CMF_EXPLORE);
							if (SUCCEEDED(hr))
							{
								CString ctxName = CString(_T("扩展名 ")) + filterExt;
								EnumerateShellMenu(hMenu, pCM, 1, ctxName, _T("*\\shell"), m_entries);
							}
							DestroyMenu(hMenu);
							pCM->Release();
						}
						pFolder->Release();
					}
					CoTaskMemFree(pidlFolder);
				}
				pDesktop->Release();
			}
			DeleteFile(strTempFile);
		}
		return;
	}

	// Context type definitions
	struct CtxDef { const TCHAR* name; const TCHAR* regPath; bool bFolder; bool bBg; bool bDrive; bool bDesktop; };
	const CtxDef ctxDefs[] = {
		{ _T("文件 (*)"),              _T("*\\shell"), false, false, false, false },
		{ _T("文件夹 (Directory)"),    _T("Directory\\shell"), true, false, false, false },
		{ _T("文件夹背景"),            _T("Directory\\Background\\shell"), true, true, false, false },
		{ _T("桌面背景"),              _T("DesktopBackground\\shell"), false, true, false, true },
		{ _T("驱动器"),                _T("Drive\\shell"), false, false, true, false },
		{ _T("所有文件"),              _T("AllFilesystemObjects\\shell"), false, false, false, false },
	};

	for (const auto& ctx : ctxDefs)
	{
		if (sel > 0)
		{
			CString selName = m_locations[sel].name;
			if (selName != _T("全部") && selName.Find(ctx.name) < 0)
				continue;
		}

		IShellFolder* pDesktop = nullptr;
		if (FAILED(SHGetDesktopFolder(&pDesktop)) || !pDesktop)
			continue;

		LPITEMIDLIST pidlFolder = nullptr;
		LPCITEMIDLIST pidlChild = nullptr;
		IShellFolder* pFolder = nullptr;
		LPCITEMIDLIST* ppidlChild = nullptr;
		UINT cidl = 0;
		bool bIsBg = ctx.bBg || ctx.bDesktop;
		CString strTempPath;

		if (ctx.bDesktop)
		{
			SHGetSpecialFolderLocation(nullptr, CSIDL_DESKTOP, &pidlFolder);
			pFolder = pDesktop;
			pFolder->AddRef();
		}
		else if (ctx.bDrive)
		{
			SHParseDisplayName(_T("C:\\"), nullptr, &pidlFolder, 0, nullptr);
			SHBindToParent(pidlFolder, IID_IShellFolder, (void**)&pFolder, &pidlChild);
			if (pidlChild) { ppidlChild = &pidlChild; cidl = 1; }
		}
		else if (ctx.bBg)
		{
			SHParseDisplayName(szTempDir, nullptr, &pidlFolder, 0, nullptr);
			pDesktop->BindToObject(pidlFolder, nullptr, IID_IShellFolder, (void**)&pFolder);
		}
		else if (ctx.bFolder)
		{
			strTempPath = CString(szTempDir) + _T("MFC_CM_Scan\\");
			CreateDirectory(strTempPath, nullptr);
			SHParseDisplayName(strTempPath, nullptr, &pidlFolder, 0, nullptr);
			SHBindToParent(pidlFolder, IID_IShellFolder, (void**)&pFolder, &pidlChild);
			if (pidlChild) { ppidlChild = &pidlChild; cidl = 1; }
		}
		else
		{
			strTempPath = CString(szTempDir) + _T("MFC_CM_Scan.txt");
			HANDLE hFile = CreateFile(strTempPath, GENERIC_WRITE, 0, nullptr,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				CloseHandle(hFile);
				SHParseDisplayName(strTempPath, nullptr, &pidlFolder, 0, nullptr);
				SHBindToParent(pidlFolder, IID_IShellFolder, (void**)&pFolder, &pidlChild);
				if (pidlChild) { ppidlChild = &pidlChild; cidl = 1; }
			}
		}

		if (pFolder)
		{
			DEFCONTEXTMENU dcm = {};
			dcm.hwnd = m_hWnd;
			dcm.psf = pFolder;
			dcm.pidlFolder = bIsBg ? pidlFolder : nullptr;
			dcm.cidl = cidl;
			dcm.apidl = ppidlChild;

			IContextMenu* pCM = nullptr;
			HRESULT hr = SHCreateDefaultContextMenu(&dcm, IID_IContextMenu, (void**)&pCM);
			if (SUCCEEDED(hr) && pCM)
			{
				HMENU hMenu = CreatePopupMenu();
				hr = pCM->QueryContextMenu(hMenu, 0, 1, 30000, CMF_NORMAL | CMF_EXPLORE);
				if (SUCCEEDED(hr))
				{
					EnumerateShellMenu(hMenu, pCM, 1, ctx.name, ctx.regPath, m_entries);
				}
				DestroyMenu(hMenu);
				pCM->Release();
			}
		}

		if (pFolder && pFolder != pDesktop) pFolder->Release();
		if (pidlFolder && pidlFolder != (LPITEMIDLIST)pidlChild) CoTaskMemFree(pidlFolder);
		pDesktop->Release();

		if (!strTempPath.IsEmpty())
		{
			if (ctx.bFolder) RemoveDirectory(strTempPath);
			else if (!ctx.bBg && !ctx.bDrive && !ctx.bDesktop) DeleteFile(strTempPath);
		}
	}
}
