// MarkdownDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "MarkdownDlg.h"
#include "afxdialogex.h"
#include <sstream>
#include <fstream>
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
}

BEGIN_MESSAGE_MAP(CMarkdownDlg, CDialogEx)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_DROPFILES()
	ON_EN_CHANGE(IDC_EDIT_MARKDOWN, &CMarkdownDlg::OnEnChangeEdit)
	ON_BN_CLICKED(IDC_BTN_OPEN, &CMarkdownDlg::OnBnClickedOpen)
END_MESSAGE_MAP()

BOOL CMarkdownDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	DragAcceptFiles(TRUE);

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

		m_fontEdit.CreatePointFont(100, _T("Consolas"));
		pEdit->SetFont(&m_fontEdit);
	}

	// Create "Open File" button
	if (m_btnOpen.Create(_T("Open File"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		CRect(0, 0, 80, 24), this, IDC_BTN_OPEN))
	{
		m_btnOpen.SetFont(GetFont());
	}

	// Get preview placeholder position, then destroy it
	CWnd* pPlaceholder = GetDlgItem(IDC_MARKDOWN_PREVIEW);
	if (pPlaceholder)
	{
		pPlaceholder->GetWindowRect(&rcPreview);
		ScreenToClient(&rcPreview);
		m_previewMargins.left = rcPreview.left;
		m_previewMargins.top = rcPreview.top;
		m_previewMargins.right = rcClient.right - rcPreview.right;
		m_previewMargins.bottom = rcClient.bottom - rcPreview.bottom;

		pPlaceholder->DestroyWindow();
	}

	// Create RichEdit for preview
	int buttonHeight = 28;
	CRect rcRich;
	rcRich.left = m_previewMargins.left;
	rcRich.top = m_previewMargins.top + buttonHeight;
	rcRich.right = rcClient.Width() - m_previewMargins.right;
	rcRich.bottom = rcClient.Height() - m_previewMargins.bottom;

	if (m_richPreview.Create(WS_CHILD | WS_VISIBLE | ES_MULTILINE |
		ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
		rcRich, this, IDC_MARKDOWN_PREVIEW))
	{
		m_fontPreview.CreatePointFont(100, _T("Segoe UI"));
		m_fontCode.CreatePointFont(90, _T("Consolas"));
		m_richPreview.SetFont(&m_fontPreview);
		m_richPreview.SetBackgroundColor(FALSE, RGB(255, 255, 255));
	}

	// Position button
	if (m_btnOpen.m_hWnd)
	{
		m_btnOpen.SetWindowPos(nullptr,
			m_previewMargins.left, m_previewMargins.top,
			80, 24, SWP_NOZORDER);
	}

	// Load sample
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
		L"- ~~Strikethrough~~\r\n"
		L"\r\n"
		L"## Code Block\r\n"
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
		L"| Feature | Status |\r\n"
		L"|---------|--------|\r\n"
		L"| Syntax Highlighting | Done |\r\n"
		L"| Tables | Done |\r\n"
		L"\r\n"
		L"*Start typing to preview!*";

	m_markdownText = sample;
	UpdateData(FALSE);

	RefreshPreview();

	return TRUE;
}

void CMarkdownDlg::PostNcDestroy()
{
	delete this;
}

void CMarkdownDlg::OnDestroy()
{
	CDialogEx::OnDestroy();
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
	RefreshPreview();
}

void CMarkdownDlg::OnDropFiles(HDROP hDropInfo)
{
	UINT nFiles = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);
	if (nFiles == 0)
		return;

	wchar_t szFile[MAX_PATH];
	DragQueryFile(hDropInfo, 0, szFile, MAX_PATH);
	DragFinish(hDropInfo);

	LoadFile(szFile);
}

void CMarkdownDlg::OnBnClickedOpen()
{
	CFileDialog dlg(TRUE, _T("*.md"), nullptr,
		OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		_T("Markdown Files (*.md)|*.md|Text Files (*.txt)|*.txt|All Files (*.*)|*.*||"),
		this);

	if (dlg.DoModal() == IDOK)
		LoadFile(dlg.GetPathName());
}

void CMarkdownDlg::LoadFile(const CString& path)
{
	CFile file;
	if (!file.Open(path, CFile::modeRead))
	{
		MessageBox(_T("Failed to open file."), _T("Error"), MB_ICONERROR);
		return;
	}

	ULONGLONG size = file.GetLength();
	if (size == 0 || size > 10 * 1024 * 1024)
	{
		file.Close();
		if (size > 10 * 1024 * 1024)
			MessageBox(_T("File too large (max 10 MB)."), _T("Error"), MB_ICONERROR);
		return;
	}

	std::string raw((size_t)size, '\0');
	file.Read(&raw[0], (UINT)size);
	file.Close();

	// Remove UTF-8 BOM if present
	if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF &&
		(unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF)
		raw = raw.substr(3);

	// Try UTF-8 first, fall back to system default (GBK on Chinese Windows)
	int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, raw.c_str(), -1, nullptr, 0);
	UINT cp = CP_UTF8;
	if (wlen == 0)
	{
		wlen = MultiByteToWideChar(CP_ACP, 0, raw.c_str(), -1, nullptr, 0);
		cp = CP_ACP;
	}

	if (wlen > 0)
	{
		wchar_t* wbuf = m_markdownText.GetBuffer(wlen);
		MultiByteToWideChar(cp, 0, raw.c_str(), -1, wbuf, wlen);
		m_markdownText.ReleaseBuffer(wlen - 1);
	}

	UpdateData(FALSE);
	RefreshPreview();

	CString title;
	title.Format(_T("Markdown Preview - %s"), PathFindFileName(path));
	SetWindowText(title);
}

void CMarkdownDlg::ResizeControls()
{
	CRect rcClient;
	GetClientRect(&rcClient);

	// Resize edit control
	CWnd* pEdit = GetDlgItem(IDC_EDIT_MARKDOWN);
	if (pEdit && ::IsWindow(pEdit->m_hWnd))
	{
		pEdit->SetWindowPos(nullptr,
			m_editMargins.left, m_editMargins.top,
			rcClient.Width() - m_editMargins.left - m_editMargins.right,
			rcClient.Height() - m_editMargins.top - m_editMargins.bottom,
			SWP_NOZORDER);
	}

	// Resize "Open File" button
	if (m_btnOpen.m_hWnd && ::IsWindow(m_btnOpen.m_hWnd))
	{
		m_btnOpen.SetWindowPos(nullptr,
			m_previewMargins.left, m_previewMargins.top,
			80, 24, SWP_NOZORDER);
	}

	// Resize RichEdit preview
	if (m_richPreview.m_hWnd && ::IsWindow(m_richPreview.m_hWnd))
	{
		int buttonHeight = m_btnOpen.m_hWnd ? 28 : 0;
		m_richPreview.SetWindowPos(nullptr,
			m_previewMargins.left, m_previewMargins.top + buttonHeight,
			rcClient.Width() - m_previewMargins.left - m_previewMargins.right,
			rcClient.Height() - m_previewMargins.top - m_previewMargins.bottom - buttonHeight,
			SWP_NOZORDER);
	}
}

void CMarkdownDlg::RefreshPreview()
{
	if (!m_richPreview.m_hWnd)
		return;
	RenderMarkdown();
}

// String-scanning helpers replacing std::regex (avoids MSVC regex crash in Release)
void CMarkdownDlg::FindPaired(const std::wstring& text, int basePos,
                              const wchar_t* marker, int markerLen,
                              FormatType type, std::vector<FormatRange>& out)
{
	size_t pos = 0;
	while (true)
	{
		size_t open = text.find(marker, pos);
		if (open == std::wstring::npos) break;
		size_t close = text.find(marker, open + markerLen);
		if (close == std::wstring::npos) break;
		out.push_back({basePos + (int)(open + markerLen), basePos + (int)close, type});
		pos = close + markerLen;
	}
}

void CMarkdownDlg::FindItalic(const std::wstring& text, int basePos, std::vector<FormatRange>& out)
{
	size_t i = 0;
	while (i < text.size())
	{
		if (text[i] == L'*')
		{
			// Skip if part of **
			if (i + 1 < text.size() && text[i + 1] == L'*') { i += 2; continue; }
			if (i > 0 && text[i - 1] == L'*') { i++; continue; }
			size_t j = i + 1;
			while (j < text.size())
			{
				if (text[j] == L'*')
				{
					if (j + 1 < text.size() && text[j + 1] == L'*') { j += 2; continue; }
					if (j > 0 && text[j - 1] == L'*') { j++; continue; }
					out.push_back({basePos + (int)(i + 1), basePos + (int)j, FMT_ITALIC});
					i = j + 1;
					break;
				}
				j++;
			}
			if (j >= text.size()) break;
		}
		else
		{
			i++;
		}
	}
}

void CMarkdownDlg::FindCode(const std::wstring& text, int basePos, std::vector<FormatRange>& out)
{
	size_t i = 0;
	while (i < text.size())
	{
		if (text[i] == L'`')
		{
			size_t j = i + 1;
			while (j < text.size() && text[j] != L'`') j++;
			if (j < text.size())
			{
				out.push_back({basePos + (int)(i + 1), basePos + (int)j, FMT_CODE});
				i = j + 1;
			}
			else
			{
				break;
			}
		}
		else
		{
			i++;
		}
	}
}

void CMarkdownDlg::FindLinks(const std::wstring& text, int basePos, std::vector<FormatRange>& out)
{
	size_t i = 0;
	while (i < text.size())
	{
		if (text[i] == L'[')
		{
			size_t closeBracket = text.find(L']', i + 1);
			if (closeBracket != std::wstring::npos && closeBracket + 1 < text.size() && text[closeBracket + 1] == L'(')
			{
				size_t closeParen = text.find(L')', closeBracket + 2);
				if (closeParen != std::wstring::npos)
				{
					out.push_back({basePos + (int)i, basePos + (int)(closeParen + 1), FMT_LINK});
					i = closeParen + 1;
					continue;
				}
			}
		}
		i++;
	}
}

// ============================================================================
// Markdown renderer using RichEdit formatting
// ============================================================================

void CMarkdownDlg::ApplyFormatting(const std::vector<FormatRange>& ranges)
{
	CRichEditCtrl& re = m_richPreview;

	for (const auto& r : ranges)
	{
		re.SetSel(r.start, r.end);

		CHARFORMAT2 cf = {};
		cf.cbSize = sizeof(CHARFORMAT2);
		cf.dwMask = CFM_BOLD | CFM_ITALIC | CFM_STRIKEOUT | CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BACKCOLOR;
		cf.dwEffects = 0;
		cf.crTextColor = RGB(0, 0, 0);
		cf.crBackColor = RGB(255, 255, 255);

		switch (r.type)
		{
		case FMT_BOLD:
			cf.dwEffects = CFE_BOLD;
			break;
		case FMT_ITALIC:
			cf.dwEffects = CFE_ITALIC;
			break;
		case FMT_STRIKE:
			cf.dwEffects = CFE_STRIKEOUT;
			cf.crTextColor = RGB(160, 160, 160);
			break;
		case FMT_CODE:
			wcscpy_s(cf.szFaceName, L"Consolas");
			cf.yHeight = 180; // 9pt
			cf.crBackColor = RGB(240, 240, 240);
			cf.crTextColor = RGB(200, 40, 40);
			break;
		case FMT_HEADER:
			cf.dwEffects = CFE_BOLD;
			if (r.headerLevel == 1)
				cf.yHeight = 360; // 18pt
			else if (r.headerLevel == 2)
				cf.yHeight = 280; // 14pt
			else if (r.headerLevel == 3)
				cf.yHeight = 240; // 12pt
			else
				cf.yHeight = 220; // 11pt
			break;
		case FMT_QUOTE:
			cf.dwEffects = CFE_ITALIC;
			cf.crTextColor = RGB(100, 100, 100);
			break;
		case FMT_LINK:
			cf.crTextColor = RGB(0, 100, 200);
			cf.dwEffects = CFE_UNDERLINE;
			break;
		}

		re.SetSelectionCharFormat(cf);
	}
}

void CMarkdownDlg::RenderMarkdown()
{
	CRichEditCtrl& re = m_richPreview;
	if (!re.m_hWnd)
		return;

	re.SetRedraw(FALSE);
	re.SetWindowText(_T(""));

	std::wstring text;
	std::vector<FormatRange> ranges;

	std::wstringstream ss(m_markdownText.GetString());
	std::wstring line;
	bool inCodeBlock = false;
	int pos = 0;

	// RichEdit uses \r (not \r\n) as paragraph delimiter internally.
	// We must use \r consistently so position calculations are correct.
#define CR L"\r"
#define CR_LEN 1

	while (std::getline(ss, line))
	{
		if (!line.empty() && line.back() == L'\r')
			line.pop_back();

		// Code block toggle
		if (line.size() >= 3 && line.substr(0, 3) == L"```")
		{
			inCodeBlock = !inCodeBlock;
			continue;
		}

		if (inCodeBlock)
		{
			text += line + CR;
			FormatRange fr;
			fr.start = pos;
			fr.end = pos + (int)line.size();
			fr.type = FMT_CODE;
			ranges.push_back(fr);
			pos += (int)line.size() + CR_LEN;
			continue;
		}

		if (line.empty())
		{
			text += CR;
			pos += CR_LEN;
			continue;
		}

		// Horizontal rule
		if (line == L"---" || line == L"***" || line == L"___")
		{
			std::wstring hr = L"\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500";
			text += hr + CR;
			FormatRange fr;
			fr.start = pos;
			fr.end = pos + (int)hr.size();
			fr.type = FMT_QUOTE;
			ranges.push_back(fr);
			pos += (int)hr.size() + CR_LEN;
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
				std::wstring headerText = line.substr(level + 1);
				text += headerText + CR;
				FormatRange fr;
				fr.start = pos;
				fr.end = pos + (int)headerText.size();
				fr.type = FMT_HEADER;
				fr.headerLevel = level;
				ranges.push_back(fr);
				pos += (int)headerText.size() + CR_LEN;
				continue;
			}
		}

		// Blockquote
		if (line[0] == L'>')
		{
			std::wstring quoteText;
			if (line.size() > 1 && line[1] == L' ')
				quoteText = L"  " + line.substr(2);
			else
				quoteText = L"  " + line.substr(1);
			text += quoteText + CR;
			FormatRange fr;
			fr.start = pos;
			fr.end = pos + (int)quoteText.size();
			fr.type = FMT_QUOTE;
			ranges.push_back(fr);
			pos += (int)quoteText.size() + CR_LEN;
			continue;
		}

		// Unordered list
		if ((line[0] == L'-' || line[0] == L'*' || line[0] == L'+') && line.size() > 1 && line[1] == L' ')
		{
			// "  \u2022 " = space(1) + space(1) + bullet(1) + space(1) = 4 chars
			std::wstring itemText = L"  \u2022 " + line.substr(2);
			text += itemText + CR;

			std::wstring content = line.substr(2);
			int basePos = pos + 4;

			FindPaired(content, basePos, L"**", 2, FMT_BOLD, ranges);
			FindItalic(content, basePos, ranges);
			FindCode(content, basePos, ranges);

			pos += (int)itemText.size() + CR_LEN;
			continue;
		}

		// Table (simplified: just show as indented text)
		if (line.find(L'|') == 0 && line.find(L'|', 1) != std::wstring::npos)
		{
			if (line.find(L"---") != std::wstring::npos)
				continue;
			std::wstring tableLine = L"  " + line;
			text += tableLine + CR;
			pos += (int)tableLine.size() + CR_LEN;
			continue;
		}

		// Regular paragraph with inline formatting
		{
			int lineStart = pos;
			text += line + CR;

			FindPaired(line, lineStart, L"**", 2, FMT_BOLD, ranges);
			FindItalic(line, lineStart, ranges);
			FindCode(line, lineStart, ranges);
			FindPaired(line, lineStart, L"~~", 2, FMT_STRIKE, ranges);
			FindLinks(line, lineStart, ranges);

			pos += (int)line.size() + CR_LEN;
		}
	}

#undef CR
#undef CR_LEN

	// Get the actual text length in RichEdit (may differ due to line ending conversion)
	re.SetWindowText(CString(text.c_str()));
	int actualLen = re.GetWindowTextLength();

	// Clamp formatting ranges to the actual text length
	for (auto& r : ranges)
	{
		if (r.start < 0) r.start = 0;
		if (r.start > actualLen) r.start = actualLen;
		if (r.end < 0) r.end = 0;
		if (r.end > actualLen) r.end = actualLen;
		if (r.start > r.end) r.start = r.end;
	}

	ApplyFormatting(ranges);

	re.SetRedraw(TRUE);
	re.Invalidate();
	re.SetSel(0, 0);
}