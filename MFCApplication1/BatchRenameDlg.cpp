#include "pch.h"
#include "framework.h"
#include "BatchRenameDlg.h"
#include "RegexGuideDlg.h"
#include "resource.h"
#include <algorithm>
#include <set>
#include <regex>

namespace fs = std::filesystem;

// 右键菜单命令ID
#define IDM_FILE_MARK_DELETE    32820
#define IDM_FILE_UNMARK_DELETE  32821
#define IDM_FILE_CHANGE_EXT     32822

// 简单的输入对话框类
class CInputDialog : public CDialogEx
{
public:
    CInputDialog(LPCTSTR prompt, LPCTSTR title, LPCTSTR defaultVal)
        : CDialogEx(IDD_INPUT_DLG), m_prompt(prompt), m_title(title), m_default(defaultVal) {}

    CString GetInput() const { return m_input; }

protected:
    virtual BOOL OnInitDialog() override
    {
        CDialogEx::OnInitDialog();
        SetWindowText(m_title);
        SetDlgItemText(IDC_INPUT_PROMPT, m_prompt);
        SetDlgItemText(IDC_INPUT_EDIT, m_default);
        GetDlgItem(IDC_INPUT_EDIT)->SetFocus();
        return FALSE;
    }
    virtual void DoDataExchange(CDataExchange* pDX) override
    {
        CDialogEx::DoDataExchange(pDX);
        DDX_Text(pDX, IDC_INPUT_EDIT, m_input);
    }

    void OnOK() override
    {
        UpdateData(TRUE);
        CDialogEx::OnOK();
    }

    DECLARE_MESSAGE_MAP()

    CString m_prompt, m_title, m_default, m_input;
    enum { IDD = IDD_INPUT_DLG };
};

BEGIN_MESSAGE_MAP(CInputDialog, CDialogEx)
END_MESSAGE_MAP()

CBatchRenameDlg::CBatchRenameDlg(CWnd* pParent, const CString& initialFolder)
    : CDialogEx(IDD_BATCH_RENAME_DLG, pParent)
    , m_folderPath(initialFolder)
{
}

void CBatchRenameDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CBatchRenameDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_RENAME_BROWSE, &CBatchRenameDlg::OnBnClickedBrowse)
    ON_BN_CLICKED(IDC_BTN_RENAME_PREVIEW, &CBatchRenameDlg::OnBnClickedFilePreview)
    ON_BN_CLICKED(IDC_BTN_RENAME_EXECUTE, &CBatchRenameDlg::OnBnClickedFileExecute)
    ON_BN_CLICKED(IDC_BTN_RENAME_CLEAR, &CBatchRenameDlg::OnBnClickedClear)
    ON_BN_CLICKED(IDC_BTN_RENAME_REGEX_HELP, &CBatchRenameDlg::OnBnClickedRegexHelp)
    ON_EN_CHANGE(IDC_EDIT_RENAME_PREFIX, &CBatchRenameDlg::OnEnChangeRule)
    ON_EN_CHANGE(IDC_EDIT_RENAME_SUFFIX, &CBatchRenameDlg::OnEnChangeRule)
    ON_EN_CHANGE(IDC_EDIT_RENAME_REPLACE_FROM, &CBatchRenameDlg::OnEnChangeRule)
    ON_EN_CHANGE(IDC_EDIT_RENAME_REPLACE_TO, &CBatchRenameDlg::OnEnChangeRule)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_FOLDER, &CBatchRenameDlg::OnTcnSelchangeTab)
    ON_BN_CLICKED(IDC_BTN_FOLDER_RENAME, &CBatchRenameDlg::OnBnClickedFolderRename)
    ON_BN_CLICKED(IDC_BTN_FOLDER_MOVE, &CBatchRenameDlg::OnBnClickedFolderMove)
    ON_BN_CLICKED(IDC_BTN_FOLDER_DELETE, &CBatchRenameDlg::OnBnClickedFolderDelete)
    ON_BN_CLICKED(IDC_BTN_FOLDER_SELECTALL, &CBatchRenameDlg::OnBnClickedFolderSelectAll)
    ON_BN_CLICKED(IDC_BTN_FOLDER_DESELECTALL, &CBatchRenameDlg::OnBnClickedFolderDeselectAll)
    ON_BN_CLICKED(IDC_BTN_FOLDER_DEST_BROWSE, &CBatchRenameDlg::OnBnClickedFolderDestBrowse)
    ON_BN_CLICKED(IDC_BTN_FOLDER_EXECUTE, &CBatchRenameDlg::OnBnClickedFolderExecute)
    ON_NOTIFY(NM_RCLICK, IDC_LIST_RENAME, &CBatchRenameDlg::OnFileListRightClick)
    ON_COMMAND(IDM_FILE_MARK_DELETE, &CBatchRenameDlg::OnFileMarkDelete)
    ON_COMMAND(IDM_FILE_UNMARK_DELETE, &CBatchRenameDlg::OnFileUnmarkDelete)
    ON_COMMAND(IDM_FILE_CHANGE_EXT, &CBatchRenameDlg::OnFileChangeExt)
    ON_BN_CLICKED(IDC_BTN_FILE_UNMARK_ALL, &CBatchRenameDlg::OnFileUnmarkAll)
    ON_BN_CLICKED(IDC_BTN_FILE_RESET_ALL, &CBatchRenameDlg::OnFileResetAll)
    ON_WM_DROPFILES()
END_MESSAGE_MAP()

// ========== 初始化 ==========

BOOL CBatchRenameDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    DragAcceptFiles(TRUE);

    // 初始化标签页
    m_tabCtrl.SubclassDlgItem(IDC_TAB_FOLDER, this);
    m_tabCtrl.InsertItem(0, _T("文件夹操作"));
    m_tabCtrl.InsertItem(1, _T("文件批量处理"));

    // 初始化文件列表
    CListCtrl* pFileList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (pFileList)
    {
        pFileList->ModifyStyle(0, LVS_REPORT);
        pFileList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        CRect rcList;
        pFileList->GetClientRect(&rcList);
        int totalWidth = rcList.Width() - ::GetSystemMetrics(SM_CXVSCROLL) - 4;
        pFileList->InsertColumn(0, _T("原文件名"), LVCFMT_LEFT, totalWidth * 2 / 5);
        pFileList->InsertColumn(1, _T("新文件名/状态"), LVCFMT_LEFT, totalWidth * 3 / 5 - totalWidth / 12);
    }

    // 初始化文件夹列表
    CListCtrl* pFolderList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (pFolderList)
    {
        pFolderList->ModifyStyle(0, LVS_REPORT);
        pFolderList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        CRect rcList;
        pFolderList->GetClientRect(&rcList);
        int totalWidth = rcList.Width() - ::GetSystemMetrics(SM_CXVSCROLL) - 4;
        pFolderList->InsertColumn(0, _T("文件夹名"), LVCFMT_LEFT, totalWidth);
    }

    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(FALSE);

    // 如果有初始文件夹路径，加载它
    if (!m_folderPath.IsEmpty())
    {
        SetDlgItemText(IDC_EDIT_RENAME_FOLDER, m_folderPath);
        LoadFolders();
        LoadFiles();
    }

    ShowTab(0);
    return TRUE;
}

// ========== 标签页切换 ==========

void CBatchRenameDlg::OnTcnSelchangeTab(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    ShowTab(m_tabCtrl.GetCurSel());
    *pResult = 0;
}

void CBatchRenameDlg::ShowTab(int nTab)
{
    m_nActiveTab = nTab;

    // 文件夹操作控件（含标签）
    static const int folderIDs[] = {
        IDC_LIST_FOLDERS, IDC_BTN_FOLDER_RENAME, IDC_BTN_FOLDER_MOVE,
        IDC_BTN_FOLDER_DELETE, IDC_BTN_FOLDER_SELECTALL, IDC_BTN_FOLDER_DESELECTALL,
        IDC_EDIT_FOLDER_DEST, IDC_BTN_FOLDER_DEST_BROWSE, IDC_BTN_FOLDER_EXECUTE,
        IDC_STATIC_TAB0_TARGET
    };
    constexpr int nFolderIDs = sizeof(folderIDs) / sizeof(folderIDs[0]);

    // 文件批量处理控件（含标签）
    static const int fileIDs[] = {
        IDC_LIST_RENAME,
        IDC_EDIT_RENAME_PREFIX, IDC_EDIT_RENAME_SUFFIX,
        IDC_EDIT_RENAME_REPLACE_FROM, IDC_EDIT_RENAME_REPLACE_TO,
        IDC_CHECK_RENAME_REGEX, IDC_BTN_RENAME_REGEX_HELP,
        IDC_CHECK_RENAME_NUMBER, IDC_EDIT_RENAME_START_NUM,
        IDC_CHECK_DELETE_MATCH, IDC_EDIT_DELETE_PATTERN,
        IDC_CHECK_DELETE_INVERT, IDC_CHECK_NUMBER_AFTER_EXT,
        IDC_BTN_RENAME_PREVIEW, IDC_BTN_RENAME_EXECUTE,
        IDC_BTN_FILE_UNMARK_ALL, IDC_BTN_FILE_RESET_ALL,
        IDC_STATIC_TAB1_PREFIX, IDC_STATIC_TAB1_SUFFIX,
        IDC_STATIC_TAB1_REPLACE_FROM, IDC_STATIC_TAB1_REPLACE_TO,
        IDC_STATIC_TAB1_START_NUM, IDC_STATIC_TAB1_TIP
    };
    constexpr int nFileIDs = sizeof(fileIDs) / sizeof(fileIDs[0]);

    // 隐藏所有tab相关控件
    for (int i = 0; i < nFolderIDs; i++)
    {
        CWnd* pWnd = GetDlgItem(folderIDs[i]);
        if (pWnd) pWnd->ShowWindow(nTab == 0 ? SW_SHOW : SW_HIDE);
    }
    for (int i = 0; i < nFileIDs; i++)
    {
        CWnd* pWnd = GetDlgItem(fileIDs[i]);
        if (pWnd) pWnd->ShowWindow(nTab == 1 ? SW_SHOW : SW_HIDE);
    }

    // 更新按钮状态
    if (nTab == 1)
    {
        GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(!m_entries.empty());
        GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(m_bPreviewDone);
    }
    else
    {
        GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(FALSE);
        GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
    }
}

// ========== 共享功能 ==========

void CBatchRenameDlg::SetFolderPath(const CString& path)
{
    m_folderPath = path;
    SetDlgItemText(IDC_EDIT_RENAME_FOLDER, m_folderPath);
    LoadFolders();
    LoadFiles();
}

void CBatchRenameDlg::OnBnClickedBrowse()
{
    BROWSEINFO bi = {0};
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = _T("选择文件夹");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
    if (!pidl) return;

    TCHAR path[MAX_PATH];
    if (::SHGetPathFromIDList(pidl, path))
    {
        SetFolderPath(path);
    }
    CoTaskMemFree(pidl);
}

void CBatchRenameDlg::OnBnClickedClear()
{
    m_folderPath.Empty();
    SetDlgItemText(IDC_EDIT_RENAME_FOLDER, _T(""));
    m_folders.clear();
    m_entries.clear();
    RefreshFolderList();
    RefreshFileList();

    SetDlgItemText(IDC_EDIT_RENAME_PREFIX, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_SUFFIX, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_REPLACE_FROM, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_REPLACE_TO, _T(""));
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_NUMBER))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_REGEX))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_MATCH))->SetCheck(BST_UNCHECKED);
    SetDlgItemText(IDC_EDIT_DELETE_PATTERN, _T(""));

    m_bPreviewDone = false;
    GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
}

void CBatchRenameDlg::OnDropFiles(HDROP hDrop)
{
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
        MessageBox(_T("仅支持拖入文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    SetFolderPath(path);
}

// ========== Tab 0: 文件夹操作 ==========

void CBatchRenameDlg::LoadFolders()
{
    m_folders.clear();

    if (m_folderPath.IsEmpty()) return;

    try
    {
        std::wstring wpath = static_cast<LPCWSTR>(m_folderPath);
        for (const auto& entry : fs::directory_iterator(wpath))
        {
            if (entry.is_directory())
            {
                FolderEntry fe;
                fe.name = entry.path().filename().c_str();
                fe.fullPath = entry.path();
                m_folders.push_back(fe);
            }
        }
    }
    catch (...) {}

    std::sort(m_folders.begin(), m_folders.end(),
        [](const FolderEntry& a, const FolderEntry& b) {
            return a.name.CompareNoCase(b.name) < 0;
        });

    RefreshFolderList();
}

void CBatchRenameDlg::RefreshFolderList()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;

    pList->DeleteAllItems();
    for (size_t i = 0; i < m_folders.size(); i++)
    {
        pList->InsertItem(static_cast<int>(i), m_folders[i].name);
    }
}

void CBatchRenameDlg::OnBnClickedFolderRename()
{
    m_folderOp = FolderOp::Rename;
    SetDlgItemText(IDC_EDIT_FOLDER_DEST, _T(""));
    GetDlgItem(IDC_EDIT_FOLDER_DEST)->SetWindowText(_T(""));
    SetDlgItemText(IDC_BTN_FOLDER_EXECUTE, _T("重命名"));
    MessageBox(_T("请在下方输入新的文件夹名称，然后点击【重命名】按钮。\n选中多个时，将按输入的名称依次重命名。"),
        _T("文件夹重命名"), MB_OK | MB_ICONINFORMATION);
}

void CBatchRenameDlg::OnBnClickedFolderMove()
{
    m_folderOp = FolderOp::Move;
    SetDlgItemText(IDC_EDIT_FOLDER_DEST, _T(""));
    SetDlgItemText(IDC_BTN_FOLDER_EXECUTE, _T("移动"));
    MessageBox(_T("请在下方输入目标路径，然后点击【移动】按钮。"),
        _T("文件夹移动"), MB_OK | MB_ICONINFORMATION);
}

void CBatchRenameDlg::OnBnClickedFolderDelete()
{
    m_folderOp = FolderOp::Delete;
    SetDlgItemText(IDC_EDIT_FOLDER_DEST, _T(""));
    GetDlgItem(IDC_EDIT_FOLDER_DEST)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_FOLDER_DEST_BROWSE)->EnableWindow(FALSE);
    SetDlgItemText(IDC_BTN_FOLDER_EXECUTE, _T("删除"));
    MessageBox(_T("选中要删除的文件夹，然后点击【删除】按钮。"),
        _T("文件夹删除"), MB_OK | MB_ICONINFORMATION);
}

void CBatchRenameDlg::OnBnClickedFolderSelectAll()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;
    for (int i = 0; i < pList->GetItemCount(); i++)
        pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
}

void CBatchRenameDlg::OnBnClickedFolderDeselectAll()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;
    for (int i = 0; i < pList->GetItemCount(); i++)
        pList->SetItemState(i, 0, LVIS_SELECTED);
}

void CBatchRenameDlg::OnBnClickedFolderDestBrowse()
{
    if (m_folderOp == FolderOp::Move)
    {
        BROWSEINFO bi = {0};
        bi.hwndOwner = m_hWnd;
        bi.lpszTitle = _T("选择目标文件夹");
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
        if (pidl)
        {
            TCHAR path[MAX_PATH];
            if (::SHGetPathFromIDList(pidl, path))
                SetDlgItemText(IDC_EDIT_FOLDER_DEST, path);
            CoTaskMemFree(pidl);
        }
    }
}

void CBatchRenameDlg::OnBnClickedFolderExecute()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;

    // 收集选中的文件夹
    std::vector<int> selected;
    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
            selected.push_back(i);
    }

    if (selected.empty())
    {
        MessageBox(_T("请先选中要操作的文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (m_folderOp == FolderOp::Delete)
    {
        CString msg;
        msg.Format(_T("确定要删除选中的 %d 个文件夹吗？此操作不可撤销！"), (int)selected.size());
        if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONWARNING) != IDYES)
            return;

        int success = 0, fail = 0;
        std::error_code ec;
        for (int idx : selected)
        {
            fs::remove_all(m_folders[idx].fullPath, ec);
            if (ec) fail++;
            else success++;
        }
        msg.Format(_T("删除完成：成功 %d 个，失败 %d 个。"), success, fail);
        MessageBox(msg, _T("结果"), MB_OK | MB_ICONINFORMATION);
        GetDlgItem(IDC_EDIT_FOLDER_DEST)->EnableWindow(TRUE);
        GetDlgItem(IDC_BTN_FOLDER_DEST_BROWSE)->EnableWindow(TRUE);
        LoadFolders();
        return;
    }

    CString destText;
    GetDlgItemText(IDC_EDIT_FOLDER_DEST, destText);
    destText.Trim();
    if (destText.IsEmpty())
    {
        MessageBox(_T("请输入目标名称或路径。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (m_folderOp == FolderOp::Rename)
    {
        // 检查非法字符
        CString invalid = _T("\\/:*?\"<>|");
        for (int i = 0; i < invalid.GetLength(); i++)
        {
            if (destText.Find(invalid[i]) != -1)
            {
                MessageBox(_T("文件夹名包含非法字符。"), _T("错误"), MB_OK | MB_ICONERROR);
                return;
            }
        }

        if (selected.size() == 1)
        {
            fs::path newPath = m_folders[selected[0]].fullPath.parent_path() / static_cast<LPCWSTR>(destText);
            std::error_code ec;
            fs::rename(m_folders[selected[0]].fullPath, newPath, ec);
            if (ec)
                MessageBox(_T("重命名失败。"), _T("错误"), MB_OK | MB_ICONERROR);
            else
                MessageBox(_T("重命名成功。"), _T("结果"), MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            // 多个时依次重命名
            int success = 0, fail = 0;
            for (size_t i = 0; i < selected.size(); i++)
            {
                CString newName;
                if (selected.size() > 1)
                    newName.Format(_T("%s_%d"), static_cast<LPCWSTR>(destText), (int)(i + 1));
                else
                    newName = destText;

                fs::path newPath = m_folders[selected[i]].fullPath.parent_path() / static_cast<LPCWSTR>(newName);
                std::error_code ec;
                fs::rename(m_folders[selected[i]].fullPath, newPath, ec);
                if (ec) fail++;
                else success++;
            }
            CString msg;
            msg.Format(_T("重命名完成：成功 %d 个，失败 %d 个。"), success, fail);
            MessageBox(msg, _T("结果"), MB_OK | MB_ICONINFORMATION);
        }
        LoadFolders();
    }
    else if (m_folderOp == FolderOp::Move)
    {
        DWORD attrs = ::GetFileAttributes(destText);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        {
            MessageBox(_T("目标路径不是有效的文件夹。"), _T("错误"), MB_OK | MB_ICONERROR);
            return;
        }

        int success = 0, fail = 0;
        std::error_code ec;
        for (int idx : selected)
        {
            fs::path newPath = fs::path(static_cast<LPCWSTR>(destText)) / m_folders[idx].fullPath.filename();
            fs::rename(m_folders[idx].fullPath, newPath, ec);
            if (ec) fail++;
            else success++;
        }
        CString msg;
        msg.Format(_T("移动完成：成功 %d 个，失败 %d 个。"), success, fail);
        MessageBox(msg, _T("结果"), MB_OK | MB_ICONINFORMATION);
        LoadFolders();
    }

    GetDlgItem(IDC_EDIT_FOLDER_DEST)->EnableWindow(TRUE);
    GetDlgItem(IDC_BTN_FOLDER_DEST_BROWSE)->EnableWindow(TRUE);
}

// ========== Tab 1: 文件批量处理 ==========

void CBatchRenameDlg::LoadFiles()
{
    m_entries.clear();
    m_bPreviewDone = false;

    if (m_folderPath.IsEmpty()) return;

    try
    {
        std::wstring wpath = static_cast<LPCWSTR>(m_folderPath);
        for (const auto& entry : fs::directory_iterator(wpath))
        {
            if (entry.is_regular_file())
            {
                RenameEntry re;
                re.oldName = entry.path().filename().c_str();
                re.newName = re.oldName;
                re.fullPath = entry.path();
                re.bMarkedDelete = false;
                m_entries.push_back(re);
            }
        }
    }
    catch (...) {}

    std::sort(m_entries.begin(), m_entries.end(),
        [](const RenameEntry& a, const RenameEntry& b) {
            return a.oldName.CompareNoCase(b.oldName) < 0;
        });

    GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(!m_entries.empty());
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);

    RefreshFileList();
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
    BOOL bDeleteMatch = static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_MATCH))->GetCheck() == BST_CHECKED;
    BOOL bDeleteInvert = static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_INVERT))->GetCheck() == BST_CHECKED;
    BOOL bNumberAfterExt = static_cast<CButton*>(GetDlgItem(IDC_CHECK_NUMBER_AFTER_EXT))->GetCheck() == BST_CHECKED;
    int startNum = GetDlgItemInt(IDC_EDIT_RENAME_START_NUM, nullptr, FALSE);

    CString deletePattern;
    if (bDeleteMatch)
        GetDlgItemText(IDC_EDIT_DELETE_PATTERN, deletePattern);

    for (size_t i = 0; i < m_entries.size(); i++)
    {
        // 匹配删除检查
        if (bDeleteMatch && !deletePattern.IsEmpty())
        {
            try
            {
                std::wstring wName = static_cast<LPCWSTR>(m_entries[i].oldName);
                std::wregex re(static_cast<LPCWSTR>(deletePattern));
                bool bMatched = std::regex_search(wName, re);
                // 反选：匹配时不删除，不匹配时删除
                bool bShouldDelete = bDeleteInvert ? !bMatched : bMatched;
                if (bShouldDelete)
                {
                    m_entries[i].bMarkedDelete = true;
                    m_entries[i].newName = _T("被删除");
                    continue;
                }
            }
            catch (const std::regex_error&) {}
        }

        if (m_entries[i].bMarkedDelete)
        {
            m_entries[i].newName = _T("被删除");
            continue;
        }

        CString name = m_entries[i].oldName;
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
                catch (const std::regex_error&) {}
            }
            else
            {
                stem.Replace(replaceFrom, replaceTo);
            }
        }

        CString numStr;
        if (bNumbering)
        {
            numStr.Format(_T("_%d"), startNum + static_cast<int>(i));
            if (bNumberAfterExt)
            {
                // 序号在后缀后：prefix + stem + suffix + _num + ext
                m_entries[i].newName = prefix + stem + suffix + numStr + ext;
            }
            else
            {
                // 序号在后缀前（默认）：prefix + stem + _num + suffix + ext
                m_entries[i].newName = prefix + stem + numStr + suffix + ext;
            }
        }
        else
        {
            m_entries[i].newName = prefix + stem + suffix + ext;
        }
    }
}

void CBatchRenameDlg::RefreshFileList()
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

void CBatchRenameDlg::OnBnClickedFilePreview()
{
    if (m_entries.empty()) return;

    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnBnClickedFileExecute()
{
    if (!m_bPreviewDone)
    {
        MessageBox(_T("请先点击预览，确认操作结果。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // 分离删除和重命名操作
    int deleteCount = 0, renameCount = 0, renameFail = 0, deleteFail = 0;

    // 确认删除
    for (const auto& entry : m_entries)
    {
        if (entry.bMarkedDelete) deleteCount++;
    }

    if (deleteCount > 0)
    {
        CString msg;
        msg.Format(_T("将删除 %d 个文件，重命名 %d 个文件。确定继续？"), deleteCount, (int)m_entries.size() - deleteCount);
        if (MessageBox(msg, _T("确认"), MB_YESNO | MB_ICONWARNING) != IDYES)
            return;
    }
    else
    {
        // 检查重名
        std::set<CString> newNames;
        for (const auto& entry : m_entries)
        {
            if (entry.bMarkedDelete) continue;
            if (newNames.count(entry.newName))
            {
                MessageBox(_T("存在重名的新文件名，请检查规则。"), _T("错误"), MB_OK | MB_ICONERROR);
                return;
            }
            newNames.insert(entry.newName);
        }

        if (MessageBox(_T("确定要执行重命名吗？此操作不可撤销。"), _T("确认"), MB_YESNO | MB_ICONWARNING) != IDYES)
            return;
    }

    std::error_code ec;

    // 先执行删除
    for (auto& entry : m_entries)
    {
        if (!entry.bMarkedDelete) continue;
        fs::remove(entry.fullPath, ec);
        if (ec) deleteFail++;
        else deleteCount++;
    }

    // 再执行重命名
    for (auto& entry : m_entries)
    {
        if (entry.bMarkedDelete) continue;
        if (entry.oldName == entry.newName) continue;

        fs::path newPath = entry.fullPath.parent_path() / static_cast<LPCWSTR>(entry.newName);
        fs::rename(entry.fullPath, newPath, ec);
        if (ec)
            renameFail++;
        else
        {
            renameCount++;
            entry.fullPath = newPath;
            entry.oldName = entry.newName;
        }
    }

    CString msg;
    msg.Format(_T("操作完成：删除 %d 个（失败 %d），重命名 %d 个（失败 %d）。"),
        deleteCount, deleteFail, renameCount, renameFail);
    MessageBox(msg, _T("结果"), MB_OK | MB_ICONINFORMATION);

    m_bPreviewDone = false;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
    LoadFiles();
}

void CBatchRenameDlg::OnEnChangeRule()
{
    m_bPreviewDone = false;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
}

void CBatchRenameDlg::OnBnClickedRegexHelp()
{
    CRegexGuideDlg dlg(this);
    dlg.DoModal();
}

// 右键菜单
void CBatchRenameDlg::OnFileListRightClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nSel = pList->GetSelectedCount();
    CPoint pt;
    ::GetCursorPos(&pt);

    CMenu menu;
    menu.CreatePopupMenu();
    if (nSel > 0)
    {
        menu.AppendMenu(MF_STRING, IDM_FILE_MARK_DELETE, _T("标记删除"));
        menu.AppendMenu(MF_STRING, IDM_FILE_UNMARK_DELETE, _T("取消标记"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, IDM_FILE_CHANGE_EXT, _T("修改后缀"));
    }
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    *pResult = 0;
}

void CBatchRenameDlg::OnFileMarkDelete()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                m_entries[i].bMarkedDelete = true;
                m_entries[i].newName = _T("被删除");
                pList->SetItemText(i, 1, _T("被删除"));
            }
        }
    }
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileUnmarkDelete()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                m_entries[i].bMarkedDelete = false;
                m_entries[i].newName = m_entries[i].oldName;
                pList->SetItemText(i, 1, m_entries[i].oldName);
            }
        }
    }
}

void CBatchRenameDlg::OnFileUnmarkAll()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    for (size_t i = 0; i < m_entries.size(); i++)
    {
        m_entries[i].bMarkedDelete = false;
    }

    // 自动执行预览
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileResetAll()
{
    if (MessageBox(_T("确定要清除所有改动吗？\n将重置前缀、后缀、替换、序号、删除标记。"),
        _T("确认"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    // 清除所有编辑框
    SetDlgItemText(IDC_EDIT_RENAME_PREFIX, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_SUFFIX, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_REPLACE_FROM, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_REPLACE_TO, _T(""));
    SetDlgItemText(IDC_EDIT_DELETE_PATTERN, _T(""));

    // 取消所有复选框
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_NUMBER))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_REGEX))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_MATCH))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_INVERT))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_NUMBER_AFTER_EXT))->SetCheck(BST_UNCHECKED);

    // 清除所有删除标记
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        m_entries[i].bMarkedDelete = false;
    }

    // 自动执行预览
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileChangeExt()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    // 获取选中项的当前后缀
    CString defaultExt = _T("txt");
    for (int i = 0; i < pList->GetItemCount(); i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                int dot = m_entries[i].oldName.ReverseFind(_T('.'));
                if (dot != -1)
                {
                    defaultExt = m_entries[i].oldName.Mid(dot + 1);
                    break;
                }
            }
        }
    }

    // 弹出输入对话框
    CString newExt;
    CInputDialog dlg(_T("请输入新后缀（不含点号）:"), _T("修改后缀"), defaultExt);
    if (dlg.DoModal() != IDOK) return;

    newExt = dlg.GetInput();
    newExt.Trim();
    if (newExt.IsEmpty()) return;

    // 检查非法字符
    CString invalid = _T("\\/:*?\"<>|.");
    for (int i = 0; i < invalid.GetLength(); i++)
    {
        if (newExt.Find(invalid[i]) != -1)
        {
            MessageBox(_T("后缀包含非法字符。"), _T("错误"), MB_OK | MB_ICONERROR);
            return;
        }
    }

    // 为所有选中项修改后缀
    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()) && !m_entries[i].bMarkedDelete)
            {
                CString name = m_entries[i].oldName;
                int dot = name.ReverseFind(_T('.'));
                CString stem = (dot > 0) ? name.Left(dot) : name;
                m_entries[i].newName = stem + _T(".") + newExt;
                pList->SetItemText(i, 1, m_entries[i].newName);
            }
        }
    }

    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}