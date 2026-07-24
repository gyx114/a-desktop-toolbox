// ContextMenuDlg.h: header file
//

#pragma once
#include "afxdialogex.h"
#include <vector>

class CContextMenuDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CContextMenuDlg)

public:
	CContextMenuDlg(CWnd* pParent = nullptr);
	virtual ~CContextMenuDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CONTEXT_MENU_DLG };
#endif

	// Public struct used by static helper functions
	struct MenuEntry
	{
		CString location;      // scope name (e.g. "文件 (*)")
		CString keyName;       // registry subkey name (verb or handler name)
		CString displayName;   // resolved display text (MUIVerb/MUI resolved)
		CString command;       // command line or DLL path
		CString regPath;       // full registry path
		HKEY   hRoot;          // registry root
		bool   bIsShellEx;     // true: COM ShellEx handler, false: static verb
		bool   bExtended;      // true: Shift+right-click only
		bool   bDisabled;      // true: LegacyDisable/ProgrammaticAccessOnly set
		bool   bEnabled;       // true: currently enabled (not disabled by user)
	};

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()

private:
	struct LocationFilter
	{
		CString name;
		CString shellPath;
		bool isShellEx;  // true: scan shellex\ContextMenuHandlers (COM-based)
	};

	std::vector<MenuEntry> m_entries;
	std::vector<LocationFilter> m_locations;

	int m_listLeft, m_listTop;
	int m_statusTop;

	void InitLocations();
	void ScanEntries(const CString& filter);
	void ScanShellExLocation(const LocationFilter& loc);
	void ScanShellExWithCom(const LocationFilter& loc);
	void ScanWithShellAPI();
	void ScanAllExtensions(const CString& filterExt = _T(""));
	static bool IsRunningAsAdmin();
	static CString ResolveClsidName(const CString& clsid);
	static CString GetShellExDisplayName(const CString& clsid, const CString& dllPath);
	static CString ResolveMUIString(const CString& raw);
	static CString ResolveProgID(const CString& ext);
	void RefreshList();
	void UpdateStatus(const CString& text);
	void AdjustColumnWidths();
	bool DeleteRegistryKeyRecursive(HKEY hParent, const CString& subKey);
	void OpenRegEditToPath(HKEY hRoot, const CString& path);
	void LoadSelfContextMenuState();
	void SaveSelfContextMenuState(bool bEnable);
	void LoadWin11ClassicState();
	void SaveWin11ClassicState(bool bEnable);
	void ToggleEntry(int index);
	void RestartExplorer();
	bool IsKeyDisabledByPrefix(const CString& keyName);

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnBnClickedRefresh();
	afx_msg void OnCbnSelchangeLocation();
	afx_msg void OnBnClickedDelete();
	afx_msg void OnBnClickedLocate();
	afx_msg void OnBnClickedCheckFolder();
	afx_msg void OnBnClickedCheckWin11Classic();
	afx_msg void OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMenuDelete();
	afx_msg void OnMenuLocate();
	afx_msg void OnMenuToggle();
};