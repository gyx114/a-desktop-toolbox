// pch.h: 这是预编译标头文件。
// 下方列出的文件仅编译一次，提高了将来生成的生成性能。
// 这还将影响 IntelliSense 性能，包括代码完成和许多代码浏览功能。
// 但是，如果此处列出的文件中的任何一个在生成之间有更新，它们全部都将被重新编译。
// 请勿在此处添加要频繁更新的文件，这将使得性能优势无效。

#ifndef PCH_H
#define PCH_H

// 避免 Windows min/max 宏干扰 std::min/std::max
#define NOMINMAX

// 添加要在此处预编译的标头
#include "framework.h"

// C++20 标准库
#include <format>
#include <filesystem>
#include <span>
#include <string_view>
#include <stop_token>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>
#include <memory>
#include <string>
#include <algorithm>
#include <ranges>
#include <atlimage.h>

#endif //PCH_H
