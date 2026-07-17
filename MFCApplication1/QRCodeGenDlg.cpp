#include "pch.h"
#include "framework.h"
#include "QRCodeGenDlg.h"
#include "qrcodegen.hpp"
#include "resource.h"
#include <gdiplus.h>
#include <fstream>

#pragma comment(lib, "gdiplus.lib")

CQRCodeGenDlg::CQRCodeGenDlg(CWnd* pParent) : CDialogEx(IDD_QRCODE_DLG, pParent)
{
}

void CQRCodeGenDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CQRCodeGenDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_QR_GENERATE, &CQRCodeGenDlg::OnBnClickedGenerate)
    ON_BN_CLICKED(IDC_BTN_QR_COPY, &CQRCodeGenDlg::OnBnClickedCopy)
    ON_BN_CLICKED(IDC_BTN_QR_SAVE, &CQRCodeGenDlg::OnBnClickedSave)
END_MESSAGE_MAP()

BOOL CQRCodeGenDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    GetDlgItem(IDC_BTN_QR_COPY)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_QR_SAVE)->EnableWindow(FALSE);
    SetDlgItemText(IDC_EDIT_QR_TEXT, _T("https://"));
    return TRUE;
}

void CQRCodeGenDlg::OnBnClickedGenerate()
{
    CString text;
    GetDlgItemText(IDC_EDIT_QR_TEXT, text);
    if (text.IsEmpty())
    {
        MessageBox(_T("请输入要生成二维码的文本。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }
    GenerateQRCode(text);
}

void CQRCodeGenDlg::OnBnClickedCopy()
{
    if (!m_hBitmap) return;

    if (::OpenClipboard(m_hWnd))
    {
        ::EmptyClipboard();
        ::SetClipboardData(CF_BITMAP, CopyImage(m_hBitmap, IMAGE_BITMAP, 0, 0, 0));
        ::CloseClipboard();
        MessageBox(_T("二维码已复制到剪贴板。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
    }
}

void CQRCodeGenDlg::OnBnClickedSave()
{
    if (!m_hBitmap) return;

    CFileDialog dlg(FALSE, _T("png"), _T("qrcode.png"),
        OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
        _T("PNG 图片 (*.png)|*.png|BMP 图片 (*.bmp)|*.bmp||"));
    if (dlg.DoModal() != IDOK) return;

    CString filePath = dlg.GetPathName();

    // 使用 GDI+ 保存为 PNG
    Gdiplus::Bitmap bmp(m_hBitmap, nullptr);
    CLSID pngClsid;
    CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}", &pngClsid);

    CString ext = dlg.GetFileExt();
    ext.MakeLower();
    if (ext == _T("bmp"))
    {
        CLSIDFromString(L"{557CF400-1A04-11D3-9A73-0000F81EF32E}", &pngClsid);
    }

    USES_CONVERSION;
    bmp.Save(T2CW(filePath), &pngClsid, nullptr);
    MessageBox(_T("二维码已保存。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void CQRCodeGenDlg::GenerateQRCode(const CString& text)
{
    try
    {
        CT2CA utf8Text(text);
        auto qr = qrcodegen::QrCode::encodeText(static_cast<const char*>(utf8Text), qrcodegen::QrCode::Ecc::LOW);

        int moduleSize = 4;
        HBITMAP hNewBitmap = CreateQRBitmap(qr, moduleSize);
        m_bitmapSize = CSize(qr.getSize() * moduleSize, qr.getSize() * moduleSize);

        // 显示在静态控件中：SetBitmap 返回旧位图，由我们负责删除
        CStatic* pImage = static_cast<CStatic*>(GetDlgItem(IDC_STATIC_QR_IMAGE));
        if (pImage && hNewBitmap)
        {
            HBITMAP hOld = pImage->SetBitmap(hNewBitmap);
            if (hOld)
                ::DeleteObject(hOld);
            if (m_hBitmap)
                ::DeleteObject(m_hBitmap);
            m_hBitmap = hNewBitmap;
            pImage->Invalidate();
            pImage->UpdateWindow();
        }

        GetDlgItem(IDC_BTN_QR_COPY)->EnableWindow(TRUE);
        GetDlgItem(IDC_BTN_QR_SAVE)->EnableWindow(TRUE);
    }
    catch (const std::exception& e)
    {
        CString err;
        err.Format(_T("生成二维码失败: %hs"), e.what());
        MessageBox(err, _T("错误"), MB_OK | MB_ICONERROR);
    }
}

HBITMAP CQRCodeGenDlg::CreateQRBitmap(const qrcodegen::QrCode& qr, int moduleSize)
{
    int size = qr.getSize();
    int imgSize = (size + 8) * moduleSize; // 留白边

    HDC hdcScreen = ::GetDC(NULL);
    HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = ::CreateCompatibleBitmap(hdcScreen, imgSize, imgSize);
    HBITMAP hOldBitmap = static_cast<HBITMAP>(::SelectObject(hdcMem, hBitmap));

    // 白色背景
    HBRUSH hWhiteBrush = static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
    RECT rc = {0, 0, imgSize, imgSize};
    ::FillRect(hdcMem, &rc, hWhiteBrush);

    // 黑色模块
    HBRUSH hBlackBrush = static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH));
    int margin = 4 * moduleSize;
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            if (qr.getModule(x, y))
            {
                RECT r = {margin + x * moduleSize, margin + y * moduleSize,
                          margin + (x + 1) * moduleSize, margin + (y + 1) * moduleSize};
                ::FillRect(hdcMem, &r, hBlackBrush);
            }
        }
    }

    ::SelectObject(hdcMem, hOldBitmap);
    ::DeleteDC(hdcMem);
    ::ReleaseDC(NULL, hdcScreen);

    return hBitmap;
}