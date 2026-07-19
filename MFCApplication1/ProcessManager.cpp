// ProcessManager.cpp: Process management functionality implementation
#include "pch.h"
#include "framework.h"
#include "ProcessManager.h"
#include <Psapi.h>

[[nodiscard]] std::vector<ProcInfo> CProcessManager::EnumerateProcesses()
{
    std::vector<ProcInfo> result;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return result;

    PROCESSENTRY32 pe32{};
    pe32.dwSize = sizeof(pe32);

    if (Process32First(hSnapshot, &pe32))
    {
        do
        {
            ProcInfo info;
            info.name = pe32.szExeFile;
            info.pid = pe32.th32ProcessID;

            // Get process path and memory
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, info.pid);
            if (hProc)
            {
                TCHAR szPath[MAX_PATH]{};
                DWORD dwSize = MAX_PATH;
                if (QueryFullProcessImageName(hProc, 0, szPath, &dwSize))
                    info.path = szPath;

                PROCESS_MEMORY_COUNTERS pmc{};
                pmc.cb = sizeof(pmc);
                if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc)))
                    info.memKB = pmc.WorkingSetSize / 1024;

                CloseHandle(hProc);
            }

            result.push_back(info);
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return result;
}

bool CProcessManager::KillProcess(DWORD pid)
{
    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProc) return false;
    BOOL ok = TerminateProcess(hProc, 0);
    CloseHandle(hProc);
    return ok != FALSE;
}

void CProcessManager::LocateProcessFile(DWORD pid)
{
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProc)
    {
        TCHAR szPath[MAX_PATH]{};
        DWORD dwSize = MAX_PATH;
        if (QueryFullProcessImageName(hProc, 0, szPath, &dwSize))
        {
            CString cmd;
            cmd.Format(_T("/select,\"%s\""), szPath);
            ::ShellExecute(nullptr, _T("open"), _T("explorer.exe"), cmd, nullptr, SW_SHOWNORMAL);
        }
        CloseHandle(hProc);
    }
}

[[nodiscard]] std::vector<StartupInfo> CProcessManager::EnumerateStartupItems()
{
    std::vector<StartupInfo> result;

    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD dwIndex = 0;
        TCHAR szName[256];
        TCHAR szData[2048];
        DWORD dwNameSize, dwDataSize, dwType;

        while (true)
        {
            dwNameSize = _countof(szName);
            dwDataSize = sizeof(szData);
            LONG lRet = RegEnumValue(hKey, dwIndex, szName, &dwNameSize, nullptr, &dwType, reinterpret_cast<LPBYTE>(szData), &dwDataSize);
            if (lRet != ERROR_SUCCESS) break;

            StartupInfo info;
            info.name = szName;
            info.cmd = szData;
            result.push_back(info);
            dwIndex++;
        }

        RegCloseKey(hKey);
    }

    return result;
}

bool CProcessManager::AddStartupItem(const CString& name, const CString& cmd)
{
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return false;

    LONG lRet = RegSetValueEx(hKey, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(static_cast<LPCTSTR>(cmd)),
        static_cast<DWORD>((cmd.GetLength() + 1) * sizeof(TCHAR)));
    RegCloseKey(hKey);
    return lRet == ERROR_SUCCESS;
}

bool CProcessManager::RemoveStartupItem(const CString& name)
{
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return false;

    LONG lRet = RegDeleteValue(hKey, name);
    RegCloseKey(hKey);
    return lRet == ERROR_SUCCESS;
}