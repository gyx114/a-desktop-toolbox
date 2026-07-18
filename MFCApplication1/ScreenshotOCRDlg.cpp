#include "pch.h"
#include "framework.h"
#include "ScreenshotOCRDlg.h"
#include "resource.h"
#include "OcrEngine.h"
#include "MFCApplication1.h"

#include <gdiplus.h>
#include <thread>
#include <string>
#include <fstream>
#include <algorithm>
#include <winhttp.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winhttp.lib")

using namespace Gdiplus;

static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT num = 0, size = 0;
    ImageCodecInfo* pImageCodecInfo = nullptr;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    pImageCodecInfo = static_cast<ImageCodecInfo*>(malloc(size));
    if (!pImageCodecInfo) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

// ========== 框选覆盖层的窗口过程 ==========
struct RegionCaptureData
{
    HBITMAP hFullScreen;   // 全屏截图（只读）
    HBITMAP hBackBuffer;   // 离屏缓冲（在此合成，再一次性 BitBlt 到屏幕）
    HBITMAP hOverlayBmp;   // 32位DIB遮罩（预创建，复用）
    DWORD*  pOverlayBits;  // 遮罩像素缓冲
    POINT   ptStart;       // 拖拽起点
    POINT   ptEnd;         // 拖拽终点
    bool    bDragging;
    bool    bDone;
    RECT    rcResult;      // 选中的区域
    int     screenW, screenH;
};

static void ResetOverlayPixels(RegionCaptureData* pData)
{
    // 整个遮罩填充半透明黑色 (BGRA: A=0x78)
    DWORD* p = pData->pOverlayBits;
    int total = pData->screenW * pData->screenH;
    for (int i = 0; i < total; i++)
        p[i] = 0x78000000;
}

static LRESULT CALLBACK RegionOverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RegionCaptureData* pData = reinterpret_cast<RegionCaptureData*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (pData && pData->hFullScreen && pData->hBackBuffer)
        {
            int sw = pData->screenW, sh = pData->screenH;

            // 1. 离屏缓冲：先画全屏截图
            HDC hdcBuf = CreateCompatibleDC(hdc);
            HBITMAP hOldBuf = static_cast<HBITMAP>(SelectObject(hdcBuf, pData->hBackBuffer));

            HDC hdcSrc = CreateCompatibleDC(hdc);
            HBITMAP hOldSrc = static_cast<HBITMAP>(SelectObject(hdcSrc, pData->hFullScreen));
            BitBlt(hdcBuf, 0, 0, sw, sh, hdcSrc, 0, 0, SRCCOPY);
            SelectObject(hdcSrc, hOldSrc);
            DeleteDC(hdcSrc);

            if (pData->bDragging && pData->hOverlayBmp && pData->pOverlayBits)
            {
                int left = std::min(pData->ptStart.x, pData->ptEnd.x);
                int top = std::min(pData->ptStart.y, pData->ptEnd.y);
                int right = std::max(pData->ptStart.x, pData->ptEnd.x);
                int bottom = std::max(pData->ptStart.y, pData->ptEnd.y);

                if (left < 0) left = 0;
                if (top < 0) top = 0;
                if (right > sw) right = sw;
                if (bottom > sh) bottom = sh;

                // 2. 离屏缓冲：挖空选区 + AlphaBlend 遮罩
                for (int y = top; y < bottom; y++)
                    memset(&pData->pOverlayBits[y * sw + left], 0,
                        (right - left) * sizeof(DWORD));

                HDC hdcOverlay = CreateCompatibleDC(hdc);
                HBITMAP hOldOverlay = static_cast<HBITMAP>(SelectObject(hdcOverlay, pData->hOverlayBmp));
                BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                AlphaBlend(hdcBuf, 0, 0, sw, sh, hdcOverlay, 0, 0, sw, sh, bf);
                SelectObject(hdcOverlay, hOldOverlay);
                DeleteDC(hdcOverlay);

                // 恢复遮罩像素
                for (int y = top; y < bottom; y++)
                {
                    DWORD* row = &pData->pOverlayBits[y * sw + left];
                    for (int x = 0; x < right - left; x++)
                        row[x] = 0x78000000;
                }

                // 3. 离屏缓冲：画蓝色选框
                HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 120, 215));
                HPEN hOldPen = static_cast<HPEN>(SelectObject(hdcBuf, hPen));
                HBRUSH hNullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
                HBRUSH hOldBrush = static_cast<HBRUSH>(SelectObject(hdcBuf, hNullBrush));
                Rectangle(hdcBuf, left, top, right, bottom);
                SelectObject(hdcBuf, hOldPen);
                SelectObject(hdcBuf, hOldBrush);
                DeleteObject(hPen);
            }

            // 4. 一次性输出到屏幕（消除闪烁）
            BitBlt(hdc, 0, 0, sw, sh, hdcBuf, 0, 0, SRCCOPY);

            SelectObject(hdcBuf, hOldBuf);
            DeleteDC(hdcBuf);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;  // 不擦除背景

    case WM_LBUTTONDOWN:
        if (pData)
        {
            pData->ptStart.x = GET_X_LPARAM(lParam);
            pData->ptStart.y = GET_Y_LPARAM(lParam);
            pData->ptEnd = pData->ptStart;
            pData->bDragging = true;
            ::SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (pData && pData->bDragging)
        {
            pData->ptEnd.x = GET_X_LPARAM(lParam);
            pData->ptEnd.y = GET_Y_LPARAM(lParam);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (pData && pData->bDragging)
        {
            ::ReleaseCapture();
            pData->bDragging = false;

            int left = std::min(pData->ptStart.x, pData->ptEnd.x);
            int top = std::min(pData->ptStart.y, pData->ptEnd.y);
            int right = std::max(pData->ptStart.x, pData->ptEnd.x);
            int bottom = std::max(pData->ptStart.y, pData->ptEnd.y);

            int w = right - left;
            int h = bottom - top;

            if (w > 5 && h > 5)
            {
                pData->rcResult = { left, top, right, bottom };
                pData->bDone = true;
                ::DestroyWindow(hwnd);
            }
            else
            {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            if (pData) pData->bDone = true;
            ::DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        // 不调用 PostQuitMessage，否则会退出整个程序
        // data.bDone 标志已经能退出本地消息循环
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ========== 框选截图 ==========
HBITMAP CScreenshotOCRDlg::CaptureRegion()
{
    // 1. 全屏截图
    int screenW = ::GetSystemMetrics(SM_CXSCREEN);
    int screenH = ::GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScreen = ::GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hFullScreen = CreateCompatibleBitmap(hdcScreen, screenW, screenH);
    HBITMAP hOld = static_cast<HBITMAP>(SelectObject(hdcMem, hFullScreen));
    BitBlt(hdcMem, 0, 0, screenW, screenH, hdcScreen, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);

    // 2. 预创建 32 位 DIB 遮罩和离屏缓冲（双缓冲消除闪烁）
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = screenW;
    bmi.bmiHeader.biHeight = -screenH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    DWORD* pOverlayBits = nullptr;
    HBITMAP hOverlayBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS,
        reinterpret_cast<void**>(&pOverlayBits), nullptr, 0);
    HBITMAP hBackBuffer = CreateCompatibleBitmap(hdcScreen, screenW, screenH);
    ::ReleaseDC(nullptr, hdcScreen);

    if (pOverlayBits)
    {
        DWORD* p = pOverlayBits;
        int total = screenW * screenH;
        for (int i = 0; i < total; i++)
            p[i] = 0x78000000;
    }

    // 3. 隐藏对话框
    ShowWindow(SW_HIDE);
    Sleep(200);

    // 4. 注册覆盖层窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = RegionOverlayProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.lpszClassName = _T("OCRRegionOverlay");
    wc.hCursor = ::LoadCursor(nullptr, IDC_CROSS);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.style = CS_HREDRAW | CS_VREDRAW;

    WNDCLASS existing = {};
    if (!GetClassInfo(wc.hInstance, wc.lpszClassName, &existing))
        RegisterClass(&wc);

    // 5. 创建全屏覆盖层
    RegionCaptureData data = {};
    data.hFullScreen = hFullScreen;
    data.hBackBuffer = hBackBuffer;
    data.hOverlayBmp = hOverlayBmp;
    data.pOverlayBits = pOverlayBits;
    data.screenW = screenW;
    data.screenH = screenH;

    HWND hOverlay = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, _T(""), WS_POPUP,
        0, 0, screenW, screenH,
        nullptr, nullptr, AfxGetInstanceHandle(), nullptr);

    if (!hOverlay)
    {
        if (hBackBuffer) DeleteObject(hBackBuffer);
        if (hOverlayBmp) DeleteObject(hOverlayBmp);
        DeleteObject(hFullScreen);
        ShowWindow(SW_SHOW);
        return nullptr;
    }

    ::SetWindowLongPtr(hOverlay, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&data));
    ::ShowWindow(hOverlay, SW_SHOW);
    ::SetForegroundWindow(hOverlay);

    // 6. 消息循环等待用户完成框选
    MSG msg;
    while (!data.bDone && GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 7. 清理（窗口已销毁）
    if (hBackBuffer) DeleteObject(hBackBuffer);
    if (hOverlayBmp) DeleteObject(hOverlayBmp);

    // 8. 提取选中区域
    HBITMAP hResult = nullptr;
    if (data.bDone && data.rcResult.right > data.rcResult.left)
    {
        int w = data.rcResult.right - data.rcResult.left;
        int h = data.rcResult.bottom - data.rcResult.top;

        HDC hdcScreen2 = ::GetDC(nullptr);
        HDC hdcSrc = CreateCompatibleDC(hdcScreen2);
        HDC hdcDst = CreateCompatibleDC(hdcScreen2);
        hResult = CreateCompatibleBitmap(hdcScreen2, w, h);
        SelectObject(hdcSrc, hFullScreen);
        SelectObject(hdcDst, hResult);
        BitBlt(hdcDst, 0, 0, w, h, hdcSrc,
            data.rcResult.left, data.rcResult.top, SRCCOPY);
        DeleteDC(hdcSrc);
        DeleteDC(hdcDst);
        ::ReleaseDC(nullptr, hdcScreen2);
    }

    DeleteObject(hFullScreen);

    // 9. 恢复对话框
    ShowWindow(SW_SHOW);
    ::SetForegroundWindow(m_hWnd);

    return hResult;
}

IMPLEMENT_DYNAMIC(CScreenshotOCRDlg, CDialogEx)

CScreenshotOCRDlg::CScreenshotOCRDlg(CWnd* pParent, CAutoClicker* /*pAutoClicker*/)
    : CDialogEx(IDD_SCREENSHOT_OCR_DLG, pParent)
{
}

CScreenshotOCRDlg::~CScreenshotOCRDlg()
{
}

BEGIN_MESSAGE_MAP(CScreenshotOCRDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_OCR_CAPTURE, &CScreenshotOCRDlg::OnBnClickedBtnOcr)
    ON_BN_CLICKED(IDC_BTN_OCR_TRANSLATE, &CScreenshotOCRDlg::OnBnClickedBtnTranslate)
    ON_BN_CLICKED(IDC_BTN_OCR_COPY, &CScreenshotOCRDlg::OnBnClickedBtnCopy)
    ON_MESSAGE(WM_OCR_COMPLETE, &CScreenshotOCRDlg::OnOcrComplete)
    ON_MESSAGE(WM_TRANSLATE_COMPLETE, &CScreenshotOCRDlg::OnTranslateComplete)
END_MESSAGE_MAP()

void CScreenshotOCRDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BOOL CScreenshotOCRDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    return TRUE;
}

void CScreenshotOCRDlg::OnOK() {}
void CScreenshotOCRDlg::OnCancel()
{
    if (m_bBusy)
    {
        MessageBox(_T("正在进行 OCR 或翻译，请稍候..."), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (m_hScreenshot) DeleteObject(m_hScreenshot);
    m_hScreenshot = nullptr;
    CDialogEx::OnCancel();
}

// ========== 图像预处理 ==========
void CScreenshotOCRDlg::PreprocessBitmap(HBITMAP& hBitmap)
{
    if (!hBitmap) return;
    BITMAP bm;
    GetObject(hBitmap, sizeof(bm), &bm);
    int w = bm.bmWidth, h = bm.bmHeight;

    bool bNeedUpscale = (w < 500 && h < 500);
    int newW = bNeedUpscale ? w * 2 : w;
    int newH = bNeedUpscale ? h * 2 : h;

    HDC hdc = ::GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdc);
    HDC hdcNew = CreateCompatibleDC(hdc);
    HBITMAP hNewBmp = CreateCompatibleBitmap(hdc, newW, newH);
    SelectObject(hdcNew, hNewBmp);
    SelectObject(hdcMem, hBitmap);

    if (bNeedUpscale)
    {
        SetStretchBltMode(hdcNew, HALFTONE);
        SetBrushOrgEx(hdcNew, 0, 0, nullptr);
        StretchBlt(hdcNew, 0, 0, newW, newH, hdcMem, 0, 0, w, h, SRCCOPY);
    }
    else
    {
        BitBlt(hdcNew, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
    }

    DeleteDC(hdcMem);
    DeleteDC(hdcNew);
    ::ReleaseDC(nullptr, hdc);

    DeleteObject(hBitmap);
    hBitmap = hNewBmp;
}

// ========== 后台 OCR 线程 ==========
void CScreenshotOCRDlg::OcrThreadProc(HBITMAP hBitmap, HWND hNotifyWnd)
{
    BITMAP bm;
    GetObject(hBitmap, sizeof(bm), &bm);
    int w = bm.bmWidth, h = bm.bmHeight;

    bool bNeedUpscale = (w < 500 && h < 500);
    int newW = bNeedUpscale ? w * 2 : w;
    int newH = bNeedUpscale ? h * 2 : h;

    HDC hdc = ::GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdc);
    HDC hdcNew = CreateCompatibleDC(hdc);
    HBITMAP hNewBmp = CreateCompatibleBitmap(hdc, newW, newH);
    SelectObject(hdcNew, hNewBmp);
    SelectObject(hdcMem, hBitmap);

    if (bNeedUpscale)
    {
        SetStretchBltMode(hdcNew, HALFTONE);
        SetBrushOrgEx(hdcNew, 0, 0, nullptr);
        StretchBlt(hdcNew, 0, 0, newW, newH, hdcMem, 0, 0, w, h, SRCCOPY);
    }
    else
    {
        BitBlt(hdcNew, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
    }

    DeleteDC(hdcMem);
    DeleteDC(hdcNew);
    ::ReleaseDC(nullptr, hdc);

    WCHAR tempPath[MAX_PATH], tempFile[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    GetTempFileNameW(tempPath, L"ocr", 0, tempFile);
    CString pngPath = tempFile;
    pngPath.Replace(_T(".tmp"), _T(".png"));
    DeleteFile(pngPath);

    {
        GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
        Bitmap* bmp = Bitmap::FromHBITMAP(hNewBmp, nullptr);
        if (bmp)
        {
            CLSID clsidPng;
            GetEncoderClsid(L"image/png", &clsidPng);
            bmp->Save(pngPath, &clsidPng, nullptr);
            delete bmp;
        }
        GdiplusShutdown(gdiplusToken);
    }
    DeleteObject(hNewBmp);

    WCHAR result[4096] = {0};
    bool bSuccess = OcrRecognizeFromFile(pngPath, result, 4096, true);
    DeleteFile(pngPath);

    size_t len = wcslen(result);
    WCHAR* pResult = new WCHAR[len + 1];
    wcscpy_s(pResult, len + 1, result);
    ::PostMessage(hNotifyWnd, WM_OCR_COMPLETE, bSuccess ? 1 : 0, reinterpret_cast<LPARAM>(pResult));
}

// ========== 开始截图按钮 ==========
void CScreenshotOCRDlg::OnBnClickedBtnOcr()
{
    if (m_bBusy)
    {
        MessageBox(_T("正在处理中，请稍候..."), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // 框选截图
    HBITMAP hCapture = CaptureRegion();
    if (!hCapture)
    {
        // 用户取消了
        return;
    }

    if (m_hScreenshot) DeleteObject(m_hScreenshot);
    m_hScreenshot = hCapture;

    BITMAP bm;
    GetObject(m_hScreenshot, sizeof(bm), &bm);
    m_screenshotWidth = bm.bmWidth;
    m_screenshotHeight = bm.bmHeight;

    // 标记忙碌，禁用按钮
    m_bBusy = true;
    GetDlgItem(IDC_BTN_OCR_CAPTURE)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(FALSE);
    SetDlgItemText(IDC_EDIT_OCR_RESULT, _T("正在识别中，请稍候..."));

    // 复制位图给后台线程
    HDC hdc = ::GetDC(nullptr);
    HDC hdcSrc = CreateCompatibleDC(hdc);
    HDC hdcDst = CreateCompatibleDC(hdc);
    HBITMAP hThreadBmp = CreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight);
    SelectObject(hdcSrc, m_hScreenshot);
    SelectObject(hdcDst, hThreadBmp);
    BitBlt(hdcDst, 0, 0, bm.bmWidth, bm.bmHeight, hdcSrc, 0, 0, SRCCOPY);
    DeleteDC(hdcSrc);
    DeleteDC(hdcDst);
    ::ReleaseDC(nullptr, hdc);

    std::thread(OcrThreadProc, hThreadBmp, m_hWnd).detach();
}

// ========== OCR 完成 ==========
LRESULT CScreenshotOCRDlg::OnOcrComplete(WPARAM wParam, LPARAM lParam)
{
    m_bBusy = false;
    GetDlgItem(IDC_BTN_OCR_CAPTURE)->EnableWindow(TRUE);
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(TRUE);

    WCHAR* pResult = reinterpret_cast<WCHAR*>(lParam);
    if (pResult)
    {
        m_ocrText = pResult;
        SetDlgItemText(IDC_EDIT_OCR_RESULT, m_ocrText);
        if (m_hScreenshot) { DeleteObject(m_hScreenshot); m_hScreenshot = nullptr; }
        delete[] pResult;
    }
    return 0;
}

// ========== 后台翻译线程 ==========
void CScreenshotOCRDlg::TranslateThreadProc(const CString& text, HWND hNotifyWnd)
{
    CString result = CallTranslateAPI(text, 10);
    int len = result.GetLength();
    WCHAR* pResult = new WCHAR[len + 1];
    wcscpy_s(pResult, len + 1, result);
    ::PostMessage(hNotifyWnd, WM_TRANSLATE_COMPLETE, 1, reinterpret_cast<LPARAM>(pResult));
}

// ========== 翻译 API ==========
CString CScreenshotOCRDlg::CallTranslateAPI(const CString& text, int timeoutSeconds)
{
    std::string encodedText;
    for (int i = 0; i < text.GetLength(); i++)
    {
        WCHAR ch = text[i];
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') ||
            (ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'_' || ch == L'.' || ch == L'~')
        {
            encodedText += static_cast<char>(ch);
        }
        else if (ch == L' ')
        {
            encodedText += '+';
        }
        else
        {
            std::wstring wch(1, ch);
            int len = WideCharToMultiByte(CP_UTF8, 0, wch.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string utf8(len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wch.c_str(), -1, &utf8[0], len, nullptr, nullptr);
            for (int j = 0; j < len - 1; j++)
            {
                char hex[4];
                sprintf_s(hex, "%%%02X", static_cast<unsigned char>(utf8[j]));
                encodedText += hex;
            }
        }
    }

    std::string postData = "q=" + encodedText + "&langpair=zh-CN|en";

    HINTERNET hSession = ::WinHttpOpen(L"MFC OCR Tool/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return _T("翻译失败：无法初始化网络。");

    DWORD timeout = timeoutSeconds * 1000;
    ::WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    ::WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    ::WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    HINTERNET hConnect = ::WinHttpConnect(hSession,
        L"api.mymemory.translated.net", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { ::WinHttpCloseHandle(hSession); return _T("翻译失败：无法连接服务器。"); }

    HINTERNET hRequest = ::WinHttpOpenRequest(hConnect, L"POST", L"/get",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { ::WinHttpCloseHandle(hConnect); ::WinHttpCloseHandle(hSession); return _T("翻译失败：无法创建请求。"); }

    LPCWSTR headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
    BOOL bResult = ::WinHttpSendRequest(hRequest, headers, static_cast<DWORD>(wcslen(headers)),
        const_cast<char*>(postData.c_str()), static_cast<DWORD>(postData.size()),
        static_cast<DWORD>(postData.size()), 0);
    if (!bResult) { ::WinHttpCloseHandle(hRequest); ::WinHttpCloseHandle(hConnect); ::WinHttpCloseHandle(hSession); return _T("翻译失败：请求超时或网络错误。"); }

    bResult = ::WinHttpReceiveResponse(hRequest, nullptr);
    if (!bResult) { ::WinHttpCloseHandle(hRequest); ::WinHttpCloseHandle(hConnect); ::WinHttpCloseHandle(hSession); return _T("翻译失败：服务器无响应。"); }

    std::string response;
    DWORD dwSize = 0;
    do
    {
        dwSize = 0;
        if (!::WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize > 0)
        {
            std::string chunk(dwSize + 1, '\0');
            DWORD dwRead = 0;
            if (::WinHttpReadData(hRequest, &chunk[0], dwSize, &dwRead))
            {
                chunk.resize(dwRead);
                response += chunk;
            }
        }
    } while (dwSize > 0);

    ::WinHttpCloseHandle(hRequest);
    ::WinHttpCloseHandle(hConnect);
    ::WinHttpCloseHandle(hSession);

    if (response.empty()) return _T("翻译失败：服务器返回空响应。");

    std::string marker = "\"translatedText\":\"";
    size_t pos = response.find(marker);
    if (pos == std::string::npos)
    {
        marker = "\"responseDetails\":\"";
        pos = response.find(marker);
        if (pos != std::string::npos)
        {
            size_t end = response.find('"', pos + marker.size());
            if (end != std::string::npos)
            {
                std::string err = response.substr(pos + marker.size(), end - pos - marker.size());
                CString cserr = _T("翻译失败：");
                int wlen = MultiByteToWideChar(CP_UTF8, 0, err.c_str(), -1, nullptr, 0);
                if (wlen > 0)
                {
                    WCHAR* buf = new WCHAR[wlen];
                    MultiByteToWideChar(CP_UTF8, 0, err.c_str(), -1, buf, wlen);
                    cserr += buf;
                    delete[] buf;
                }
                return cserr;
            }
        }
        return _T("翻译失败：无法解析响应。");
    }

    size_t start = pos + marker.size();
    size_t end = response.find('"', start);
    if (end == std::string::npos) return _T("翻译失败：响应格式异常。");

    std::string translated = response.substr(start, end - start);
    std::string unescaped;
    for (size_t i = 0; i < translated.size(); i++)
    {
        if (translated[i] == '\\' && i + 1 < translated.size())
        {
            if (translated[i + 1] == 'n') { unescaped += '\n'; i++; }
            else if (translated[i + 1] == 't') { unescaped += '\t'; i++; }
            else if (translated[i + 1] == '"') { unescaped += '"'; i++; }
            else unescaped += translated[i];
        }
        else unescaped += translated[i];
    }

    CString result;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, unescaped.c_str(), -1, nullptr, 0);
    if (wlen > 0)
    {
        WCHAR* buf = new WCHAR[wlen];
        MultiByteToWideChar(CP_UTF8, 0, unescaped.c_str(), -1, buf, wlen);
        result = buf;
        delete[] buf;
    }
    if (result.IsEmpty()) return _T("翻译失败：结果为空。");
    return result;
}

// ========== 翻译按钮 ==========
void CScreenshotOCRDlg::OnBnClickedBtnTranslate()
{
    if (m_bBusy)
    {
        MessageBox(_T("正在处理中，请稍候..."), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }
    CString text;
    GetDlgItemText(IDC_EDIT_OCR_RESULT, text);
    text.Trim();
    if (text.IsEmpty())
    {
        MessageBox(_T("请先截图识别文字。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (text.Find(_T("失败")) >= 0 || text.Find(_T("错误")) >= 0)
    {
        MessageBox(_T("OCR 识别未成功，无法翻译。"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    m_bBusy = true;
    GetDlgItem(IDC_BTN_OCR_CAPTURE)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(FALSE);
    SetDlgItemText(IDC_EDIT_OCR_TRANSLATED, _T("正在翻译中，请稍候..."));
    std::thread(TranslateThreadProc, text, m_hWnd).detach();
}

// ========== 翻译完成 ==========
LRESULT CScreenshotOCRDlg::OnTranslateComplete(WPARAM /*wParam*/, LPARAM lParam)
{
    m_bBusy = false;
    GetDlgItem(IDC_BTN_OCR_CAPTURE)->EnableWindow(TRUE);
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(TRUE);

    WCHAR* pResult = reinterpret_cast<WCHAR*>(lParam);
    if (pResult)
    {
        m_translatedText = pResult;
        SetDlgItemText(IDC_EDIT_OCR_TRANSLATED, m_translatedText);
        delete[] pResult;
    }
    return 0;
}

// ========== 复制按钮 ==========
void CScreenshotOCRDlg::OnBnClickedBtnCopy()
{
    CString text;
    GetDlgItemText(IDC_EDIT_OCR_TRANSLATED, text);
    if (text.IsEmpty()) GetDlgItemText(IDC_EDIT_OCR_RESULT, text);
    if (text.IsEmpty()) { MessageBox(_T("没有可复制的内容。"), _T("提示"), MB_OK | MB_ICONINFORMATION); return; }

    if (::OpenClipboard(m_hWnd))
    {
        ::EmptyClipboard();
        int len = (text.GetLength() + 1) * sizeof(WCHAR);
        HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem)
        {
            void* pMem = ::GlobalLock(hMem);
            if (pMem) { memcpy(pMem, static_cast<LPCWSTR>(text), len); ::GlobalUnlock(hMem); }
            ::SetClipboardData(CF_UNICODETEXT, hMem);
        }
        ::CloseClipboard();
    }
}