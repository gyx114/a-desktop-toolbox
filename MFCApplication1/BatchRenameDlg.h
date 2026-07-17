#pragma once
#include "afxdialogex.h"
#include "resource.h"
#include <vector>
#include <filesystem>

struct RenameEntry {
    CString oldName;
    CString newName;
    std::filesystem::path fullPath;
};

class CBatchRenameDlg : public CDialogEx
{
public:
    CBatchRenameDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_BATCH_RENAME_DLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    DECLARE_MESSAGE_MAP()

private:
    afx_msg void OnBnClickedBrowse();
    afx_msg void OnBnClickedPreview();
    afx_msg void OnBnClickedExecute();
    afx_msg void OnBnClickedClear();
    afx_msg void OnBnClickedRegexHelp();
    afx_msg void OnEnChangeRule();
    afx_msg void OnDropFiles(HDROP hDrop);

    void LoadFiles(const CString& folderPath);
    void ApplyRules();
    void RefreshList();

    std::vector<RenameEntry> m_entries;
    CString m_folderPath;
    bool m_bPreviewDone{false};
};