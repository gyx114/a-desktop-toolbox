#pragma once
#include "afxdialogex.h"
#include "resource.h"

class CAutoClicker;

class CScreenshotOCRDlg : public CDialogEx
{
public:
    CScreenshotOCRDlg(CWnd* pParent = nullptr, CAutoClicker* pAutoClicker = nullptr);
    enum { IDD = IDD_SCREENSHOT_OCR_DLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    DECLARE_MESSAGE_MAP()

private:
    afx_msg void OnBnClickedCapture();
    afx_msg void OnBnClickedCopyResult();
    afx_msg LRESULT OnCaptureComplete(WPARAM wParam, LPARAM lParam);

    CString RecognizeText(HBITMAP hBitmap);

    HBITMAP m_hCapturedBitmap{nullptr};
    CAutoClicker* m_pAutoClicker{nullptr};
};