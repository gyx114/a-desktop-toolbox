// MarkdownDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "MarkdownDlg.h"
#include "afxdialogex.h"
#include <MsHTML.h>
#include <ExDisp.h>

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

	if (m_btnOpen.Create(_T("Open File"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		CRect(0, 0, 80, 24), this, IDC_BTN_OPEN))
	{
		m_btnOpen.SetFont(GetFont());
	}

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

	int buttonHeight = 28;
	CRect rcBrowser;
	rcBrowser.left = m_previewMargins.left;
	rcBrowser.top = m_previewMargins.top + buttonHeight;
	rcBrowser.right = rcClient.Width() - m_previewMargins.right;
	rcBrowser.bottom = rcClient.Height() - m_previewMargins.bottom;

	if (m_browser.CreateControl(CLSID_WebBrowser, nullptr,
		WS_VISIBLE | WS_CHILD, rcBrowser, this, IDC_MARKDOWN_PREVIEW))
	{
		LPUNKNOWN pUnk = m_browser.GetControlUnknown();
		if (pUnk)
		{
			IWebBrowser2* pWeb2 = nullptr;
			if (SUCCEEDED(pUnk->QueryInterface(IID_IWebBrowser2, (void**)&pWeb2)))
			{
				BSTR bstrBlank = SysAllocString(L"about:blank");
				pWeb2->Navigate(bstrBlank, nullptr, nullptr, nullptr, nullptr);
				SysFreeString(bstrBlank);
				pWeb2->Release();
			}
		}
	}

	if (m_btnOpen.m_hWnd)
	{
		m_btnOpen.SetWindowPos(nullptr,
			m_previewMargins.left, m_previewMargins.top,
			80, 24, SWP_NOZORDER);
	}

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

void CMarkdownDlg::OnDestroy()
{
	CDialogEx::OnDestroy();
}

BOOL CMarkdownDlg::PreTranslateMessage(MSG* pMsg)
{
	if (IsDialogMessage(pMsg))
		return TRUE;
	return CDialogEx::PreTranslateMessage(pMsg);
}

void CMarkdownDlg::PostNcDestroy()
{
	CDialogEx::PostNcDestroy();
	delete this;
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
	if (path.IsEmpty())
		return;

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

	if (raw.size() >= 3 &&
		(unsigned char)raw[0] == 0xEF &&
		(unsigned char)raw[1] == 0xBB &&
		(unsigned char)raw[2] == 0xBF)
	{
		raw = raw.substr(3);
	}

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

	CWnd* pEdit = GetDlgItem(IDC_EDIT_MARKDOWN);
	if (pEdit && ::IsWindow(pEdit->m_hWnd))
	{
		pEdit->SetWindowPos(nullptr,
			m_editMargins.left, m_editMargins.top,
			rcClient.Width() - m_editMargins.left - m_editMargins.right,
			rcClient.Height() - m_editMargins.top - m_editMargins.bottom,
			SWP_NOZORDER);
	}

	if (m_btnOpen.m_hWnd && ::IsWindow(m_btnOpen.m_hWnd))
	{
		m_btnOpen.SetWindowPos(nullptr,
			m_previewMargins.left, m_previewMargins.top,
			80, 24, SWP_NOZORDER);
	}

	if (m_browser.m_hWnd && ::IsWindow(m_browser.m_hWnd))
	{
		int buttonHeight = m_btnOpen.m_hWnd ? 28 : 0;
		m_browser.SetWindowPos(nullptr,
			m_previewMargins.left, m_previewMargins.top + buttonHeight,
			rcClient.Width() - m_previewMargins.left - m_previewMargins.right,
			rcClient.Height() - m_previewMargins.top - m_previewMargins.bottom - buttonHeight,
			SWP_NOZORDER);
	}
}

void CMarkdownDlg::RefreshPreview()
{
	if (!m_browser.m_hWnd)
		return;
	SetBrowserHtml(MarkdownToHtml(m_markdownText));
}

void CMarkdownDlg::SetBrowserHtml(const CString& html)
{
	LPUNKNOWN pUnk = m_browser.GetControlUnknown();
	if (!pUnk)
		return;

	IWebBrowser2* pWeb2 = nullptr;
	if (FAILED(pUnk->QueryInterface(IID_IWebBrowser2, (void**)&pWeb2)))
		return;

	IDispatch* pDocDisp = nullptr;
	if (FAILED(pWeb2->get_Document(&pDocDisp)) || !pDocDisp)
	{
		pWeb2->Release();
		return;
	}

	IHTMLDocument2* pDoc = nullptr;
	if (FAILED(pDocDisp->QueryInterface(IID_IHTMLDocument2, (void**)&pDoc)))
	{
		pDocDisp->Release();
		pWeb2->Release();
		return;
	}

	BSTR bstrHtml = html.AllocSysString();
	SAFEARRAY* psa = SafeArrayCreateVector(VT_VARIANT, 0, 1);
	if (psa)
	{
		VARIANT* pv = nullptr;
		if (SUCCEEDED(SafeArrayAccessData(psa, (void**)&pv)))
		{
			pv->vt = VT_BSTR;
			pv->bstrVal = bstrHtml;
			SafeArrayUnaccessData(psa);
			pDoc->close();
			pDoc->write(psa);
		}
		SafeArrayDestroy(psa);
	}
	else
	{
		SysFreeString(bstrHtml);
	}

	pDoc->Release();
	pDocDisp->Release();
	pWeb2->Release();
}

CString CMarkdownDlg::EscapeHtml(const CString& text)
{
	CString out;
	out.Preallocate(text.GetLength() * 2);
	for (int i = 0; i < text.GetLength(); ++i)
	{
		TCHAR ch = text[i];
		switch (ch)
		{
		case L'&': out += _T("&amp;"); break;
		case L'<': out += _T("&lt;"); break;
		case L'>': out += _T("&gt;"); break;
		case L'\"': out += _T("&quot;"); break;
		default: out += ch; break;
		}
	}
	return out;
}

CString CMarkdownDlg::FormatInline(const CString& text)
{
	CString out;
	int i = 0;
	int n = text.GetLength();
	while (i < n)
	{
		// Link [text](url)
		if (text[i] == _T('['))
		{
			int j = text.Find(_T(']'), i + 1);
			if (j > i + 1 && j + 1 < n && text[j + 1] == _T('('))
			{
				int k = text.Find(_T(')'), j + 2);
				if (k > j + 2)
				{
					CString linkText = text.Mid(i + 1, j - i - 1);
					CString url = text.Mid(j + 2, k - j - 2);
					out += _T("<a href=\"") + EscapeHtml(url) + _T("\">") + FormatInline(linkText) + _T("</a>");
					i = k + 1;
					continue;
				}
			}
		}

		// Code `...`
		if (text[i] == _T('`'))
		{
			int j = text.Find(_T('`'), i + 1);
			if (j > i + 1)
			{
				CString code = text.Mid(i + 1, j - i - 1);
				out += _T("<code>") + EscapeHtml(code) + _T("</code>");
				i = j + 1;
				continue;
			}
		}

		// Bold **...**
		if (i + 1 < n && text[i] == _T('*') && text[i + 1] == _T('*'))
		{
			int j = i + 2;
			while (j + 1 < n && !(text[j] == _T('*') && text[j + 1] == _T('*'))) ++j;
			if (j + 1 < n)
			{
				CString bold = text.Mid(i + 2, j - i - 2);
				out += _T("<strong>") + FormatInline(bold) + _T("</strong>");
				i = j + 2;
				continue;
			}
		}

		// Strikethrough ~~...~~
		if (i + 1 < n && text[i] == _T('~') && text[i + 1] == _T('~'))
		{
			int j = text.Find(_T("~~"), i + 2);
			if (j > i + 2)
			{
				CString strike = text.Mid(i + 2, j - i - 2);
				out += _T("<del>") + FormatInline(strike) + _T("</del>");
				i = j + 2;
				continue;
			}
		}

		// Italic *...* (single)
		if (text[i] == _T('*'))
		{
			int j = text.Find(_T('*'), i + 1);
			if (j > i + 1)
			{
				CString italic = text.Mid(i + 1, j - i - 1);
				out += _T("<em>") + FormatInline(italic) + _T("</em>");
				i = j + 1;
				continue;
			}
		}

		out += EscapeHtml(text.Mid(i, 1));
		++i;
	}
	return out;
}

CString CMarkdownDlg::MarkdownToHtml(const CString& markdown)
{
	CString html;
	html += _T("<!DOCTYPE html><html><head><meta charset=\"UTF-8\">");
	html += _T("<style>");
	html += _T("body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;");
	html += _T("font-size:14px;line-height:1.6;color:#24292f;background:#ffffff;padding:16px;margin:0;}");
	html += _T("h1,h2,h3,h4,h5,h6{font-weight:600;line-height:1.25;margin-top:24px;margin-bottom:16px;}");
	html += _T("h1{font-size:2em;border-bottom:1px solid #d0d7de;padding-bottom:.3em;}");
	html += _T("h2{font-size:1.5em;border-bottom:1px solid #d0d7de;padding-bottom:.3em;}");
	html += _T("h3{font-size:1.25em;}");
	html += _T("p{margin-top:0;margin-bottom:16px;}");
	html += _T("a{color:#0969da;text-decoration:none;}a:hover{text-decoration:underline;}");
	html += _T("code{font-family:ui-monospace,SFMono-Regular,SF Mono,Menlo,Consolas,monospace;");
	html += _T("font-size:85%;background-color:rgba(175,184,193,0.2);padding:.2em .4em;border-radius:6px;}");
	html += _T("pre{background-color:#f6f8fa;border-radius:6px;padding:16px;overflow:auto;margin-bottom:16px;}");
	html += _T("pre code{background:transparent;padding:0;font-size:100%;}");
	html += _T("blockquote{color:#57606a;border-left:.25em solid #d0d7de;padding:0 1em;margin:0 0 16px 0;}");
	html += _T("blockquote p{margin-bottom:0;}");
	html += _T("ul,ol{padding-left:2em;margin-bottom:16px;}");
	html += _T("li{margin-bottom:.25em;}");
	html += _T("hr{height:.25em;padding:0;margin:24px 0;background:#d0d7de;border:0;}");
	html += _T("table{border-collapse:collapse;border-spacing:0;margin-bottom:16px;width:auto;}");
	html += _T("th,td{border:1px solid #d0d7de;padding:6px 13px;}");
	html += _T("th{background:#f6f8fa;font-weight:600;}");
	html += _T("tr:nth-child(2n){background:#f6f8fa;}");
	html += _T("del{color:#57606a;text-decoration:line-through;}");
	html += _T("</style></head><body>");

	CString body;
	bool inCodeBlock = false;
	bool inList = false;
	bool inQuote = false;
	bool inTable = false;
	bool inTHead = false;
	bool inParagraph = false;

	CStringArray lines;
	{
		int start = 0;
		while (start < markdown.GetLength())
		{
			int end = markdown.Find(_T("\n"), start);
			if (end < 0)
			{
				lines.Add(markdown.Mid(start));
				break;
			}
			CString line = markdown.Mid(start, end - start);
			line.Remove(_T('\r'));
			lines.Add(line);
			start = end + 1;
		}
	}

	auto CloseBlocks = [&](bool keepParagraph)
	{
		if (inCodeBlock) { body += _T("</code></pre>"); inCodeBlock = false; }
		if (inList) { body += _T("</ul>"); inList = false; }
		if (inQuote) { body += _T("</blockquote>"); inQuote = false; }
		if (inTable) { body += _T("</tbody></table>"); inTable = false; inTHead = false; }
		if (inParagraph && !keepParagraph) { body += _T("</p>"); inParagraph = false; }
	};

	for (int idx = 0; idx < lines.GetSize(); ++idx)
	{
		CString line = lines[idx];

		if (line.GetLength() >= 3 && line.Left(3) == _T("```"))
		{
			if (inCodeBlock)
			{
				body += _T("</code></pre>");
				inCodeBlock = false;
			}
			else
			{
				CloseBlocks(false);
				CString lang = line.Mid(3).Trim();
				body += _T("<pre><code");
				if (!lang.IsEmpty())
					body += _T(" class=\"language-") + EscapeHtml(lang) + _T("\"");
				body += _T(">");
				inCodeBlock = true;
			}
			continue;
		}

		if (inCodeBlock)
		{
			body += EscapeHtml(line);
			body += _T("\n");
			continue;
		}

		if (line.IsEmpty())
		{
			CloseBlocks(false);
			continue;
		}

		if (line == _T("---") || line == _T("***") || line == _T("___"))
			CloseBlocks(false);

		bool isList = false;
		bool isQuote = false;
		bool isTable = false;
		CString listContent;
		CString quoteContent;

		if ((line[0] == _T('-') || line[0] == _T('*') || line[0] == _T('+')) &&
			line.GetLength() > 1 && line[1] == _T(' '))
		{
			isList = true;
			listContent = line.Mid(2);
		}

		if (line[0] == _T('>'))
		{
			isQuote = true;
			if (line.GetLength() > 1 && line[1] == _T(' '))
				quoteContent = line.Mid(2);
			else
				quoteContent = line.Mid(1);
		}

		if (line.Find(_T('|')) >= 0)
		{
			isTable = true;
		}

		if (isTable)
		{
			CString trimmed = line.Trim(_T("|"));
			if (trimmed.Find(_T("---")) >= 0)
			{
				inTHead = false;
				continue;
			}

			if (!inTable)
			{
				CloseBlocks(false);
				body += _T("<table><thead>");
				inTable = true;
				inTHead = true;
			}

			CStringArray cells;
			{
				int pos = 0;
				while (pos < line.GetLength())
				{
					if (line[pos] == _T('|')) { ++pos; continue; }
					int next = line.Find(_T('|'), pos);
					CString cell = next < 0 ? line.Mid(pos) : line.Mid(pos, next - pos);
					cell.Trim();
					cells.Add(cell);
					pos = next < 0 ? line.GetLength() : next + 1;
				}
			}

			if (inTHead)
			{
				body += _T("<tr>");
				for (int i = 0; i < cells.GetSize(); ++i)
					body += _T("<th>") + FormatInline(cells[i]) + _T("</th>");
				body += _T("</tr></thead><tbody>");
				inTHead = false;
			}
			else
			{
				body += _T("<tr>");
				for (int i = 0; i < cells.GetSize(); ++i)
					body += _T("<td>") + FormatInline(cells[i]) + _T("</td>");
				body += _T("</tr>");
			}
			continue;
		}

		if (inTable) { body += _T("</tbody></table>"); inTable = false; inTHead = false; }

		if (isQuote)
		{
			if (!inQuote)
			{
				CloseBlocks(false);
				body += _T("<blockquote>");
				inQuote = true;
			}
			body += _T("<p>") + FormatInline(quoteContent) + _T("</p>");
			continue;
		}

		if (inQuote) { body += _T("</blockquote>"); inQuote = false; }

		if (isList)
		{
			if (!inList)
			{
				CloseBlocks(false);
				body += _T("<ul>");
				inList = true;
			}
			body += _T("<li>") + FormatInline(listContent) + _T("</li>");
			continue;
		}

		if (inList) { body += _T("</ul>"); inList = false; }

		if (line == _T("---") || line == _T("***") || line == _T("___"))
		{
			body += _T("<hr>");
			continue;
		}

		if (line[0] == _T('#'))
		{
			CloseBlocks(false);
			int level = 0;
			while (level < line.GetLength() && level < 6 && line[level] == _T('#'))
				++level;
			if (level > 0 && level < line.GetLength() && line[level] == _T(' '))
			{
				CString title = line.Mid(level + 1);
				CString tag;
				tag.Format(_T("<h%d>%s</h%d>"), level, FormatInline(title), level);
				body += tag;
				continue;
			}
		}

		if (!inParagraph)
		{
			body += _T("<p>");
			inParagraph = true;
		}
		else
		{
			body += _T(" ");
		}
		body += FormatInline(line);
	}

	CloseBlocks(false);

	html += body;
	html += _T("</body></html>");
	return html;
}
