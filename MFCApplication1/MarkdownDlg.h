// MarkdownDlg.h: header file
//

#pragma once
#include "afxdialogex.h"
#include <richedit.h>

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

	DECLARE_MESSAGE_MAP()

private:
	CString m_markdownText;
	CRichEditCtrl m_wndPreview;
	CRect m_editMargins;
	CRect m_previewMargins;

	void UpdatePreview();
	void ResizeControls();

	// RichEdit rendering helpers
	int GetRichEndPos();
	void AppendRichLine(const CString& text, const CHARFORMAT2& cf,
		const PARAFORMAT2* pf = nullptr);
	void AppendRichSegments(const CString& line, CHARFORMAT2 baseCf);
	void RenderHeader(const CString& text, int level);
	void RenderCodeBlock(const CString& text);

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnEnChangeEdit();
};