// StickyNoteDlg.h: header file
//

#pragma once
#include "afxdialogex.h"

// CStickyNoteDlg dialog

class CStickyNoteDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CStickyNoteDlg)

public:
	CStickyNoteDlg(CWnd* pParent = nullptr);
	virtual ~CStickyNoteDlg();
	void SaveIfNeeded();  // called by main dialog on shutdown

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_STICKY_NOTE_DLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnClose();
	virtual void OnOK();
	virtual void OnCancel();

	DECLARE_MESSAGE_MAP()

private:
	CString m_noteText;
	CString m_savePath;
	CRect m_editMargins;
	CString GetSavePath();
	bool EnsureSavePath();
	void SaveNote();
	void ResizeEdit();
	afx_msg void OnBnClickedBrowse();
	afx_msg void OnSize(UINT nType, int cx, int cy);
};