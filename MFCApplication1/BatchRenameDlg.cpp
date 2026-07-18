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
#define IDM_FILE_RESTORE_EXT    32823
#define IDM_FOLDER_RENAME       32824
#define IDM_FOLDER_MOVE         32825
#define IDM_FOLDER_DELETE       32826
#define IDM_FOLDER_EXPLORE      32827
#define IDM_FILE_EXPLORE        32828
#define IDM_FILE_IGNORE         32829
#define IDM_FILE_UNIGNORE       32830

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
    ON_BN_CLICKED(IDC_BTN_RENAME_REFRESH, &CBatchRenameDlg::OnBnClickedRefresh)
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
    ON_COMMAND(IDC_BTN_FOLDER_SELECTALL, &CBatchRenameDlg::OnBnClickedFolderSelectAll)
    ON_COMMAND(IDC_BTN_FOLDER_DESELECTALL, &CBatchRenameDlg::OnBnClickedFolderDeselectAll)
    ON_BN_CLICKED(IDC_BTN_CURRENT_RENAME, &CBatchRenameDlg::OnBnClickedCurrentRename)
    ON_BN_CLICKED(IDC_BTN_CURRENT_MOVE, &CBatchRenameDlg::OnBnClickedCurrentMove)
    ON_BN_CLICKED(IDC_BTN_CURRENT_DELETE, &CBatchRenameDlg::OnBnClickedCurrentDelete)
    ON_NOTIFY(NM_RCLICK, IDC_LIST_FOLDERS, &CBatchRenameDlg::OnFolderListRightClick)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FOLDERS, &CBatchRenameDlg::OnFolderListCheckChanged)
    ON_COMMAND(IDM_FOLDER_RENAME, &CBatchRenameDlg::OnBnClickedFolderRename)
    ON_COMMAND(IDM_FOLDER_MOVE, &CBatchRenameDlg::OnBnClickedFolderMove)
    ON_COMMAND(IDM_FOLDER_DELETE, &CBatchRenameDlg::OnBnClickedFolderDelete)
    ON_COMMAND(IDM_FOLDER_EXPLORE, &CBatchRenameDlg::OnFolderExplore)
    ON_COMMAND(IDM_FILE_EXPLORE, &CBatchRenameDlg::OnFileExplore)
    ON_COMMAND(IDM_FILE_IGNORE, &CBatchRenameDlg::OnFileIgnore)
    ON_COMMAND(IDM_FILE_UNIGNORE, &CBatchRenameDlg::OnFileUnignore)
    ON_NOTIFY(NM_RCLICK, IDC_LIST_RENAME, &CBatchRenameDlg::OnFileListRightClick)
    ON_COMMAND(IDM_FILE_MARK_DELETE, &CBatchRenameDlg::OnFileMarkDelete)
    ON_COMMAND(IDM_FILE_UNMARK_DELETE, &CBatchRenameDlg::OnFileUnmarkDelete)
    ON_COMMAND(IDM_FILE_CHANGE_EXT, &CBatchRenameDlg::OnFileChangeExt)
    ON_COMMAND(IDM_FILE_RESTORE_EXT, &CBatchRenameDlg::OnFileRestoreExt)
    ON_BN_CLICKED(IDC_BTN_FILE_UNMARK_ALL, &CBatchRenameDlg::OnFileUnmarkAll)
    ON_BN_CLICKED(IDC_BTN_FILE_RESET_ALL, &CBatchRenameDlg::OnFileResetAll)
    ON_BN_CLICKED(IDC_CHECK_DELETE_INVERT, &CBatchRenameDlg::OnCheckDeleteInvert)
    ON_BN_CLICKED(IDC_BTN_IGNORE_CLEAR, &CBatchRenameDlg::OnBnClickedIgnoreClear)
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
        pFolderList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
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
        IDC_BTN_CURRENT_RENAME, IDC_BTN_CURRENT_MOVE, IDC_BTN_CURRENT_DELETE,
        IDC_STATIC_CURRENT_DIR, IDC_STATIC_SUBDIR, IDC_STATIC_FOLDER_SELCOUNT
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
        IDC_STATIC_TAB1_START_NUM, IDC_STATIC_TAB1_TIP,
        IDC_CHECK_IGNORE_EXT, IDC_EDIT_IGNORE_EXT,
        IDC_CHECK_IGNORE_MATCH, IDC_EDIT_IGNORE_PATTERN,
        IDC_CHECK_IGNORE_REGEX, IDC_BTN_IGNORE_CLEAR,
        IDC_STATIC_IGNORE_GROUP
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

BOOL CBatchRenameDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_F5)
    {
        OnBnClickedRefresh();
        return TRUE;
    }
    return CDialogEx::PreTranslateMessage(pMsg);
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

void CBatchRenameDlg::OnBnClickedRefresh()
{
    if (m_folderPath.IsEmpty()) return;
    LoadFolders();
    LoadFiles();
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
    UpdateFolderSelectionCount();
}

std::vector<int> CBatchRenameDlg::GetCheckedFolders()
{
    std::vector<int> checked;
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return checked;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (ListView_GetCheckState(pList->m_hWnd, i))
            checked.push_back(i);
    }
    return checked;
}

void CBatchRenameDlg::UpdateFolderSelectionCount()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    int nChecked = 0;
    for (int i = 0; i < nCount; i++)
    {
        if (ListView_GetCheckState(pList->m_hWnd, i))
            nChecked++;
    }

    CString text;
    text.Format(_T("已选中: %d/%d"), nChecked, nCount);
    SetDlgItemText(IDC_STATIC_FOLDER_SELCOUNT, text);
}

void CBatchRenameDlg::OnBnClickedFolderRename()
{
    std::vector<int> selected = GetCheckedFolders();
    if (selected.empty())
    {
        MessageBox(_T("请先勾选要重命名的文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // 弹出输入对话框
    CString defaultName = m_folders[selected[0]].name;
    CInputDialog dlg(_T("请输入新的文件夹名称:"), _T("重命名文件夹"), defaultName);
    if (dlg.DoModal() != IDOK) return;

    CString newName = dlg.GetInput();
    newName.Trim();
    if (newName.IsEmpty()) return;

    // 检查非法字符
    CString invalid = _T("\\/:*?\"<>|");
    for (int i = 0; i < invalid.GetLength(); i++)
    {
        if (newName.Find(invalid[i]) != -1)
        {
            MessageBox(_T("文件夹名包含非法字符。"), _T("错误"), MB_OK | MB_ICONERROR);
            return;
        }
    }

    if (selected.size() == 1)
    {
        fs::path newPath = m_folders[selected[0]].fullPath.parent_path() / static_cast<LPCWSTR>(newName);
        std::error_code ec;
        fs::rename(m_folders[selected[0]].fullPath, newPath, ec);
        if (ec)
            MessageBox(_T("重命名失败。"), _T("错误"), MB_OK | MB_ICONERROR);
        else
            MessageBox(_T("重命名成功。"), _T("结果"), MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        int success = 0, fail = 0;
        for (size_t i = 0; i < selected.size(); i++)
        {
            CString numberedName;
            numberedName.Format(_T("%s_%d"), static_cast<LPCWSTR>(newName), (int)(i + 1));
            fs::path newPath = m_folders[selected[i]].fullPath.parent_path() / static_cast<LPCWSTR>(numberedName);
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

void CBatchRenameDlg::OnBnClickedFolderMove()
{
    std::vector<int> selected = GetCheckedFolders();
    if (selected.empty())
    {
        MessageBox(_T("请先勾选要移动的文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // 弹出浏览对话框选择目标路径
    BROWSEINFO bi = {0};
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = _T("选择目标文件夹");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
    if (!pidl) return;

    TCHAR destPath[MAX_PATH];
    if (!::SHGetPathFromIDList(pidl, destPath))
    {
        CoTaskMemFree(pidl);
        return;
    }
    CoTaskMemFree(pidl);

    int success = 0, fail = 0;
    std::error_code ec;
    for (int idx : selected)
    {
        fs::path newPath = fs::path(destPath) / m_folders[idx].fullPath.filename();
        fs::rename(m_folders[idx].fullPath, newPath, ec);
        if (ec) fail++;
        else success++;
    }
    CString msg;
    msg.Format(_T("移动完成：成功 %d 个，失败 %d 个。"), success, fail);
    MessageBox(msg, _T("结果"), MB_OK | MB_ICONINFORMATION);

    LoadFolders();
}

void CBatchRenameDlg::OnBnClickedFolderDelete()
{
    std::vector<int> selected = GetCheckedFolders();
    if (selected.empty())
    {
        MessageBox(_T("请先勾选要删除的文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

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

    LoadFolders();
}

void CBatchRenameDlg::OnBnClickedCurrentRename()
{
    if (m_folderPath.IsEmpty())
    {
        MessageBox(_T("请先选择文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    fs::path curPath(static_cast<LPCWSTR>(m_folderPath));
    CString curName = curPath.filename().c_str();

    CInputDialog dlg(_T("请输入新的目录名称:"), _T("重命名当前目录"), curName);
    if (dlg.DoModal() != IDOK) return;

    CString newName = dlg.GetInput();
    newName.Trim();
    if (newName.IsEmpty() || newName == curName) return;

    CString invalid = _T("\\/:*?\"<>|");
    for (int i = 0; i < invalid.GetLength(); i++)
    {
        if (newName.Find(invalid[i]) != -1)
        {
            MessageBox(_T("目录名包含非法字符。"), _T("错误"), MB_OK | MB_ICONERROR);
            return;
        }
    }

    fs::path newPath = curPath.parent_path() / static_cast<LPCWSTR>(newName);
    std::error_code ec;
    fs::rename(curPath, newPath, ec);
    if (ec)
    {
        MessageBox(_T("重命名失败。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    SetFolderPath(newPath.c_str());
    MessageBox(_T("重命名成功。"), _T("结果"), MB_OK | MB_ICONINFORMATION);
}

void CBatchRenameDlg::OnBnClickedCurrentMove()
{
    if (m_folderPath.IsEmpty())
    {
        MessageBox(_T("请先选择文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    BROWSEINFO bi = {0};
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = _T("选择目标文件夹");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
    if (!pidl) return;

    TCHAR destPath[MAX_PATH];
    if (!::SHGetPathFromIDList(pidl, destPath))
    {
        CoTaskMemFree(pidl);
        return;
    }
    CoTaskMemFree(pidl);

    fs::path curPath(static_cast<LPCWSTR>(m_folderPath));
    fs::path newPath = fs::path(destPath) / curPath.filename();

    CString msg;
    msg.Format(_T("确定将 \"%s\" 移动到 \"%s\" 吗？"), curPath.filename().c_str(), destPath);
    if (MessageBox(msg, _T("确认移动"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    std::error_code ec;
    fs::rename(curPath, newPath, ec);
    if (ec)
    {
        MessageBox(_T("移动失败。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    SetFolderPath(newPath.c_str());
    MessageBox(_T("移动成功。"), _T("结果"), MB_OK | MB_ICONINFORMATION);
}

void CBatchRenameDlg::OnBnClickedCurrentDelete()
{
    if (m_folderPath.IsEmpty())
    {
        MessageBox(_T("请先选择文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    fs::path curPath(static_cast<LPCWSTR>(m_folderPath));
    CString msg;
    msg.Format(_T("确定要删除当前目录 \"%s\" 及其所有内容吗？\n此操作不可撤销！"), curPath.filename().c_str());
    if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONWARNING) != IDYES)
        return;

    std::error_code ec;
    fs::remove_all(curPath, ec);
    if (ec)
    {
        MessageBox(_T("删除失败。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    // 清除当前路径
    m_folderPath.Empty();
    SetDlgItemText(IDC_EDIT_RENAME_FOLDER, _T(""));
    m_folders.clear();
    m_entries.clear();
    RefreshFolderList();
    RefreshFileList();

    MessageBox(_T("删除成功。"), _T("结果"), MB_OK | MB_ICONINFORMATION);
}

void CBatchRenameDlg::OnBnClickedFolderSelectAll()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;
    for (int i = 0; i < pList->GetItemCount(); i++)
        ListView_SetCheckState(pList->m_hWnd, i, TRUE);
    UpdateFolderSelectionCount();
}

void CBatchRenameDlg::OnBnClickedFolderDeselectAll()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;
    for (int i = 0; i < pList->GetItemCount(); i++)
        ListView_SetCheckState(pList->m_hWnd, i, FALSE);
    UpdateFolderSelectionCount();
}

void CBatchRenameDlg::OnFolderListRightClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;

    int nChecked = static_cast<int>(GetCheckedFolders().size());
    CPoint pt;
    ::GetCursorPos(&pt);

    CMenu menu;
    menu.CreatePopupMenu();
    if (nChecked > 0)
    {
        menu.AppendMenu(MF_STRING, IDM_FOLDER_RENAME, _T("重命名"));
        menu.AppendMenu(MF_STRING, IDM_FOLDER_MOVE, _T("移动"));
        menu.AppendMenu(MF_STRING, IDM_FOLDER_DELETE, _T("删除"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, IDM_FOLDER_EXPLORE, _T("在资源管理器中定位"));
    }
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, IDC_BTN_FOLDER_SELECTALL, _T("全选"));
    menu.AppendMenu(MF_STRING, IDC_BTN_FOLDER_DESELECTALL, _T("取消全选"));
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    *pResult = 0;
}

void CBatchRenameDlg::OnFolderListCheckChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    // 将列表选中状态同步到复选框：当用户点击/Shift+点击/拖拽选中时，自动勾选
    NMLISTVIEW* pNMLV = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    if ((pNMLV->uChanged & LVIF_STATE) && pNMLV->iItem >= 0)
    {
        BOOL bWasSelected = (pNMLV->uOldState & LVIS_SELECTED) != 0;
        BOOL bIsSelected = (pNMLV->uNewState & LVIS_SELECTED) != 0;
        if (bWasSelected != bIsSelected)
        {
            CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
            if (pList)
            {
                BOOL bChecked = ListView_GetCheckState(pList->m_hWnd, pNMLV->iItem);
                if (bIsSelected != bChecked)
                    ListView_SetCheckState(pList->m_hWnd, pNMLV->iItem, bIsSelected);
            }
        }
    }
    UpdateFolderSelectionCount();
    *pResult = 0;
}

void CBatchRenameDlg::OnFolderExplore()
{
    std::vector<int> selected = GetCheckedFolders();
    if (selected.empty()) return;

    CString path = m_folders[selected[0]].fullPath.c_str();
    CString cmd;
    cmd.Format(_T("/select,\"%s\""), static_cast<LPCWSTR>(path));
    ShellExecute(nullptr, _T("open"), _T("explorer"), cmd, nullptr, SW_SHOW);
}

void CBatchRenameDlg::OnFileExplore()
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
                CString path = m_entries[i].fullPath.c_str();
                CString cmd;
                cmd.Format(_T("/select,\"%s\""), static_cast<LPCWSTR>(path));
                ShellExecute(nullptr, _T("open"), _T("explorer"), cmd, nullptr, SW_SHOW);
                return;
            }
        }
    }
}

void CBatchRenameDlg::OnFileIgnore()
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
                m_manualIgnored.insert(static_cast<size_t>(i));
                m_entries[i].bIgnored = true;
                m_entries[i].bMarkedDelete = false;
            }
        }
    }
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileUnignore()
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
                m_manualIgnored.erase(static_cast<size_t>(i));
            }
        }
    }
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnBnClickedIgnoreClear()
{
    if (m_manualIgnored.empty()) return;
    m_manualIgnored.clear();
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

// ========== Tab 1: 文件批量处理 ==========

void CBatchRenameDlg::LoadFiles()
{
    m_entries.clear();
    m_manualIgnored.clear();
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

void CBatchRenameDlg::ApplyIgnoreRules()
{
    BOOL bIgnoreExt = static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_EXT))->GetCheck() == BST_CHECKED;
    BOOL bIgnoreMatch = static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_MATCH))->GetCheck() == BST_CHECKED;
    BOOL bIgnoreRegex = static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_REGEX))->GetCheck() == BST_CHECKED;

    CString ignoreExtStr, ignorePattern;
    if (bIgnoreExt) GetDlgItemText(IDC_EDIT_IGNORE_EXT, ignoreExtStr);
    if (bIgnoreMatch) GetDlgItemText(IDC_EDIT_IGNORE_PATTERN, ignorePattern);

    for (size_t i = 0; i < m_entries.size(); i++)
    {
        // 手动忽略优先
        if (m_manualIgnored.count(i) > 0)
        {
            m_entries[i].bIgnored = true;
            m_entries[i].bMarkedDelete = false;
            continue;
        }

        m_entries[i].bIgnored = false;

        // 按后缀忽略
        if (bIgnoreExt && !ignoreExtStr.IsEmpty())
        {
            CString ext = PathFindExtension(m_entries[i].oldName);
            ext.MakeLower();
            CString extList = ignoreExtStr;
            extList.MakeLower();

            int start = 0;
            while (start < extList.GetLength())
            {
                CString token = extList.Tokenize(_T(";"), start);
                token.Trim();
                if (!token.IsEmpty())
                {
                    if (token[0] != _T('.'))
                        token = _T('.') + token;
                    if (ext == token)
                    {
                        m_entries[i].bIgnored = true;
                        m_entries[i].bMarkedDelete = false;
                        break;
                    }
                }
            }
        }

        if (m_entries[i].bIgnored) continue;

        // 按匹配忽略
        if (bIgnoreMatch && !ignorePattern.IsEmpty())
        {
            try
            {
                std::wstring wName = static_cast<LPCWSTR>(m_entries[i].oldName);
                if (bIgnoreRegex)
                {
                    std::wregex re(static_cast<LPCWSTR>(ignorePattern));
                    if (std::regex_search(wName, re))
                    {
                        m_entries[i].bIgnored = true;
                        m_entries[i].bMarkedDelete = false;
                    }
                }
                else
                {
                    if (wName.find(static_cast<LPCWSTR>(ignorePattern)) != std::wstring::npos)
                    {
                        m_entries[i].bIgnored = true;
                        m_entries[i].bMarkedDelete = false;
                    }
                }
            }
            catch (const std::regex_error&) {}
        }
    }
}

void CBatchRenameDlg::ApplyRules()
{
    // 先应用忽略规则
    ApplyIgnoreRules();

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

    // 计算非忽略、非删除的文件数量，用于序号位数
    int activeCount = 0;
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (!m_entries[i].bIgnored && !m_entries[i].bMarkedDelete)
            activeCount++;
    }

    int numDigits = 1;
    if (activeCount > 0)
    {
        int temp = activeCount + startNum - 1;
        while (temp >= 10) { numDigits++; temp /= 10; }
    }

    int numIdx = 0;

    for (size_t i = 0; i < m_entries.size(); i++)
    {
        // 被忽略的文件：不参与任何操作
        if (m_entries[i].bIgnored)
        {
            m_entries[i].newName = m_entries[i].oldName;
            m_entries[i].bMarkedDelete = false;
            continue;
        }

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
        if (m_entries[i].bCustomExt)
        {
            // 使用用户自定义的后缀
            if (dotPos > 0)
                stem = name.Left(dotPos);
            else
                stem = name;
            ext = m_entries[i].customExt;
        }
        else if (dotPos > 0)
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
            numIdx++;
            numStr.Format(_T("_%0*d"), numDigits, startNum + numIdx - 1);
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
        CString status;
        if (m_entries[i].bIgnored)
            status = _T("已忽略");
        else
            status = m_entries[i].newName;
        pList->SetItemText(idx, 1, status);
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
        if (!entry.bIgnored && entry.bMarkedDelete) deleteCount++;
    }

    if (deleteCount > 0)
    {
        CString msg;
        int activeCount = 0;
        for (const auto& entry : m_entries)
        {
            if (!entry.bIgnored && !entry.bMarkedDelete) activeCount++;
        }
        msg.Format(_T("将删除 %d 个文件，重命名 %d 个文件。确定继续？"), deleteCount, activeCount);
        if (MessageBox(msg, _T("确认"), MB_YESNO | MB_ICONWARNING) != IDYES)
            return;
    }
    else
    {
        // 检查重名
        std::set<CString> newNames;
        for (const auto& entry : m_entries)
        {
            if (entry.bIgnored || entry.bMarkedDelete) continue;
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
    int actualDeleted = 0;
    for (auto& entry : m_entries)
    {
        if (entry.bIgnored || !entry.bMarkedDelete) continue;
        fs::remove(entry.fullPath, ec);
        if (ec) deleteFail++;
        else actualDeleted++;
    }

    // 再执行重命名
    for (auto& entry : m_entries)
    {
        if (entry.bIgnored || entry.bMarkedDelete) continue;
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
        actualDeleted, deleteFail, renameCount, renameFail);
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
        menu.AppendMenu(MF_STRING, IDM_FILE_IGNORE, _T("忽略"));
        menu.AppendMenu(MF_STRING, IDM_FILE_UNIGNORE, _T("取消忽略"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, IDM_FILE_MARK_DELETE, _T("标记删除"));
        menu.AppendMenu(MF_STRING, IDM_FILE_UNMARK_DELETE, _T("取消标记"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, IDM_FILE_CHANGE_EXT, _T("修改后缀"));
        menu.AppendMenu(MF_STRING, IDM_FILE_RESTORE_EXT, _T("恢复原后缀名"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, IDM_FILE_EXPLORE, _T("在资源管理器中定位"));
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

void CBatchRenameDlg::OnCheckDeleteInvert()
{
    BOOL bChecked = static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_INVERT))->GetCheck();
    if (bChecked == BST_CHECKED)
    {
        static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_MATCH))->SetCheck(BST_CHECKED);
    }
}

void CBatchRenameDlg::OnFileUnmarkAll()
{
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
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_EXT))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_MATCH))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_REGEX))->SetCheck(BST_UNCHECKED);
    SetDlgItemText(IDC_EDIT_IGNORE_EXT, _T(""));
    SetDlgItemText(IDC_EDIT_IGNORE_PATTERN, _T(""));

    // 清除所有删除标记和忽略
    m_manualIgnored.clear();
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        m_entries[i].bMarkedDelete = false;
        m_entries[i].bCustomExt = false;
        m_entries[i].customExt.Empty();
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
                m_entries[i].customExt = _T(".") + newExt;
                m_entries[i].bCustomExt = true;
            }
        }
    }

    // 自动预览以显示完整结果
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileRestoreExt()
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
                m_entries[i].bCustomExt = false;
                m_entries[i].customExt.Empty();
            }
        }
    }

    // 自动预览以恢复原始后缀
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnCancel()
{
    DestroyWindow();
}

void CBatchRenameDlg::PostNcDestroy()
{
    delete this;
}