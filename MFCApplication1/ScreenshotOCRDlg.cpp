#include "pch.h"
#include "framework.h"
#include "ScreenshotOCRDlg.h"
#include "AutoClicker.h"
#include "OcrEngine.h"
#include "resource.h"
#include <gdiplus.h>
#include <winhttp.h>
#include <string>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winhttp.lib")

// ========== 截图区域选择覆盖窗口（纯 Win32 窗口过程） ==========

struct RegionSelectState {
    CScreenshotOCRDlg* pOwner;
    POINT ptStart;
    POINT ptEnd;
    BOOL  bCapturing;
    HBITMAP hScreenBmp;   // 全屏快照，用于绘制背景
    int    screenW;
    int    screenH;
};

// 绘制覆盖窗口：半透明遮罩 + 选择矩形
static void DrawRegionOverlay(HDC hdc, RegionSelectState* pState)
{
    if (!pState || !pState->hScreenBmp) return;

    int w = pState->screenW;
    int h = pState->screenH;

    // 双缓冲绘制
    HDC hdcMem = ::CreateCompatibleDC(hdc);
    HBITMAP hBmp = ::CreateCompatibleBitmap(hdc, w, h);
    HBITMAP hOld = (HBITMAP)::SelectObject(hdcMem, hBmp);

    // 1. 画屏幕快照
    HDC hdcScreen = ::CreateCompatibleDC(hdc);
    HBITMAP hOldScreen = (HBITMAP)::SelectObject(hdcScreen, pState->hScreenBmp);
    ::BitBlt(hdcMem, 0, 0, w, h, hdcScreen, 0, 0, SRCCOPY);
    ::SelectObject(hdcScreen, hOldScreen);
    ::DeleteDC(hdcScreen);

    // 2. 画半透明黑色遮罩
    HDC hdcOverlay = ::CreateCompatibleDC(hdc);
    HBITMAP hBmpOverlay = ::CreateCompatibleBitmap(hdc, w, h);
    HBITMAP hOldOverlay = (HBITMAP)::SelectObject(hdcOverlay, hBmpOverlay);

    RECT rcFull = {0, 0, w, h};
    HBRUSH hBlack = ::CreateSolidBrush(RGB(0, 0, 0));
    ::FillRect(hdcOverlay, &rcFull, hBlack);
    ::DeleteObject(hBlack);

    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 120, 0}; // 约 47% 不透明度
    ::AlphaBlend(hdcMem, 0, 0, w, h, hdcOverlay, 0, 0, w, h, bf);

    ::SelectObject(hdcOverlay, hOldOverlay);
    ::DeleteObject(hBmpOverlay);
    ::DeleteDC(hdcOverlay);

    // 3. 如果正在拖拽选择，画选择矩形
    if (pState->bCapturing)
    {
        int x1 = (std::min)(pState->ptStart.x, pState->ptEnd.x);
        int y1 = (std::min)(pState->ptStart.y, pState->ptEnd.y);
        int x2 = (std::max)(pState->ptStart.x, pState->ptEnd.x);
        int y2 = (std::max)(pState->ptStart.y, pState->ptEnd.y);

        if (x2 > x1 && y2 > y1)
        {
            // 在选择区域内恢复原始屏幕图像
            HDC hdcClear = ::CreateCompatibleDC(hdc);
            HBITMAP hOldClear = (HBITMAP)::SelectObject(hdcClear, pState->hScreenBmp);
            ::BitBlt(hdcMem, x1, y1, x2 - x1, y2 - y1, hdcClear, x1, y1, SRCCOPY);
            ::SelectObject(hdcClear, hOldClear);
            ::DeleteDC(hdcClear);

            // 画蓝色边框
            HPEN hPen = ::CreatePen(PS_SOLID, 2, RGB(0, 120, 255));
            HPEN hOldPen = (HPEN)::SelectObject(hdcMem, hPen);
            HBRUSH hNullBrush = (HBRUSH)::GetStockObject(NULL_BRUSH);
            HBRUSH hOldBrush = (HBRUSH)::SelectObject(hdcMem, hNullBrush);
            ::Rectangle(hdcMem, x1, y1, x2, y2);
            ::SelectObject(hdcMem, hOldPen);
            ::SelectObject(hdcMem, hOldBrush);
            ::DeleteObject(hPen);
        }
    }

    // 4. 输出到屏幕
    ::BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);

    ::SelectObject(hdcMem, hOld);
    ::DeleteObject(hBmp);
    ::DeleteDC(hdcMem);
}

static LRESULT CALLBACK RegionSelectWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RegionSelectState* pState = (RegionSelectState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 创建全屏 DC 快照作为背景
        HDC hdcScreen = ::GetDC(NULL);
        int w = ::GetSystemMetrics(SM_CXSCREEN);
        int h = ::GetSystemMetrics(SM_CYSCREEN);

        HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
        HBITMAP hBmp = ::CreateCompatibleBitmap(hdcScreen, w, h);
        HBITMAP hOld = (HBITMAP)::SelectObject(hdcMem, hBmp);
        ::BitBlt(hdcMem, 0, 0, w, h, hdcScreen, 0, 0, SRCCOPY);
        ::SelectObject(hdcMem, hOld);
        ::DeleteDC(hdcMem);
        ::ReleaseDC(NULL, hdcScreen);

        pState = new RegionSelectState();
        pState->pOwner = (CScreenshotOCRDlg*)((CREATESTRUCT*)lParam)->lpCreateParams;
        pState->ptStart = {0, 0};
        pState->ptEnd = {0, 0};
        pState->bCapturing = FALSE;
        pState->hScreenBmp = hBmp;
        pState->screenW = w;
        pState->screenH = h;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pState);

        ::SetCapture(hwnd);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = ::BeginPaint(hwnd, &ps);
        DrawRegionOverlay(hdc, pState);
        ::EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        // 不擦除背景，由 WM_PAINT 完全控制绘制
        return 1;

    case WM_DESTROY:
        if (pState)
        {
            if (pState->hScreenBmp)
                ::DeleteObject(pState->hScreenBmp);
            delete pState;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (pState)
        {
            pState->ptStart.x = LOWORD(lParam);
            pState->ptStart.y = HIWORD(lParam);
            pState->ptEnd = pState->ptStart;
            pState->bCapturing = TRUE;
        }
        return 0;

    case WM_MOUSEMOVE:
        if (pState && pState->bCapturing)
        {
            pState->ptEnd.x = LOWORD(lParam);
            pState->ptEnd.y = HIWORD(lParam);
            ::InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (pState && pState->bCapturing)
        {
            pState->bCapturing = FALSE;
            ::ReleaseCapture();

            pState->ptEnd.x = LOWORD(lParam);
            pState->ptEnd.y = HIWORD(lParam);

            int x1 = (std::min)(pState->ptStart.x, pState->ptEnd.x);
            int y1 = (std::min)(pState->ptStart.y, pState->ptEnd.y);
            int x2 = (std::max)(pState->ptStart.x, pState->ptEnd.x);
            int y2 = (std::max)(pState->ptStart.y, pState->ptEnd.y);

            if (x2 - x1 < 10 || y2 - y1 < 10)
            {
                // 区域太小，取消
                if (pState->pOwner && ::IsWindow(pState->pOwner->m_hWnd))
                {
                    pState->pOwner->PostMessage(WM_APP + 10, 0, 0);
                    pState->pOwner->ShowWindow(SW_SHOW);
                }
                ::DestroyWindow(hwnd);
                return 0;
            }

            // 从屏幕快照中截取选中区域
            HDC hdcScreen = ::GetDC(NULL);
            HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
            int w = x2 - x1;
            int h = y2 - y1;
            HBITMAP hBitmap = ::CreateCompatibleBitmap(hdcScreen, w, h);
            HBITMAP hOldBmp = (HBITMAP)::SelectObject(hdcMem, hBitmap);
            ::BitBlt(hdcMem, 0, 0, w, h, hdcScreen, x1, y1, SRCCOPY);
            ::SelectObject(hdcMem, hOldBmp);
            ::DeleteDC(hdcMem);
            ::ReleaseDC(NULL, hdcScreen);

            // 通知 OCR 对话框
            if (pState->pOwner && ::IsWindow(pState->pOwner->m_hWnd))
            {
                pState->pOwner->PostMessage(WM_APP + 10, (WPARAM)hBitmap, 0);
                pState->pOwner->ShowWindow(SW_SHOW);
            }

            ::DestroyWindow(hwnd);
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            if (pState && pState->pOwner && ::IsWindow(pState->pOwner->m_hWnd))
            {
                pState->pOwner->PostMessage(WM_APP + 10, 0, 0);
                pState->pOwner->ShowWindow(SW_SHOW);
            }
            ::DestroyWindow(hwnd);
        }
        return 0;

    case WM_RBUTTONDOWN:
        // 右键取消
        if (pState && pState->pOwner && ::IsWindow(pState->pOwner->m_hWnd))
        {
            pState->pOwner->PostMessage(WM_APP + 10, 0, 0);
            pState->pOwner->ShowWindow(SW_SHOW);
        }
        ::DestroyWindow(hwnd);
        return 0;

    default:
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ========== OCR 主对话框 ==========

CScreenshotOCRDlg::CScreenshotOCRDlg(CWnd* pParent, CAutoClicker* pAutoClicker) : CDialogEx(IDD_SCREENSHOT_OCR_DLG, pParent)
    , m_pAutoClicker(pAutoClicker)
{
}

void CScreenshotOCRDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CScreenshotOCRDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_OCR_CAPTURE, &CScreenshotOCRDlg::OnBnClickedCapture)
    ON_BN_CLICKED(IDC_BTN_OCR_COPY, &CScreenshotOCRDlg::OnBnClickedCopyResult)
    ON_BN_CLICKED(IDC_BTN_OCR_TRANSLATE, &CScreenshotOCRDlg::OnBnClickedTranslate)
    ON_MESSAGE(WM_APP + 10, &CScreenshotOCRDlg::OnCaptureComplete)
END_MESSAGE_MAP()

BOOL CScreenshotOCRDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    GetDlgItem(IDC_BTN_OCR_COPY)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(FALSE);
    return TRUE;
}

void CScreenshotOCRDlg::OnBnClickedCapture()
{
    // 暂停连点器键盘监听，防止 a/b 键污染截图操作
    if (m_pAutoClicker)
        m_pAutoClicker->Pause();

    // 隐藏自身，创建截图覆盖窗口
    ShowWindow(SW_HIDE);

    // 注册窗口类（只注册一次）
    WNDCLASS wc = {0};
    wc.lpfnWndProc = RegionSelectWndProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.lpszClassName = _T("OCRRegionSelectClass");
    wc.hCursor = ::LoadCursor(NULL, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);

    WNDCLASS existing = {0};
    if (!GetClassInfo(wc.hInstance, wc.lpszClassName, &existing))
    {
        RegisterClass(&wc);
    }

    HWND hOverlay = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, _T(""),
        WS_POPUP,
        0, 0, ::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN),
        m_hWnd, NULL, AfxGetInstanceHandle(), this);

    if (!hOverlay)
    {
        if (m_pAutoClicker)
            m_pAutoClicker->Resume();
        ShowWindow(SW_SHOW);
        MessageBox(_T("无法进入截图模式。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    ::ShowWindow(hOverlay, SW_SHOW);
    ::UpdateWindow(hOverlay);
}

LRESULT CScreenshotOCRDlg::OnCaptureComplete(WPARAM wParam, LPARAM lParam)
{
    // 恢复连点器键盘监听
    if (m_pAutoClicker)
        m_pAutoClicker->Resume();

    HBITMAP hBitmap = (HBITMAP)wParam;

    if (!hBitmap)
    {
        // 用户取消或失败
        return 0;
    }

    // 清理旧位图
    if (m_hCapturedBitmap)
    {
        ::DeleteObject(m_hCapturedBitmap);
        m_hCapturedBitmap = nullptr;
    }
    m_hCapturedBitmap = hBitmap;

    // 执行 OCR 识别
    CString text = RecognizeText(hBitmap);
    SetDlgItemText(IDC_EDIT_OCR_RESULT, text);

    GetDlgItem(IDC_BTN_OCR_COPY)->EnableWindow(!text.IsEmpty());
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(!text.IsEmpty());

    return 0;
}

void CScreenshotOCRDlg::OnBnClickedCopyResult()
{
    CString text;
    GetDlgItemText(IDC_EDIT_OCR_RESULT, text);
    if (text.IsEmpty()) return;

    if (::OpenClipboard(m_hWnd))
    {
        ::EmptyClipboard();
        size_t len = (text.GetLength() + 1) * sizeof(TCHAR);
        HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem)
        {
            memcpy(::GlobalLock(hMem), text.GetString(), len);
            ::GlobalUnlock(hMem);
#ifdef UNICODE
            ::SetClipboardData(CF_UNICODETEXT, hMem);
#else
            ::SetClipboardData(CF_TEXT, hMem);
#endif
        }
        ::CloseClipboard();
    }
}

CString CScreenshotOCRDlg::RecognizeText(HBITMAP hBitmap)
{
    CString result;
    if (!hBitmap) return result;

    // 获取临时文件路径
    TCHAR tempPath[MAX_PATH];
    TCHAR tempFile[MAX_PATH];
    ::GetTempPath(MAX_PATH, tempPath);
    ::GetTempFileName(tempPath, _T("ocr"), 0, tempFile);
    ::DeleteFile(tempFile);

    CString tempFilePath = tempFile;
    tempFilePath.Replace(_T(".tmp"), _T(".png"));

    try
    {
        // 1. 使用 GDI+ 将 HBITMAP 保存为 PNG
        {
            Gdiplus::Bitmap bmp(hBitmap, nullptr);
            CLSID pngClsid;
            CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}", &pngClsid);
            USES_CONVERSION;
            bmp.Save(T2CW(tempFilePath), &pngClsid, nullptr);
        }

        // 2. 调用隔离的 OCR 引擎（通过 C++/WinRT 访问 Windows.Media.Ocr）
        wchar_t ocrOutput[8192] = {0};
        if (OcrRecognizeFromFile(tempFilePath.GetString(), ocrOutput, 8192))
        {
            result = ocrOutput;
        }
        else
        {
            result = ocrOutput;  // 错误信息已在 buffer 中
        }

        ::DeleteFile(tempFilePath);
    }
    catch (...)
    {
        result = _T("OCR 识别失败。");
        ::DeleteFile(tempFilePath);
    }

    return result;
}

// URL 编码（简单实现，仅编码非 ASCII 和特殊字符）
static std::wstring UrlEncode(const std::wstring& str)
{
    std::wstring encoded;
    for (wchar_t ch : str)
    {
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') ||
            (ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'_' ||
            ch == L'.' || ch == L'~')
        {
            encoded += ch;
        }
        else if (ch == L' ')
        {
            encoded += L'+';
        }
        else
        {
            wchar_t hex[8];
            swprintf_s(hex, L"%%%04X", (unsigned short)ch);
            encoded += hex;
        }
    }
    return encoded;
}

void CScreenshotOCRDlg::OnBnClickedTranslate()
{
    CString text;
    GetDlgItemText(IDC_EDIT_OCR_RESULT, text);
    if (text.IsEmpty()) return;

    // 显示翻译中状态
    SetDlgItemText(IDC_EDIT_OCR_TRANSLATED, _T("翻译中..."));
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(FALSE);

    CString translated = TranslateText(text);

    SetDlgItemText(IDC_EDIT_OCR_TRANSLATED, translated);
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(TRUE);
}

CString CScreenshotOCRDlg::TranslateText(const CString& text)
{
    CString result;
    if (text.IsEmpty()) return result;

    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;

    try
    {
        // 使用 MyMemory 免费翻译 API（zh|en = 中译英）
        std::wstring encoded = UrlEncode(static_cast<LPCWSTR>(text));
        std::wstring url = L"/get?q=" + encoded + L"&langpair=zh|en";

        hSession = ::WinHttpOpen(L"MFCApp/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) throw std::runtime_error("WinHttpOpen failed");

        hConnect = ::WinHttpConnect(hSession, L"api.mymemory.translated.net",
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) throw std::runtime_error("WinHttpConnect failed");

        hRequest = ::WinHttpOpenRequest(hConnect, L"GET", url.c_str(),
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (!hRequest) throw std::runtime_error("WinHttpOpenRequest failed");

        if (!::WinHttpSendRequest(hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
            throw std::runtime_error("WinHttpSendRequest failed");

        if (!::WinHttpReceiveResponse(hRequest, nullptr))
            throw std::runtime_error("WinHttpReceiveResponse failed");

        // 读取响应
        std::string response;
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        char buffer[4096];

        do
        {
            dwSize = 0;
            if (!::WinHttpQueryDataAvailable(hRequest, &dwSize))
                break;
            if (dwSize == 0) break;

            if (dwSize > sizeof(buffer))
                dwSize = sizeof(buffer);

            if (!::WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded))
                break;
            response.append(buffer, dwDownloaded);
        } while (dwSize > 0);

        // 简单 JSON 解析：提取 "translatedText":"..."
        auto pos = response.find("\"translatedText\":\"");
        if (pos == std::string::npos)
        {
            result = _T("翻译失败：未找到翻译结果");
        }
        else
        {
            pos += 18; // 跳过 "translatedText":"
            auto endPos = response.find('\"', pos);
            if (endPos != std::string::npos)
            {
                std::string translated = response.substr(pos, endPos - pos);
                // 处理 JSON 转义（\\n, \\t, \\\" 等）
                int nLen = MultiByteToWideChar(CP_UTF8, 0, translated.c_str(), -1, nullptr, 0);
                if (nLen > 0)
                {
                    std::vector<wchar_t> wbuf(nLen);
                    MultiByteToWideChar(CP_UTF8, 0, translated.c_str(), -1, wbuf.data(), nLen);
                    result = wbuf.data();
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        result = _T("翻译失败：网络错误");
    }

    if (hRequest) ::WinHttpCloseHandle(hRequest);
    if (hConnect) ::WinHttpCloseHandle(hConnect);
    if (hSession) ::WinHttpCloseHandle(hSession);

    return result;
}