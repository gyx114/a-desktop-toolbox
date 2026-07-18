#pragma once
#include "afxdialogex.h"
#include "resource.h"

class CRegexGuideDlg : public CDialogEx
{
public:
    CRegexGuideDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_REGEX_GUIDE_DLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnCancel() override;
    virtual void PostNcDestroy() override;

    DECLARE_MESSAGE_MAP()
};