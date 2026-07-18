#pragma once

#include "afxwin.h"
#include "resource.h"

// 自定义消息：OCR识别完成，wParam=0失败/1成功，lParam=PWSTR结果文本
#define WM_OCR_COMPLETE       (WM_USER + 100)
#define WM_TRANSLATE_COMPLETE (WM_USER + 101)

class CScreenshotOCRDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CScreenshotOCRDlg)

public:
    CScreenshotOCRDlg(CWnd* pParent = nullptr, class CAutoClicker* pAutoClicker = nullptr);
    virtual ~CScreenshotOCRDlg();

    enum { IDD = IDD_SCREENSHOT_OCR_DLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override;
    virtual void OnCancel() override;

    afx_msg void OnBnClickedBtnOcr();
    afx_msg void OnBnClickedBtnTranslate();
    afx_msg void OnBnClickedBtnCopy();

    afx_msg LRESULT OnOcrComplete(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTranslateComplete(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    // 图像预处理：放大 + 增强对比度
    void PreprocessBitmap(HBITMAP& hBitmap);

    // 框选截图：隐藏对话框→全屏截图→拖拽框选→返回选区位图
    HBITMAP CaptureRegion();

    // 后台线程：OCR 识别
    static void OcrThreadProc(HBITMAP hBitmap, HWND hNotifyWnd);

    // 后台线程：翻译
    static void TranslateThreadProc(const CString& text, HWND hNotifyWnd);

    // 翻译 API 调用（带超时）
    static CString CallTranslateAPI(const CString& text, int timeoutSeconds = 10);

    bool m_bBusy{false};

    HBITMAP m_hScreenshot{nullptr};
    int m_screenshotWidth{0};
    int m_screenshotHeight{0};

    CString m_ocrText;
    CString m_translatedText;
};