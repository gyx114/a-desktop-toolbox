// MarkdownDlg.h: header file
//

#pragma once
#include "afxdialogex.h"

class CMarkdownDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CMarkdownDlg)

public:
	CMarkdownDlg(CWnd* pParent = nullptr);
	virtual ~CMarkdownDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MARKDOWN_DLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()

private:
	CString m_markdownText;
	CFont m_fontEdit;
	CButton m_btnOpen;
	CWnd m_browser;
	CRect m_editMargins;
	CRect m_previewMargins;

	void ResizeControls();
	void RefreshPreview();
	void LoadFile(const CString& path);
	void SetBrowserHtml(const CString& html);
	static CString MarkdownToHtml(const CString& markdown);
	static CString EscapeHtml(const CString& text);
	static CString FormatInline(const CString& text);

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	afx_msg void OnEnChangeEdit();
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnBnClickedOpen();
};
