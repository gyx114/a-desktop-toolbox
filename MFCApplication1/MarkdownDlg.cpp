// MarkdownDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "MarkdownDlg.h"
#include "afxdialogex.h"
#include <regex>
#include <sstream>
#include <vector>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMarkdownDlg, CDialogEx)

CMarkdownDlg::CMarkdownDlg(CWnd* pParent)
	: CDialogEx(IDD_MARKDOWN_DLG, pParent)
{
}

CMarkdownDlg::~CMarkdownDlg()
{
}

void CMarkdownDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_MARKDOWN, m_markdownText);
	DDX_Control(pDX, IDC_MARKDOWN_PREVIEW, m_wndPreview);
}

BEGIN_MESSAGE_MAP(CMarkdownDlg, CDialogEx)
	ON_WM_SIZE()
	ON_EN_CHANGE(IDC_EDIT_MARKDOWN, &CMarkdownDlg::OnEnChangeEdit)
END_MESSAGE_MAP()

BOOL CMarkdownDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Calculate margins for resize
	CRect rcClient, rcEdit, rcPreview;
	GetClientRect(&rcClient);

	CWnd* pEdit = GetDlgItem(IDC_EDIT_MARKDOWN);
	if (pEdit)
	{
		pEdit->GetWindowRect(&rcEdit);
		ScreenToClient(&rcEdit);
		m_editMargins.left = rcEdit.left;
		m_editMargins.top = rcEdit.top;
		m_editMargins.right = rcClient.right - rcEdit.right;
		m_editMargins.bottom = rcClient.bottom - rcEdit.bottom;
	}

	if (m_wndPreview.m_hWnd)
	{
		m_wndPreview.GetWindowRect(&rcPreview);
		ScreenToClient(&rcPreview);
		m_previewMargins.left = rcPreview.left;
		m_previewMargins.top = rcPreview.top;
		m_previewMargins.right = rcClient.right - rcPreview.right;
		m_previewMargins.bottom = rcClient.bottom - rcPreview.bottom;

		// Set default font for the preview
		CHARFORMAT2 cf = {};
		cf.cbSize = sizeof(CHARFORMAT2);
		cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
		cf.yHeight = 200;  // 10pt
		cf.crTextColor = RGB(51, 51, 51);
		_tcscpy_s(cf.szFaceName, _T("Segoe UI"));
		m_wndPreview.SetDefaultCharFormat(cf);

		// Set background color
		m_wndPreview.SetBackgroundColor(FALSE, RGB(255, 255, 255));

		// Enable word wrap
		m_wndPreview.SetTargetDevice(nullptr, 0);
	}

	// Load sample content
	static const wchar_t* sample = L"# Markdown Preview\r\n"
		L"\r\n"
		L"Welcome to the **Markdown Editor**!\r\n"
		L"\r\n"
		L"## Features\r\n"
		L"\r\n"
		L"- **Bold** and *italic* text\r\n"
		L"- `inline code`\r\n"
		L"- [Links](https://example.com)\r\n"
		L"- > Blockquotes\r\n"
		L"\r\n"
		L"## Code\r\n"
		L"\r\n"
		L"```cpp\r\n"
		L"#include <iostream>\r\n"
		L"int main() {\r\n"
		L"    std::cout << \"Hello!\";\r\n"
		L"    return 0;\r\n"
		L"}\r\n"
		L"```\r\n"
		L"\r\n"
		L"---\r\n"
		L"\r\n"
		L"### Table\r\n"
		L"\r\n"
		L"| Col 1 | Col 2 |\r\n"
		L"|-------|-------|\r\n"
		L"| A     | B     |\r\n"
		L"\r\n"
		L"*Start typing to preview!*";

	m_markdownText = sample;
	UpdateData(FALSE);
	UpdatePreview();

	return TRUE;
}

void CMarkdownDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType != SIZE_MINIMIZED)
		ResizeControls();
}

void CMarkdownDlg::OnEnChangeEdit()
{
	UpdateData(TRUE);
	UpdatePreview();
}

void CMarkdownDlg::ResizeControls()
{
	CRect rcClient;
	GetClientRect(&rcClient);

	CWnd* pEdit = GetDlgItem(IDC_EDIT_MARKDOWN);
	if (pEdit && ::IsWindow(pEdit->m_hWnd))
	{
		pEdit->SetWindowPos(nullptr,
			m_editMargins.left, m_editMargins.top,
			rcClient.Width() - m_editMargins.left - m_editMargins.right,
			rcClient.Height() - m_editMargins.top - m_editMargins.bottom,
			SWP_NOZORDER);
	}

	if (m_wndPreview.m_hWnd && ::IsWindow(m_wndPreview.m_hWnd))
	{
		m_wndPreview.SetWindowPos(nullptr,
			m_previewMargins.left, m_previewMargins.top,
			rcClient.Width() - m_previewMargins.left - m_previewMargins.right,
			rcClient.Height() - m_previewMargins.top - m_previewMargins.bottom,
			SWP_NOZORDER);
	}
}

// Get the end position of the RichEdit content
int CMarkdownDlg::GetRichEndPos()
{
	return m_wndPreview.GetTextLength();
}

// Append a line of text with given character format and optional paragraph format
void CMarkdownDlg::AppendRichLine(const CString& text, const CHARFORMAT2& cf,
	const PARAFORMAT2* pf)
{
	int nStart = GetRichEndPos();
	m_wndPreview.SetSel(nStart, nStart);

	// Apply paragraph format if provided
	if (pf)
	{
		PARAFORMAT2 pfCopy = *pf;
		m_wndPreview.SetParaFormat(pfCopy);
	}

	// Apply character format
	CHARFORMAT2 cfCopy = cf;
	m_wndPreview.SetSelectionCharFormat(cfCopy);

	// Insert the text with newline
	CString lineText = text + _T("\r\n");
	m_wndPreview.ReplaceSel(lineText);
}

// Render a header line with larger font
void CMarkdownDlg::RenderHeader(const CString& text, int level)
{
	CHARFORMAT2 cf = {};
	cf.cbSize = sizeof(CHARFORMAT2);
	cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BOLD;
	cf.crTextColor = RGB(30, 30, 30);
	cf.dwEffects = CFE_BOLD;
	_tcscpy_s(cf.szFaceName, _T("Segoe UI"));

	// Font sizes: h1=28pt, h2=22pt, h3=18pt, h4=16pt, h5=14pt, h6=13pt
	static const int sizes[] = { 0, 560, 440, 360, 320, 280, 260 };
	cf.yHeight = (level >= 1 && level <= 6) ? sizes[level] : 280;

	PARAFORMAT2 pf = {};
	pf.cbSize = sizeof(PARAFORMAT2);
	pf.dwMask = PFM_SPACEBEFORE | PFM_SPACEAFTER;
	pf.dySpaceBefore = (level <= 2) ? 200 : 120;
	pf.dySpaceAfter = 60;

	AppendRichLine(text, cf, &pf);
}

// Render a code block line with monospace font and gray background
void CMarkdownDlg::RenderCodeBlock(const CString& text)
{
	CHARFORMAT2 cf = {};
	cf.cbSize = sizeof(CHARFORMAT2);
	cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BACKCOLOR;
	cf.yHeight = 180;  // 9pt
	cf.crTextColor = RGB(50, 50, 50);
	cf.crBackColor = RGB(245, 245, 245);
	_tcscpy_s(cf.szFaceName, _T("Consolas"));

	PARAFORMAT2 pf = {};
	pf.cbSize = sizeof(PARAFORMAT2);
	pf.dwMask = PFM_OFFSET;
	pf.dxOffset = 300;  // indent

	AppendRichLine(text, cf, &pf);
}

// Default character format for normal text
static CHARFORMAT2 GetDefaultCf()
{
	CHARFORMAT2 cf = {};
	cf.cbSize = sizeof(CHARFORMAT2);
	cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
	cf.yHeight = 200;  // 10pt
	cf.crTextColor = RGB(51, 51, 51);
	_tcscpy_s(cf.szFaceName, _T("Segoe UI"));
	return cf;
}

// Parse a line for inline formatting markers and render as segments
void CMarkdownDlg::AppendRichSegments(const CString& line, CHARFORMAT2 baseCf)
{
	std::wstring text = line.GetString();
	int nStart = GetRichEndPos();
	m_wndPreview.SetSel(nStart, nStart);

	// Start with base format
	m_wndPreview.SetSelectionCharFormat(baseCf);

	// Process inline formatting markers using a simple state machine
	// We scan the text and emit segments with appropriate formatting
	std::wstring result;
	size_t i = 0;
	size_t len = text.length();

	// Current formatting state
	bool inBold = false;
	bool inItalic = false;
	bool inCode = false;
	bool inStrike = false;

	while (i < len)
	{
		// Check for **bold**
		if (i + 1 < len && text[i] == L'*' && text[i + 1] == L'*')
		{
			// Flush accumulated text
			if (!result.empty())
			{
				m_wndPreview.ReplaceSel(CString(result.c_str()));
				result.clear();
			}
			inBold = !inBold;
			// Update char format
			CHARFORMAT2 cf = baseCf;
			if (inBold) cf.dwEffects |= CFE_BOLD;
			if (inItalic) cf.dwEffects |= CFE_ITALIC;
			cf.dwMask |= CFM_BOLD | CFM_ITALIC;
			m_wndPreview.SetSelectionCharFormat(cf);
			i += 2;
			continue;
		}

		// Check for *italic* (single * not followed by *)
		if (text[i] == L'*' && (i + 1 >= len || text[i + 1] != L'*'))
		{
			if (!result.empty())
			{
				m_wndPreview.ReplaceSel(CString(result.c_str()));
				result.clear();
			}
			inItalic = !inItalic;
			CHARFORMAT2 cf = baseCf;
			if (inBold) cf.dwEffects |= CFE_BOLD;
			if (inItalic) cf.dwEffects |= CFE_ITALIC;
			cf.dwMask |= CFM_BOLD | CFM_ITALIC;
			m_wndPreview.SetSelectionCharFormat(cf);
			i++;
			continue;
		}

		// Check for `code`
		if (text[i] == L'`')
		{
			if (!result.empty())
			{
				m_wndPreview.ReplaceSel(CString(result.c_str()));
				result.clear();
			}
			inCode = !inCode;
			CHARFORMAT2 cf;
			cf.cbSize = sizeof(CHARFORMAT2);
			cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BACKCOLOR | CFM_BOLD | CFM_ITALIC;
			if (inCode)
			{
				cf.yHeight = 180;
				cf.crTextColor = RGB(200, 40, 40);
				cf.crBackColor = RGB(245, 245, 245);
				_tcscpy_s(cf.szFaceName, _T("Consolas"));
			}
			else
			{
				cf = baseCf;
				if (inBold) cf.dwEffects |= CFE_BOLD;
				if (inItalic) cf.dwEffects |= CFE_ITALIC;
				cf.dwMask |= CFM_BOLD | CFM_ITALIC;
			}
			m_wndPreview.SetSelectionCharFormat(cf);
			i++;
			continue;
		}

		// Check for ~~strikethrough~~
		if (i + 1 < len && text[i] == L'~' && text[i + 1] == L'~')
		{
			if (!result.empty())
			{
				m_wndPreview.ReplaceSel(CString(result.c_str()));
				result.clear();
			}
			inStrike = !inStrike;
			CHARFORMAT2 cf = baseCf;
			if (inBold) cf.dwEffects |= CFE_BOLD;
			if (inItalic) cf.dwEffects |= CFE_ITALIC;
			if (inStrike) cf.dwEffects |= CFE_STRIKEOUT;
			cf.dwMask |= CFM_BOLD | CFM_ITALIC | CFM_STRIKEOUT;
			m_wndPreview.SetSelectionCharFormat(cf);
			i += 2;
			continue;
		}

		// Check for [link text](url) - skip the URL, just show the text
		if (text[i] == L'[')
		{
			// Find the closing bracket and opening paren
			size_t closeBracket = text.find(L']', i);
			size_t openParen = (closeBracket != std::wstring::npos)
				? closeBracket + 1 : std::wstring::npos;
			if (openParen != std::wstring::npos && openParen < len && text[openParen] == L'(')
			{
				size_t closeParen = text.find(L')', openParen);
				if (closeParen != std::wstring::npos)
				{
					// Flush accumulated text
					if (!result.empty())
					{
						m_wndPreview.ReplaceSel(CString(result.c_str()));
						result.clear();
					}
					// Extract link text
					std::wstring linkText = text.substr(i + 1, closeBracket - i - 1);
					// Render as blue underlined text
					CHARFORMAT2 cf = baseCf;
					cf.dwMask = CFM_COLOR | CFM_UNDERLINE | CFM_BOLD | CFM_ITALIC;
					cf.crTextColor = RGB(3, 102, 214);
					cf.dwEffects |= CFE_UNDERLINE;
					if (inBold) cf.dwEffects |= CFE_BOLD;
					if (inItalic) cf.dwEffects |= CFE_ITALIC;
					m_wndPreview.SetSelectionCharFormat(cf);
					m_wndPreview.ReplaceSel(CString(linkText.c_str()));
					// Restore formatting
					cf = baseCf;
					if (inBold) cf.dwEffects |= CFE_BOLD;
					if (inItalic) cf.dwEffects |= CFE_ITALIC;
					cf.dwMask |= CFM_BOLD | CFM_ITALIC;
					m_wndPreview.SetSelectionCharFormat(cf);
					i = closeParen + 1;
					continue;
				}
			}
		}

		result += text[i];
		i++;
	}

	// Flush remaining text
	if (!result.empty())
	{
		m_wndPreview.ReplaceSel(CString(result.c_str()));
	}

	// Add newline
	m_wndPreview.ReplaceSel(_T("\r\n"));
}

void CMarkdownDlg::UpdatePreview()
{
	m_wndPreview.SetRedraw(FALSE);
	m_wndPreview.SetWindowText(_T(""));

	// Re-apply default formatting
	CHARFORMAT2 defCf = GetDefaultCf();
	m_wndPreview.SetDefaultCharFormat(defCf);

	std::wstringstream ss(m_markdownText.GetString());
	std::wstring line;
	bool inCodeBlock = false;

	while (std::getline(ss, line))
	{
		// Remove trailing \r
		if (!line.empty() && line.back() == L'\r')
			line.pop_back();

		// Code block toggle
		if (line.size() >= 3 && line.substr(0, 3) == L"```")
		{
			inCodeBlock = !inCodeBlock;
			continue;
		}

		// Code block content
		if (inCodeBlock)
		{
			RenderCodeBlock(CString(line.c_str()));
			continue;
		}

		// Empty line
		if (line.empty())
		{
			AppendRichLine(_T(""), defCf);
			continue;
		}

		// Horizontal rule
		if (line == L"---" || line == L"***" || line == L"___")
		{
			CHARFORMAT2 cf = defCf;
			cf.dwMask |= CFM_COLOR;
			cf.crTextColor = RGB(200, 200, 200);

			PARAFORMAT2 pf = {};
			pf.cbSize = sizeof(PARAFORMAT2);
			pf.dwMask = PFM_SPACEBEFORE | PFM_SPACEAFTER;
			pf.dySpaceBefore = 100;
			pf.dySpaceAfter = 100;

			AppendRichLine(_T("─────────────────────────────────"), cf, &pf);
			continue;
		}

		// Header
		if (line[0] == L'#')
		{
			int level = 0;
			while (level < (int)line.size() && line[level] == L'#')
				level++;
			if (level <= 6 && level < (int)line.size() && line[level] == L' ')
			{
				CString text = line.substr(level + 1).c_str();
				RenderHeader(text, level);
				continue;
			}
		}

		// Blockquote
		if (line[0] == L'>')
		{
			CString text;
			if (line.size() > 1 && line[1] == L' ')
				text = line.substr(2).c_str();
			else
				text = line.substr(1).c_str();

			CHARFORMAT2 cf = defCf;
			cf.dwMask |= CFM_COLOR | CFM_ITALIC;
			cf.crTextColor = RGB(100, 100, 100);
			cf.dwEffects |= CFE_ITALIC;

			PARAFORMAT2 pf = {};
			pf.cbSize = sizeof(PARAFORMAT2);
			pf.dwMask = PFM_OFFSET;
			pf.dxOffset = 300;

			AppendRichLine(text, cf, &pf);
			continue;
		}

		// Unordered list
		if ((line[0] == L'-' || line[0] == L'*' || line[0] == L'+')
			&& line.size() > 1 && line[1] == L' ')
		{
			CString bullet = L"  \x2022  ";  // bullet character
			CString text = bullet + CString(line.substr(2).c_str());

			PARAFORMAT2 pf = {};
			pf.cbSize = sizeof(PARAFORMAT2);
			pf.dwMask = PFM_OFFSET;
			pf.dxOffset = 200;

			AppendRichSegments(text, defCf);
			// Fix paragraph format for the just-inserted line
			int nEnd = GetRichEndPos();
			// Find start of this line
			CString allText;
			m_wndPreview.GetWindowText(allText);
			int nLineStart = allText.ReverseFind(L'\n');
			if (nLineStart < 0) nLineStart = 0;
			else nLineStart++;

			m_wndPreview.SetSel(nLineStart, nEnd);
			m_wndPreview.SetParaFormat(pf);
			continue;
		}

		// Ordered list
		if (line.size() > 2 && line[0] >= L'0' && line[0] <= L'9'
			&& line[1] == L'.' && line[2] == L' ')
		{
			size_t dotPos = line.find(L'.');
			CString num = line.substr(0, dotPos).c_str();
			CString text = CString(num) + _T("  ") + CString(line.substr(dotPos + 2).c_str());

			PARAFORMAT2 pf = {};
			pf.cbSize = sizeof(PARAFORMAT2);
			pf.dwMask = PFM_OFFSET;
			pf.dxOffset = 200;

			AppendRichSegments(text, defCf);
			// Fix paragraph format
			int nEnd = GetRichEndPos();
			CString allText;
			m_wndPreview.GetWindowText(allText);
			int nLineStart = allText.ReverseFind(L'\n');
			if (nLineStart < 0) nLineStart = 0;
			else nLineStart++;

			m_wndPreview.SetSel(nLineStart, nEnd);
			m_wndPreview.SetParaFormat(pf);
			continue;
		}

		// Table
		if (line.find(L'|') == 0 && line.find(L'|', 1) != std::wstring::npos)
		{
			// Skip separator rows
			if (line.find(L"---") != std::wstring::npos)
				continue;

			// Render as indented text with pipe separators
			CString text = L"  " + CString(line.c_str());

			CHARFORMAT2 cf = defCf;
			cf.dwMask |= CFM_SIZE;
			cf.yHeight = 180;

			PARAFORMAT2 pf = {};
			pf.cbSize = sizeof(PARAFORMAT2);
			pf.dwMask = PFM_OFFSET;
			pf.dxOffset = 200;

			AppendRichLine(text, cf, &pf);
			continue;
		}

		// Regular paragraph with inline formatting
		CString text(line.c_str());
		AppendRichSegments(text, defCf);
	}

	m_wndPreview.SetRedraw(TRUE);
	m_wndPreview.Invalidate();
}