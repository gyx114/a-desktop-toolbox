// MarkdownDlg.h: header file
//

#pragma once
#include "afxdialogex.h"
#include "afxcmn.h"

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
	virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()

private:
	enum FormatType { FMT_BOLD, FMT_ITALIC, FMT_CODE, FMT_STRIKE, FMT_HEADER, FMT_QUOTE, FMT_LINK };

	struct FormatRange {
		int start;
		int end;
		FormatType type;
		int headerLevel = 0; // 1-6 for headers
	};

	CString m_markdownText;
	CFont m_fontEdit;
	CFont m_fontPreview;
	CFont m_fontCode;
	CButton m_btnOpen;
	CRichEditCtrl m_richPreview;
	CRect m_editMargins;
	CRect m_previewMargins;

	void ResizeControls();
	void RefreshPreview();
	void LoadFile(const CString& path);
	void RenderMarkdown();
	void ApplyFormatting(const std::vector<FormatRange>& ranges);

	// Inline format scanners (replace std::regex to avoid Release crash)
	static void FindPaired(const std::wstring& text, int basePos,
	                       const wchar_t* marker, int markerLen,
	                       FormatType type, std::vector<FormatRange>& out);
	static void FindItalic(const std::wstring& text, int basePos, std::vector<FormatRange>& out);
	static void FindCode(const std::wstring& text, int basePos, std::vector<FormatRange>& out);
	static void FindLinks(const std::wstring& text, int basePos, std::vector<FormatRange>& out);

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	afx_msg void OnEnChangeEdit();
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnBnClickedOpen();
};