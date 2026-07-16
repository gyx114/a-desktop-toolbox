// ProcessManager.h: 进程管理功能封装
#pragma once
#include <vector>
#include <TlHelp32.h>

// 进程信息结构
struct ProcInfo
{
    CString name;
    DWORD pid{0};
    CString path;
    SIZE_T memKB{0};
};

// 启动项信息结构
struct StartupInfo
{
    CString name;
    CString cmd;
};

// 进程管理器
class CProcessManager
{
public:
    // 枚举所有运行中的进程
    [[nodiscard]] static std::vector<ProcInfo> EnumerateProcesses();

    // 结束指定进程
    static bool KillProcess(DWORD pid);

    // 在资源管理器中定位进程文件
    static void LocateProcessFile(DWORD pid);

    // 获取启动项列表
    [[nodiscard]] static std::vector<StartupInfo> EnumerateStartupItems();

    // 添加启动项
    static bool AddStartupItem(const CString& name, const CString& cmd);

    // 删除启动项
    static bool RemoveStartupItem(const CString& name);
};