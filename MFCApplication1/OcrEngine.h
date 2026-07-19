#pragma once

// Recognize text from a PNG file, write result to output buffer (wide chars)
// Returns true on success, false on failure
// preferChinese: whether to prefer the Chinese OCR engine
bool OcrRecognizeFromFile(const wchar_t* filePath, wchar_t* output, int outputSize, bool preferChinese = true);