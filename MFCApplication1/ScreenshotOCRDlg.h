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
    // 框选截图：隐藏对话框→全屏截图→拖拽框选→返回选区位图
    HBITMAP CaptureRegion();

    // 后台线程：OCR 识别
    static void OcrThreadProc(HBITMAP hBitmap, HWND hNotifyWnd);

    // 后台线程：翻译（传入语言对）
    static void TranslateThreadProc(const CString& text, const CString& langPair, HWND hNotifyWnd);

    // 翻译 API 调用（带超时和语言对）
    static CString CallTranslateAPI(const CString& text, const CString& langPair, int timeoutSeconds = 10);

    // 获取当前选中的语言对（如 "zh-CN|en"）
    CString GetSelectedLangPair() const;

    bool m_bBusy{ false };

    HBITMAP m_hScreenshot{ nullptr };
    int m_screenshotWidth{ 0 };
    int m_screenshotHeight{ 0 };

    CString m_ocrText;
    CString m_translatedText;

    // 语言对映射：界面显示名 → API 参数
    static const std::pair<const wchar_t*, const wchar_t*> s_langPairs[];
    static constexpr int s_langPairCount = 6;
};