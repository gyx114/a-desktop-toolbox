// OcrEngine.cpp - 隔离在非 MFC 编译单元中，避免 C++/WinRT 与 MFC 头文件冲突
// 此文件不使用预编译头

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Globalization.h>

#include <string>
#include <vector>

using namespace winrt;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Globalization;

// 尝试创建指定语言的 OCR 引擎
static OcrEngine TryCreateEngine(const wchar_t* langTag)
{
    try
    {
        auto lang = Language(langTag);
        return OcrEngine::TryCreateFromLanguage(lang);
    }
    catch (...) { return nullptr; }
}

// 尝试多种语言创建 OCR 引擎，返回第一个可用的
static OcrEngine CreateBestEngine(bool preferChinese)
{
    // 优先尝试用户系统语言
    auto engine = OcrEngine::TryCreateFromUserProfileLanguages();
    if (engine) return engine;

    // 如果用户语言不可用，按优先级尝试
    if (preferChinese)
    {
        // 中文优先
        const wchar_t* langs[] = { L"zh-Hans", L"zh-Hant", L"zh-CN", L"zh-TW", L"en", L"ja", L"ko" };
        for (auto lang : langs)
        {
            engine = TryCreateEngine(lang);
            if (engine) return engine;
        }
    }
    else
    {
        const wchar_t* langs[] = { L"en", L"zh-Hans", L"zh-CN", L"ja", L"ko" };
        for (auto lang : langs)
        {
            engine = TryCreateEngine(lang);
            if (engine) return engine;
        }
    }

    return nullptr;
}

bool OcrRecognizeFromFile(const wchar_t* filePath, wchar_t* output, int outputSize, bool preferChinese)
{
    if (!filePath || !output || outputSize <= 0) return false;

    try
    {
        // 创建 OCR 引擎（尝试多种语言）
        auto engine = CreateBestEngine(preferChinese);
        if (!engine)
        {
            wcsncpy_s(output, outputSize, L"OCR引擎创建失败，请确保系统已安装语言包。", _TRUNCATE);
            return false;
        }

        // 通过 StorageFile 加载图片
        auto file = StorageFile::GetFileFromPathAsync(filePath).get();
        auto stream = file.OpenAsync(FileAccessMode::Read).get();
        auto decoder = BitmapDecoder::CreateAsync(stream).get();
        auto frame = decoder.GetFrameAsync(0).get();

        auto swBitmap = frame.GetSoftwareBitmapAsync().get();
        if (!swBitmap)
        {
            wcsncpy_s(output, outputSize, L"无法解码截图。", _TRUNCATE);
            return false;
        }

        // 转换为 Gray8 格式（OcrEngine 要求）
        if (swBitmap.BitmapPixelFormat() != BitmapPixelFormat::Gray8)
        {
            swBitmap = SoftwareBitmap::Convert(swBitmap, BitmapPixelFormat::Gray8);
        }

        // 执行 OCR 识别
        auto ocrResult = engine.RecognizeAsync(swBitmap).get();

        std::wstring text;
        auto lines = ocrResult.Lines();
        for (uint32_t i = 0; i < lines.Size(); i++)
        {
            text += lines.GetAt(i).Text().c_str();
            text += L"\r\n";
        }

        if (text.empty())
            text = L"未能识别出文字。";

        wcsncpy_s(output, outputSize, text.c_str(), _TRUNCATE);
        return true;
    }
    catch (const winrt::hresult_error& e)
    {
        std::wstring err = L"OCR 识别失败 (0x";
        wchar_t hex[16];
        swprintf_s(hex, L"%08X)", static_cast<unsigned int>(e.code()));
        err += hex;
        wcsncpy_s(output, outputSize, err.c_str(), _TRUNCATE);
        return false;
    }
    catch (...)
    {
        wcsncpy_s(output, outputSize, L"OCR 识别失败：未知错误。", _TRUNCATE);
        return false;
    }
}