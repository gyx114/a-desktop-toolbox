/* 
 * QR Code generator library (C++)
 * 
 * Copyright (c) Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/qr-code-generator-library
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace qrcodegen {

class QrCode final {
public:
	enum class Ecc { LOW = 0, MEDIUM = 1, QUARTILE = 2, HIGH = 3 };

	static QrCode encodeText(const char* text, Ecc ecl);
	static QrCode encodeText(const std::string& text, Ecc ecl);
	
	int getSize() const;
	bool getModule(int x, int y) const;
	
private:
	QrCode(int version, Ecc ecl, std::vector<uint8_t>&& dataCodewords, int mask);
	
	int m_version;
	int m_size;
	Ecc m_errorCorrectionLevel;
	int m_mask;
	std::vector<std::vector<bool>> m_modules;
	std::vector<std::vector<bool>> m_isFunction;
	
	static int getNumRawDataModules(int ver);
	static void drawFunctionPatterns(std::vector<std::vector<bool>>& modules, int ver);
	static void drawFormatBits(std::vector<std::vector<bool>>& modules, int ecl, int mask);
	static void applyMask(std::vector<std::vector<bool>>& modules, int mask);
	static long getPenaltyScore(const std::vector<std::vector<bool>>& modules);
	static std::vector<uint8_t> addEccAndInterleave(std::vector<uint8_t>& data, int ver, Ecc ecl);
	static void appendBits(std::vector<uint8_t>& data, uint32_t val, int len);
	static void appendBytes(const std::vector<uint8_t>& seg, int mode, std::vector<uint8_t>& result, int ver, Ecc ecl);
};

}  // namespace qrcodegen