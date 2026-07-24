// EnvVarDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "EnvVarDlg.h"
#include "afxdialogex.h"
#include <algorithm>
#include <fstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Local input dialog for add/edit environment variable name and value
class CEnvInputDlg : public CDialogEx
{
public:
	CString m_strName;
	CString m_strValue;
	bool m_bEditMode;
	CString m_strTitle;

	CEnvInputDlg(bool bEdit, const CString& title, CWnd* pParent)
		: CDialogEx(IDD_ENVVAR_INPUT_DLG, pParent), m_bEditMode(bEdit), m_strTitle(title) {}

protected:
	virtual BOOL OnInitDialog()
	{
		CDialogEx::OnInitDialog();
		SetWindowText(m_strTitle);
		SetDlgItemText(IDC_ENV_INPUT_NAME, m_strName);
		SetDlgItemText(IDC_ENV_INPUT_VALUE, m_strValue);
		if (m_bEditMode)
			GetDlgItem(IDC_ENV_INPUT_NAME)->EnableWindow(FALSE);
		return TRUE;
	}
	virtual void OnOK()
	{
		GetDlgItemText(IDC_ENV_INPUT_NAME, m_strName);
		GetDlgItemText(IDC_ENV_INPUT_VALUE, m_strValue);
		m_strName.Trim();
		m_strValue.Trim();
		if (m_strName.IsEmpty())
		{
			MessageBox(_T("变量名不能为空。"), _T("提示"), MB_ICONWARNING);
			return;
		}
		CDialogEx::OnOK();
	}
};

IMPLEMENT_DYNAMIC(CEnvVarDlg, CDialogEx)

CEnvVarDlg::CEnvVarDlg(CWnd* pParent)
	: CDialogEx(IDD_ENVVAR_DLG, pParent)
	, m_listSysLeft(0), m_listSysTop(0)
	, m_listUserLeft(0), m_listUserTop(0)
	, m_buttonTop(0)
	, m_statusTop(0)
{
}

CEnvVarDlg::~CEnvVarDlg()
{
}

void CEnvVarDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CEnvVarDlg, CDialogEx)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BTN_ENV_ADD, &CEnvVarDlg::OnBnClickedEnvAdd)
	ON_BN_CLICKED(IDC_BTN_ENV_EDIT, &CEnvVarDlg::OnBnClickedEnvEdit)
	ON_BN_CLICKED(IDC_BTN_ENV_DELETE, &CEnvVarDlg::OnBnClickedEnvDelete)
	ON_BN_CLICKED(IDC_BTN_ENV_REFRESH, &CEnvVarDlg::OnBnClickedEnvRefresh)
	ON_BN_CLICKED(IDC_BTN_ENV_EXPORT, &CEnvVarDlg::OnBnClickedEnvExport)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_ENV_SYSTEM, &CEnvVarDlg::OnNMDblclkListEnv)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_ENV_USER, &CEnvVarDlg::OnNMDblclkListEnv)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_ENV_SYSTEM, &CEnvVarDlg::OnNMRclickListEnv)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_ENV_USER, &CEnvVarDlg::OnNMRclickListEnv)
END_MESSAGE_MAP()

BOOL CEnvVarDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_F5)
	{
		RefreshAll();
		return TRUE;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

// ========== Registry helpers ==========

bool CEnvVarDlg::ReadEnvVars(bool bSystem, std::vector<EnvVarEntry>& outVars)
{
	outVars.clear();

	HKEY hRoot = bSystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	const TCHAR* szSubKey = bSystem
		? _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment")
		: _T("Environment");

	HKEY hKey = nullptr;
	if (RegOpenKeyEx(hRoot, szSubKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return false;

	DWORD dwIndex = 0;
	TCHAR szName[MAX_PATH];
	DWORD cbName = MAX_PATH;
	TCHAR szValue[32768];
	DWORD cbValue = sizeof(szValue);
	DWORD dwType = 0;

	while (RegEnumValue(hKey, dwIndex, szName, &cbName,
		nullptr, &dwType, (LPBYTE)szValue, &cbValue) == ERROR_SUCCESS)
	{
		if (dwType == REG_SZ || dwType == REG_EXPAND_SZ)
		{
			EnvVarEntry entry;
			entry.name = szName;
			entry.value = szValue;
			entry.bSystem = bSystem;
			outVars.push_back(entry);
		}

		dwIndex++;
		cbName = MAX_PATH;
		cbValue = sizeof(szValue);
	}

	RegCloseKey(hKey);

	std::sort(outVars.begin(), outVars.end(),
		[](const EnvVarEntry& a, const EnvVarEntry& b) {
			return _tcsicmp(a.name, b.name) < 0;
		});

	return true;
}

bool CEnvVarDlg::WriteEnvVar(const CString& name, const CString& value, bool bSystem)
{
	HKEY hRoot = bSystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	const TCHAR* szSubKey = bSystem
		? _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment")
		: _T("Environment");

	HKEY hKey = nullptr;
	if (RegOpenKeyEx(hRoot, szSubKey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
		return false;

	LONG lResult = RegSetValueEx(hKey, name, 0, REG_EXPAND_SZ,
		(const BYTE*)value.GetString(),
		(value.GetLength() + 1) * sizeof(TCHAR));

	RegCloseKey(hKey);

	if (lResult == ERROR_SUCCESS)
		BroadcastEnvChange();

	return lResult == ERROR_SUCCESS;
}

bool CEnvVarDlg::DeleteEnvVar(const CString& name, bool bSystem)
{
	HKEY hRoot = bSystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	const TCHAR* szSubKey = bSystem
		? _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment")
		: _T("Environment");

	HKEY hKey = nullptr;
	if (RegOpenKeyEx(hRoot, szSubKey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
		return false;

	LONG lResult = RegDeleteValue(hKey, name);
	RegCloseKey(hKey);

	if (lResult == ERROR_SUCCESS)
		BroadcastEnvChange();

	return lResult == ERROR_SUCCESS;
}

void CEnvVarDlg::BroadcastEnvChange()
{
	SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
		(LPARAM)_T("Environment"), SMTO_ABORTIFHUNG, 5000, nullptr);
}

// ========== Dialog initialization ==========

BOOL CEnvVarDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	auto ReadRect = [&](int id) -> CRect {
		CRect rc(0, 0, 0, 0);
		CWnd* pWnd = GetDlgItem(id);
		if (pWnd) { pWnd->GetWindowRect(&rc); ScreenToClient(&rc); }
		return rc;
	};

	CRect rcSys = ReadRect(IDC_LIST_ENV_SYSTEM);
	m_listSysLeft = rcSys.left; m_listSysTop = rcSys.top;

	CRect rcUser = ReadRect(IDC_LIST_ENV_USER);
	m_listUserLeft = rcUser.left; m_listUserTop = rcUser.top;

	CRect rcBtn = ReadRect(IDC_BTN_ENV_ADD);
	m_buttonTop = rcBtn.top;

	CRect rcStatus = ReadRect(IDC_STATIC_ENV_STATUS);
	m_statusTop = rcStatus.top;

	CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
	if (pListSys)
	{
		pListSys->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		pListSys->InsertColumn(0, _T("变量名"), LVCFMT_LEFT, 180);
		pListSys->InsertColumn(1, _T("值"), LVCFMT_LEFT, 332);
	}

	CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);
	if (pListUser)
	{
		pListUser->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		pListUser->InsertColumn(0, _T("变量名"), LVCFMT_LEFT, 180);
		pListUser->InsertColumn(1, _T("值"), LVCFMT_LEFT, 332);
	}

	RefreshAll();

	return TRUE;
}

void CEnvVarDlg::PostNcDestroy()
{
	CDialogEx::PostNcDestroy();
	delete this;
}

// ========== Layout ==========

void CEnvVarDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType != SIZE_MINIMIZED && IsWindow(m_hWnd))
	{
		int nListWidth = (cx - m_listSysLeft - 10) / 2;
		int nListHeight = m_buttonTop - m_listSysTop - 4;

		CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
		if (pListSys)
		{
			pListSys->SetWindowPos(nullptr, m_listSysLeft, m_listSysTop,
				nListWidth, nListHeight, SWP_NOZORDER);
			AdjustColumnWidths(IDC_LIST_ENV_SYSTEM);
		}

		CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);
		if (pListUser)
		{
			pListUser->SetWindowPos(nullptr, m_listUserLeft, m_listUserTop,
				nListWidth, nListHeight, SWP_NOZORDER);
			AdjustColumnWidths(IDC_LIST_ENV_USER);
		}
	}
}

void CEnvVarDlg::AdjustColumnWidths(int listId)
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(listId);
	if (!pList || !pList->GetSafeHwnd()) return;

	CRect rcList;
	pList->GetClientRect(&rcList);
	int nWidth = rcList.Width();
	if (nWidth <= 0) return;

	pList->SetColumnWidth(0, nWidth * 35 / 100);
	pList->SetColumnWidth(1, nWidth * 65 / 100);
}

// ========== Data refresh ==========

void CEnvVarDlg::RefreshAll()
{
	ReadEnvVars(true, m_systemVars);
	ReadEnvVars(false, m_userVars);

	PopulateList(IDC_LIST_ENV_SYSTEM, m_systemVars);
	PopulateList(IDC_LIST_ENV_USER, m_userVars);

	CString status;
	status.Format(_T("系统变量: %d 个  |  用户变量: %d 个"),
		(int)m_systemVars.size(), (int)m_userVars.size());
	UpdateStatus(status);
}

void CEnvVarDlg::PopulateList(int listId, const std::vector<EnvVarEntry>& vars)
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(listId);
	if (!pList) return;

	pList->SetRedraw(FALSE);
	pList->DeleteAllItems();

	for (size_t i = 0; i < vars.size(); ++i)
	{
		int idx = pList->InsertItem((int)i, vars[i].name);
		pList->SetItemText(idx, 1, vars[i].value);
	}

	pList->SetRedraw(TRUE);
}

void CEnvVarDlg::UpdateStatus(const CString& text)
{
	SetDlgItemText(IDC_STATIC_ENV_STATUS, text);
}

// ========== Button handlers ==========

void CEnvVarDlg::OnBnClickedEnvAdd()     { OnAdd(); }
void CEnvVarDlg::OnBnClickedEnvEdit()    { OnEdit(); }
void CEnvVarDlg::OnBnClickedEnvDelete()  { OnDelete(); }
void CEnvVarDlg::OnBnClickedEnvRefresh() { RefreshAll(); UpdateStatus(_T("已刷新")); }
void CEnvVarDlg::OnBnClickedEnvExport()  { OnExport(); }
void CEnvVarDlg::OnNMDblclkListEnv(NMHDR*, LRESULT* pResult) { *pResult = 0; OnEdit(); }

void CEnvVarDlg::OnNMRclickListEnv(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	NMITEMACTIVATE* pNMItem = (NMITEMACTIVATE*)pNMHDR;
	CListCtrl* pList = (CListCtrl*)GetDlgItem((int)pNMHDR->idFrom);
	if (!pList) return;

	// Select the right-clicked item if not already selected
	int nItem = pNMItem->iItem;
	if (nItem >= 0)
	{
		// If the clicked item is not selected, select only it
		if (pList->GetItemState(nItem, LVIS_SELECTED) != LVIS_SELECTED)
		{
			for (int i = pList->GetItemCount() - 1; i >= 0; --i)
				pList->SetItemState(i, 0, LVIS_SELECTED);
			pList->SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
		}
	}

	// Build popup menu
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, 1, _T("编辑"));
	menu.AppendMenu(MF_STRING, 2, _T("删除"));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, 3, _T("复制变量名"));
	menu.AppendMenu(MF_STRING, 4, _T("复制变量值"));

	CPoint pt;
	GetCursorPos(&pt);
	int nCmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, this);
	menu.DestroyMenu();

	switch (nCmd)
	{
	case 1: OnEdit(); break;
	case 2: OnDelete(); break;
	case 3: OnCopyName(); break;
	case 4: OnCopyValue(); break;
	}
}

// ========== Helper: get selected item from either list ==========

static bool GetSelectedEnvVar(CWnd* pParent, int idSys, int idUser,
	CListCtrl*& pOutList, std::vector<CEnvVarDlg::EnvVarEntry>*& pOutVars, bool& bOutSystem, int& nOutIdx)
{
	pOutList = nullptr;
	pOutVars = nullptr;

	// Try system list first
	CListCtrl* pListSys = (CListCtrl*)pParent->GetDlgItem(idSys);
	POSITION pos = pListSys->GetFirstSelectedItemPosition();
	if (pos)
	{
		pOutList = pListSys;
		nOutIdx = pListSys->GetNextSelectedItem(pos);
		return true;  // bOutSystem stays false (caller must set it)
	}

	// Try user list
	CListCtrl* pListUser = (CListCtrl*)pParent->GetDlgItem(idUser);
	pos = pListUser->GetFirstSelectedItemPosition();
	if (pos)
	{
		pOutList = pListUser;
		nOutIdx = pListUser->GetNextSelectedItem(pos);
		return true;
	}

	return false;
}

// ========== Actions ==========

void CEnvVarDlg::OnAdd()
{
	CString msg = _T("要添加系统变量还是用户变量？\n\n选\"是\"=系统变量，选\"否\"=用户变量");
	int nRet = MessageBox(msg, _T("添加环境变量"), MB_YESNOCANCEL | MB_ICONQUESTION);
	if (nRet == IDCANCEL) return;
	bool bSystem = (nRet == IDYES);

	CEnvInputDlg dlg(false, _T("添加环境变量"), this);
	if (dlg.DoModal() != IDOK) return;

	if (!WriteEnvVar(dlg.m_strName, dlg.m_strValue, bSystem))
	{
		CString errMsg;
		errMsg.Format(_T("无法写入变量 %s。\n\n可能需要管理员权限来修改系统变量。"),
			dlg.m_strName);
		MessageBox(errMsg, _T("写入失败"), MB_ICONERROR);
	}
	else
	{
		RefreshAll();
		CString status;
		status.Format(_T("已添加%s变量: %s"), bSystem ? _T("系统") : _T("用户"), dlg.m_strName);
		UpdateStatus(status);
	}
}

void CEnvVarDlg::OnEdit()
{
	CListCtrl* pList = nullptr;
	std::vector<EnvVarEntry>* pVars = nullptr;
	bool bSystem = false;

	// Check system list
	CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
	POSITION pos = pListSys->GetFirstSelectedItemPosition();
	if (pos)
	{
		pList = pListSys;
		pVars = &m_systemVars;
		bSystem = true;
	}
	else
	{
		CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);
		pos = pListUser->GetFirstSelectedItemPosition();
		if (pos)
		{
			pList = pListUser;
			pVars = &m_userVars;
			bSystem = false;
		}
	}

	if (!pList || !pVars)
	{
		MessageBox(_T("请先选择要编辑的变量。"), _T("提示"), MB_ICONINFORMATION);
		return;
	}

	int idx = pList->GetNextSelectedItem(pos);
	if (idx < 0 || idx >= (int)pVars->size()) return;

	const auto& entry = (*pVars)[idx];

	CEnvInputDlg dlg(true, _T("编辑环境变量"), this);
	dlg.m_strName = entry.name;
	dlg.m_strValue = entry.value;
	if (dlg.DoModal() != IDOK) return;

	if (!WriteEnvVar(dlg.m_strName, dlg.m_strValue, bSystem))
	{
		CString errMsg;
		errMsg.Format(_T("无法写入变量 %s。\n\n可能需要管理员权限来修改系统变量。"),
			dlg.m_strName);
		MessageBox(errMsg, _T("写入失败"), MB_ICONERROR);
	}
	else
	{
		RefreshAll();
		CString status;
		status.Format(_T("已更新%s变量: %s"), bSystem ? _T("系统") : _T("用户"), dlg.m_strName);
		UpdateStatus(status);
	}
}

void CEnvVarDlg::OnDelete()
{
	CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
	CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);

	CListCtrl* pList = nullptr;
	std::vector<EnvVarEntry>* pVars = nullptr;
	bool bSystem = false;
	POSITION pos = nullptr;

	pos = pListSys->GetFirstSelectedItemPosition();
	if (pos)
	{
		pList = pListSys;
		pVars = &m_systemVars;
		bSystem = true;
	}
	else
	{
		pos = pListUser->GetFirstSelectedItemPosition();
		if (pos)
		{
			pList = pListUser;
			pVars = &m_userVars;
			bSystem = false;
		}
	}

	if (!pList || !pVars)
	{
		MessageBox(_T("请先选择要删除的变量。"), _T("提示"), MB_ICONINFORMATION);
		return;
	}

	int idx = pList->GetNextSelectedItem(pos);
	if (idx < 0 || idx >= (int)pVars->size()) return;

	const auto& entry = (*pVars)[idx];

	CString msg;
	msg.Format(_T("确定要删除%s变量 \"%s\" 吗？"),
		bSystem ? _T("系统") : _T("用户"), entry.name);
	if (MessageBox(msg, _T("确认删除"), MB_ICONWARNING | MB_YESNO) != IDYES)
		return;

	if (!DeleteEnvVar(entry.name, bSystem))
	{
		CString errMsg;
		errMsg.Format(_T("无法删除变量 %s。\n\n可能需要管理员权限来删除系统变量。"),
			entry.name);
		MessageBox(errMsg, _T("删除失败"), MB_ICONERROR);
	}
	else
	{
		RefreshAll();
		CString status;
		status.Format(_T("已删除%s变量: %s"), bSystem ? _T("系统") : _T("用户"), entry.name);
		UpdateStatus(status);
	}
}

void CEnvVarDlg::OnCopyName()
{
	CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
	CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);

	std::vector<EnvVarEntry>* pVars = nullptr;
	POSITION pos = nullptr;
	int idx = -1;

	pos = pListSys->GetFirstSelectedItemPosition();
	if (pos) { pVars = &m_systemVars; idx = pListSys->GetNextSelectedItem(pos); }
	else
	{
		pos = pListUser->GetFirstSelectedItemPosition();
		if (pos) { pVars = &m_userVars; idx = pListUser->GetNextSelectedItem(pos); }
	}

	if (!pVars || idx < 0 || idx >= (int)pVars->size()) return;

	const auto& entry = (*pVars)[idx];

	if (OpenClipboard())
	{
		EmptyClipboard();
		int nLen = (entry.name.GetLength() + 1) * sizeof(TCHAR);
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nLen);
		if (hMem)
		{
			memcpy(GlobalLock(hMem), entry.name.GetString(), nLen);
			GlobalUnlock(hMem);
			SetClipboardData(CF_UNICODETEXT, hMem);
		}
		CloseClipboard();
	}
	UpdateStatus(_T("已复制变量名: ") + entry.name);
}

void CEnvVarDlg::OnCopyValue()
{
	CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
	CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);

	std::vector<EnvVarEntry>* pVars = nullptr;
	POSITION pos = nullptr;
	int idx = -1;

	pos = pListSys->GetFirstSelectedItemPosition();
	if (pos) { pVars = &m_systemVars; idx = pListSys->GetNextSelectedItem(pos); }
	else
	{
		pos = pListUser->GetFirstSelectedItemPosition();
		if (pos) { pVars = &m_userVars; idx = pListUser->GetNextSelectedItem(pos); }
	}

	if (!pVars || idx < 0 || idx >= (int)pVars->size()) return;

	const auto& entry = (*pVars)[idx];

	if (OpenClipboard())
	{
		EmptyClipboard();
		int nLen = (entry.value.GetLength() + 1) * sizeof(TCHAR);
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nLen);
		if (hMem)
		{
			memcpy(GlobalLock(hMem), entry.value.GetString(), nLen);
			GlobalUnlock(hMem);
			SetClipboardData(CF_UNICODETEXT, hMem);
		}
		CloseClipboard();
	}
	UpdateStatus(_T("已复制变量值: ") + entry.value);
}

void CEnvVarDlg::OnExport()
{
	CFileDialog dlg(FALSE, _T(".txt"), _T("env_vars.txt"),
		OFN_OVERWRITEPROMPT,
		_T("文本文件 (*.txt)|*.txt|环境变量文件 (*.env)|*.env|所有文件 (*.*)|*.*||"),
		this);

	if (dlg.DoModal() != IDOK) return;

	CString filePath = dlg.GetPathName();

	std::ofstream ofs(filePath.GetString(), std::ios::out | std::ios::trunc);
	if (!ofs.is_open())
	{
		MessageBox(_T("无法创建文件。"), _T("导出失败"), MB_ICONERROR);
		return;
	}

	ofs << "# System Environment Variables\n";
	for (const auto& v : m_systemVars)
	{
		CStringA nameA(v.name);
		CStringA valueA(v.value);
		ofs << nameA.GetString() << "=" << valueA.GetString() << "\n";
	}

	ofs << "\n# User Environment Variables\n";
	for (const auto& v : m_userVars)
	{
		CStringA nameA(v.name);
		CStringA valueA(v.value);
		ofs << nameA.GetString() << "=" << valueA.GetString() << "\n";
	}

	ofs.close();

	CString status;
	status.Format(_T("已导出到: %s"), filePath);
	UpdateStatus(status);
}