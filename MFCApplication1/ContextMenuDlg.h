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

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()

private:
	struct MenuEntry
	{
		CString location;
		CString keyName;
		CString displayName;
		CString command;
		CString regPath;
		HKEY   hRoot;
	};

	std::vector<MenuEntry> m_entries;

	struct LocationFilter
	{
		CString name;
		CString shellPath;
		bool isShellEx;  // true: scan shellex\ContextMenuHandlers (COM-based)
	};

	std::vector<LocationFilter> m_locations;

	int m_listLeft, m_listTop;
	int m_statusTop;

	void InitLocations();
	void ScanEntries(const CString& filter);
	void ScanShellExLocation(const LocationFilter& loc);
	static CString ResolveClsidName(const CString& clsid);
	void RefreshList();
	void UpdateStatus(const CString& text);
	void AdjustColumnWidths();
	bool DeleteRegistryKeyRecursive(HKEY hParent, const CString& subKey);
	void OpenRegEditToPath(HKEY hRoot, const CString& path);
	void LoadSelfContextMenuState();
	void SaveSelfContextMenuState(bool bEnable);

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnBnClickedRefresh();
	afx_msg void OnCbnSelchangeLocation();
	afx_msg void OnBnClickedDelete();
	afx_msg void OnBnClickedLocate();
	afx_msg void OnBnClickedCheckFolder();
	afx_msg void OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMenuDelete();
	afx_msg void OnMenuLocate();
};
