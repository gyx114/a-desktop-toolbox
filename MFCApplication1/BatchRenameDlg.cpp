#include "pch.h"
#include "framework.h"
#include "BatchRenameDlg.h"
#include "RegexGuideDlg.h"
#include "resource.h"
#include <algorithm>
#include <set>
#include <regex>

namespace fs = std::filesystem;

CBatchRenameDlg::CBatchRenameDlg(CWnd* pParent) : CDialogEx(IDD_BATCH_RENAME_DLG, pParent)
{
}

void CBatchRenameDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CBatchRenameDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_RENAME_BROWSE, &CBatchRenameDlg::OnBnClickedBrowse)
    ON_BN_CLICKED(IDC_BTN_RENAME_PREVIEW, &CBatchRenameDlg::OnBnClickedPreview)
    ON_BN_CLICKED(IDC_BTN_RENAME_EXECUTE, &CBatchRenameDlg::OnBnClickedExecute)
    ON_BN_CLICKED(IDC_BTN_RENAME_CLEAR, &CBatchRenameDlg::OnBnClickedClear)
    ON_BN_CLICKED(IDC_BTN_RENAME_REGEX_HELP, &CBatchRenameDlg::OnBnClickedRegexHelp)
    ON_EN_CHANGE(IDC_EDIT_RENAME_PREFIX, &CBatchRenameDlg::OnEnChangeRule)
    ON_EN_CHANGE(IDC_EDIT_RENAME_SUFFIX, &CBatchRenameDlg::OnEnChangeRule)
    ON_EN_CHANGE(IDC_EDIT_RENAME_REPLACE_FROM, &CBatchRenameDlg::OnEnChangeRule)
    ON_EN_CHANGE(IDC_EDIT_RENAME_REPLACE_TO, &CBatchRenameDlg::OnEnChangeRule)
    ON_WM_DROPFILES()
END_MESSAGE_MAP()

BOOL CBatchRenameDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    DragAcceptFiles(TRUE);

    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (pList)
    {
        pList->ModifyStyle(0, LVS_REPORT);
        pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        CRect rcList;
        pList->GetClientRect(&rcList);
        int totalWidth = rcList.Width() - ::GetSystemMetrics(SM_CXVSCROLL) - 4;
        int col0Width = totalWidth * 2 / 5;
        int col1Width = totalWidth - col0Width;

        pList->InsertColumn(0, _T("原文件名"), LVCFMT_LEFT, col0Width);
        pList->InsertColumn(1, _T("新文件名"), LVCFMT_LEFT, col1Width);
    }

    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(FALSE);

    return TRUE;
}

void CBatchRenameDlg::OnBnClickedBrowse()
{
    BROWSEINFO bi = {0};
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = _T("选择要批量重命名的文件夹");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
    if (!pidl) return;

    TCHAR path[MAX_PATH];
    if (::SHGetPathFromIDList(pidl, path))
    {
        m_folderPath = path;
        SetDlgItemText(IDC_EDIT_RENAME_FOLDER, m_folderPath);
        LoadFiles(m_folderPath);
    }

    CoTaskMemFree(pidl);
}

void CBatchRenameDlg::LoadFiles(const CString& folderPath)
{
    m_entries.clear();
    m_bPreviewDone = false;

    try
    {
        std::wstring wpath = static_cast<LPCWSTR>(folderPath);
        for (const auto& entry : fs::directory_iterator(wpath))
        {
            if (entry.is_regular_file())
            {
                RenameEntry re;
                re.oldName = entry.path().filename().c_str();
                re.newName = re.oldName;
                re.fullPath = entry.path();
                m_entries.push_back(re);
            }
        }
    }
    catch (...)
    {
        MessageBox(_T("读取文件夹失败。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    // 按文件名排序
    std::sort(m_entries.begin(), m_entries.end(),
        [](const RenameEntry& a, const RenameEntry& b) {
            return a.oldName.CompareNoCase(b.oldName) < 0;
        });

    GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(!m_entries.empty());
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);

    RefreshList();
}

void CBatchRenameDlg::ApplyRules()
{
    CString prefix, suffix, replaceFrom, replaceTo;

    GetDlgItemText(IDC_EDIT_RENAME_PREFIX, prefix);
    GetDlgItemText(IDC_EDIT_RENAME_SUFFIX, suffix);
    GetDlgItemText(IDC_EDIT_RENAME_REPLACE_FROM, replaceFrom);
    GetDlgItemText(IDC_EDIT_RENAME_REPLACE_TO, replaceTo);

    BOOL bNumbering = static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_NUMBER))->GetCheck() == BST_CHECKED;
    BOOL bRegex = static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_REGEX))->GetCheck() == BST_CHECKED;
    int startNum = GetDlgItemInt(IDC_EDIT_RENAME_START_NUM, nullptr, FALSE);

    for (size_t i = 0; i < m_entries.size(); i++)
    {
        CString name = m_entries[i].oldName;

        // 分离文件名和扩展名
        int dotPos = name.ReverseFind(_T('.'));
        CString stem, ext;
        if (dotPos > 0)
        {
            stem = name.Left(dotPos);
            ext = name.Mid(dotPos);
        }
        else
        {
            stem = name;
            ext = _T("");
        }

        // 替换
        if (!replaceFrom.IsEmpty())
        {
            if (bRegex)
            {
                try
                {
                    std::wstring wStem = static_cast<LPCWSTR>(stem);
                    std::wstring wFrom = static_cast<LPCWSTR>(replaceFrom);
                    std::wstring wTo = static_cast<LPCWSTR>(replaceTo);
                    std::wregex re(wFrom);
                    std::wstring result = std::regex_replace(wStem, re, wTo);
                    stem = result.c_str();
                }
                catch (const std::regex_error&)
                {
                    // 正则表达式无效，保持原样
                }
            }
            else
            {
                stem.Replace(replaceFrom, replaceTo);
            }
        }

        // 序号
        CString numStr;
        if (bNumbering)
        {
            numStr.Format(_T("_%d"), startNum + static_cast<int>(i));
        }

        // 组合
        m_entries[i].newName = prefix + stem + numStr + suffix + ext;
    }
}

void CBatchRenameDlg::RefreshList()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    pList->DeleteAllItems();
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        int idx = pList->InsertItem(static_cast<int>(i), m_entries[i].oldName);
        pList->SetItemText(idx, 1, m_entries[i].newName);
    }
}

void CBatchRenameDlg::OnBnClickedPreview()
{
    if (m_entries.empty()) return;

    ApplyRules();
    RefreshList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnBnClickedExecute()
{
    if (!m_bPreviewDone)
    {
        MessageBox(_T("请先点击预览，确认重命名结果。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // 检查是否有重名
    std::set<CString> newNames;
    for (const auto& entry : m_entries)
    {
        if (newNames.count(entry.newName))
        {
            MessageBox(_T("存在重名的新文件名，请检查规则。"), _T("错误"), MB_OK | MB_ICONERROR);
            return;
        }
        newNames.insert(entry.newName);
    }

    if (MessageBox(_T("确定要执行重命名吗？此操作不可撤销。"), _T("确认"), MB_YESNO | MB_ICONWARNING) != IDYES)
        return;

    int successCount = 0;
    int failCount = 0;
    std::error_code ec;

    for (auto& entry : m_entries)
    {
        if (entry.oldName == entry.newName) continue;

        fs::path newPath = entry.fullPath.parent_path() / static_cast<LPCWSTR>(entry.newName);
        fs::rename(entry.fullPath, newPath, ec);
        if (ec)
        {
            failCount++;
        }
        else
        {
            successCount++;
            entry.fullPath = newPath;
            entry.oldName = entry.newName;
        }
    }

    CString msg;
    msg.Format(_T("重命名完成：成功 %d 个，失败 %d 个。"), successCount, failCount);
    MessageBox(msg, _T("结果"), MB_OK | MB_ICONINFORMATION);

    m_bPreviewDone = false;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
}

void CBatchRenameDlg::OnEnChangeRule()
{
    m_bPreviewDone = false;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
}

void CBatchRenameDlg::OnDropFiles(HDROP hDrop)
{
    // 只接受第一个拖入项
    UINT nFiles = ::DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
    if (nFiles == 0)
    {
        ::DragFinish(hDrop);
        return;
    }

    TCHAR path[MAX_PATH];
    ::DragQueryFile(hDrop, 0, path, MAX_PATH);
    ::DragFinish(hDrop);

    // 检查是否为文件夹
    DWORD attrs = ::GetFileAttributes(path);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        MessageBox(_T("仅支持拖入文件夹，请拖入文件夹而不是文件。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // 加载文件夹
    m_folderPath = path;
    SetDlgItemText(IDC_EDIT_RENAME_FOLDER, m_folderPath);
    LoadFiles(m_folderPath);
}

void CBatchRenameDlg::OnBnClickedClear()
{
    SetDlgItemText(IDC_EDIT_RENAME_PREFIX, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_SUFFIX, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_REPLACE_FROM, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_REPLACE_TO, _T(""));
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_NUMBER))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_REGEX))->SetCheck(BST_UNCHECKED);

    m_bPreviewDone = false;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
}

void CBatchRenameDlg::OnBnClickedRegexHelp()
{
    CRegexGuideDlg dlg(this);
    dlg.DoModal();
}