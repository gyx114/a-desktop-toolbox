// Utils.h: 通用工具函数声明
#pragma once

// 格式化 Windows 错误码为可读字符串
[[nodiscard]] CString FormatLastError(DWORD err);

// 将文本复制到剪贴板
void CopyToClipboard(HWND hwnd, const CString& text);

// 检查当前进程是否以管理员权限运行
[[nodiscard]] bool IsProcessElevated();

// 弹窗询问用户是否以管理员权限重启，返回 true 表示已启动重启
[[nodiscard]] bool PromptRestartElevated();

// 从 ini 获取已保存路径，若无效则弹窗让用户选择并保存
[[nodiscard]] CString GetOrAskPath(CWnd* pParent, LPCTSTR pszKey, LPCTSTR pszTitle, bool bFolder = false);

// 从三个编辑框（时、分、秒）解析总秒数
[[nodiscard]] int ParseShutdownSeconds(CDialogEx* pDlg);

// 允许 UIPI 消息（用于跨权限窗口通信）
void AllowUIPIMessage(HWND hwnd, UINT msg, BOOL allow);

// 以当前桌面用户（非管理员）身份启动进程
bool LaunchProcessAsShellUser(LPCTSTR exe, LPCTSTR params, CString* pError = nullptr);

// 检查窗口句柄是否有效（非空且 IsWindow 为真）
[[nodiscard]] inline bool IsValidWindow(HWND hWnd) noexcept
{
    return hWnd != nullptr && ::IsWindow(hWnd);
}