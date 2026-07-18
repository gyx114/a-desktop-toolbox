#pragma once
#include "afxdialogex.h"
#include "resource.h"
#include <vector>
#include <filesystem>

struct RenameEntry {
    CString oldName;
    CString newName;
    std::filesystem::path fullPath;
    bool bMarkedDelete{false};
};

struct FolderEntry {
    CString name;
    std::filesystem::path fullPath;
};

class CBatchRenameDlg : public CDialogEx
{
public:
    CBatchRenameDlg(CWnd* pParent = nullptr, const CString& initialFolder = _T(""));
    enum { IDD = IDD_BATCH_RENAME_DLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    DECLARE_MESSAGE_MAP()

private:
    // Tab control
    CTabCtrl m_tabCtrl;
    afx_msg void OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult);
    void ShowTab(int nTab);

    // Shared
    CString m_folderPath;
    afx_msg void OnBnClickedBrowse();
    afx_msg void OnBnClickedClear();
    afx_msg void OnDropFiles(HDROP hDrop);
    void SetFolderPath(const CString& path);

    // Tab 0: 文件夹操作
    void LoadFolders();
    void RefreshFolderList();
    afx_msg void OnBnClickedFolderRename();
    afx_msg void OnBnClickedFolderMove();
    afx_msg void OnBnClickedFolderDelete();
    afx_msg void OnBnClickedFolderSelectAll();
    afx_msg void OnBnClickedFolderDeselectAll();
    afx_msg void OnBnClickedFolderDestBrowse();
    afx_msg void OnBnClickedFolderExecute();
    std::vector<FolderEntry> m_folders;
    enum class FolderOp { Rename, Move, Delete };
    FolderOp m_folderOp{FolderOp::Rename};

    // Tab 1: 文件批量处理
    void LoadFiles();
    void ApplyRules();
    void RefreshFileList();
    afx_msg void OnBnClickedFilePreview();
    afx_msg void OnBnClickedFileExecute();
    afx_msg void OnEnChangeRule();
    afx_msg void OnBnClickedRegexHelp();
    afx_msg void OnFileListRightClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnFileMarkDelete();
    afx_msg void OnFileUnmarkDelete();
    afx_msg void OnFileUnmarkAll();
    afx_msg void OnFileResetAll();
    afx_msg void OnFileChangeExt();
    std::vector<RenameEntry> m_entries;
    bool m_bPreviewDone{false};
    int m_nActiveTab{0};
};