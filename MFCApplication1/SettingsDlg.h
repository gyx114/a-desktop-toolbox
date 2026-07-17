#pragma once

class CSettingsDlg : public CDialogEx
{
public:
    CSettingsDlg(CWnd* pParent = nullptr);
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    void OnBrowseBili();
    void OnBrowseWeChat();
    void OnBrowseQQ();
    void OnBrowseVSCode();
    void OnBrowseVS();
    void OnBrowseGitBash();
    void OnBrowseYuanbao();
    void OnBrowseStudy();
    void OnBrowseDownload();
    void OnBrowseScreenshot();

private:
    void BrowseFile(UINT id, LPCTSTR title);
    void BrowseFolder(UINT id, LPCTSTR title);
    DECLARE_MESSAGE_MAP()
};