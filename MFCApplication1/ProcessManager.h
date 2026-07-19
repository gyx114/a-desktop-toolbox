// ProcessManager.h: Process management functionality encapsulation
#pragma once
#include <vector>
#include <TlHelp32.h>

// Process information structure
struct ProcInfo
{
    CString name;
    DWORD pid{0};
    CString path;
    SIZE_T memKB{0};
};

// Startup item information structure
struct StartupInfo
{
    CString name;
    CString cmd;
};

// Process manager
class CProcessManager
{
public:
    // Enumerate all running processes
    [[nodiscard]] static std::vector<ProcInfo> EnumerateProcesses();

    // Terminate specified process
    static bool KillProcess(DWORD pid);

    // Locate process file in Explorer
    static void LocateProcessFile(DWORD pid);

    // Get startup item list
    [[nodiscard]] static std::vector<StartupInfo> EnumerateStartupItems();

    // Add startup item
    static bool AddStartupItem(const CString& name, const CString& cmd);

    // Remove startup item
    static bool RemoveStartupItem(const CString& name);
};