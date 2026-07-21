// StickyNoteDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "StickyNoteDlg.h"
#include "afxdialogex.h"
#include <fstream>
#include <filesystem>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CStickyNoteDlg, CDialogEx)

CStickyNoteDlg::CStickyNoteDlg(CWnd* pParent)
	: CDialogEx(IDD_STICKY_NOTE_DLG, pParent)
{
}

CStickyNoteDlg::~CStickyNoteDlg()
{
}

void CStickyNoteDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_STICKY_NOTE, m_noteText);
}

BEGIN_MESSAGE_MAP(CStickyNoteDlg, CDialogEx)
	ON_WM_CLOSE()
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BTN_STICKY_BROWSE, &CStickyNoteDlg::OnBnClickedBrowse)
END_MESSAGE_MAP()

CString CStickyNoteDlg::GetSavePath()
{
	CString folder = AfxGetApp()->GetProfileString(L"StickyNote", L"SaveFolder", L"");
	if (folder.IsEmpty() || !std::filesystem::exists(std::filesystem::path(folder.GetString())))
	{
		WCHAR exePath[MAX_PATH];
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		folder = exePath;
		int pos = folder.ReverseFind(L'\\');
		if (pos >= 0)
			folder = folder.Left(pos + 1);
	}
	return folder + L"sticky_note.txt";
}

bool CStickyNoteDlg::EnsureSavePath()
{
	CString folder = AfxGetApp()->GetProfileString(L"StickyNote", L"SaveFolder", L"");
	if (!folder.IsEmpty() && std::filesystem::exists(std::filesystem::path(folder.GetString())))
		return true;

	CFolderPickerDialog dlg(nullptr, 0, this);
	dlg.m_ofn.lpstrTitle = L"请选择便签保存目录";
	if (dlg.DoModal() == IDOK)
	{
		AfxGetApp()->WriteProfileString(L"StickyNote", L"SaveFolder", dlg.GetPathName());
		return true;
	}
	return false;
}

void CStickyNoteDlg::SaveNote()
{
	UpdateData(TRUE);
	if (m_noteText.IsEmpty())
		return;

	if (!EnsureSavePath())
		return;

	CString filePath = GetSavePath();
	std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
	if (ofs.is_open())
	{
		CStringA utf8(m_noteText);
		ofs.write(utf8, utf8.GetLength());
		ofs.close();
	}
}

void CStickyNoteDlg::SaveIfNeeded()
{
	SaveNote();
}

BOOL CStickyNoteDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Remove dialog icon from title bar
	SetIcon(NULL, FALSE);
	SetIcon(NULL, TRUE);

	// Set always-on-top
	SetWindowPos(&CWnd::wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	// Calculate edit control margins for resize
	CWnd* pEdit = GetDlgItem(IDC_EDIT_STICKY_NOTE);
	if (pEdit)
	{
		CRect rcClient, rcEdit;
		GetClientRect(&rcClient);
		pEdit->GetWindowRect(&rcEdit);
		ScreenToClient(&rcEdit);
		m_editMargins.left = rcEdit.left;
		m_editMargins.top = rcEdit.top;
		m_editMargins.right = rcClient.right - rcEdit.right;
		m_editMargins.bottom = rcClient.bottom - rcEdit.bottom;
	}

	return TRUE;
}

void CStickyNoteDlg::OnBnClickedBrowse()
{
	CString currentFolder = AfxGetApp()->GetProfileString(L"StickyNote", L"SaveFolder", L"");
	CFolderPickerDialog dlg(currentFolder.IsEmpty() ? nullptr : (LPCTSTR)currentFolder, 0, this);
	dlg.m_ofn.lpstrTitle = L"选择便签保存目录";
	if (dlg.DoModal() == IDOK)
	{
		AfxGetApp()->WriteProfileString(L"StickyNote", L"SaveFolder", dlg.GetPathName());
	}
}

void CStickyNoteDlg::OnClose()
{
	SaveNote();
	ShowWindow(SW_HIDE);
}

void CStickyNoteDlg::OnOK()
{
	SaveNote();
	ShowWindow(SW_HIDE);
}

void CStickyNoteDlg::OnCancel()
{
	ShowWindow(SW_HIDE);
}

void CStickyNoteDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType != SIZE_MINIMIZED)
		ResizeEdit();
}

void CStickyNoteDlg::ResizeEdit()
{
	CWnd* pEdit = GetDlgItem(IDC_EDIT_STICKY_NOTE);
	if (pEdit && ::IsWindow(pEdit->m_hWnd))
	{
		CRect rcClient;
		GetClientRect(&rcClient);
		pEdit->SetWindowPos(nullptr,
			m_editMargins.left,
			m_editMargins.top,
			rcClient.Width() - m_editMargins.left - m_editMargins.right,
			rcClient.Height() - m_editMargins.top - m_editMargins.bottom,
			SWP_NOZORDER);
	}
}