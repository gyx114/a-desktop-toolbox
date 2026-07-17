#pragma once

// 从 PNG 文件识别文字，结果写入 output 缓冲区（宽字符）
// 返回 true 表示成功，false 表示失败
bool OcrRecognizeFromFile(const wchar_t* filePath, wchar_t* output, int outputSize);