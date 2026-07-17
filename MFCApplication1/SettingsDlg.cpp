#include "pch.h"
#include "framework.h"
#include "SettingsDlg.h"
#include "resource.h"

CSettingsDlg::CSettingsDlg(CWnd* pParent /*= nullptr*/) : CDialogEx(IDD_SETTINGS_DIALOG, pParent) {}

void CSettingsDlg::OnBrowseBili() { BrowseFile(IDC_EDIT_BILI_PATH, _T("选择Bilibili可执行文件")); }
void CSettingsDlg::OnBrowseWeChat() { BrowseFile(IDC_EDIT_WECHAT_PATH, _T("选择微信可执行文件")); }
void CSettingsDlg::OnBrowseQQ() { BrowseFile(IDC_EDIT_QQ_PATH, _T("选择QQ可执行文件")); }
void CSettingsDlg::OnBrowseVSCode() { BrowseFile(IDC_EDIT_VSCODE_PATH, _T("选择VS Code可执行文件")); }
void CSettingsDlg::OnBrowseVS() { BrowseFile(IDC_EDIT_VS_PATH, _T("选择Visual Studio可执行文件")); }
void CSettingsDlg::OnBrowseGitBash() { BrowseFile(IDC_EDIT_GITBASH_PATH, _T("选择Git Bash可执行文件")); }
void CSettingsDlg::OnBrowseYuanbao() { BrowseFile(IDC_EDIT_YUANBAO_PATH, _T("选择元宝可执行文件")); }
void CSettingsDlg::OnBrowseStudy() { BrowseFolder(IDC_EDIT_STUDY_PATH, _T("选择学习文件夹")); }
void CSettingsDlg::OnBrowseDownload() { BrowseFolder(IDC_EDIT_DOWNLOAD_PATH, _T("选择下载文件夹")); }
void CSettingsDlg::OnBrowseScreenshot() { BrowseFolder(IDC_EDIT_SCREENSHOT_DIR, _T("选择截图保存目录")); }

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BROWSE_BILI, &CSettingsDlg::OnBrowseBili)
    ON_BN_CLICKED(IDC_BROWSE_WECHAT, &CSettingsDlg::OnBrowseWeChat)
    ON_BN_CLICKED(IDC_BROWSE_QQ, &CSettingsDlg::OnBrowseQQ)
    ON_BN_CLICKED(IDC_BROWSE_VSCODE, &CSettingsDlg::OnBrowseVSCode)
    ON_BN_CLICKED(IDC_BROWSE_VS, &CSettingsDlg::OnBrowseVS)
    ON_BN_CLICKED(IDC_BROWSE_GITBASH, &CSettingsDlg::OnBrowseGitBash)
    ON_BN_CLICKED(IDC_BROWSE_YUANBAO, &CSettingsDlg::OnBrowseYuanbao)
    ON_BN_CLICKED(IDC_BROWSE_STUDY, &CSettingsDlg::OnBrowseStudy)
    ON_BN_CLICKED(IDC_BROWSE_DOWNLOAD, &CSettingsDlg::OnBrowseDownload)
    ON_BN_CLICKED(IDC_BROWSE_SCREENSHOT, &CSettingsDlg::OnBrowseScreenshot)
END_MESSAGE_MAP()

BOOL CSettingsDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetDlgItemText(IDC_EDIT_DEFAULT_NAME, AfxGetApp()->GetProfileString(_T("Template"), _T("DefaultReportName"), _T("")));
    SetDlgItemText(IDC_EDIT_BILI_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("BiliPath"), _T("")));
    SetDlgItemText(IDC_EDIT_WECHAT_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("WeChatPath"), _T("")));
    SetDlgItemText(IDC_EDIT_QQ_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("QQPath"), _T("")));
    SetDlgItemText(IDC_EDIT_VSCODE_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("VSCodePath"), _T("")));
    SetDlgItemText(IDC_EDIT_VS_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("VSPath"), _T("")));
    SetDlgItemText(IDC_EDIT_GITBASH_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("GitBashPath"), _T("")));
    SetDlgItemText(IDC_EDIT_YUANBAO_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("YuanbaoPath"), _T("")));
    SetDlgItemText(IDC_EDIT_STUDY_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("StudyFolder"), _T("")));
    SetDlgItemText(IDC_EDIT_DOWNLOAD_PATH, AfxGetApp()->GetProfileString(_T("Paths"), _T("DownloadFolder"), _T("")));
    SetDlgItemText(IDC_EDIT_MOOC_URL, AfxGetApp()->GetProfileString(_T("Sites"), _T("MoocUrl"), _T("")));
    SetDlgItemText(IDC_EDIT_SDUCS_URL, AfxGetApp()->GetProfileString(_T("Sites"), _T("Sducs"), _T("")));

    // 截图保存目录，默认桌面
    CString strDefaultScreenshot;
    TCHAR szDesktop[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, 0, szDesktop)))
        strDefaultScreenshot = szDesktop;
    SetDlgItemText(IDC_EDIT_SCREENSHOT_DIR,
        AfxGetApp()->GetProfileString(_T("Paths"), _T("ScreenshotDir"), strDefaultScreenshot));

    // 连点器间隔，默认100ms
    SetDlgItemInt(IDC_EDIT_CLICK_INTERVAL,
        AfxGetApp()->GetProfileInt(_T("AutoClicker"), _T("IntervalMs"), 100));
    return TRUE;
}

void CSettingsDlg::OnOK()
{
    CString v;
    GetDlgItemText(IDC_EDIT_DEFAULT_NAME, v); AfxGetApp()->WriteProfileString(_T("Template"), _T("DefaultReportName"), v);
    GetDlgItemText(IDC_EDIT_BILI_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("BiliPath"), v);
    GetDlgItemText(IDC_EDIT_WECHAT_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("WeChatPath"), v);
    GetDlgItemText(IDC_EDIT_QQ_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("QQPath"), v);
    GetDlgItemText(IDC_EDIT_VSCODE_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("VSCodePath"), v);
    GetDlgItemText(IDC_EDIT_VS_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("VSPath"), v);
    GetDlgItemText(IDC_EDIT_GITBASH_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("GitBashPath"), v);
    GetDlgItemText(IDC_EDIT_YUANBAO_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("YuanbaoPath"), v);
    GetDlgItemText(IDC_EDIT_STUDY_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("StudyFolder"), v);
    GetDlgItemText(IDC_EDIT_DOWNLOAD_PATH, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("DownloadFolder"), v);
    GetDlgItemText(IDC_EDIT_SCREENSHOT_DIR, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("ScreenshotDir"), v);
    GetDlgItemText(IDC_EDIT_MOOC_URL, v); AfxGetApp()->WriteProfileString(_T("Sites"), _T("MoocUrl"), v);
    GetDlgItemText(IDC_EDIT_SDUCS_URL, v); AfxGetApp()->WriteProfileString(_T("Sites"), _T("Sducs"), v);
    AfxGetApp()->WriteProfileInt(_T("AutoClicker"), _T("IntervalMs"), GetDlgItemInt(IDC_EDIT_CLICK_INTERVAL));
    CDialogEx::OnOK();
}

void CSettingsDlg::BrowseFile(UINT id, LPCTSTR title)
{
    CFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, NULL, this);
    dlg.m_ofn.lpstrTitle = title;
    if (dlg.DoModal() == IDOK) SetDlgItemText(id, dlg.GetPathName());
}

void CSettingsDlg::BrowseFolder(UINT id, LPCTSTR title)
{
    CFolderPickerDialog dlg(NULL, 0, this);
    dlg.m_ofn.lpstrTitle = title;
    if (dlg.DoModal() == IDOK) SetDlgItemText(id, dlg.GetPathName());
}